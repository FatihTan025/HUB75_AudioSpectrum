/**
  ******************************************************************************
  * @file    Settings.h
  * @brief   Ayar tablosu -- kendini tanitan parametreler, yedek SRAM'de kayit
  ******************************************************************************
  * Her ayar tabloda bir kez tanimlanir (ad, tur, aralik, varsayilan).
  * PC / telefon uygulamasi ve ekran menusu listeyi buradan ogrenir.
  *
  * Iki kopya vardir:
  *   Canli  : RAM'de, SET_Set ile hemen degisir ve uygulanir
  *   Kayitli: RTC yedek SRAM'de (4 KB, VBAT pili ile korunur)
  * SET_Save canliyi kayitliya yazar, SET_Load geri doner.
  * Kayitli kopya bozuksa (pil bitti, tablo degisti) fabrika ayarlari yuklenir.
  ******************************************************************************
  */
#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdint.h>

/* --- Parametre turleri (protokolde de ayni) --------------------------- */
#define SET_TYPE_BOOL			0U
#define SET_TYPE_INT			1U
#define SET_TYPE_ENUM			2U		/* Secenekler "A|B|C"					*/
#define SET_TYPE_COLOR			3U		/* 0x00RRGGBB							*/
#define SET_TYPE_PALETTE		4U		/* Ileride: tema renk listesi			*/
#define SET_TYPE_HUE			5U		/* Renk tonu 0..359 (kaydirma cubugu)	*/
#define SET_TYPE_SWATCH			6U		/* Bloklu renk cubugu: Min..Max secim,	*/
										/* Options "Ad:RRGGBB|..." (gecisli: "Ad:RRGGBB,RRGGBB") */

/* --- Bayraklar ---------------------------------------------------------- */
#define SET_FLAG_PER_MODE		0x01U	/* Her modun kendi degeri var			*/
#define SET_FLAG_READONLY		0x02U
#define SET_FLAG_NOSAVE			0x04U	/* Kaydedilmez (canli)					*/
#define SET_FLAG_HIDDEN			0x08U	/* Gosterilmez: gorunume sabit ayar		*/
#define SET_FLAG_ADVANCED		0x10U	/* Gelismis: PC aracinda gorunur, Android'de gizli */

#define SET_MODE_GLOBAL			0xFFU	/* Mod no yerine: genel ayar			*/
#define SET_NAME_LEN			16U

/* --- Durum kodlari (protokoldeki ile ayni) ------------------------------ */
#define SET_OK					0U
#define SET_ERR_PARAM			2U
#define SET_ERR_RANGE			3U
#define SET_ERR_READONLY		6U

/* --- Parametre kimlikleri ------------------------------------------------
     0x00xx genel, 0x01xx mod ortak, 0x02xx alev/efekt, 0x03xx muziksiz	*/
#define SET_ID_BRIGHTNESS		0x0001U
#define SET_ID_AUTO_SEC			0x0002U
#define SET_ID_FREQ_LO			0x0003U	/* Alt frekans, Hz	*/
#define SET_ID_FREQ_HI			0x0004U	/* Ust frekans, Hz	*/
#define SET_ID_SENS				0x0005U	/* Vurus hassasiyeti 0..100	*/
#define SET_ID_FAV				0x0006U	/* Favori modlar: bit n = mod n (gizli)	*/
#define SET_ID_CLS_COLOR       0x0101U
#define SET_ID_CLS_BAR_RGB     0x0102U
#define SET_ID_CLS_PEAK        0x0103U
#define SET_ID_CLS_PEAK_RGB    0x0104U
#define SET_ID_CLS_WIDTH       0x0106U
#define SET_ID_CLS_TYPE        0x0107U
#define SET_ID_CLS_SHAPE       0x0108U
#define SET_ID_CLS_BLEND       0x010DU
#define SET_ID_CLS_PEAK_COL    0x0110U
#define SET_ID_CLS_LAYOUT      0x0112U
#define SET_ID_CLS_PEAK_STYLE  0x0113U	/* Gizli: gorunumun sabit tepe sekli	*/
#define SET_ID_SPEED           0x0120U
#define SET_ID_REACT           0x0121U
#define SET_ID_FIRE_COLOR		0x0206U	/* Alev rengi: hazir paletlerden biri	*/
#define SET_ID_LL_COLOR		0x0208U	/* Isik Cizgisi: renk ailesi (blok)	*/
#define SET_ID_PAL		0x0209U	/* Katmanli Dag / Orgu Serit: renk ailesi (8)	*/
#define SET_ID_SN_ISIK		0x0301U	/* Kar: tane parlakligi %			*/
#define SET_ID_SN_YOGUN		0x0302U	/* Kar: tane sayisi (Az/Orta/Cok)	*/
#define SET_ID_SN_METEOR	0x0303U	/* Kar: kayan yildiz sikligi		*/
#define SET_ID_SAAT_YAZI	0x0304U	/* Saatli modlar: saat fontu		*/
#define SET_ID_SAAT_YER		0x0305U	/* Saatli modlar: hazir yerlesim	*/
#define SET_ID_SM_BOY		0x0306U	/* Somine: alev boyu				*/
#define SET_ID_SM_KIV		0x0307U	/* Somine: kivilcim sikligi			*/
#define SET_ID_YM_SIDDET	0x0308U	/* Yagmur: siddet					*/
#define SET_ID_YM_RUZGAR	0x0309U	/* Yagmur: ruzgar (egim)			*/
#define SET_ID_SH_DALGA		0x030AU	/* Sahil: dalga						*/
#define SET_ID_SH_RENK		0x030BU	/* Sahil: renk (Gece/Gun batimi/Gunduz)	*/

