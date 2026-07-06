#include "alog_thd.h"
#include <math.h>
#include <stddef.h>

//***************************
// 模块内部状态
//***************************
static Alog_THD_Result_t alog_thd_last_result;
static uint8_t alog_thd_has_result = 0U;

//***************************
// 内部工具函数
//***************************

/**
 * @brief   将AD7606原始码值转换为电压
 */
static float Alog_THD_RawToVolt(int16_t raw, float adc_range_v)
{
    return ((float)raw * adc_range_v) / 32768.0f;
}

/**
 * @brief   清空结果结构体
 */
static void Alog_THD_ClearResult(Alog_THD_Result_t *result)
{
    result->vpp = 0.0f;
    result->vmax = 0.0f;
    result->vmin = 0.0f;
    result->dc_mean = 0.0f;
    result->thd_percent = 0.0f;

    for (uint8_t i = 0U; i < ALOG_THD_HARMONIC_COUNT; i++)
    {
        result->harmonic_v[i] = 0.0f;
    }
}

/**
 * @brief   计算基础幅度参数
 */
static void Alog_THD_CalcBasicInfo(const int16_t *samples,
                                   uint16_t sample_count,
                                   float adc_range_v,
                                   Alog_THD_Result_t *result)
{
    float value;
    float sum = 0.0f;

    result->vmax = Alog_THD_RawToVolt(samples[0], adc_range_v);
    result->vmin = result->vmax;

    for (uint16_t i = 0U; i < sample_count; i++)
    {
        value = Alog_THD_RawToVolt(samples[i], adc_range_v);
        sum += value;

        if (value > result->vmax)
        {
            result->vmax = value;
        }
        if (value < result->vmin)
        {
            result->vmin = value;
        }
    }

    result->dc_mean = sum / (float)sample_count;
    result->vpp = result->vmax - result->vmin;
}

/**
 * @brief   使用Goertzel算法提取指定频率的峰值幅度
 * @note    输入电压先减去直流均值；输出为正弦峰值，不是RMS值。
 */
static float Alog_THD_CalcTonePeak(const int16_t *samples,
                                   uint16_t sample_count,
                                   const Alog_THD_Config_t *config,
                                   float target_freq_hz,
                                   float dc_mean)
{
    float normalized_freq = target_freq_hz / (float)config->sample_rate_hz;
    float omega = 2.0f * 3.14159265358979323846f * normalized_freq;
    float coeff = 2.0f * cosf(omega);
    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;
    float value;
    float power;

    for (uint16_t i = 0U; i < sample_count; i++)
    {
        value = Alog_THD_RawToVolt(samples[i], config->adc_range_v) - dc_mean;
        q0 = value + coeff * q1 - q2;
        q2 = q1;
        q1 = q0;
    }

    power = q1 * q1 + q2 * q2 - coeff * q1 * q2;
    if (power <= 0.0f)
    {
        return 0.0f;
    }

    return (2.0f * sqrtf(power)) / (float)sample_count;
}

/**
 * @brief   检查输入参数是否合法
 */
static Alog_THD_Status_t Alog_THD_CheckParam(const int16_t *samples,
                                             uint16_t sample_count,
                                             const Alog_THD_Config_t *config,
                                             const Alog_THD_Result_t *result)
{
    if ((samples == NULL) || (config == NULL) || (result == NULL))
    {
        return ALOG_THD_ERR_NULL;
    }

    if ((sample_count == 0U) ||
        (config->sample_rate_hz == 0U) ||
        (config->target_freq_hz <= 0.0f) ||
        (config->adc_range_v <= 0.0f) ||
        (config->min_fundamental_v < 0.0f))
    {
        return ALOG_THD_ERR_PARAM;
    }

    if ((config->target_freq_hz * (float)ALOG_THD_HARMONIC_COUNT) >=
        ((float)config->sample_rate_hz * 0.5f))
    {
        return ALOG_THD_ERR_PARAM;
    }

    return ALOG_THD_OK;
}

//***************************
// 对外函数接口
//***************************

Alog_THD_Status_t Alog_THD_Init(Alog_THD_Config_t *config)
{
    if (config == NULL)
    {
        return ALOG_THD_ERR_NULL;
    }

    config->sample_rate_hz = 20000U;
    config->target_freq_hz = 1000.0f;
    config->adc_range_v = 5.0f;
    config->min_fundamental_v = 0.02f;

    Alog_THD_ClearResult(&alog_thd_last_result);
    alog_thd_has_result = 0U;

    return ALOG_THD_OK;
}

Alog_THD_Status_t Alog_THD_Calc(const int16_t *samples,
                                uint16_t sample_count,
                                const Alog_THD_Config_t *config,
                                Alog_THD_Result_t *result)
{
    Alog_THD_Status_t status;
    float harmonic_sum_sq = 0.0f;

    status = Alog_THD_CheckParam(samples, sample_count, config, result);
    if (status != ALOG_THD_OK)
    {
        return status;
    }

    Alog_THD_ClearResult(result);
    Alog_THD_CalcBasicInfo(samples, sample_count, config->adc_range_v, result);

    for (uint8_t i = 0U; i < ALOG_THD_HARMONIC_COUNT; i++)
    {
        result->harmonic_v[i] = Alog_THD_CalcTonePeak(samples,
                                                      sample_count,
                                                      config,
                                                      config->target_freq_hz * (float)(i + 1U),
                                                      result->dc_mean);
    }

    if (result->harmonic_v[0] < config->min_fundamental_v)
    {
        result->thd_percent = 0.0f;
        alog_thd_last_result = *result;
        alog_thd_has_result = 1U;
        return ALOG_THD_ERR_NO_SIGNAL;
    }

    for (uint8_t i = 1U; i < ALOG_THD_HARMONIC_COUNT; i++)
    {
        harmonic_sum_sq += result->harmonic_v[i] * result->harmonic_v[i];
    }

    result->thd_percent = (sqrtf(harmonic_sum_sq) / result->harmonic_v[0]) * 100.0f;
    alog_thd_last_result = *result;
    alog_thd_has_result = 1U;

    return ALOG_THD_OK;
}

Alog_THD_Status_t Alog_THD_GetLastResult(Alog_THD_Result_t *result)
{
    if (result == NULL)
    {
        return ALOG_THD_ERR_NULL;
    }

    if (alog_thd_has_result == 0U)
    {
        Alog_THD_ClearResult(result);
        return ALOG_THD_ERR_NO_SIGNAL;
    }

    *result = alog_thd_last_result;
    return ALOG_THD_OK;
}
