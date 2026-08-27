#include "mq_senser.h"
#include "adc.h"
#include <math.h>
#include "cmsis_os.h"
#include <string.h>

/* ADC1 DMA 双通道采集结果（ADC 中断自动更新） */
uint16_t MQ_Adc_Result[2] = {0};

/* 滑动平均滤波窗口大小 */
#define MQ_FILTER_WIN   10

/* MQ-136 滤波器状态（硫化氢） */
static uint16_t mq136_buf[MQ_FILTER_WIN];
static uint8_t  mq136_idx = 0;
static uint32_t mq136_sum = 0;
static uint8_t  mq136_fill = 0;   /* 预热计数：当前已填充的有效样本数 */

/* MQ-137 滤波器状态（氨气） */
static uint16_t mq137_buf[MQ_FILTER_WIN];
static uint8_t  mq137_idx = 0;
static uint32_t mq137_sum = 0;
static uint8_t  mq137_fill = 0;

/**
  * @brief  滑动平均滤波器初始化
  */
void MQ_Filter_Init(void)
{
    memset(mq136_buf, 0, sizeof(mq136_buf));
    memset(mq137_buf, 0, sizeof(mq137_buf));
    mq136_idx = mq137_idx = 0;
    mq136_sum = mq137_sum = 0;
    mq136_fill = mq137_fill = 0;
}

/**
  * @brief  滑动平均滤波（无取模运算，支持预热期）
  * @param  new_val  新采样值
  * @param  buf      环形缓冲区
  * @param  idx      当前写入位置
  * @param  sum      窗口内数值和
  * @param  fill     已填充样本数（预热期用）
  * @retval uint16_t 滤波后输出值
  * @note   前 10 次按实际填充数平均，避免输出被 0 拉低
  *         使用指针传参实现双通道复用，节省代码空间
  */
static inline uint16_t MQ_Filter(uint16_t new_val, uint16_t *buf,
                                  uint8_t *idx, uint32_t *sum,
                                  uint8_t *fill)
{
    *sum -= buf[*idx];
    buf[*idx] = new_val;
    *sum += new_val;

    if(++(*idx) >= MQ_FILTER_WIN) *idx = 0;
    if(*fill < MQ_FILTER_WIN)      (*fill)++;

    return (uint16_t)(*sum / *fill);
}

/**
  * @brief  ADC 采样值转电压
  * @param  adc_val  12 位 ADC 采样值（0~4095）
  * @retval float    电压值（V）
  */
static inline float ADC_to_Voltage(uint16_t adc_val)
{
    return (float)adc_val * (MQ_ADC_REF / MQ_ADC_RES);
}

/**
  * @brief  计算传感器负载电阻 Rs
  * @param  voltage  传感器输出电压
  * @retval float    Rs 阻值（Ω）
  * @note   公式：Rs = RL × (Vcc - Vout) / Vout
  *         电压下限钳位 0.01V，防止除零
  */
static inline float MQ_Calc_Rs(float voltage)
{
    if(voltage <= 0.01f) voltage = 0.01f;
    return MQ_RL * (MQ_ADC_REF - voltage) / voltage;
}

/**
  * @brief  MQ-136 硫化氢浓度计算
  * @param  adc_val  滤波后的 ADC 采样值
  * @retval float    H2S 浓度（ppm）
  * @note   使用厂商标定曲线：ppm = 16 × (Rs/R0)^(-2.8)
  *         输出范围钳位 0~100 ppm
  */
float MQ136_Get_H2S_ppm(uint16_t adc_val)
{
    float voltage = ADC_to_Voltage(adc_val);
    float Rs      = MQ_Calc_Rs(voltage);
    float ratio   = Rs / MQ136_R0;

    /* powf：单精度浮点幂函数，比 double 的 pow() 更适合无 FPU 的 F103 */
    float ppm = 16.0f * powf(ratio, -2.8f);

    if(ppm < 0.0f)   ppm = 0.0f;
    if(ppm > 100.0f) ppm = 100.0f;
    return ppm;
}

/**
  * @brief  MQ-137 氨气浓度计算
  * @param  adc_val  滤波后的 ADC 采样值
  * @retval float    NH3 浓度（ppm）
  * @note   使用厂商标定曲线：ppm = 25 × (Rs/R0)^(-3.0)
  *         输出范围钳位 0~200 ppm
  */
float MQ137_Get_NH3_ppm(uint16_t adc_val)
{
    float voltage = ADC_to_Voltage(adc_val);
    float Rs      = MQ_Calc_Rs(voltage);
    float ratio   = Rs / MQ137_R0;

    float ppm = 25.0f * powf(ratio, -3.0f);

    if(ppm < 0.0f)   ppm = 0.0f;
    if(ppm > 200.0f) ppm = 200.0f;
    return ppm;
}

/**
  * @brief  获取 MQ 传感器最终数据（带滤波和报警判断）
  * @param  mq_data  输出数据结构体指针
  * @note   1. 临界区保护读取 DMA 双通道数据，防止读到新旧混合值
  *         2. 分别对两通道做滑动平均滤波
  *         3. 转换为 ppm 浓度并判断报警阈值
  */
void MQ_GetData(MQ_Data_t *mq_data)
{
    uint16_t raw[2];

    /* DMA 循环传输两通道，关中断读取防止数据撕裂 */
    taskENTER_CRITICAL();
    raw[0] = MQ_Adc_Result[0];
    raw[1] = MQ_Adc_Result[1];
    taskEXIT_CRITICAL();

    uint16_t mq136_raw = MQ_Filter(raw[0], mq136_buf, &mq136_idx,
                                   &mq136_sum, &mq136_fill);
    uint16_t mq137_raw = MQ_Filter(raw[1], mq137_buf, &mq137_idx,
                                   &mq137_sum, &mq137_fill);

    mq_data->h2s_ppm = MQ136_Get_H2S_ppm(mq136_raw);
    mq_data->nh3_ppm = MQ137_Get_NH3_ppm(mq137_raw);

    mq_data->gas_alert = 0;
    if(mq_data->h2s_ppm > H2S_ALARM_THRESHOLD || 
       mq_data->nh3_ppm > NH3_ALARM_THRESHOLD)
    {
        mq_data->gas_alert = 1;
    }
}

/**
  * @brief  启动 ADC1 DMA 双通道循环采集
  */
void MQ_ADC_Init(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)MQ_Adc_Result, 2);
}

/**
  * @brief  MQ 传感器预热
  * @param  time_ms  预热时间（毫秒）
  * @note   使用 osDelay 实现非阻塞延时，不占用 CPU
  */
void MQ_PreHeat(uint32_t time_ms)
{
    osDelay(time_ms);
}
