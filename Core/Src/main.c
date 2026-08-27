/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 嵌入式空气多参数检测仪主程序
  *                   基于 STM32F103 + FreeRTOS，实现多任务并行采集与显示
  ******************************************************************************
  */

#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "stdio.h"
#include "lcd.h"          // LCD 显示屏驱动（SPI 接口）
#include "ds1302.h"       // DS1302 实时时钟驱动
#include "mq_senser.h"    // MQ 系列气体传感器驱动（ADC 采集）

uint8_t rx_buf;   // ESP8266 WiFi 模块串口接收单字节缓冲区

void SystemClock_Config(void);
void MX_FREERTOS_Init(void);  // FreeRTOS 内核对象初始化（任务、队列、信号量等）

/**
  * @brief  应用程序入口函数
  * @retval int  正常不会返回，控制权移交 FreeRTOS 调度器
  * @note   系统启动流程：HAL 初始化 → 时钟配置 → 外设初始化 → 
  *         MQ 传感器 ADC DMA 启动 → FreeRTOS 初始化 → 启动调度器
  */
int main(void)
{
  /* 复位所有外设，初始化 Flash 接口和 SysTick */
  HAL_Init();

  /* 配置系统时钟为 72MHz（HSE 8MHz × 2 倍频） */
  SystemClock_Config();

  /* 初始化所有已配置的外设 */
  MX_GPIO_Init();
  MX_DMA_Init();            // DMA 初始化，用于 ADC 多通道数据搬运
  MX_USART1_UART_Init();    // USART1：调试串口 / 预留通信接口
  MX_ADC1_Init();           // ADC1：MQ 传感器模拟量采集（多通道 + DMA）
  MX_SPI1_Init();           // SPI1：LCD 显示屏通信接口
  MX_USART2_UART_Init();    // USART2：ESP8266 WiFi 模块通信接口
  MX_ADC2_Init();           // ADC2：备用 ADC 通道（温湿度或其他传感器）

  /* 启动 USART2 中断接收，用于接收 ESP8266 返回的数据 */
  HAL_UART_Receive_IT(&huart2, &rx_buf, 1);
  
  /* 启动 MQ 传感器的 ADC DMA 采集（ADC1 双通道循环采样） */
  /* 采用 DMA 方式可减轻 CPU 负担，适合 FreeRTOS 多任务环境 */
  MQ_ADC_Init();

  /* 初始化 FreeRTOS 内核对象（任务、队列、信号量等） */
  MX_FREERTOS_Init();

  /* 启动 FreeRTOS 任务调度器，此后控制权交由操作系统 */
  osKernelStart();

  /* 理论上程序不会执行到这里，因为调度器已接管 CPU */
  while (1)
  {
  }
}

/**
  * @brief  系统时钟配置函数
  * @retval None
  * @note   配置目标：SYSCLK = 72MHz，APB1/APB2 = 72MHz
  *         时钟源选择：外部高速晶振 HSE（8MHz）→ PLL 倍频 ×2
  *         ADC 时钟：PCLK2 / 2 = 36MHz（在 ADC 允许范围内）
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /* 配置 RCC 振荡器：启用 HSE，PLL 源选择 HSE，倍频系数 2 */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* 配置 CPU、AHB 和 APB 总线时钟 */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // 系统时钟源：PLL 输出
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;         // AHB 不分频：72MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;           // APB1 不分频：72MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;           // APB2 不分频：72MHz

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  
  /* 配置 ADC 外设时钟：PCLK2 / 2 = 36MHz */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  HAL 定时器周期中断回调函数（非阻塞模式）
  * @param  htim  TIM 句柄
  * @retval None
  * @note   当 TIM3 中断发生时，HAL_TIM_IRQHandler() 内部调用此函数，
  *         直接调用 HAL_IncTick() 递增全局变量 uwTick，作为 HAL 时基
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)
  {
    HAL_IncTick();  // 维护 HAL 库内部时基，供延时函数等使用
  }
}

/**
  * @brief  错误处理函数
  * @retval None
  * @note   当 HAL 库检测到致命错误时调用，进入死循环并关闭中断
  *         实际项目中可在此添加看门狗复位或错误指示灯
  */
void Error_Handler(void)
{
  __disable_irq();  // 关闭全局中断，防止错误扩散
  while (1)
  {
    /* 可在此处添加 LED 闪烁等错误指示 */
  }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  参数断言失败报告函数
  * @param  file  指向源文件名的指针
  * @param  line  断言失败所在的行号
  * @retval None
  * @note   仅在定义 USE_FULL_ASSERT 时生效，用于调试阶段参数检查
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* 可添加 printf 输出到调试串口，定位参数错误位置 */
}
#endif
