/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   TIM1 - HUB75 DMA senkronizasyon timer'i
  *
  * TIM3'ten TIM1'e TASINDI. Sebep:
  *   TIM3 APB1'de -> DMA istekleri yalnizca DMA1'e gider.
  *   STM32F4'te DMA1'in peripheral portu bus matrix'e bagli degil, GPIO ise
  *   AHB1'de. Yani DMA1 ile GPIOC->ODR'ye yazmak imkansiz.
  *   TIM1 APB2'de ve TIM1_UP -> DMA2_Stream5 / Channel 6 esleseme var.
  *
  * Hicbir PWM kanali aktif degil - CLK ve OE bitleri piksel verisiyle
  * birlikte GPIOC->ODR'ye yaziliyor (bkz. HUB75_DMA.c).
  *
  * ZAMANLAMA
  *   TIM1 clock = PCLK2 x 2 = 90 MHz x 2 = 180 MHz
  *   ARR = HUB75_TIMER_PERIOD (20) -> UEV her 21 tick = 116.7 ns
  *   2 byte / piksel, 512 byte / satir -> 59.7 us / satir-plane
  *   8 satir x 8 plane -> tazeleme ~262 Hz
  *
  *   Panel cok hizli gelirse HUB75_Config.h'ta HUB75_TIMER_PERIOD'u buyut.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "tim.h"

/* USER CODE BEGIN 0 */
#include "HUB75_Config.h"		/* TIM3 OE ayarlari (HUB75_OE_TIM_ARR, polarite) */
/* USER CODE END 0 */

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_up;

/* TIM1 init function */
void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = HUB75_TIMER_PERIOD;		/* Tek kaynak: HUB75_Config.h */
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  /* NOT: Hicbir kanal / PWM baslatilmiyor. TIM1_CHx pinleri kullanilmiyor,
     bu yuzden BDTR->MOE'ye de gerek yok. Sadece Update event -> DMA. */
  /* USER CODE END TIM1_Init 2 */

}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM1)
  {
  /* USER CODE BEGIN TIM1_MspInit 0 */

  /* USER CODE END TIM1_MspInit 0 */
    /* TIM1 clock enable */
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* TIM1 DMA Init */
    /* TIM1_UP Init */
    hdma_tim1_up.Instance = DMA2_Stream5;
    hdma_tim1_up.Init.Channel = DMA_CHANNEL_6;
    hdma_tim1_up.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_tim1_up.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim1_up.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim1_up.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tim1_up.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_tim1_up.Init.Mode = DMA_NORMAL;
    hdma_tim1_up.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma_tim1_up.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_tim1_up) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(tim_baseHandle,hdma[TIM_DMA_ID_UPDATE],hdma_tim1_up);

  /* USER CODE BEGIN TIM1_MspInit 1 */

  /* USER CODE END TIM1_MspInit 1 */
  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle)
{

  if(tim_baseHandle->Instance==TIM1)
  {
  /* USER CODE BEGIN TIM1_MspDeInit 0 */

  /* USER CODE END TIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM1_CLK_DISABLE();

    /* TIM1 DMA DeInit */
    HAL_DMA_DeInit(tim_baseHandle->hdma[TIM_DMA_ID_UPDATE]);
  /* USER CODE BEGIN TIM1_MspDeInit 1 */

  /* USER CODE END TIM1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* ==========================================================================
   TIM3 -- HUB75 OE darbesi (One Pulse Mode, CH2 = PC7, AF2)

   CubeMX'te TIM3 TANIMLI DEGIL, bu yuzden kod burada (USER CODE icinde)
   duruyor ve yeniden uretimde silinmez. CubeMX'te TIM3 acilirsa bu blok
   ile cakisir -- o durumda bu blogu kaldir.

   PWM2 + One Pulse:
     CNT <  CCR      -> PASIF (panel karanlik)
     CCR <= CNT <= ARR -> AKTIF (panel yanik)
     tasma           -> sayac 0'da durur, 0 < CCR oldugu icin PASIF
   CCR ve ARR preload KAPALI: yazilan deger ANINDA gecerli.
   Darbe HUB75_DMA.c icinde baslatilir (CCR=1, ARR=darbe suresi).
   ========================================================================== */
TIM_HandleTypeDef htim3;

void MX_TIM3_Init(void)
{
	TIM_OC_InitTypeDef sConfigOC = {0};

	htim3.Instance               = TIM3;
	htim3.Init.Prescaler         = 0;
	htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
	htim3.Init.Period            = HUB75_OE_TIM_ARR;
	htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if(HAL_TIM_PWM_Init(&htim3) != HAL_OK)
		Error_Handler();

	sConfigOC.OCMode     = TIM_OCMODE_PWM2;
	sConfigOC.Pulse      = 1;
	sConfigOC.OCPolarity = HUB75_OE_ACTIVE_HIGH ? TIM_OCPOLARITY_HIGH : TIM_OCPOLARITY_LOW;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if(HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
		Error_Handler();

	/* HAL, PWM kanalinda CCR preload'u (OC2PE) acar -- kapat */
	htim3.Instance->CCMR1 &= ~TIM_CCMR1_OC2PE;
	htim3.Instance->CR1   |= TIM_CR1_OPM;

	if(HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK)
		Error_Handler();

	/* Baslangicta karanlik: sayaci durdur, cikisi PASIF'e zorla */
	htim3.Instance->CR1  &= ~TIM_CR1_CEN;
	htim3.Instance->CCMR1 = (htim3.Instance->CCMR1 & ~TIM_CCMR1_OC2M) | TIM_CCMR1_OC2M_2;
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *timHandle)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if(timHandle->Instance == TIM3)
	{
		__HAL_RCC_TIM3_CLK_ENABLE();
		__HAL_RCC_GPIOC_CLK_ENABLE();

		/* PC7 -> TIM3_CH2 (AF2). gpio.c'deki GPIO cikis ayarinin USTUNE yazar */
		GPIO_InitStruct.Pin       = GPIO_PIN_7;
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull      = GPIO_NOPULL;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *timHandle)
{
	if(timHandle->Instance == TIM3)
	{
		__HAL_RCC_TIM3_CLK_DISABLE();
		HAL_GPIO_DeInit(GPIOC, GPIO_PIN_7);
	}
}

/* USER CODE END 1 */
