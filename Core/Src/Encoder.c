/**
  ******************************************************************************
  * @file    Encoder.c
  * @brief   Doner encoder -- bkz. Encoder.h
  ******************************************************************************
  * PB6 = TIM4_CH1 (A), PB7 = TIM4_CH2 (B), AF2, pull-up.
  * TIM4 encoder modu 3 (her iki kenar): bir tik = 4 sayim.
  * PB5 = buton (aktif LOW, pull-up).
  ******************************************************************************
  */
#include "Encoder.h"
#include "SpecConfig.h"

static int16_t	LastCnt = 0;
static int16_t	Acc     = 0;			/* Tik tamamlanmamis sayim		*/
static uint8_t	Raw = 0, Stable = 0;
static uint32_t	ChangeTick = 0, PressTick = 0;
static uint8_t	Turned = 0, LongDone = 0;

void ENC_Init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
	(void)RCC->APB1ENR;

	/* PB6/PB7: alternatif fonksiyon AF2 (TIM4), pull-up. PB5: giris, pull-up */
	GPIOB->MODER  = (GPIOB->MODER & ~((3U << 10) | (3U << 12) | (3U << 14))) | (2U << 12) | (2U << 14);
	GPIOB->PUPDR  = (GPIOB->PUPDR & ~((3U << 10) | (3U << 12) | (3U << 14))) | (1U << 10) | (1U << 12) | (1U << 14);
	GPIOB->AFR[0] = (GPIOB->AFR[0] & ~((15U << 24) | (15U << 28))) | (2U << 24) | (2U << 28);

	/* TIM4: encoder modu 3, iki girise de sayisal suzgec (temas sicramasi) */
	TIM4->CR1   = 0;
	TIM4->SMCR  = 3U;									/* SMS = 011		*/
	TIM4->CCMR1 = (1U << 0) | (1U << 8)					/* CC1S, CC2S = TI	*/
	            | (CFG_ENC_FILTER << 4) | (CFG_ENC_FILTER << 12);
	TIM4->CCER  = 0;
	TIM4->ARR   = 0xFFFFU;
	TIM4->CNT   = 0;
	TIM4->CR1   = TIM_CR1_CEN;
	LastCnt = 0;
}

void ENC_Poll(uint32_t Now, ENC_Event *e)
{
	const int16_t Cnt = (int16_t)TIM4->CNT;
	uint8_t r;

	e->Steps = 0; e->Click = 0; e->Long = 0;

	/* Cevirme: 4 sayim = 1 tik */
	Acc    += (int16_t)(Cnt - LastCnt);
	LastCnt = Cnt;
	if(Acc >= 4 || Acc <= -4)
	{
		e->Steps = (int8_t)((Acc / 4) * CFG_ENC_DIR);
		Acc     %= 4;
	}

	/* Buton: titresim suzgeci */
	r = (GPIOB->IDR & (1U << 5)) ? 0U : 1U;
	if(r != Raw) { Raw = r; ChangeTick = Now; }
	else if(r != Stable && (Now - ChangeTick) >= CFG_BTN_DEBOUNCE_MS)
	{
		Stable = r;
		if(Stable) { PressTick = Now; Turned = 0; LongDone = 0; }
		else if(!Turned && !LongDone) e->Click = 1;
	}
	if(Stable && e->Steps) Turned = 1;
	if(Stable && !Turned && !LongDone && (Now - PressTick) >= CFG_ENC_LONG_MS)
	{
		LongDone = 1;
		e->Long  = 1;
	}
	e->Held = Stable;
}
