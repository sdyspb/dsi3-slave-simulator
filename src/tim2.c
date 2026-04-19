/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim2.c
  * @brief   This file provides code for the configuration
  *          of the TIM2 peripheral.
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

/* Includes ------------------------------------------------------------------*/
#include "tim2.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

TIM_HandleTypeDef htim2;
uint32_t microsecond_counter = 0;  // 32-bit counter for microseconds

/* TIM2 init function */
void MX_TIM2_Init(void)
{
  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 199; // Prescaler for 400MHz/200 = 2MHz timer clock
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 199; // 2MHz / 200 = 10kHz -> 100 microseconds period
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */
  // Start the timer and enable interrupts
  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END TIM2_Init 2 */
}



/* USER CODE BEGIN 1 */

// Function to get the current microsecond counter value
uint32_t get_us_tick(void)
{
  return microsecond_counter;
}

// This function will be called from the interrupt handler in stm32h7xx_it.c
void TIM2_IncrementCounter(void)
{
  microsecond_counter++;
}

/* USER CODE END 1 */