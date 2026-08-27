/**
  ******************************************************************************
  * @file           : freertos.c
  * @brief          : FreeRTOS 任务创建与内核配置
  *                   定义 5 个应用任务：传感器采集、WiFi 通信、LCD 显示、
  *                   系统状态管理、心跳指示
  ******************************************************************************
  */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "system_data.h"      // 全局系统数据结构
#include "sensor_task.h"      // 传感器数据采集任务
#include "wifi_task.h"        // ESP8266 WiFi 通信任务
#include "system_task.h"      // 系统状态监控任务
#include "heartbeat_task.h"   // 心跳指示灯任务
#include "display_task.h"     // LCD 显示刷新任务

/* MQTT 连接状态标志，0=未连接，1=已连接（由 WiFi 任务设置） */
volatile uint8_t wifi_ok = 0;

/* 任务句柄，用于后续任务管理（挂起、恢复、删除等） */
osThreadId Sensor_TaskHandle;
osThreadId WiFi_TaskHandle;
osThreadId LCD_TaskHandle;
osThreadId System_TaskHandle;
osThreadId Heartbeat_TaskHandle;

/* 空闲任务静态内存分配（使用静态分配避免动态内存碎片） */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

/**
  * @brief  获取空闲任务内存（FreeRTOS 静态分配回调）
  * @param  ppxIdleTaskTCBBuffer    空闲任务 TCB 缓冲区指针
  * @param  ppxIdleTaskStackBuffer  空闲任务栈缓冲区指针
  * @param  pulIdleTaskStackSize    空闲任务栈大小指针
  * @retval None
  * @note   FreeRTOS 启用静态内存分配时，内核调用此函数获取空闲任务内存
  *         避免使用 heap，提高嵌入式系统稳定性
  */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/**
  * @brief  FreeRTOS 内核初始化与任务创建
  * @param  None
  * @retval None
  * @note   任务优先级设计：
  *         - AboveNormal：System_Task（状态监控）、Heartbeat_Task（指示灯）
  *         - Normal：Sensor_Task（数据采集）、WiFi_Task（网络通信）
  *         - Low：LCD_Task（显示刷新，人机交互允许延迟）
  *         栈大小分配：WiFi 任务最大（1024 字），因协议解析需要较大栈空间
  */
void MX_FREERTOS_Init(void)
{
  /* 初始化全局系统数据结构（传感器数据、时间、阈值等） */
  SystemData_Init();

  /* 创建传感器数据采集任务：优先级 Normal，栈 512 字 */
  osThreadDef(Sensor_Task, StartSensor_Task, osPriorityNormal, 0, 512);
  Sensor_TaskHandle = osThreadCreate(osThread(Sensor_Task), NULL);

  /* 创建 WiFi 通信任务：优先级 Normal，栈 1024 字（AT 指令解析需要较大栈） */
  osThreadDef(WiFi_Task, StartWiFi_Task, osPriorityNormal, 0, 1024);
  WiFi_TaskHandle = osThreadCreate(osThread(WiFi_Task), NULL);

  /* 创建 LCD 显示任务：优先级 Low，栈 512 字（显示刷新允许较低优先级） */
  osThreadDef(LCD_Task, StartLCD_Task, osPriorityLow, 0, 512);
  LCD_TaskHandle = osThreadCreate(osThread(LCD_Task), NULL);

  /* 创建系统状态管理任务：优先级 AboveNormal，栈 256 字（轻量级监控） */
  osThreadDef(System_Task, StartSystem_Task, osPriorityAboveNormal, 0, 256);
  System_TaskHandle = osThreadCreate(osThread(System_Task), NULL);

  /* 创建心跳指示灯任务：优先级 AboveNormal，栈 128 字（最轻量级） */
  osThreadDef(Heartbeat_Task, StartHeartbeat_Task, osPriorityAboveNormal, 0, 128);
  Heartbeat_TaskHandle = osThreadCreate(osThread(Heartbeat_Task), NULL);
}
