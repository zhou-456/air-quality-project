#include "led.h"

/**
  * @brief  LED 状态初始化
  * @param  None
  * @retval None
  * @note   上电默认全部熄灭（高电平熄灭，低电平点亮）
  */
void LED_Init(void)
{
    LED1_Off();
    LED2_Off();
}

/**
  * @brief  LED1 点亮（PC13 低电平）
  */
void LED1_On(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  LED1 熄灭（PC13 高电平）
  */
void LED1_Off(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
}

/**
  * @brief  LED1 状态翻转
  */
void LED1_Toggle(void)
{
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}

/**
  * @brief  LED2 点亮
  */
void LED2_On(void)
{
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  LED2 熄灭
  */
void LED2_Off(void)
{
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

/**
  * @brief  LED2 状态翻转
  */
void LED2_Toggle(void)
{
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
}
