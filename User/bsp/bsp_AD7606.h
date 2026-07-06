#ifndef __BSP_AD7606_H
#define __BSP_AD7606_H

#include "header.h"

//=======================================
// 1. AD7606 THD采样参数
//=======================================
// 正式THD采样使用20kHz、1000点；CubeMX中TIM1 PWM触发频率也必须改为20kHz。
#define AD7606_SAMPLE_RATE_HZ 20000U
#define AD7606_SAMPLE_COUNT 1000U

// 默认按AD7606 +/-5V量程换算。如硬件RANGE选择为+/-10V，请改为10.0f。
#define AD7606_INPUT_RANGE_V 5.0f

//=======================================
// 2. 引脚宏定义
//=======================================
// OS 过采样控制引脚
// PE11 --> OS0, PE12 --> OS1, PE13 --> OS2
#define AD7606OS0_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET)
#define AD7606OS0_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET)
#define AD7606OS1_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET)
#define AD7606OS1_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET)
#define AD7606OS2_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET)
#define AD7606OS2_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET)

// 转换启动引脚：PE9/PA11由TIM1 PWM触发，以下GPIO宏仅保留给手动调试。
#define AD7606_CONVST_A_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET)
#define AD7606_CONVST_A_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET)
#define AD7606_CONVST_B_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET)
#define AD7606_CONVST_B_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET)

// PA5 --> SCLK
#define AD7606_SCLK_H HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)
#define AD7606_SCLK_L HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)

// PE14 --> RESET
#define AD7606_RESET_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET)
#define AD7606_RESET_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET)

// PE8 --> CS
#define AD7606_CS_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET)
#define AD7606_CS_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET)

// PB3 --> BUSY
#define AD7606_BUSY HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)

// PA6 --> DB7/DOUTA，当前只读取CH1(uo)
#define AD7606_DOUTA HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6)

//=======================================
// 3. 外部变量声明（兼容旧代码）
//=======================================
extern volatile int16_t AD7606_Channel_Data;
extern volatile uint8_t AD7606_Data_Ready;

//=======================================
// 4. AD7606函数声明
//=======================================
void AD7606_Init(void);
void AD7606_SETOS(uint8_t osv);
void AD7606_RESET(void);

void AD7606_Frame_Start(void);
uint8_t AD7606_Frame_IsReady(void);
uint16_t AD7606_Frame_Copy(int16_t *dst, uint16_t max_len);
uint16_t AD7606_Frame_GetWriteIndex(void);
uint32_t AD7606_GetSampleRateHz(void);

#endif /* __BSP_AD7606_H */
