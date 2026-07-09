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
extern ADC_HandleTypeDef hadc1;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_ADC1_Init(void);
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LARGE_HAND_NTC_ADC_Pin GPIO_PIN_0
#define LARGE_HAND_NTC_ADC_GPIO_Port GPIOC
#define SMALL_HAND_NTC_ADC_Pin GPIO_PIN_1
#define SMALL_HAND_NTC_ADC_GPIO_Port GPIOC
#define SYSTEM_POWER_CTRL_Pin GPIO_PIN_4
#define SYSTEM_POWER_CTRL_GPIO_Port GPIOA
#define POWER_ON_OFF_Pin GPIO_PIN_5
#define POWER_ON_OFF_GPIO_Port GPIOA
#define START_LED_CTRL_Pin GPIO_PIN_6
#define START_LED_CTRL_GPIO_Port GPIOA
#define BEEP_Pin GPIO_PIN_7
#define BEEP_GPIO_Port GPIOA
#define LGS6302EP_1_EN_Pin GPIO_PIN_0
#define LGS6302EP_1_EN_GPIO_Port GPIOB
#define LGS6302EP_2_EN_Pin GPIO_PIN_1
#define LGS6302EP_2_EN_GPIO_Port GPIOB
#define VACUUM_PUMP_CTRL_Pin GPIO_PIN_10
#define VACUUM_PUMP_CTRL_GPIO_Port GPIOB
#define SMALL_HAND_TEMP_CTRL_Pin GPIO_PIN_10
#define SMALL_HAND_TEMP_CTRL_GPIO_Port GPIOC
#define LARGE_HAND_TEMP_CTRL_Pin GPIO_PIN_11
#define LARGE_HAND_TEMP_CTRL_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
