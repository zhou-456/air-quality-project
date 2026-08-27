#ifndef __SHT30_H
#define __SHT30_H

#include "main.h"

/* 引脚定义 */
#define SHT30_SCL_PIN       GPIO_PIN_6
#define SHT30_SDA_PIN       GPIO_PIN_7
#define SHT30_GPIO_PORT     GPIOB

/* 引脚操作宏 */
#define SHT30_SDA_READ()    HAL_GPIO_ReadPin(SHT30_GPIO_PORT, SHT30_SDA_PIN)
#define SHT30_SCL_HIGH()    HAL_GPIO_WritePin(SHT30_GPIO_PORT, SHT30_SCL_PIN, GPIO_PIN_SET)
#define SHT30_SCL_LOW()     HAL_GPIO_WritePin(SHT30_GPIO_PORT, SHT30_SCL_PIN, GPIO_PIN_RESET)
#define SHT30_SDA_HIGH()    HAL_GPIO_WritePin(SHT30_GPIO_PORT, SHT30_SDA_PIN, GPIO_PIN_SET)
#define SHT30_SDA_LOW()     HAL_GPIO_WritePin(SHT30_GPIO_PORT, SHT30_SDA_PIN, GPIO_PIN_RESET)

/* I2C 地址 */
#define SHT30_ADDR_WRITE    0x88
#define SHT30_ADDR_READ     0x89

/**
  * @brief  SHT30 初始化
  */
void SHT30_Init(void);

/**
  * @brief  读取温湿度（兼容旧接口，无返回值）
  */
void SHT30_ReadData(float *temp, float *humi);

/**
  * @brief  读取温湿度（带返回值，0=成功，1=失败）
  */
uint8_t SHT30_ReadDataEx(float *temp, float *humi);

#endif
