#ifndef __KEY_H
#define __KEY_H

#include "main.h"
#include "cmsis_os.h"

/* 短按键值（1~4 对应 4 个独立按键） */
#define KEY_NONE    0
#define KEY1        1
#define KEY2        2
#define KEY3        3
#define KEY4        4

/* 长按键值（短按 + 10，与短按键值区分） */
#define KEY1_LONG   11
#define KEY2_LONG   12
#define KEY3_LONG   13
#define KEY4_LONG   14

/* 长按阈值：800ms（基于 FreeRTOS tick，默认 1ms/tick） */
#define LONG_PRESS_TIME  800

/**
  * @brief  按键扫描（状态机版，零延时）
  * @retval 键值编码
  */
uint8_t KEY_Scan(void);

#endif
