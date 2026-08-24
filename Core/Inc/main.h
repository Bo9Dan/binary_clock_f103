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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BOARD_LED_Pin GPIO_PIN_13
#define BOARD_LED_GPIO_Port GPIOC
#define BTN_MODE_Pin GPIO_PIN_0
#define BTN_MODE_GPIO_Port GPIOA
#define BTN_PLUS_Pin GPIO_PIN_1
#define BTN_PLUS_GPIO_Port GPIOA
#define BTN_MINUS_Pin GPIO_PIN_2
#define BTN_MINUS_GPIO_Port GPIOA
#define BTN_OK_Pin GPIO_PIN_3
#define BTN_OK_GPIO_Port GPIOA
#define LATCH_595_Pin GPIO_PIN_4
#define LATCH_595_GPIO_Port GPIOA
#define CLK_595_Pin GPIO_PIN_5
#define CLK_595_GPIO_Port GPIOA
#define DATA_595_Pin GPIO_PIN_7
#define DATA_595_GPIO_Port GPIOA
#define OE_595_PWM_Pin GPIO_PIN_0
#define OE_595_PWM_GPIO_Port GPIOB
#define DS3231_INT_Pin GPIO_PIN_12
#define DS3231_INT_GPIO_Port GPIOB
#define DS3231_INT_EXTI_IRQn EXTI15_10_IRQn
#define APDS_INT_Pin GPIO_PIN_14
#define APDS_INT_GPIO_Port GPIOB
#define APDS_INT_EXTI_IRQn EXTI15_10_IRQn
#define BUZZER_Pin GPIO_PIN_15
#define BUZZER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
