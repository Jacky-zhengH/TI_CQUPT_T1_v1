#include "header.h"
#include "app_proccess.h"
#include "bsp_AD7606.h"
#include "alog_thd.h"

//*********************************************************************************************************
extern UART_HandleTypeDef huart1; // HMI 控制串口
extern UART_HandleTypeDef huart3; // PC 调试串口
//*********************************************************************************************************

typedef enum
{
    APP_MEAS_IDLE = 0,
    APP_MEAS_REFRESH_UO,
    APP_MEAS_WAIT_THD_FRAME,
    APP_MEAS_THD_FRAME_READY
} App_Measure_State_t;

static uint8_t hmi_rx_buffer[1];          // HMI 单字节命令接收缓冲区
static char debug_buffer[160];            // PC串口调试发送缓冲区
static volatile uint8_t hmi_cmd_flag = 0; // 新命令标志位
static volatile uint8_t hmi_cmd_data = 0; // 最新命令字节

static int16_t app_sample_buffer[AD7606_SAMPLE_COUNT];
static uint8_t app_frame_valid = 0;
static uint8_t app_thd_request = 0;
static uint32_t app_frame_counter = 0;

static float app_uo_vpp = 0.0f;
static float app_uo_vmax = 0.0f;
static float app_uo_vmin = 0.0f;
static App_Measure_State_t app_measure_state = APP_MEAS_IDLE;

static Alog_THD_Config_t app_thd_config;
static Alog_THD_Result_t app_thd_result;
static Alog_THD_Status_t app_thd_status = ALOG_THD_ERR_NO_SIGNAL;
static uint8_t app_measure_result_valid = 0;

//=========================================================================================================
// 1. 基础功能函数
//=========================================================================================================
/**
 * @name HMI_Process_Init
 * @brief 启动应用层接收与测量任务框架
 */
void HMI_Process_Init(void)
{
    HAL_UART_Receive_IT(&huart1, hmi_rx_buffer, 1);
    app_thd_status = Alog_THD_Init(&app_thd_config);
    app_thd_config.sample_rate_hz = AD7606_GetSampleRateHz();
    app_thd_config.target_freq_hz = 1000.0f;
    app_thd_config.adc_range_v = AD7606_INPUT_RANGE_V;
    app_thd_config.min_fundamental_v = 0.02f;

    AD7606_Frame_Start();
    app_measure_state = APP_MEAS_REFRESH_UO;
    Debug_printf("[APP] UART1 RX started, measure tasks ready. Fs=%luHz, N=%u, range=+/-%.1fV, alog=%d\r\n",
                 (unsigned long)app_thd_config.sample_rate_hz,
                 (unsigned int)AD7606_SAMPLE_COUNT,
                 app_thd_config.adc_range_v,
                 app_thd_status);
}

/**
 * @name    HMI_Send_Cmd
 * @brief   向HMI串口屏发送原始指令
 */
void HMI_Send_Cmd(const char *cmd_string)
{
    char cmd_buffer[100];
    int len = snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\xff\xff\xff", cmd_string);

    if (len > 0)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)cmd_buffer, len, HAL_MAX_DELAY);
    }
}

/**
 * @name    Debug_printf
 * @brief   PC串口调试打印
 */
void Debug_printf(const char *text, ...)
{
    va_list args;
    va_start(args, text);
    int len = vsnprintf(debug_buffer, sizeof(debug_buffer), text, args);
    va_end(args);

    if (len > 0)
    {
        HAL_UART_Transmit(&huart3, (uint8_t *)debug_buffer, len, 100);
    }
}

//=========================================================================================================
// 3. 应用任务函数
//=========================================================================================================
/**
 * @brief   按键响应任务：A键请求THD计算帧，S键重新开始采样
 */
static void Task_Button_Response(void)
{
    uint8_t cmd;

    if (hmi_cmd_flag == 0U)
    {
        return;
    }

    __disable_irq();
    cmd = hmi_cmd_data;
    hmi_cmd_flag = 0;
    __enable_irq();

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    Debug_printf("[KEY] RX cmd='%c' hex=0x%02X\r\n", cmd, cmd);

    if (cmd == 'A')
    {
        app_thd_request = 1;
        app_frame_valid = 0;
        app_measure_state = APP_MEAS_WAIT_THD_FRAME;
        AD7606_Frame_Start();
        Debug_printf("[KEY] THD button accepted, capturing one full frame for later algorithm.\r\n");
    }
    else if (cmd == 'S')
    {
        app_frame_valid = 0;
        app_thd_request = 0;
        app_measure_state = APP_MEAS_REFRESH_UO;
        AD7606_Frame_Start();
        Debug_printf("[KEY] Sample frame restarted manually.\r\n");
    }
    else
    {
        Debug_printf("[KEY] Unknown cmd, use 'A'=capture THD frame, 'S'=restart sample.\r\n");
    }
}

