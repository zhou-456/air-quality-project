#include "system_data.h"
#include "string.h"

/* 全局传感器数据结构（受互斥锁保护，防止多任务竞态访问） */
static SensorData_t g_data = {0};

/* 数据互斥锁：保护 g_data 读写，防止多任务同时访问导致数据不一致 */
static osMutexId dataMutex = NULL;

/* LCD 互斥锁：保护 SPI 总线访问，防止显示任务与其他任务冲突 */
osMutexId lcdMutexHandle = NULL;

/* ESP8266 互斥锁：保护串口 2 访问，防止 WiFi 任务与中断回调冲突 */
osMutexId esp8266MutexHandle = NULL;

/**
  * @brief  系统数据与互斥锁初始化
  * @param  None
  * @retval None
  * @note   创建 3 个互斥锁：
  *         - dataMutex：保护全局传感器数据结构
  *         - lcdMutexHandle：保护 LCD SPI 总线（共享资源）
  *         - esp8266MutexHandle：保护 ESP8266 USART2 串口（共享资源）
  *         必须在 FreeRTOS 调度器启动前调用
  */
void SystemData_Init(void)
{
    osMutexDef(dMutex);
    dataMutex = osMutexCreate(osMutex(dMutex));
    
    osMutexDef(lcdM);
    lcdMutexHandle = osMutexCreate(osMutex(lcdM));
    
    osMutexDef(espM);
    esp8266MutexHandle = osMutexCreate(osMutex(espM));
}

/**
  * @brief  发布最新传感器数据（生产者调用）
  * @param  src  指向待写入的 SensorData_t 结构体
  * @retval None
  * @note   通过互斥锁保证原子性写入，同时记录系统节拍戳
  *         典型调用者：Sensor_Task 采集完成后更新数据
  */
void SystemData_Publish(SensorData_t* src)
{
    if(dataMutex == NULL) return;
    
    osMutexWait(dataMutex, osWaitForever);
    memcpy(&g_data, src, sizeof(SensorData_t));
    g_data.tick = osKernelSysTick();  // 记录数据更新时间戳
    osMutexRelease(dataMutex);
}

/**
  * @brief  读取当前传感器数据（消费者调用）
  * @param  dst  指向目标缓冲区，用于接收数据副本
  * @retval uint8_t  1=成功，0=失败（互斥锁未初始化）
  * @note   通过互斥锁保证原子性读取，获取的是数据快照
  *         典型调用者：LCD_Task 刷新显示、WiFi_Task 上传云端
  */
uint8_t SystemData_Read(SensorData_t* dst)
{
    if(dataMutex == NULL) return 0;
    
    osMutexWait(dataMutex, osWaitForever);
    memcpy(dst, &g_data, sizeof(SensorData_t));
    osMutexRelease(dataMutex);
    return 1;
}

/**
  * @brief  获取 LCD 总线访问权
  * @param  timeout_ms  等待超时时间（毫秒），osWaitForever 表示永久等待
  * @retval uint8_t  1=获取成功，0=失败（超时或锁未初始化）
  * @note   LCD 使用 SPI1 总线，属于共享资源，必须通过互斥锁串行访问
  */
uint8_t LCD_Lock(uint32_t timeout_ms)
{
    if(lcdMutexHandle == NULL) return 0;
    return (osMutexWait(lcdMutexHandle, timeout_ms) == osOK);
}

/**
  * @brief  释放 LCD 总线访问权
  * @param  None
  * @retval None
  */
void LCD_Unlock(void)
{
    if(lcdMutexHandle) osMutexRelease(lcdMutexHandle);
}

/**
  * @brief  获取 ESP8266 串口访问权
  * @param  timeout_ms  等待超时时间（毫秒）
  * @retval uint8_t  1=获取成功，0=失败
  * @note   ESP8266 使用 USART2，WiFi 任务与串口中断回调可能并发访问
  */
uint8_t ESP_Lock(uint32_t timeout_ms)
{
    if(esp8266MutexHandle == NULL) return 0;
    return (osMutexWait(esp8266MutexHandle, timeout_ms) == osOK);
}

/**
  * @brief  释放 ESP8266 串口访问权
  * @param  None
  * @retval None
  */
void ESP_Unlock(void)
{
    if(esp8266MutexHandle) osMutexRelease(esp8266MutexHandle);
}
