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

/* --- Kayma ogrenme (otomatik ince ayar) ------------------------------------
   Guvenilir bir kaynaktan (telefonun ag saati, ileride NTP) tam saat+tarih
   geldikce cihaz kendi saatinin ne kadar kaydigini olcer ve RTC'nin ince
   ayarini (CALR, ~0.95 ppm adim) duzeltir. Olcum bir "referans" anindan
   (son ogrenme) beri biriken toplam farka dayanir: kisa araliklarla gelen
   esitlemeler de birikir, uzun sure baglanti olmasa da olcum bozulmaz.
   Onlemler (hepsi kayma yerine "ayar degisikligi" sayilip ogrenilmez):
     - Kaynak guvenilir degil (elle, PC araci, telefonun otomatik saati kapali)
     - Tek esitlemede fark 30 sn + gecen sure x 250 ppm'den (en cok 30 dk) buyuk
       (saat dilimi / yanlis tarih / pil bitmis)
     - Referanstan beri sure < OGR_EN_AZ_SN ya da > OGR_EN_COK_SN, geri gitmis
     - Biriken fark < OGR_FARK_SN (olcum cozunurlugunun alti: biriktirmeye devam)
     - Olculen kayma > KAY_EN_COK_PPB (fiziksel olarak inandirici degil)
     - Saat kaynagi degismis (RTC <-> yazilim saati) ya da acilista saat yeniden basladi
   Duzeltme guvenle carpilir (fark / (fark + 2 sn): kucuk farkta temkinli),
   bir seferde en cok ADIM_EN_COK_PPB degisir, toplam +-DUZ_EN_COK_PPB ile sinirli.
   Durum RTC yedek yazmaclarinda (VBAT), sihirli sayi + saglama ile korunur;
   bozuksa her sey sifirlanir (duzeltme yok, en kotu durum = eski davranis).
   ------------------------------------------------------------------------- */
#define ESIT_TABAN_SN		30			/* Tek esitlemede izin: 30 sn + sure x KAY_EN_COK, en cok ESIT_ATLA_SN	*/
#define ESIT_ATLA_SN		1800		/* Saat dilimi (>= 30 dk) asla kayma sayilmaz			*/
#define OGR_EN_AZ_SN		(6UL * 3600UL)		/* Referanstan beri en az bu kadar gecmeli			*/
#define OGR_EN_COK_SN		(400UL * 86400UL)	/* Bundan eski referans gecersiz					*/
#define OGR_FARK_SN			3			/* Biriken fark en az bu kadar olunca ogrenilir		*/
#define KAY_EN_COK_PPB		250000L		/* Olculen kayma en cok 250 ppm						*/
#define ADIM_EN_COK_PPB		60000L		/* Bir ogrenmede en cok 60 ppm degisir				*/
#define DUZ_EN_COK_PPB		300000L		/* Toplam duzeltme siniri (CALR: -487..+488 ppm)		*/

#define BKP_SIHIR			0x5A470001UL	/* Surum 1								*/
typedef enum { KYN_YOK = 0, KYN_RTC = 1, KYN_YAZ = 2 } Kaynak;
/* Yedek yazmaclar: 0 sihir, 1 referans ani (2000'den sn, 0 = yok), 2 biriken fark (sn, isaretli),
   3 referansin kaynagi, 4 RTC duzeltmesi (ppb), 5 yazilim saati duzeltmesi (ppb), 6 ogrenme sayisi, 7 saglama */
#define BKP(n)				(*(&RTC->BKP0R + (n)))

typedef enum { RTC_YOK = 0, RTC_BEKLE, RTC_HAZIR } RtcDurum;

static RtcDurum	Rtc;
static uint32_t	LseBas;
static uint32_t	TabanSn;			/* Yazilim saati: 2000-01-01'den beri sn = TabanSn + (Ms - TabanMs) / 1000 */
static uint64_t	Ms, TabanMs;		/* Duzeltilmis ms sayaci (HAL_GetTick 49 gunde tasar; bu tasmaz)	*/
static uint32_t	SonTick;
static int64_t	MsKes;				/* Duzeltmenin ms kesri (ppb * ms)						*/

