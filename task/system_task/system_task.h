#ifndef __SYSTEM_TASK_H
#define __SYSTEM_TASK_H

/**
  * @brief  系统控制任务入口
  * @param  argument  FreeRTOS 标准任务参数
  * @retval None
  * @note   优先级：AboveNormal（报警响应需及时）
  *         栈大小：256 字
  *         周期：20ms 轮询 + 500ms 报警判断
  */
void StartSystem_Task(void const * argument);

#endif
