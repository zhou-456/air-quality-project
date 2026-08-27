#ifndef __PM25_H
#define __PM25_H

#include "main.h"

/* PM2.5 传感器 LED 驱动引脚（PA11） */
#define PM25_LED_PORT       GPIOA
#define PM25_LED_PIN        GPIO_PIN_11

/**
  * @brief  PM2.5 传感器初始化
  */
void PM25_Init(void);

/**
  * @brief  获取 PM2.5 浓度（中值滤波后）
  * @param  pm25_ug_m3  输出浓度指针（ug/m3）
  */
void PM25_GetData(float *pm25_ug_m3);

/**
  * @brief  获取原始 ADC 采样值（调试）
  * @retval uint16_t  单次采样值
  */
uint16_t PM25_GetRaw(void);

#endif