static struct { uint32_t Ref, Kyn, Sayi; int32_t Fark, DuzRtc, DuzYaz; } Kl;	/* Yedek yazmaclarin kopyasi */

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

static int32_t Sinirla(int32_t v, int32_t s) { return (v > s) ? s : (v < -s) ? -s : v; }

/* --- Yedek yazmaclar ------------------------------------------------------- */
static uint32_t Saglama(void)
{
	return BKP_SIHIR ^ (Kl.Ref * 2654435761UL) ^ ((uint32_t)Kl.Fark * 40503UL) ^ (Kl.Kyn << 28) ^
	       ((uint32_t)Kl.DuzRtc * 69069UL) ^ ((uint32_t)Kl.DuzYaz * 1103515245UL) ^ (Kl.Sayi << 8) ^ 0xA5C3E1F7UL;
}

static void KlYaz(void)
{
	BKP(1) = Kl.Ref; BKP(2) = (uint32_t)Kl.Fark; BKP(3) = Kl.Kyn; BKP(4) = (uint32_t)Kl.DuzRtc;
	BKP(5) = (uint32_t)Kl.DuzYaz; BKP(6) = Kl.Sayi; BKP(7) = Saglama(); BKP(0) = BKP_SIHIR;
}

static void KlOku(void)
{
	Kl.Ref = BKP(1); Kl.Fark = (int32_t)BKP(2); Kl.Kyn = BKP(3); Kl.DuzRtc = (int32_t)BKP(4);
	Kl.DuzYaz = (int32_t)BKP(5); Kl.Sayi = BKP(6);
	if(BKP(0) != BKP_SIHIR || BKP(7) != Saglama() || Kl.Kyn > KYN_YAZ ||
	   Kl.DuzRtc > DUZ_EN_COK_PPB || Kl.DuzRtc < -DUZ_EN_COK_PPB || Kl.DuzYaz > DUZ_EN_COK_PPB || Kl.DuzYaz < -DUZ_EN_COK_PPB ||
	   Kl.Fark > ESIT_ATLA_SN || Kl.Fark < -ESIT_ATLA_SN)
	{
		Kl.Ref = 0U; Kl.Fark = 0; Kl.Kyn = KYN_YOK; Kl.DuzRtc = Kl.DuzYaz = 0; Kl.Sayi = 0U;	/* Bozuk / ilk acilis: duzeltme yok */
		KlYaz();
	}
}

static void RefSil(void) { Kl.Ref = 0U; Kl.Fark = 0; Kl.Kyn = KYN_YOK; KlYaz(); }

/* RTC ince ayari: ppb -> CALR (CALP = +488.5 ppm, CALM adimi -0.9537 ppm; 2^20 darbede) */
static void RtcKalibre(int32_t Ppb)
{
	int32_t x = (int32_t)(((int64_t)Sinirla(Ppb, DUZ_EN_COK_PPB) * 1048576LL + (Ppb >= 0 ? 500000000LL : -500000000LL)) / 1000000000LL);
	uint32_t Calr, t = 100000U;
	Calr = (x > 0) ? (RTC_CALR_CALP | (uint32_t)(512 - ((x > 512) ? 512 : x))) : (uint32_t)((-x > 511) ? 511 : -x);
	RTC->WPR = 0xCAU;
	RTC->WPR = 0x53U;
	while((RTC->ISR & RTC_ISR_RECALPF) && --t) {}
	if(t) RTC->CALR = Calr;
	RTC->WPR = 0xFFU;
}

/* --- Saat kaynaklari ------------------------------------------------------- */
static void YazilimIlerle(void)									/* Duzeltmeli ms sayaci */
{
	const uint32_t t = HAL_GetTick(), d = t - SonTick;
	SonTick = t;
	Ms    += d;
	MsKes += (int64_t)d * Kl.DuzYaz;							/* +ppb: hizlandir */
	if(MsKes >= 1000000000LL || MsKes <= -1000000000LL)
	{
		const int64_t k = MsKes / 1000000000LL;
		Ms += (uint64_t)k; MsKes -= k * 1000000000LL;
	}
}

