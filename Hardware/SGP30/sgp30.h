#ifndef __SGP30_H
#define __SGP30_H

#include "main.h"

/* 引脚定义（来自 main.h） */
#define SGP30_SCL_PIN     SGP30_SCL_Pin
#define SGP30_SCL_PORT    SGP30_SCL_GPIO_Port
#define SGP30_SDA_PIN     SGP30_SDA_Pin
#define SGP30_SDA_PORT    SGP30_SDA_GPIO_Port

/* SGP30 命令字 */
#define SGP30_CMD_INIT_AIR_QUALITY      0x2003
#define SGP30_CMD_MEASURE_AIR_QUALITY   0x2008

/**
  * @brief  SGP30 初始化
  * @retval 0=成功，1=失败
  */
uint8_t SGP30_Init(void);

/**
  * @brief  读取空气质量数据
  * @param  eco2   等效 CO2 浓度（ppm）
  * @param  tvoc   TVOC 浓度（ppb）
  * @retval 0=成功，1=失败
  */
uint8_t SGP30_ReadAirQuality(uint16_t *eco2, uint16_t *tvoc);

/**
  * @brief  读取序列号（调试）
  */
uint8_t SGP30_GetSerialID(uint32_t *serial_id);

/**
  * @brief  底层写命令（外部如需直接发命令可用）
  */
uint8_t sgp30_write_command(uint16_t cmd);

#endif
