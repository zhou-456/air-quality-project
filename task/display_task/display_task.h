#ifndef __DISPLAY_TASK_H
#define __DISPLAY_TASK_H

#include <stdint.h>

/* 显示页面枚举 */
#define PAGE_MAIN       0   /* 主监控页：温湿度 + CO2 + TVOC + PM2.5 + 日期 */
#define PAGE_GAS        1   /* 气体详情页：H2S + NH3 + CO2 + TVOC + PM2.5 */
#define PAGE_SYSTEM     2   /* 系统状态页：模块状态 + 运行时间 */
#define PAGE_SETTINGS   3   /* 阈值设置页：6 项报警阈值编辑 */
#define PAGE_MAX        4   /* 总页面数，用于循环切换 */

/**
  * @brief  报警阈值结构体
  * @note   每项参数分警告（warn）和危险（danger）两级阈值
  *         对应 UI 颜色：绿色 < 警告 < 黄色 < 危险 < 红色
  */
typedef struct {
    float pm25_warn;    /* PM2.5 警告阈值 */
    float pm25_danger;  /* PM2.5 危险阈值 */
    float h2s_warn;     /* H2S 警告阈值 */
    float h2s_danger;   /* H2S 危险阈值 */
    float nh3_warn;     /* NH3 警告阈值 */
    float nh3_danger;   /* NH3 危险阈值 */
} AlarmThreshold_t;

/* 当前显示页面（由 system_task.c 按键处理修改） */
extern volatile uint8_t g_current_page;

/* 报警阈值全局变量（由 system_task.c 编辑，display_task.c 显示） */
extern AlarmThreshold_t g_alarm_thresh;

/* 设置页编辑状态：0=浏览模式，1~6=正在编辑第几项阈值 */
extern volatile uint8_t g_setting_idx;

/**
  * @brief  LCD 显示刷新任务入口
  * @param  argument  FreeRTOS 标准任务参数
  * @retval None
  * @note   优先级：Low（显示允许延迟，不阻塞传感器采集）
  *         栈大小：512 字（大量字符串格式化操作）
  *         刷新周期：300ms
  *         页面切换时全屏清屏，同页内局部刷新
  */
void StartLCD_Task(void const * argument);

#endif
