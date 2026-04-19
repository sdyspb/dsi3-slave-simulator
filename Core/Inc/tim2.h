/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TIM2_H__
#define __TIM2_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

extern TIM_HandleTypeDef htim2; // Make htim2 available to other modules

void MX_TIM2_Init(void);

/* USER CODE BEGIN Prototypes */
uint32_t get_us_tick(void);
void TIM2_IncrementCounter(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __TIM2_H__ */