#include "heartbeat_task.h"
#include "main.h"
#include "cmsis_os.h"

/**
  * @brief  心跳指示灯任务
  * @param  argument  FreeRTOS 任务参数（未使用）
  * @retval None
  * @note   任务职责：每 500ms 翻转 LED1 状态，提供系统运行状态视觉指示
  *         优先级 AboveNormal，确保即使低优先级任务阻塞，指示灯仍能正常闪烁
  */
void StartHeartbeat_Task(void const * argument)
{
    for(;;)
    {
        /* LED1 状态翻转，直观显示系统是否正常运行 */
        HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
        
        /* 500ms 闪烁周期，人眼可清晰辨识的呼吸灯效果 */
        osDelay(500);
    }
}
