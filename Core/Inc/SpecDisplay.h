/**
  ******************************************************************************
  * @file    SpecDisplay.h
  * @brief   Muzik gorselleri -- 128x32, 17 mod
  ******************************************************************************
  * Cizim dogrudan HUB75 surucusunun cizim tamponuna yapilir (64x64 mantiksal
  * alan, Y 32..63 = ikinci panel); ayri tuval ve kopyalama yoktur.
  * Ayarlar SpecConfig.h icinde.
  ******************************************************************************
  */
#ifndef __SPECDISPLAY_H__
#define __SPECDISPLAY_H__

#include "main.h"
#include "SpecConfig.h"
#include "Spectrum.h"

#define SPEC_WIDTH			128
#define SPEC_HEIGHT			32

typedef enum
{
	SPEC_MODE_CLASSIC = 0,		/* Klasik: tum cubuk cesitleri ayarla	*/
	SPEC_MODE_FIRE,				/* Gercekci alev					*/
	SPEC_MODE_FIRE_BARS,		/* Alev cubuklar					*/
	SPEC_MODE_FIRE_BLUE,		/* Ortadan simetrik mavi gaz alevi	*/
	SPEC_MODE_BLAST,			/* Vurusta zeminden patlama			*/
	SPEC_MODE_CBLAST,			/* Ortadan kenarlara bas patlamasi	*/
	SPEC_MODE_FIRE_BALL,		/* Alev + her kick buyuk alev halkasi	*/
	SPEC_MODE_RINGS,
	SPEC_MODE_FOUNTAIN,
	SPEC_MODE_WAVE,
	SPEC_MODE_SPECWAVE,			/* Renkli Dalga: simetrik cubuk + sinus seritleri	*/
	SPEC_MODE_LIGHTLINE,		/* Isik Cizgisi: uzay, isik cizgisi + ileri ucus yildiz alani	*/
	SPEC_MODE_LAYERS,			/* Katmanli Dag: 3 ust uste dag silueti	*/
	SPEC_MODE_WEAVE,			/* Orgu Serit: dolanan ince cizgilerden serit	*/
	SPEC_MODE_AURORA,			/* Aurora: yildizli gokte isik perdesi + kayan yildiz	*/
	SPEC_MODE_SNOW,				/* Kar (muziksiz): gece kar yagisi, arkada golge saat	*/
	SPEC_MODE_HEARTH,			/* Somine (muziksiz): odun, alev, rakamlar alevin icinde	*/
	SPEC_MODE_COUNT
} SPEC_Mode;

/* Temel parlaklik %, 0 = ekran kapali. Vurus parlatmasi bunun ustune eklenir. */
extern uint8_t SPEC_Brightness;

/* Klasik modun ayarlari -- Settings.c yazar, sonra SPEC_ClsApply() */
typedef struct
{
	uint8_t		Width;		/* 1..4 px									*/
	uint8_t		Type;		/* 0 duz, 1 neon cerceve					*/
	uint8_t		Shape;		/* 0 yukari, 1 sarkan, 2 simetrik			*/
	uint8_t		ColorMode;	/* 0 tek renk, 1 Hi-Fi (alttan uste, Bar tonundan), 2 yatay gokkusagi, 3 akan */
	uint8_t		Blend;		/* 0 yumusak, 1 bolgeli						*/
	uint8_t		Peak;		/* 0 kapali, 1 nokta, 2 top					*/
	uint8_t		PeakColor;	/* 0 sabit (PeakRGB), 1 bant rengi			*/
	uint8_t		Layout;		/* 0 bas solda, 1 bas ortada (iki yana ayna)	*/
	uint32_t	Bar, PeakRGB;
} SPEC_ClsCfg;
extern SPEC_ClsCfg SPEC_Cls;


/* Hareket (her gorunumun kendi degeri, Settings.c yazar)
   SPEC_Speed: 0 yavas, 1 normal, 2 yuksek, 3 cok yuksek -- tempoya gore; cubuk dususunu etkilemez
   SPEC_React: 0 sakin (sadece guclu bas), 1 normal, 2 hareketli (her vurus)
   SPEC_ShowNo: mod degisince sol ustte gosterilen numara (gorunum no)	*/
extern uint8_t SPEC_Speed, SPEC_React, SPEC_ShowNo;
extern uint8_t SPEC_SnIsik, SPEC_SnYogun, SPEC_SnMeteor;	/* Kar: parlaklik %, 0..2, 0..2	*/
extern uint8_t SPEC_SaatYazi, SPEC_SaatYer;			/* Saatli modlar: font, yerlesim	*/
extern uint8_t SPEC_SmBoy, SPEC_SmKiv;				/* Somine: alev boyu 0..2, kivilcim 0..2	*/
uint8_t        SPEC_SaatYaziSayisi(void);			/* Font tablosundaki font sayisi	*/
extern uint8_t SPEC_Pal;						/* Katmanli Dag / Orgu Serit: renk ailesi 0..7	*/
extern uint8_t SPEC_LineSel;				/* Isik Cizgisi: renk ailesi 0..7 (0 = kirmizi>sari, orijinal)	*/
void    SPEC_ClsApply(void);
uint8_t SPEC_ClsBands(void);

/* Alev paleti. Sel: 0 modun kendi, 1 ates, 2 mavi gaz, 3 yesil, 4 mor, 5 buz,
   6 ozel (Kor / Govde / Uc), 7 pembe. Aktif mod degistikten SONRA cagrilmali.	*/
void SPEC_SetFirePalette(uint8_t Sel, uint32_t Kor, uint32_t Govde, uint32_t Uc);

void SPEC_DisplayInit(void);

/* Mod degistir (durum sifirlanir, sol ustte numara gosterilir) */
void SPEC_SetMode(SPEC_Mode Mode);

/* Yeni spektrum karesinden sonra cagir: cizer ve HUB75'e aktarir.
   HUB75_UpdateDisplay() cagiran tarafin isidir.						*/
void SPEC_Draw(void);
/* Ust bant OSD (panel menusu): Metin UTF-8, buyuk harfe cevrilir; Ton 0..359 ise altinda
   gokkusagi seridi ve imlec (-1 = yok); Ms sonra kaybolur. */
#define SPEC_OSD_BEYAZ	360		/* SPEC_Osd Ton: beyaz (renk seridinin basi) */
void     SPEC_Osd(const char *Metin, int16_t Ton, uint16_t Ms);
uint16_t SPEC_OsdGenislik(const char *Metin);		/* Piksel */
void SPEC_ShowLevel(uint8_t Percent);		/* Kisa sure alt kenarda seviye cubugu (parlaklik)	*/

#endif /* __SPECDISPLAY_H__ */
