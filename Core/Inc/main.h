/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED1_Pin GPIO_PIN_0
#define LED1_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_1
#define LED2_GPIO_Port GPIOC
#define KEY4_Pin GPIO_PIN_2
#define KEY4_GPIO_Port GPIOC
#define KEY1_Pin GPIO_PIN_3
#define KEY1_GPIO_Port GPIOC
#define CS_Pin GPIO_PIN_4
#define CS_GPIO_Port GPIOA
#define DC_Pin GPIO_PIN_0
#define DC_GPIO_Port GPIOB
#define RST_Pin GPIO_PIN_1
#define RST_GPIO_Port GPIOB
#define DS1302_DAT_Pin GPIO_PIN_10
#define DS1302_DAT_GPIO_Port GPIOB
#define BEEP_Pin GPIO_PIN_11
#define BEEP_GPIO_Port GPIOB
#define DS1302_RST_Pin GPIO_PIN_15
#define DS1302_RST_GPIO_Port GPIOB
#define ALARMLED_Pin GPIO_PIN_12
#define ALARMLED_GPIO_Port GPIOA
#define KEY2_Pin GPIO_PIN_10
#define KEY2_GPIO_Port GPIOC
#define KEY3_Pin GPIO_PIN_11
#define KEY3_GPIO_Port GPIOC
#define DS1302_CE_Pin GPIO_PIN_5
#define DS1302_CE_GPIO_Port GPIOB
#define SHT30_SCL_Pin GPIO_PIN_6
#define SHT30_SCL_GPIO_Port GPIOB
#define SHT30_SDA_Pin GPIO_PIN_7
#define SHT30_SDA_GPIO_Port GPIOB
#define SGP30_SCL_Pin GPIO_PIN_8
#define SGP30_SCL_GPIO_Port GPIOB
#define SGP30_SDA_Pin GPIO_PIN_9
#define SGP30_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
