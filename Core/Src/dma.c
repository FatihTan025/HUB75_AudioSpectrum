/**
  ******************************************************************************
  * @file    dma.c
  * @brief   DMA2 clock ve HUB75 (DMA2_Stream5) kesmesi
  *
  * hdma_tim1_up handle'i tim.c icinde. ADC DMA'si (DMA2_Stream0)
  * Audio_ADC.c icinde register seviyesinde kurulur.
  ******************************************************************************
  */
#include "dma.h"

void MX_DMA_Init(void)
{
	__HAL_RCC_DMA2_CLK_ENABLE();

	/* HUB75 tarama -- en yuksek oncelik */
	HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);
}
