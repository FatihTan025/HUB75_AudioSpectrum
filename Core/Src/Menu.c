/**
  ******************************************************************************
  * @file    Menu.c
  * @brief   Panel menusu -- bkz. Menu.h
  ******************************************************************************
  */
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "Menu.h"
#include "Settings.h"
#include "SpecDisplay.h"
#include "Protocol.h"

#define MENU_ZAMAN_MS	8000U			/* Islem yoksa kapan					*/
#define AD_MS			1000U			/* Ayara gecince once yalniz adi		*/
#define TON_ADIM		10				/* Renk: tik basina derece				*/

static uint8_t	Acik;
static uint16_t	Sira;					/* Tablodaki indeks						*/
static uint8_t	Mod;					/* Menu hangi mod icin acildi			*/
static uint32_t	SonIslem, AdBitis;
static char		Satir[48];

/* Kullaniciya gorunen mod ayari mi? (gizli/gelismis degil, bu modda var, kosulu tutuyor) */
static uint8_t Uygun(uint16_t i)
{
	const SET_Param *d = SET_Desc(i);
	int32_t v;

	if(!d || !(d->Flags & SET_FLAG_PER_MODE) || (d->Flags & (SET_FLAG_HIDDEN | SET_FLAG_ADVANCED | SET_FLAG_READONLY)))
		return 0U;
	if(SET_Get(Mod, d->Id, &v) != SET_OK)
		return 0U;
	if(d->ShowId && (SET_Get(Mod, d->ShowId, &v) != SET_OK || v < d->ShowLo || v > d->ShowHi))
		return 0U;
	return 1U;
}

static int32_t Bul(uint16_t Bas)
{
	for(uint16_t i = Bas; i < SET_Count(); i++)
		if(Uygun(i)) return (int32_t)i;
	return -1;
}

/* Secenek listesinden (ENUM "A|B|C", BLOK "Ad:RRGGBB|...") n'inci adi kopyalar */
static void Secenek(const char *s, int32_t n, char *o, uint32_t Boy)
{
	uint32_t k = 0;
	while(n > 0 && *s) { if(*s++ == '|') n--; }
	while(*s && *s != '|' && *s != ':' && k + 1U < Boy) o[k++] = *s++;
	o[k] = 0;
}

/* RGB <-> ton (tam doygun) */
static int32_t RgbTon(uint32_t c)
{
	int32_t r = (int32_t)((c >> 16) & 0xFF), g = (int32_t)((c >> 8) & 0xFF), b = (int32_t)(c & 0xFF);
	int32_t mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn, h;
	if(d == 0) return 0;
	if(mx == r)      h = 60 * (g - b) / d;
	else if(mx == g) h = 120 + 60 * (b - r) / d;
	else             h = 240 + 60 * (r - g) / d;
	return (h + 360) % 360;
}

/* Renk ayari konumu: 0 = beyaz, 1..36 = ton (k-1)*10 derece. Encoder mordan sonra beyaza gecer. */
#define RENK_KONUM	(1 + 360 / TON_ADIM)

static uint8_t Beyazmi(uint32_t c)
{
	int32_t r = (int32_t)((c >> 16) & 0xFF), g = (int32_t)((c >> 8) & 0xFF), b = (int32_t)(c & 0xFF);
	int32_t mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
	return (mx > 0 && (mx - mn) * 4 < mx) ? 1U : 0U;			/* Doygunluk < %25 */
}

static uint32_t TonRgb(int32_t h)
{
	uint32_t r, g, b;

	h = ((h % 360) + 360) % 360;
	switch(h / 60)
	{
	case 0:  r = 255; g = (uint32_t)(255 * h / 60);         b = 0;   break;
	case 1:  r = (uint32_t)(255 * (120 - h) / 60); g = 255; b = 0;   break;
	case 2:  r = 0;   g = 255; b = (uint32_t)(255 * (h - 120) / 60); break;
	case 3:  r = 0;   g = (uint32_t)(255 * (240 - h) / 60); b = 255; break;
	case 4:  r = (uint32_t)(255 * (h - 240) / 60); g = 0;   b = 255; break;
	default: r = 255; g = 0;   b = (uint32_t)(255 * (360 - h) / 60); break;
	}
	return (r << 16) | (g << 8) | b;
}

