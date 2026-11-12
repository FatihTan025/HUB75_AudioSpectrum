/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.h
  * @brief   TIM1 - HUB75 DMA senkronizasyon timer'i
  *
  * TIM1 APB2'de oldugu icin TIM1_UP DMA istegi DMA2'ye gidebilir. GPIO
  * AHB1'de oldugundan DMA2 zorunludur (DMA1 GPIO'ya erisemez).
  * Hicbir PWM kanali aktif degil; CLK ve OE bitleri piksel verisinin icinde.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern TIM_HandleTypeDef htim1;

/* USER CODE BEGIN Private defines */
/* HUB75_DMA.c bu handle'i kullanir: TIM1 Update event tetiklemesiyle
   GPIOC->ODR'ye piksel/CLK/OE byte'i yazan DMA stream'i */
extern DMA_HandleTypeDef hdma_tim1_up;

/* TIM3_CH2 (PC7) -- HUB75 OE darbesi */
extern TIM_HandleTypeDef htim3;
/* USER CODE END Private defines */

void MX_TIM1_Init(void);

/* USER CODE BEGIN Prototypes */
void MX_TIM3_Init(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __TIM_H__ */

