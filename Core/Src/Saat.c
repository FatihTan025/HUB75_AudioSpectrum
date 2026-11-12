/**
  ******************************************************************************
  * @file    Saat.c
  * @brief   RTC saati -- bkz. Saat.h
  ******************************************************************************
  * HAL RTC modulu projede yok; yazmaclar dogrudan kullanilir.
  * Takvim (tarih) simdilik kullanilmiyor, sadece saat:dakika:saniye.
  ******************************************************************************
  */
#include "main.h"
#include "Saat.h"

#define LSE_BEKLE_MS	3000U		/* Bu surede kristal hazir olmazsa yazilim saati	*/

typedef enum { RTC_YOK = 0, RTC_BEKLE, RTC_HAZIR } RtcDurum;

static RtcDurum	Rtc;
static uint32_t	LseBas;
static uint32_t	TabanSn, TabanMs;		/* Yazilim saati: TabanSn + (Simdi - TabanMs) / 1000	*/

static uint32_t Bcd(uint32_t v)		{ return ((v / 10U) << 4) | (v % 10U); }
static uint32_t Ondalik(uint32_t b)	{ return ((b >> 4) & 0x0FU) * 10U + (b & 0x0FU); }

static uint32_t YazilimSn(void)
{
	return (TabanSn + (HAL_GetTick() - TabanMs) / 1000U) % 86400U;
}

static uint32_t RtcOku(void)
{
	uint32_t a, b;

	do { a = RTC->TR; b = RTC->TR; } while(a != b);		/* BYPSHAD: iki okuma ayni olmali */
	return Ondalik((a >> 16) & 0x3FU) * 3600U + Ondalik((a >> 8) & 0x7FU) * 60U + Ondalik(a & 0x7FU);
}

static void RtcYaz(uint32_t Sn)
{
	uint32_t t = 100000U;

	RTC->WPR = 0xCAU;									/* Yazma korumasini ac	*/
	RTC->WPR = 0x53U;
	RTC->ISR |= RTC_ISR_INIT;
	while(!(RTC->ISR & RTC_ISR_INITF) && --t) {}
	RTC->PRER = 255U;									/* Senkron:  32768 / 128 / 256 = 1 Hz	*/
	RTC->PRER = (127U << 16) | 255U;					/* Asenkron (ayri yazilmali)		*/
	RTC->TR   = (Bcd(Sn / 3600U) << 16) | (Bcd((Sn / 60U) % 60U) << 8) | Bcd(Sn % 60U);
	RTC->CR   = (RTC->CR & ~RTC_CR_FMT) | RTC_CR_BYPSHAD;	/* 24 saat, golge yazmac yok		*/
	RTC->ISR &= ~RTC_ISR_INIT;
	RTC->WPR  = 0xFFU;
}

void SAAT_Init(void)
{
	TabanSn = 12U * 3600U;
	TabanMs = HAL_GetTick();

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	PWR->CR      |= PWR_CR_DBP;							/* Yedek alana yazma izni */

	if((RCC->BDCR & RCC_BDCR_RTCEN) && (RCC->BDCR & RCC_BDCR_LSERDY))
	{
		Rtc     = RTC_HAZIR;							/* Pil sayesinde saat suruyor */
		TabanSn = RtcOku();
		return;
	}
	RCC->BDCR |= RCC_BDCR_LSEON;						/* Kristali baslat, beklemeden devam */
	LseBas     = HAL_GetTick();
	Rtc        = RTC_BEKLE;
}

void SAAT_Poll(void)
{
	if(Rtc != RTC_BEKLE)
		return;
	if(RCC->BDCR & RCC_BDCR_LSERDY)
	{
		if((RCC->BDCR & RCC_BDCR_RTCSEL) == 0U)
			RCC->BDCR |= RCC_BDCR_RTCSEL_0;				/* RTC saati = LSE */
		RCC->BDCR |= RCC_BDCR_RTCEN;
		RtcYaz(YazilimSn());							/* O ana kadarki yazilim saatini aktar */
		Rtc = RTC_HAZIR;
	}
	else if((HAL_GetTick() - LseBas) > LSE_BEKLE_MS)
		Rtc = RTC_YOK;									/* Kristal yok: yazilim saati */
}

void SAAT_Get(uint8_t *Sa, uint8_t *Dk, uint8_t *Sn)
{
	uint32_t s = (Rtc == RTC_HAZIR) ? RtcOku() : YazilimSn();

	*Sa = (uint8_t)(s / 3600U);
	*Dk = (uint8_t)((s / 60U) % 60U);
	*Sn = (uint8_t)(s % 60U);
}

void SAAT_Set(uint8_t Sa, uint8_t Dk, uint8_t Sn)
{
	uint32_t s = (uint32_t)Sa * 3600U + (uint32_t)Dk * 60U + Sn;

	TabanSn = s;
	TabanMs = HAL_GetTick();
	if(Rtc == RTC_HAZIR)
		RtcYaz(s);
}
