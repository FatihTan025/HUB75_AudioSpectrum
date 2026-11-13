/**
  ******************************************************************************
  * @file    Saat.c
  * @brief   RTC saati -- bkz. Saat.h
  ******************************************************************************
  * HAL RTC modulu projede yok; yazmaclar dogrudan kullanilir.
  * Saat + takvim (2000-2099). Ic sayac: 2000-01-01 00:00:00'dan beri saniye.
  ******************************************************************************
  */
#include "main.h"
#include "Saat.h"

#define LSE_BEKLE_MS	3000U		/* Bu surede kristal hazir olmazsa yazilim saati	*/

typedef enum { RTC_YOK = 0, RTC_BEKLE, RTC_HAZIR } RtcDurum;

static RtcDurum	Rtc;
static uint32_t	LseBas;
static uint32_t	TabanSn, TabanMs;		/* Yazilim saati: 2000-01-01'den beri sn = TabanSn + (Simdi - TabanMs) / 1000 */

static uint32_t Bcd(uint32_t v)		{ return ((v / 10U) << 4) | (v % 10U); }
static uint32_t Ondalik(uint32_t b)	{ return ((b >> 4) & 0x0FU) * 10U + (b & 0x0FU); }

static uint8_t AyGun(uint32_t Ay, uint32_t Yil)
{
	static const uint8_t G[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	return (uint8_t)((Ay == 2U && (Yil % 4U) == 0U) ? 29U : G[Ay - 1U]);	/* 2000-2099: her 4 yil artik */
}

/* 2000-01-01'den beri gun <-> tarih */
static uint32_t Gunler(uint32_t Gun, uint32_t Ay, uint32_t Yil)
{
	uint32_t d = (Yil - 2000U) * 365U + (Yil - 1997U) / 4U, m;	/* Onceki artik yillar */

	for(m = 1U; m < Ay; m++)
		d += AyGun(m, Yil);
	return d + Gun - 1U;
}

static void Tarih(uint32_t d, uint32_t *Gun, uint32_t *Ay, uint32_t *Yil)
{
	uint32_t y = 2000U, m = 1U;

	while(d >= ((y % 4U) ? 365U : 366U)) { d -= (y % 4U) ? 365U : 366U; y++; }
	while(d >= AyGun(m, y)) { d -= AyGun(m, y); m++; }
	*Gun = d + 1U; *Ay = m; *Yil = y;
}

static uint32_t YazilimSn(void)
{
	return TabanSn + (HAL_GetTick() - TabanMs) / 1000U;
}

static uint32_t RtcOku(void)
{
	uint32_t a, d, b;

	do { a = RTC->TR; d = RTC->DR; b = RTC->TR; } while(a != b);	/* BYPSHAD: tutarli okuma */
	return Gunler(Ondalik(d & 0x3FU), Ondalik((d >> 8) & 0x1FU), 2000U + Ondalik((d >> 16) & 0xFFU)) * 86400U
		 + Ondalik((a >> 16) & 0x3FU) * 3600U + Ondalik((a >> 8) & 0x7FU) * 60U + Ondalik(a & 0x7FU);
}

static void RtcYaz(uint32_t Top)
{
	uint32_t t = 100000U, Sn = Top % 86400U, g, a, y;

	Tarih(Top / 86400U, &g, &a, &y);
	RTC->WPR = 0xCAU;									/* Yazma korumasini ac	*/
	RTC->WPR = 0x53U;
	RTC->ISR |= RTC_ISR_INIT;
	while(!(RTC->ISR & RTC_ISR_INITF) && --t) {}
	RTC->PRER = 255U;									/* Senkron:  32768 / 128 / 256 = 1 Hz	*/
	RTC->PRER = (127U << 16) | 255U;					/* Asenkron (ayri yazilmali)		*/
	RTC->TR   = (Bcd(Sn / 3600U) << 16) | (Bcd((Sn / 60U) % 60U) << 8) | Bcd(Sn % 60U);
	RTC->DR   = (Bcd(y - 2000U) << 16) | ((((Top / 86400U) + 5U) % 7U + 1U) << 13)	/* Haftanin gunu: 1 = Pzt */
			  | (Bcd(a) << 8) | Bcd(g);
	RTC->CR   = (RTC->CR & ~RTC_CR_FMT) | RTC_CR_BYPSHAD;	/* 24 saat, golge yazmac yok		*/
	RTC->ISR &= ~RTC_ISR_INIT;
	RTC->WPR  = 0xFFU;
}

static uint32_t Simdi(void)		{ return (Rtc == RTC_HAZIR) ? RtcOku() : YazilimSn(); }

static void Yaz(uint32_t Top)
{
	TabanSn = Top;
	TabanMs = HAL_GetTick();
	if(Rtc == RTC_HAZIR)
		RtcYaz(Top);
}

void SAAT_Init(void)
{
	TabanSn = Gunler(1U, 1U, 2026U) * 86400U + 12U * 3600U;	/* Ayarlanana kadar */
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
	uint32_t s = Simdi() % 86400U;

	*Sa = (uint8_t)(s / 3600U);
	*Dk = (uint8_t)((s / 60U) % 60U);
	*Sn = (uint8_t)(s % 60U);
}

void SAAT_TarihGet(uint8_t *Gun, uint8_t *Ay, uint16_t *Yil)
{
	uint32_t g, a, y;

	Tarih(Simdi() / 86400U, &g, &a, &y);
	*Gun = (uint8_t)g; *Ay = (uint8_t)a; *Yil = (uint16_t)y;
}

uint8_t SAAT_Set(uint8_t Sa, uint8_t Dk, uint8_t Sn)
{
	if(Sa > 23U || Dk > 59U || Sn > 59U)
		return 0U;
	Yaz((Simdi() / 86400U) * 86400U + (uint32_t)Sa * 3600U + (uint32_t)Dk * 60U + Sn);
	return 1U;
}

uint8_t SAAT_TarihSet(uint8_t Gun, uint8_t Ay, uint16_t Yil)
{
	if(Yil < 2000U || Yil > 2099U || Ay < 1U || Ay > 12U || Gun < 1U || Gun > AyGun(Ay, Yil))
		return 0U;
	Yaz(Gunler(Gun, Ay, Yil) * 86400U + Simdi() % 86400U);
	return 1U;
}

/* Sicaklik / nem: disaridan (CMD_SENSOR) gelir; 10 dk yeni deger gelmezse gecersiz */
static int16_t  SnsT;
static uint8_t  SnsN, SnsVar;
static uint32_t SnsMs;

void SAAT_SensorYaz(int16_t Sicaklik10, uint8_t Nem)
{
	SnsT = Sicaklik10; SnsN = (Nem > 100U) ? 100U : Nem; SnsMs = HAL_GetTick(); SnsVar = 1U;
}

uint8_t SAAT_SensorOku(int16_t *Sicaklik10, uint8_t *Nem)
{
	*Sicaklik10 = SnsT; *Nem = SnsN;
	return (SnsVar && (HAL_GetTick() - SnsMs) < 600000UL) ? 1U : 0U;
}
