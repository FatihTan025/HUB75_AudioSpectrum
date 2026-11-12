/**
  ******************************************************************************
  * @file    Audio_ADC.h
  * @brief   PA5 (ADC1_IN5) ses ornekleme: TIM2 TRGO -> ADC1 -> DMA2_Stream0
  ******************************************************************************
  * DMA tamponu 2 x HOP ornek, circular. Her yari (HOP ornek) dolunca kesme
  * onceki kareyi HOP kadar kaydirip yeni ornekleri sona ekler:
  *   -> FFT_SIZE uzunlukta, eskiden yeniye kare. Iki kare tamponu.
  * HAL ADC kutuphanesi KULLANILMAZ (register seviyesi), CubeMX gerekmez.
  ******************************************************************************
  */
#ifndef __AUDIO_ADC_H__
#define __AUDIO_ADC_H__

#include "main.h"
#include "SpecConfig.h"

#define AUDIO_SAMPLE_RATE		CFG_SAMPLE_RATE
#define AUDIO_FFT_SIZE			CFG_FFT_SIZE
#define AUDIO_HOP_SIZE			CFG_HOP_SIZE
#define AUDIO_DMA_IRQ_PRIORITY	5			/* HUB75 (0) bunu kesebilir		*/

/* TIM2 saati: APB1 timer = 90 MHz */
#define AUDIO_TIM_CLOCK			90000000U

/* Hazir kare: NULL degilse AUDIO_FFT_SIZE ornek, eskiden yeniye.
   Okuyan taraf isi bitince AUDIO_ReleaseBlock() cagirir.			*/
const uint16_t *AUDIO_GetReadyBlock(void);
void AUDIO_ReleaseBlock(void);

/* Islenmeden ustune yazilan kare sayisi -- 0 kalmali */
extern volatile uint32_t AUDIO_OverrunCount;
extern volatile uint32_t AUDIO_FrameCount;

void AUDIO_Init(void);

#endif /* __AUDIO_ADC_H__ */