/* OSD'yi gunceller: once yalniz ad, sonra "AD: DEGER" (sigmazsa yalniz deger) */
static void Goster(uint32_t Now)
{
	const SET_Param *d = SET_Desc(Sira);
	char Deger[32] = "";
	int32_t v = 0, Ton = -1;

	if(!d) return;
	(void)SET_Get(Mod, d->Id, &v);
	switch(d->Type)
	{
	case SET_TYPE_BOOL:		strcpy(Deger, v ? "A\303\207IK" : "KAPALI");	break;		/* ACIK */
	case SET_TYPE_ENUM:
	case SET_TYPE_SWATCH:	Secenek(d->Options, v - d->Min, Deger, sizeof(Deger));	break;
	case SET_TYPE_HUE:		Ton = v;									break;
	case SET_TYPE_COLOR:	Ton = Beyazmi((uint32_t)v) ? SPEC_OSD_BEYAZ : RgbTon((uint32_t)v);	break;
	default:				snprintf(Deger, sizeof(Deger), "%ld", (long)v);	break;
	}
	if((int32_t)(AdBitis - Now) > 0 || !Deger[0])
		snprintf(Satir, sizeof(Satir), "%s", d->Name);
	else
	{
		snprintf(Satir, sizeof(Satir), "%s: %s", d->Name, Deger);
		if(SPEC_OsdGenislik(Satir) > SPEC_WIDTH)
			snprintf(Satir, sizeof(Satir), "%s", Deger);
	}
	SPEC_Osd(Satir, (int16_t)Ton, MENU_ZAMAN_MS);
}

uint8_t MENU_Acik(void) { return Acik; }

void MENU_Kapat(void)
{
	if(!Acik) return;
	Acik = 0;
	SPEC_Osd("TAMAM", -1, 900U);
}

void MENU_Ileri(void)
{
	int32_t i;
	const uint32_t Now = HAL_GetTick();

	if(!Acik)
	{
		Mod = SET_GetCurMode();
		i = Bul(0);
		if(i < 0) { SPEC_Osd("AYAR YOK", -1, 900U); return; }
		Acik = 1;
	}
	else
	{
		i = Bul((uint16_t)(Sira + 1U));
		if(i < 0) { MENU_Kapat(); return; }
	}
	Sira     = (uint16_t)i;
	SonIslem = Now;
	AdBitis  = Now + AD_MS;
	Goster(Now);
}

void MENU_Degistir(int32_t Adim)
{
	const SET_Param *d;
	int32_t v, Uyg, Eski;
	const uint32_t Now = HAL_GetTick();

	if(!Acik || !Adim || !(d = SET_Desc(Sira))) return;
	if(SET_Get(Mod, d->Id, &v) != SET_OK) { MENU_Kapat(); return; }
	Eski = v;
	switch(d->Type)
	{
	case SET_TYPE_BOOL:		v = !v;															break;
	case SET_TYPE_HUE:		v = ((v + Adim * TON_ADIM) % 360 + 360) % 360;					break;
	case SET_TYPE_COLOR:
	{
		int32_t k = Beyazmi((uint32_t)v) ? 0 : 1 + (RgbTon((uint32_t)v) + TON_ADIM / 2) / TON_ADIM % (RENK_KONUM - 1);
		k = ((k + Adim) % RENK_KONUM + RENK_KONUM) % RENK_KONUM;
		v = (k == 0) ? 0xFFFFFF : (int32_t)TonRgb((k - 1) * TON_ADIM);
		break;
	}
	default:
		v += Adim * ((d->Step > 0) ? d->Step : 1);
		if(v < d->Min) v = d->Min;
		if(v > d->Max) v = d->Max;
		break;
	}
	{
		Uyg = v;
		if((SET_Set(Mod, d->Id, v, &Uyg) == SET_OK || Uyg != v) && Uyg != Eski)
		{
			SET_MarkLocalChange();
			PROTO_NotifyParam(Mod, d->Id, Uyg);
		}
	}
	SonIslem = Now;
	AdBitis  = Now;								/* Cevirince deger gorunsun */
	if(!Uygun(Sira))							/* Bagli ayar gizlendiyse (or. Renk degisti) */
	{
		int32_t i = Bul(Sira);
		if(i < 0) i = Bul(0);
		if(i < 0) { MENU_Kapat(); return; }
		Sira = (uint16_t)i;
	}
	Goster(Now);
}

void MENU_Poll(uint32_t Now)
{
	static uint8_t AdGosterildi;

	if(!Acik) return;
	if(SET_GetCurMode() != Mod || (Now - SonIslem) > MENU_ZAMAN_MS)
	{
		MENU_Kapat();
		return;
	}
	/* Ad suresi dolunca degeri goster (bir kez) */
	if((int32_t)(AdBitis - Now) <= 0) { if(!AdGosterildi) { AdGosterildi = 1; Goster(Now); } }
	else AdGosterildi = 0;
}
