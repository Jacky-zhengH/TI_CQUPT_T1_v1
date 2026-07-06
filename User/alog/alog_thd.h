#ifndef __ALOG_THD_H
#define __ALOG_THD_H

#include <stdint.h>

//***************************
// THD算法公共参数
//***************************
#define ALOG_THD_HARMONIC_COUNT 5U

typedef enum
{
    ALOG_THD_OK = 0,           // 计算成功
    ALOG_THD_ERR_NULL,         // 空指针错误
    ALOG_THD_ERR_PARAM,        // 参数错误
    ALOG_THD_ERR_NO_SIGNAL     // 基波过小，认为无有效信号
} Alog_THD_Status_t;

typedef struct
{
    uint32_t sample_rate_hz;   // 采样率，单位Hz
    float target_freq_hz;      // 基波目标频率，赛题默认为1kHz
    float adc_range_v;         // AD7606正负量程幅值，例如+/-5V填5.0f
    float min_fundamental_v;   // 最小有效基波峰值，低于该值认为无信号
} Alog_THD_Config_t;

typedef struct
{
    float vpp;                                      // 峰峰值，单位V
    float vmax;                                     // 最大值，单位V
    float vmin;                                     // 最小值，单位V
    float dc_mean;                                  // 直流均值，单位V
    float harmonic_v[ALOG_THD_HARMONIC_COUNT];      // 1~5次谐波峰值，单位V
    float thd_percent;                              // THD百分比
} Alog_THD_Result_t;

//***************************
// 对外函数接口
//***************************

/**
 * @brief   初始化THD算法模块默认参数
 * @param   config 算法配置结构体
 * @return  ALOG_THD_OK表示成功
 */
Alog_THD_Status_t Alog_THD_Init(Alog_THD_Config_t *config);

/**
 * @brief   根据一帧AD7606原始采样计算Vpp和1~5次谐波THD
 * @param   samples AD7606原始码值数组
 * @param   sample_count 采样点数
 * @param   config 算法配置
 * @param   result 计算结果
 * @return  计算状态
 */
Alog_THD_Status_t Alog_THD_Calc(const int16_t *samples,
                                uint16_t sample_count,
                                const Alog_THD_Config_t *config,
                                Alog_THD_Result_t *result);

/**
 * @brief   读取最近一次THD计算结果
 * @param   result 结果输出指针
 * @return  ALOG_THD_OK表示已有有效结果
 */
Alog_THD_Status_t Alog_THD_GetLastResult(Alog_THD_Result_t *result);

#endif /* __ALOG_THD_H */
