#include "sensor_task.h"
#include "system_data.h"
#include "sht30.h"       // SHT30 温湿度传感器驱动（I2C）
#include "sgp30.h"       // SGP30 空气质量传感器驱动（I2C，CO?/TVOC）
#include "pm25.h"        // PM2.5 粉尘传感器驱动（ADC）
#include "mq_senser.h"   // MQ-136/MQ-137 气体传感器驱动（ADC）
#include "ds1302.h"      // DS1302 实时时钟驱动（SPI/GPIO 模拟）
#include <stdio.h>

/**
  * @brief  传感器数据采集任务
  * @param  argument  FreeRTOS 任务参数（未使用）
  * @retval None
  * @note   任务职责：周期性采集 6 类空气参数 + RTC 时间，发布到全局数据结构
  *         
  *         采集参数与传感器对应关系：
  *         ├─ 温度/湿度    → SHT30（I2C，每秒读取）
  *         ├─ eCO2/TVOC   → SGP30（I2C，每秒读取）
  *         ├─ PM2.5       → 激光粉尘传感器（ADC，每 5 秒读取，降低功耗）
  *         ├─ H?S/NH?     → MQ-136/MQ-137（ADC + DMA，每秒读取）
  *         └─ 时间戳      → DS1302（独立 RTC，每秒读取）
  *         
  *         启动延迟 15 秒：等待 SGP30 传感器初始化完成（首次上电需要预热）
  */
void StartSensor_Task(void const * argument)
{
    /* 初始化所有传感器外设 */
    SHT30_Init();       // I2C 温湿度传感器
    SGP30_Init();       // I2C 空气质量传感器（需预热）
    DS1302_Init();      // 独立 RTC（备用电池供电，掉电保持）
    
    /* 首次烧录或 RTC 电池耗尽时，取消注释下行手动设置时间 */
    /* 格式：年(后两位), 月, 日, 时, 分, 秒, 星期(1-7) */
    //DS1302_SetTime(26, 5, 25, 19, 45, 0, 3);
    
    MQ_Filter_Init();   // MQ 传感器 ADC DMA + 数字滤波初始化
    PM25_Init();        // ADC粉尘传感器

    /* 本地数据缓冲区 */
    SensorData_t data = {0};
    MQ_Data_t mq;
    DS1302_TimeTypeDef time;
    float pm25_val = 0.0f;
    uint8_t pm25_cnt = 0;

    /* SGP30 上电后需要约 15 秒预热才能达到稳定精度 */
    osDelay(15000);

    for(;;)
    {
        /* 1. 读取温湿度（SHT30，I2C 通信，单次测量模式） */
        SHT30_ReadData(&data.temp, &data.humi);

        /* 2. 读取空气质量（SGP30，I2C 通信，返回等效 CO? 和 TVOC） */
        SGP30_ReadAirQuality(&data.eco2, &data.tvoc);

        /* 3. 读取 MQ 气体传感器（ADC1 双通道 DMA + 滑动平均滤波） */
        MQ_GetData(&mq);

        /* 4. 读取 RTC 时间（DS1302，独立时钟源，不受主 MCU 影响） */
        DS1302_ReadTime(&time);

        /* 5. PM2.5 降频采集：每循环 5 次（即 5 秒）读取一次，减少激光模块功耗 */
        pm25_cnt++;
        if(pm25_cnt >= 5)
        {
            PM25_GetData(&pm25_val);
            data.pm25 = pm25_val;
            pm25_cnt = 0;
        }

        /* 组装数据结构体 */
        data.h2s_ppm = mq.h2s_ppm;      // 硫化氢浓度
        data.nh3_ppm = mq.nh3_ppm;      // 氨气浓度
        data.year = (uint8_t)(time.year % 100);  // 取后两位（如 2026 → 26）
        data.mon  = time.month;
        data.day  = time.date;
        data.hour = time.hour;
        data.min  = time.minute;
        data.sec  = time.second;

        /* 通过互斥锁发布到全局数据区，供显示任务和 WiFi 任务消费 */
        SystemData_Publish(&data);

        /* 1 秒采集周期，与 SGP30 默认测量周期匹配 */
        osDelay(1000);
    }
}
