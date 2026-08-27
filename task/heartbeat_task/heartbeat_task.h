#ifndef __HEARTBEAT_TASK_H
#define __HEARTBEAT_TASK_H

/**
  * @brief  心跳指示灯任务入口
  * @param  argument  FreeRTOS 标准任务参数
  * @retval None
  * @note   优先级：AboveNormal
  *         栈大小：128 字（轻量级任务）
  *         周期：500ms（LED 闪烁）
  */
void StartHeartbeat_Task(void const * argument);

#endif
