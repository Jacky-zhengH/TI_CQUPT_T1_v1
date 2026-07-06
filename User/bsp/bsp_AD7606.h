#ifndef __BSP_AD7606_H
#define __BSP_AD7606_H

#include "header.h"
//=======================================
// 1.引脚宏定义
//=======================================
// OS 过采样控制引脚
//  PE11 --> OS0
//  PE12 --> OS1
//  PE13 --> OS2
#define AD7606OS0_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET)
#define AD7606OS0_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_RESET)
#define AD7606OS1_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET)
#define AD7606OS1_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET)
#define AD7606OS2_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_SET)
#define AD7606OS2_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_13, GPIO_PIN_RESET)
// 转换启动引脚
//  PE9 --> CONVA
//  PA11 --> CONVB -->修改为使用定时器PWM触发
// （以下有关CONV的宏定义暂时没有作用）
#define AD7606_CONVST_A_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET)
#define AD7606_CONVST_A_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET)
#define AD7606_CONVST_B_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET)
#define AD7606_CONVST_B_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET)
// 时钟
//  PA5 --> SCLK
#define AD7606_SCLK_H HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)
#define AD7606_SCLK_L HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)
// 置位标志位
//  PE14 --> RESET
#define AD7606_RESET_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET)
#define AD7606_RESET_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_RESET)
// 数字片选
//  PE8 --> CS
#define AD7606_CS_H HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET)
#define AD7606_CS_L HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET)
// BUSY（读取）标志位
// PB3 --> BUSY
#define AD7606_BUSY HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) // GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3)
// 串行数据读取（1~4通道）
// PA6 --> DB7:DOUTA
// 串行出口（B）DB8:DOUTB
#define AD7606_DOUTA HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) // GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4)
// #define AD7606_DOUTB HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) // GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5)

//=======================================
// 2.外部变量声明
//=======================================

extern volatile int16_t AD7606_Channel_Data;
extern volatile uint8_t AD7606_Data_Ready;
//=======================================
// 3. AD7606函数声明
//=======================================

void AD7606_Init(void);
// void AD7606_STARTCONV(void);
void AD7606_SETOS(uint8_t osv);
void AD7606_RESET(void);
// void AD7606_Sample_Task(void);

#endif /* __BSP_AD7606_H */
