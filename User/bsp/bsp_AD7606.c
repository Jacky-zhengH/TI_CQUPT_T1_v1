#include "bsp_AD7606.h"
#include "tim.h"

//=======================================
// 变量声明
//=======================================
volatile int16_t AD7606_Channel_Data = 0; // 最近一次CH1采样值
volatile uint8_t AD7606_Data_Ready = 0;   // 单点采样标志，兼容旧代码

static volatile int16_t ad7606_sample_buffer[AD7606_SAMPLE_COUNT];
static volatile uint16_t ad7606_sample_index = 0;
static volatile uint8_t ad7606_frame_ready = 0;
static volatile uint8_t ad7606_frame_running = 0;

//*********************************************************************************************************

/**
 * @brief   微秒级/纳秒级短延时（软件阻塞）
 * @param   nCount 延时长度
 */
static void Delay_u32(uint32_t nCount)
{
    while (nCount--)
    {
        __NOP();
    }
}

/**
 * @brief   进入短临界区
 */
static uint32_t AD7606_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/**
 * @brief   退出短临界区
 */
static void AD7606_ExitCritical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/**
 * @brief   AD7606硬件复位函数
 */
void AD7606_RESET(void)
{
    AD7606_RESET_L;
    Delay_u32(10);
    AD7606_RESET_H;
    Delay_u32(100);
    AD7606_RESET_L;
    Delay_u32(100);
}

/**
 * @brief   设置过采样率
 * @param   osv 范围[0,6]，对应[无过采样,2,4,8,16,32,64]
 */
void AD7606_SETOS(uint8_t osv)
{
    switch (osv)
    {
    case 0:
        AD7606OS0_L;
        AD7606OS1_L;
        AD7606OS2_L;
        break;
    case 1:
        AD7606OS0_H;
        AD7606OS1_L;
        AD7606OS2_L;
        break;
    case 2:
        AD7606OS0_L;
        AD7606OS1_H;
        AD7606OS2_L;
        break;
    case 3:
        AD7606OS0_H;
        AD7606OS1_H;
        AD7606OS2_L;
        break;
    case 4:
        AD7606OS0_L;
        AD7606OS1_L;
        AD7606OS2_H;
        break;
    case 5:
        AD7606OS0_H;
        AD7606OS1_L;
        AD7606OS2_H;
        break;
    case 6:
        AD7606OS0_L;
        AD7606OS1_H;
        AD7606OS2_H;
        break;
    default:
        AD7606OS0_L;
        AD7606OS1_L;
        AD7606OS2_L;
        break;
    }
}

/**
 * @brief   AD7606初始化函数
 * @note    TIM1 PWM负责周期触发CONVST，BUSY中断负责读取DOUTA。
 */
void AD7606_Init(void)
{
    AD7606_CS_H;
    AD7606_SCLK_H;

    // THD测量需要稳定采样时钟，默认关闭过采样，避免降低有效采样率。
    AD7606_SETOS(0);
    AD7606_RESET();
    AD7606_Frame_Start();

    // 启动CONV_A和CONV_B PWM触发。
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

/**
 * @brief   AD7606单通道数据读取函数
 * @note    当前只读取CH1，对应被测输出uo。
 */
static int16_t AD7606_OneChanel_ReadBytes(void)
{
    uint16_t usData = 0;

    for (uint8_t i = 0; i < 16; i++)
    {
        AD7606_SCLK_L;
        Delay_u32(5);
        usData <<= 1;
        if (AD7606_DOUTA == GPIO_PIN_SET)
        {
            usData |= 0x0001U;
        }
        AD7606_SCLK_H;
        Delay_u32(5);
    }

    return (int16_t)usData;
}

/**
 * @brief   开始采集一帧THD分析数据
 */
void AD7606_Frame_Start(void)
{
    uint32_t primask = AD7606_EnterCritical();

    ad7606_sample_index = 0;
    ad7606_frame_ready = 0;
    ad7606_frame_running = 1;
    AD7606_Data_Ready = 0;

    AD7606_ExitCritical(primask);
}

/**
 * @brief   查询一帧采样是否完成
 */
uint8_t AD7606_Frame_IsReady(void)
{
    return ad7606_frame_ready;
}

/**
 * @brief   复制完整采样帧到应用层缓冲区
 * @param   dst 目标缓冲区
 * @param   max_len 目标缓冲区长度
 * @return  实际复制点数
 */
uint16_t AD7606_Frame_Copy(int16_t *dst, uint16_t max_len)
{
    uint16_t copy_len;
    uint32_t primask;

    if ((dst == NULL) || (ad7606_frame_ready == 0U))
    {
        return 0;
    }

    copy_len = AD7606_SAMPLE_COUNT;
    if (copy_len > max_len)
    {
        copy_len = max_len;
    }

    // 帧完成后中断不再写入缓冲区；这里仍短暂关中断，避免状态被重新启动采样打断。
    primask = AD7606_EnterCritical();
    for (uint16_t i = 0; i < copy_len; i++)
    {
        dst[i] = ad7606_sample_buffer[i];
    }
    AD7606_ExitCritical(primask);

    return copy_len;
}

/**
 * @brief   获取当前帧已写入点数，便于调试采样进度
 */
uint16_t AD7606_Frame_GetWriteIndex(void)
{
    uint16_t index;
    uint32_t primask = AD7606_EnterCritical();

    index = ad7606_sample_index;
    AD7606_ExitCritical(primask);

    return index;
}

/**
 * @brief   获取当前软件认为的采样率
 */
uint32_t AD7606_GetSampleRateHz(void)
{
    return AD7606_SAMPLE_RATE_HZ;
}

//*********************************************************************************************************
// GPIO EXTI 中断回调
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    int16_t sample;

    if (GPIO_Pin == GPIO_PIN_3)
    {
        if ((ad7606_frame_running == 0U) && (ad7606_frame_ready != 0U))
        {
            return;
        }
        AD7606_CS_L;
        sample = AD7606_OneChanel_ReadBytes();
        AD7606_CS_H;

        AD7606_Channel_Data = sample;
        AD7606_Data_Ready = 1;

        if ((ad7606_frame_running != 0U) && (ad7606_frame_ready == 0U))
        {
            ad7606_sample_buffer[ad7606_sample_index] = sample;
            ad7606_sample_index++;

            if (ad7606_sample_index >= AD7606_SAMPLE_COUNT)
            {
                ad7606_frame_running = 0;
                ad7606_frame_ready = 1;
            }
        }
    }
}
//*********************************************************************************************************