static uint32_t YazilimSn(void)
{
	YazilimIlerle();
	return TabanSn + (uint32_t)((Ms - TabanMs) / 1000U);
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

static Kaynak SuankiKaynak(void)	{ return (Rtc == RTC_HAZIR) ? KYN_RTC : KYN_YAZ; }
static uint32_t Simdi(void)		{ return (Rtc == RTC_HAZIR) ? RtcOku() : YazilimSn(); }

static void Yaz(uint32_t Top)
{
	YazilimIlerle();
	TabanSn = Top;
	TabanMs = Ms;
	if(Rtc == RTC_HAZIR)
		RtcYaz(Top);
}

/* Guvenilir esitleme: farki biriktirir, yeterince bilgi varsa kaymayi ogrenir */
static void Ogren(uint32_t Dogru)
{
	const Kaynak k = SuankiKaynak();
	const int32_t e = (int32_t)(Simdi() - Dogru);					/* + : cihaz ileride */
	int32_t *Duz = (k == KYN_RTC) ? &Kl.DuzRtc : &Kl.DuzYaz;

	const int32_t Izin = (Kl.Ref && Dogru > Kl.Ref) ?
	                     (int32_t)((ESIT_TABAN_SN + (uint64_t)(Dogru - Kl.Ref) * KAY_EN_COK_PPB / 1000000000ULL < ESIT_ATLA_SN) ?
	                               ESIT_TABAN_SN + (uint64_t)(Dogru - Kl.Ref) * KAY_EN_COK_PPB / 1000000000ULL : ESIT_ATLA_SN) : ESIT_TABAN_SN;

	if(e > Izin || e < -Izin || Kl.Ref == 0U || Kl.Kyn != (uint32_t)k ||
	   Dogru <= Kl.Ref || (Dogru - Kl.Ref) > OGR_EN_COK_SN)
	{																/* Ayar degisikligi / ilk olcum: yeni referans */
		Kl.Ref = Dogru; Kl.Fark = 0; Kl.Kyn = (uint32_t)k; KlYaz();
		return;
	}
	Kl.Fark += e;
	if(Kl.Fark > ESIT_ATLA_SN || Kl.Fark < -ESIT_ATLA_SN)			/* Birikim olmayacak kadar buyuk: guvenme */
	{
		Kl.Ref = Dogru; Kl.Fark = 0; KlYaz();
		return;
	}
	{
		const uint32_t Sure = Dogru - Kl.Ref;
		const int32_t  Af = (Kl.Fark < 0) ? -Kl.Fark : Kl.Fark;
		int64_t Kay;
		if(Sure < OGR_EN_AZ_SN || Af < OGR_FARK_SN) { KlYaz(); return; }	/* Biriktirmeye devam */
		Kay = (int64_t)Kl.Fark * 1000000000LL / (int64_t)Sure;		/* ppb, + : hizli */
		if(Kay > KAY_EN_COK_PPB || Kay < -KAY_EN_COK_PPB)
		{															/* Inandirici degil: referansi yenile, duzeltmeye dokunma */
			Kl.Ref = Dogru; Kl.Fark = 0; KlYaz();
			return;
		}
		Kay = Kay * Af / (Af + 2);									/* Olcum guveni: +-2 sn belirsizlik */
		*Duz = Sinirla(*Duz - Sinirla((int32_t)Kay, ADIM_EN_COK_PPB), DUZ_EN_COK_PPB);
		Kl.Ref = Dogru; Kl.Fark = 0; Kl.Sayi++;
		KlYaz();
		if(k == KYN_RTC) RtcKalibre(Kl.DuzRtc);
	}
}

void SAAT_Init(void)
{
	TabanSn = Gunler(1U, 1U, 2026U) * 86400U + 12U * 3600U;	/* Ayarlanana kadar */
	SonTick = HAL_GetTick();
	Ms = TabanMs = 0U; MsKes = 0;

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	PWR->CR      |= PWR_CR_DBP;							/* Yedek alana yazma izni */
	KlOku();

	if((RCC->BDCR & RCC_BDCR_RTCEN) && (RCC->BDCR & RCC_BDCR_LSERDY))
	{
		Rtc     = RTC_HAZIR;							/* Pil sayesinde saat suruyor */
		TabanSn = RtcOku();
		RtcKalibre(Kl.DuzRtc);							/* Ogrenilmis ince ayar (yazmac zaten korunur, tutarli olsun) */
		if(Kl.Kyn != KYN_RTC) RefSil();
		return;
	}
	RefSil();											/* Saat yeniden basladi: eski referans anlamsiz */
	RCC->BDCR |= RCC_BDCR_LSEON;						/* Kristali baslat, beklemeden devam */
	LseBas     = HAL_GetTick();
	Rtc        = RTC_BEKLE;
}

void SAAT_Poll(void)
{
	YazilimIlerle();									/* HAL_GetTick tasmasini kacirmasin */
	if(Rtc != RTC_BEKLE)
		return;
	if(RCC->BDCR & RCC_BDCR_LSERDY)
	{
		if((RCC->BDCR & RCC_BDCR_RTCSEL) == 0U)
			RCC->BDCR |= RCC_BDCR_RTCSEL_0;				/* RTC saati = LSE */
		RCC->BDCR |= RCC_BDCR_RTCEN;
		RtcYaz(YazilimSn());							/* O ana kadarki yazilim saatini aktar */
		RtcKalibre(Kl.DuzRtc);
		Rtc = RTC_HAZIR;
		RefSil();										/* Kaynak degisti */
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

/* Elle ayar (yalniz saat ya da yalniz tarih): ogrenmez, referans silinir */
uint8_t SAAT_Set(uint8_t Sa, uint8_t Dk, uint8_t Sn)
{
	if(Sa > 23U || Dk > 59U || Sn > 59U)
		return 0U;
	Yaz((Simdi() / 86400U) * 86400U + (uint32_t)Sa * 3600U + (uint32_t)Dk * 60U + Sn);
	RefSil();
	return 1U;
}

uint8_t SAAT_TarihSet(uint8_t Gun, uint8_t Ay, uint16_t Yil)
{
	if(Yil < 2000U || Yil > 2099U || Ay < 1U || Ay > 12U || Gun < 1U || Gun > AyGun(Ay, Yil))
		return 0U;
	Yaz(Gunler(Gun, Ay, Yil) * 86400U + Simdi() % 86400U);
	RefSil();
	return 1U;
}

/* Tam saat + tarih. Guvenilir (ag saatli telefon / NTP) ise once kaymayi olcer.
   Tum alanlar dogrulanir; biri gecersizse hicbir sey degismez. */
uint8_t SAAT_Esitle(uint8_t Sa, uint8_t Dk, uint8_t Sn, uint8_t Gun, uint8_t Ay, uint16_t Yil, uint8_t Guvenilir)
{
	uint32_t Dogru;
	if(Sa > 23U || Dk > 59U || Sn > 59U || Yil < 2000U || Yil > 2099U || Ay < 1U || Ay > 12U || Gun < 1U || Gun > AyGun(Ay, Yil))
		return 0U;
	Dogru = Gunler(Gun, Ay, Yil) * 86400U + (uint32_t)Sa * 3600U + (uint32_t)Dk * 60U + Sn;
	if(Guvenilir) Ogren(Dogru);
	else          RefSil();
	Yaz(Dogru);
	return 1U;
}

/* Durum (PC araci / hata ayiklama): duzeltme ppb, ogrenme sayisi, biriken fark, referanstan beri sn */
void SAAT_KalDurum(int32_t *DuzPpb, uint32_t *Sayi, int32_t *Fark, uint32_t *Sure)
{
	*DuzPpb = (SuankiKaynak() == KYN_RTC) ? Kl.DuzRtc : Kl.DuzYaz;
	*Sayi = Kl.Sayi; *Fark = Kl.Fark;
	*Sure = (Kl.Ref && Simdi() > Kl.Ref) ? Simdi() - Kl.Ref : 0U;
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
