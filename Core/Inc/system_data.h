#ifndef __SYSTEM_DATA_H
#define __SYSTEM_DATA_H

#include "cmsis_os.h"

/**
  * @brief  传感器数据结构体
  * @note   包含所有检测参数及时间戳：
  *         - 温湿度：temp（℃）、humi（%RH）
  *         - 颗粒物：pm25（μg/m3）
  *         - 有害气体：h2s_ppm（硫化氢）、nh3_ppm（氨气）
  *         - 空气质量：eco2（等效 CO?，ppm）、tvoc（总挥发性有机物，ppb）
  *         - 时间戳：DS1302 年月日时分秒 + FreeRTOS 系统节拍 tick
  */
typedef struct {
    float    temp;      /* 温度，单位 ℃ */
    float    humi;      /* 湿度，单位 %RH */
    float    pm25;      /* PM2.5 浓度，单位 μg/m3 */
    float    h2s_ppm;   /* 硫化氢浓度，单位 ppm */
    float    nh3_ppm;   /* 氨气浓度，单位 ppm */
    uint16_t eco2;      /* 等效 CO?，单位 ppm */
    uint16_t tvoc;      /* TVOC，单位 ppb */
    uint8_t  year;      /* 年（如 24 表示 2024） */
    uint8_t  mon;       /* 月 1-12 */
    uint8_t  day;       /* 日 1-31 */
    uint8_t  hour;      /* 时 0-23 */
    uint8_t  min;       /* 分 0-59 */
    uint8_t  sec;       /* 秒 0-59 */
    uint32_t tick;      /* FreeRTOS 系统节拍，用于判断数据新鲜度 */
} SensorData_t;

/* LCD 互斥锁句柄（全局，供 display_task.c 等外部模块调用） */
extern osMutexId lcdMutexHandle;

/* ESP8266 互斥锁句柄（全局，供 wifi_task.c 等外部模块调用） */
extern osMutexId esp8266MutexHandle;

/**
  * @brief  初始化系统数据与互斥锁
  */
void SystemData_Init(void);

/**
  * @brief  发布传感器数据（线程安全，生产者接口）
  */
void SystemData_Publish(SensorData_t* data);

/**
  * @brief  读取传感器数据（线程安全，消费者接口）
  * @retval 1=成功，0=失败
  */
uint8_t SystemData_Read(SensorData_t* data);

/**
  * @brief  获取 LCD 总线访问权（阻塞式）
  */
uint8_t LCD_Lock(uint32_t timeout_ms);

/**
  * @brief  释放 LCD 总线访问权
  */
void LCD_Unlock(void);

/**
  * @brief  获取 ESP8266 串口访问权（阻塞式）
  */
uint8_t ESP_Lock(uint32_t timeout_ms);

/**
  * @brief  释放 ESP8266 串口访问权
  */
void ESP_Unlock(void);

#endif
