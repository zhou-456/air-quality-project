#ifndef __SENSOR_TASK_H
#define __SENSOR_TASK_H

/**
  * @brief  传感器数据采集任务入口
  * @param  argument  FreeRTOS 标准任务参数
  * @retval None
  * @note   优先级：Normal
  *         栈大小：512 字（I2C/ADC 通信协议栈占用较多）
  *         周期：1 秒（PM2.5 实际 5 秒）
  *         启动延迟：15 秒（SGP30 预热）
  */
void StartSensor_Task(void const * argument);

#endif
