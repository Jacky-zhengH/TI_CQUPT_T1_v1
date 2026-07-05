#include "header.h"
#include "app_proccess.h"
//*********************************************************************************************************
extern UART_HandleTypeDef huart1; // HMI 控制串口
extern UART_HandleTypeDef huart3; // printf 调试串口
//*********************************************************************************************************
static uint8_t hmi_rx_buffer[50]; // HMI 接收缓冲区
static char debug_buffer[128];    // 电脑串口调试 接收缓冲区
static uint8_t hmi_cmd_flag = 0;  // 新指令标志位 (0 = false)
static uint16_t hmi_cmd_size = 0; // 新指令长度
//*********************************************************************************************************

//*********************************************************************************************************
/**
 * @name    HMI_Process_Init()
 * @brief   启动HMI的处理逻辑
 * @note    启动第一次DMA接收
 * @param   无
 */
void HMI_Process_Init(void)
{
    // 启动HMI串口(USART1)的空闲中断DMA接收
    // HAL_UARTEx_ReceiveToIdle_DMA(&huart1, hmi_rx_buffer, sizeof(hmi_rx_buffer));
    HAL_UART_Receive_IT(&huart1, hmi_rx_buffer, 1);
}

/**
 * @name    HMI_Send_Cmd()
 * @brief   向 HMI 串口屏 (USART1) 发送原始指令 (带 0xFF 结尾)
 * @note    可以用于比如切换页面（page pagenam）
 * @param   *cmd_string：指令（类型为char*）
 */
void HMI_Send_Cmd(const char *cmd_string)
{
    char cmd_buffer[100]; // 缓冲区
    int len = snprintf(cmd_buffer, sizeof(cmd_buffer), "%s\xff\xff\xff", cmd_string);

    // 使用阻塞式发送
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd_buffer, len, HAL_MAX_DELAY);
}

/**
 * @name    Debug_prinf()
 * @brief   自定义串口发送文本
 * @note    用于向电脑发送串口信息，用于调试
 * @param   *text:文本
 * @param   ...:可变变量
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

// 封装：向串口屏特定文本控件发送浮点数
static void HMI_Update_FloatText(const char *obj_name, float value, const char *unit)
{
    char buf[64];
    // 陶晶驰/Nextion 格式: t0.txt="1.23V"
    snprintf(buf, sizeof(buf), "%s.txt=\"%.5f %s\"", obj_name, value, unit);
    HMI_Send_Cmd(buf);
}

// 封装：向串口屏特定文本控件发送字符串
static void HMI_Update_StringText(const char *obj_name, const char *str)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", obj_name, str);
    HMI_Send_Cmd(buf);
}

/**
 * @name   Task_HMI_Display_Update
 * @brief  任务三：以固定频率刷新 HMI 屏幕显示 (避免频繁通信卡死串口)
 */
static void Task_HMI_Display_Update(void)
{
    static uint32_t display_timer = 0;

    // 每 200 毫秒刷新一次屏幕 (人眼看着很流畅，且节约资源)
    if (HAL_GetTick() - display_timer > 200)
    {
        display_timer = HAL_GetTick();

        //
        HMI_Update_FloatText("t2", 1, "V");
        HMI_Update_FloatText("t3", 2, "V");
        HMI_Update_StringText("t4", "runing...");

        Debug_printf("[Voltage] V: %7.3f V | S: %7.3f\r\n",
                     1.0, 1.0);
        Debug_printf("[second] Gain: %7.3f ", 4.0);
    }
}

static void Task_HMI_Command_Process(void)
{
    Debug_printf("flag = %d\r\n", hmi_cmd_flag);
    if (hmi_cmd_flag == 1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // 指示灯闪烁
        Debug_printf("=> RX Trig! Size:%d, Data[0]:%c (Hex:%02X)\r\n",
                     hmi_cmd_size, hmi_rx_buffer[0], hmi_rx_buffer[0]);

        // if (hmi_rx_buffer[0] == 'A') // 诉求：按下测量按钮，刷新第二问 HMI
        // {
        //     Debug_printf("=> Button A Pressed: Update Display\r\n");

        //     // 后台算好的平滑电压上屏
        //     HMI_Update_FloatText("t3", global_v1, "V");
        //     HMI_Update_FloatText("t4", global_v2, "V");
        //     HMI_Update_FloatText("t5", global_v3, "V");

        //     // 第二问参数上屏
        //     HMI_Update_FloatText("t10", global_gain, "");
        //     HMI_Update_FloatText("t11", global_rin, "R");
        //     HMI_Update_FloatText("t12", global_rout, "R");

        //     // 电脑串口打印，方便排查
        //     Debug_printf("[Meas] Gain:%.2f | Rin:%.1f | Rout:%.1f\r\n", global_gain, global_rin, global_rout);
        // }
        if (hmi_rx_buffer[0] == 'A')
        {
            HMI_Update_StringText("t5", "cmd:A");
        }
        else if (hmi_rx_buffer[0] == 'B')
        {
            HMI_Update_StringText("t5", "cmd:B");
        }
        else if (hmi_rx_buffer[0] == 'C')
        {
            HMI_Update_StringText("t5", "cmd:C");
        }
        hmi_cmd_flag = 0;
    }
}