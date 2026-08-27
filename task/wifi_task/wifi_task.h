#ifndef __WIFI_TASK_H
#define __WIFI_TASK_H

/**
  * @brief  WiFi 通信任务入口
  * @param  argument  FreeRTOS 标准任务参数
  * @retval None
  * @note   优先级：Normal
  *         栈大小：1024 字（AT 指令解析 + MQTT 协议栈占用较大）
  *         数据上报周期：10 秒
  */
void StartWiFi_Task(void const * argument);

#endif
