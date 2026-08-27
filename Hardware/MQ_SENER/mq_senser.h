#ifndef MQ_SENSER_H
#define MQ_SENSER_H

#include "main.h"

/* ADC 通道定义 */
#define MQ136_CHANNEL      ADC_CHANNEL_0  /* PA0 - MQ-136 硫化氢 */
#define MQ137_CHANNEL      ADC_CHANNEL_1  /* PA1 - MQ-137 氨气 */

/* 传感器校准参数（需在洁净空气中标定 R0） */
#define MQ136_R0          10000.0f   /* MQ-136 洁净空气阻值（Ω） */
#define MQ137_R0          10000.0f   /* MQ-137 洁净空气阻值（Ω） */
#define MQ_RL             10000.0f   /* 负载电阻（Ω） */

#define MQ_ADC_REF        3.3f       /* ADC 参考电压（V） */
#define MQ_ADC_RES        4095.0f    /* ADC 分辨率（12 位） */

/* 气体报警阈值（ppm） */
#define H2S_ALARM_THRESHOLD  50.0f
#define NH3_ALARM_THRESHOLD  100.0f

/**
  * @brief  MQ 传感器数据结构体
  */
typedef struct {
    float h2s_ppm;     /* 硫化氢浓度（ppm） */
    float nh3_ppm;     /* 氨气浓度（ppm） */
    uint8_t gas_alert; /* 报警标志：0=正常，1=超标 */
} MQ_Data_t;

/* 全局 DMA 采集数组（ADC 中断自动更新） */
extern uint16_t MQ_Adc_Result[2];

/* 函数声明 */
void MQ_ADC_Init(void);                  /* 启动 ADC DMA 采集 */
void MQ_Filter_Init(void);               /* 滑动平均滤波器初始化 */
void MQ_PreHeat(uint32_t time_ms);       /* 传感器预热（非阻塞） */
void MQ_GetData(MQ_Data_t *mq_data);     /* 获取滤波后浓度数据 */

/* 浓度计算函数（可直接调用调试） */
float MQ136_Get_H2S_ppm(uint16_t adc_val);
float MQ137_Get_NH3_ppm(uint16_t adc_val);

#endif
