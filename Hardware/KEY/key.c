#include "key.h"

/**
  * @brief  按键扫描（状态机版，零延时，纯查询）
  * @retval 键值：KEY_NONE / KEY1~4 / KEY1_LONG~4
  * @note   设计要点：
  *         1. 依赖调用者 20ms 周期调度，自然消抖（20ms > 机械抖动 5~10ms）
  *         2. 状态机 3 态：空闲 → 按下中 → 长按已触发
  *         3. 长按阈值 800ms，基于 FreeRTOS 系统节拍（默认 1ms/tick）
  *         4. 零阻塞：全程无延时，适合实时系统
  */
uint8_t KEY_Scan(void)
{
    static uint8_t  key_state = 0;      /* 0=空闲, 1=按下中, 2=长按已触发 */
    static uint32_t press_tick = 0;     /* 按下时刻的系统节拍 */
    static uint8_t  key_code = KEY_NONE;/* 当前按下的键值 */

    uint8_t key_now = KEY_NONE;

    /* 读取当前物理状态（按优先级顺序，KEY1 优先级最高） */
    if(HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == 0)       key_now = KEY1;
    else if(HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == 0) key_now = KEY2;
    else if(HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == 0) key_now = KEY3;
    else if(HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin) == 0) key_now = KEY4;

    switch(key_state)
    {
        case 0: /* 空闲：等待按下 */
            if(key_now != KEY_NONE)
            {
                key_code   = key_now;
                press_tick = osKernelSysTick();
                key_state  = 1;
            }
            return KEY_NONE;

        case 1: /* 按下中：检测释放（短按）或超时（长按） */
            if(key_now == KEY_NONE)
            {
                /* 释放 → 短按生效，返回键值 */
                key_state = 0;
                return key_code;
            }
            else if(key_now != key_code)
            {
                /* 按键中途变化（异常抖动或同时按下），丢弃 */
                key_state = 0;
                return KEY_NONE;
            }

            /* 长按判断：按下时间超过阈值 */
            if((osKernelSysTick() - press_tick) >= LONG_PRESS_TIME)
            {
                key_state = 2;                 /* 标记长按已触发，防止重复上报 */
                return key_code + 10;          /* 长按键值 = 短按键值 + 10 */
            }
            return KEY_NONE;

        case 2: /* 长按已触发：等待释放，期间不再返回键值 */
            if(key_now == KEY_NONE)
            {
                key_state = 0;                 /* 释放，回归空闲 */
            }
            return KEY_NONE;

        default:
            key_state = 0;
            return KEY_NONE;
    }
}
