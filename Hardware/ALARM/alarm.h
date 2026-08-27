#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f1xx_hal.h"
#include "main.h" 

// 报警模块初始化
void Alarm_Init(void);

// 报警灯驱动
void Alarm_On(void);
void Alarm_Off(void);
void Alarm_Toggle(void);

// 蜂鸣器驱动
void Beep_On(void);
void Beep_Off(void);
void Beep_Toggle(void);

// 组合报警驱动（灯+蜂鸣器）
void Alarm_Beep_On(void);
void Alarm_Beep_Off(void);

#endif /* __ALARM_H */