/**
 * @brief   幅度刷新任务：持续用完整采样帧刷新uo峰峰值
 */
static void Task_Uo_Amplitude_Refresh(void)
{
    uint16_t copy_len;

    if (AD7606_Frame_IsReady() == 0U)
    {
        return;
    }

    copy_len = AD7606_Frame_Copy(app_sample_buffer, AD7606_SAMPLE_COUNT);
    if (copy_len == 0U)
    {
        return;
    }

    // 在主循环中调用算法模块，先使用其基础幅度结果刷新uo。
    app_thd_status = Alog_THD_Calc(app_sample_buffer, copy_len, &app_thd_config, &app_thd_result);
    app_uo_vpp = app_thd_result.vpp;
    app_uo_vmax = app_thd_result.vmax;
    app_uo_vmin = app_thd_result.vmin;
    app_measure_result_valid = 1;
    app_frame_valid = 1;
    app_frame_counter++;

    Debug_printf("[UO] frame=%lu len=%u Vmin=%.4fV Vmax=%.4fV Vpp=%.4fV status=%d\r\n",
                 (unsigned long)app_frame_counter,
                 (unsigned int)copy_len,
                 app_uo_vmin,
                 app_uo_vmax,
                 app_uo_vpp,
                 app_thd_status);

    if (app_thd_request == 0U)
    {
        app_frame_valid = 0;
        app_measure_state = APP_MEAS_REFRESH_UO;
        AD7606_Frame_Start();
    }
    else
    {
        app_measure_state = APP_MEAS_THD_FRAME_READY;
    }
}

/**
 * @brief   THD请求任务：只搭建触发框架，算法后续放到User/alog实现
 */
static void Task_THD_Request_Process(void)
{
    if ((app_thd_request == 0U) || (app_frame_valid == 0U))
    {
        return;
    }

    Debug_printf("[THD] frame ready: Vpp=%.4fV THD=%.2f%% status=%d\r\n",
                 app_thd_result.vpp,
                 app_thd_result.thd_percent,
                 app_thd_status);

    app_thd_request = 0;
    app_frame_valid = 0;
    app_measure_state = APP_MEAS_REFRESH_UO;
    AD7606_Frame_Start();
}

/**
 * @brief   HMI幅度显示任务：周期刷新uo峰峰值，避免串口屏刷新过快
 */
static void Task_HMI_Amplitude_Display(void)
{
    static uint32_t hmi_display_timer = 0;
    char hmi_cmd[64];

    if (app_measure_result_valid == 0U)
    {
        return;
    }

    if (HAL_GetTick() - hmi_display_timer < 500U)
    {
        return;
    }
    hmi_display_timer = HAL_GetTick();

    // 将uo幅度值写入串口屏t2文本控件，显示两位小数。
    snprintf(hmi_cmd, sizeof(hmi_cmd), "t2.txt=\"%.2fV\"", app_uo_vpp);
    HMI_Send_Cmd(hmi_cmd);
}

/**
 * @brief   数据状态任务：仅通过PC串口输出，不写HMI显示代码
 */
static void Task_Data_Status_Debug(void)
{
    static uint32_t debug_timer = 0;
    const char *state_text = "IDLE";

    if (HAL_GetTick() - debug_timer < 500U)
    {
        return;
    }
    debug_timer = HAL_GetTick();

    if (app_measure_state == APP_MEAS_REFRESH_UO)
    {
        state_text = "REFRESH_UO";
    }
    else if (app_measure_state == APP_MEAS_WAIT_THD_FRAME)
    {
        state_text = "WAIT_THD";
    }
    else if (app_measure_state == APP_MEAS_THD_FRAME_READY)
    {
        state_text = "THD_FRAME_READY";
    }

    Debug_printf("[STAT] uo_vpp=%.4fV state=%s sample=%u/%u alog=%d\r\n",
                 app_uo_vpp,
                 state_text,
                 (unsigned int)AD7606_Frame_GetWriteIndex(),
                 (unsigned int)AD7606_SAMPLE_COUNT,
                 app_thd_status);
}

//=========================================================================================================
// 4. 主轮询整合与中断回调
//=========================================================================================================
/**
 * @name   App_Main_Process_Poll
 * @brief  放在main.c的while(1)中，统筹调度所有应用任务
 */
void App_Main_Process_Poll(void)
{
    Task_Button_Response();
    Task_Uo_Amplitude_Refresh();
    Task_THD_Request_Process();
    Task_HMI_Amplitude_Display();
    Task_Data_Status_Debug();
}

/**
 * @brief USART接收完成回调，只保存命令并重启接收
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        hmi_cmd_data = hmi_rx_buffer[0];
        hmi_cmd_flag = 1;
        HAL_UART_Receive_IT(huart, hmi_rx_buffer, 1);
    }
}