typedef struct
{
	uint16_t	Id;
	uint8_t		Type;
	uint8_t		Flags;
	uint32_t	ModeMask;		/* PER_MODE ise hangi modlarda var (bit = mod)	*/
	int32_t		Min;
	int32_t		Max;
	int32_t		Step;
	int32_t		Def;
	const char	*Name;			/* UTF-8									*/
	const char	*Group;
	const char	*Options;		/* ENUM: "A|B|C", INT: birim, diger: ""		*/
	void		(*Apply)(uint8_t Mode, int32_t Value);
	uint16_t	ShowId;			/* 0 = hep gorunur; degilse bu ayar...		*/
	int16_t		ShowLo, ShowHi;	/* ...bu aralikta iken gorunur (ayni mod)	*/
} SET_Param;

/* Gorunum ("mod"): kullanicinin sectigi sey. Bir cizim motoru (SPEC_Mode)
   + o motorun ayarlarina kendi varsayilanlari. Ayarlar gorunum basina saklanir. */
typedef struct
{
	const char		*Name;
	uint8_t			Kind;			/* 0 = muzik, 1 = muziksiz				*/
	uint8_t			Family;			/* 0 klasik, 1 modern, 2 animasyon, 3 ortam (grup/sira) */
	uint8_t			Engine;			/* SPEC_Mode							*/
	uint8_t			NOvr;
	const int32_t	(*Ovr)[2];		/* { Id, varsayilan }					*/
} SET_ModeInfo;

uint8_t				SET_LookCount(void);

void				SET_Init(void);			/* Yukle + uygula				*/
void				SET_Poll(uint32_t Now);	/* Otomatik kayit				*/

uint16_t			SET_Count(void);
const SET_Param		*SET_Desc(uint16_t Index);
const SET_ModeInfo	*SET_Mode(uint8_t Mode);

uint8_t				SET_Get(uint8_t Mode, uint16_t Id, int32_t *Value);
uint8_t				SET_Set(uint8_t Mode, uint16_t Id, int32_t Value, int32_t *Applied);
int32_t				SET_Global(uint16_t Id);	/* Kisa yol, hata kontrolu yok	*/

uint8_t				SET_GetCurMode(void);
uint8_t				SET_NextAuto(uint8_t Mode);	/* Oto gecis: siradaki muzikli favori	*/
void				SET_SetCurMode(uint8_t Mode);

const char			*SET_GetName(void);
void				SET_SetName(const char *Name, uint8_t Len);

void				SET_Save(void);
void				SET_Load(void);			/* Kayitli yoksa fabrika		*/
void				SET_Factory(void);		/* Canliyi fabrika yapar		*/

/* Dugme ile yapilan degisiklik: birkac saniye sonra kendiliginden kaydet */
void				SET_MarkLocalChange(void);

/* Ekran acik / kapali (kaydedilmez) */
void				SET_SetPower(uint8_t On);
uint8_t				SET_GetPower(void);

#endif /* __SETTINGS_H__ */
