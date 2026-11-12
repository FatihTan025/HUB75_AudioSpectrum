/**
  ******************************************************************************
  * @file    Settings.c
  * @brief   Ayar tablosu ve yedek SRAM kaydi -- bkz. Settings.h
  ******************************************************************************
  * Yeni ayar eklemek: Table[]'a bir satir + gerekiyorsa Apply fonksiyonu.
  * Tablo duzeni degisince eski kayit gecersiz sayilir (fabrika ayari).
  * Metinler UTF-8, Turkce harfler sekizli kacisla yazildi (dosya kodlamasi
  * ne olursa olsun bozulmasin diye). Okunur hali yanlarinda.
  ******************************************************************************
  */
#pragma GCC optimize("O2")

#include <string.h>
#include <stddef.h>
#include "main.h"
#include "Settings.h"
#include "SpecConfig.h"
#include "SpecDisplay.h"
#include "HUB75.h"

#define SET_GLOBAL_MAX			32U		/* Genel ayar yuvasi				*/
#define SET_MODE_MAX			24U		/* Mod basina ayar yuvasi			*/
#define SET_MAGIC				0x314B5053UL	/* "SPK1"					*/
#define SET_AUTOSAVE_MS			3000U

/* ==========================================================================
   Mod listesi -- SPEC_Mode sirasiyla AYNI
   ========================================================================== */
/* ==========================================================================
   Gorunumler -- kullanicinin listesi, sira = mod numarasi.
   Her biri bir cizim motoru + o motorun ayarlarina kendi varsayilanlari.
   ========================================================================== */
#define O(...)		((const int32_t[][2]){ __VA_ARGS__ })
#define LOOK(Name, Fam, Eng, ...)	{ Name, 0, Fam, Eng, \
	(uint8_t)(sizeof(O(__VA_ARGS__)) / sizeof(O(__VA_ARGS__)[0])), O(__VA_ARGS__) }
#define LOOK0(Name, Fam, Eng)		{ Name, 0, Fam, Eng, 0, 0 }
#define ORTAM(Name, Eng)			{ Name, 1, 3, Eng, 0, 0 }		/* Muziksiz (Kind 1), Ortam ailesi */

static const SET_ModeInfo ModeInfo[] =
{
	/* --- Klasik (aile 0) --- */
	LOOK0("Klasik",							0, SPEC_MODE_CLASSIC),
	LOOK("Neon",							0, SPEC_MODE_CLASSIC,
		{ SET_ID_CLS_WIDTH, 4 }, { SET_ID_CLS_TYPE, 1 }, { SET_ID_CLS_PEAK_STYLE, 2 }, { SET_ID_CLS_BAR_RGB, 0x00C8FF }),
	LOOK("Orta Da\304\237",					0, SPEC_MODE_CLASSIC,	/* Orta Dag */
		{ SET_ID_CLS_WIDTH, 2 }, { SET_ID_CLS_LAYOUT, 1 }, { SET_ID_CLS_BAR_RGB, 0xFF0080 }),	/* Hi-Fi: pembe -> mor -> mavi */
	LOOK0("F\304\261skiye",					0, SPEC_MODE_FOUNTAIN),	/* Fiskiye */
	LOOK0("Renkli Dalga",					0, SPEC_MODE_SPECWAVE),
	LOOK0("Dalga",							0, SPEC_MODE_WAVE),
	LOOK0("Katmanl\304\261 Da\304\237",				0, SPEC_MODE_LAYERS),
	/* --- Modern (aile 1) --- */
	LOOK0("I\305\237\304\261k \303\207izgisi",			1, SPEC_MODE_LIGHTLINE),
	LOOK("\303\226rg\303\274 \305\236erit",				1, SPEC_MODE_WEAVE,
		{ SET_ID_PAL, 7 }),
	LOOK0("Aurora",							1, SPEC_MODE_AURORA),
	/* --- Animasyon (aile 2) --- */
	LOOK0("Alev",							2, SPEC_MODE_FIRE),
	LOOK0("Alev \303\207ubuk",				2, SPEC_MODE_FIRE_BARS),	/* Alev Cubuk */
	LOOK0("Orta Alev",						2, SPEC_MODE_FIRE_BLUE),	/* Ortadan iki yana */
	LOOK0("Alev Halka",						2, SPEC_MODE_FIRE_BALL),
	LOOK0("Patlama",						2, SPEC_MODE_BLAST),
	LOOK0("Orta Patlama",					2, SPEC_MODE_CBLAST),
	LOOK0("Halka",							2, SPEC_MODE_RINGS),
	/* --- Ortam (aile 3, Kind 1 = muziksiz) --- */
	ORTAM("Kar",							SPEC_MODE_SNOW),
};

#define LOOK_COUNT	(sizeof(ModeInfo) / sizeof(ModeInfo[0]))

/* ==========================================================================
   Uygulama fonksiyonlari
   ========================================================================== */
static uint8_t	PowerOn = 1U;

static void ApplyBrightness(uint8_t Mode, int32_t Value)
{
	(void)Mode;
	SPEC_Brightness = PowerOn ? (uint8_t)Value : 0U;	/* Her karede bu temel alinir */
	HUB75_SetBrightness(SPEC_Brightness);
}

static void ApplyClassic(uint8_t Mode, int32_t Value)
{
	(void)Mode; (void)Value;		/* Aktif modun tum Klasik degerleri birlikte okunur */
	SPEC_Cls.ColorMode = (uint8_t)SET_Global(SET_ID_CLS_COLOR);
	SPEC_Cls.Bar       = (uint32_t)SET_Global(SET_ID_CLS_BAR_RGB);
	SPEC_Cls.Peak      = SET_Global(SET_ID_CLS_PEAK) ? (uint8_t)SET_Global(SET_ID_CLS_PEAK_STYLE) : 0U;
	SPEC_Cls.PeakRGB   = (uint32_t)SET_Global(SET_ID_CLS_PEAK_RGB);
	SPEC_Cls.Width     = (uint8_t)SET_Global(SET_ID_CLS_WIDTH);
	SPEC_Cls.Type      = (uint8_t)SET_Global(SET_ID_CLS_TYPE);
	SPEC_Cls.Shape     = (uint8_t)SET_Global(SET_ID_CLS_SHAPE);
	SPEC_Cls.Blend     = (uint8_t)SET_Global(SET_ID_CLS_BLEND);
	SPEC_Cls.Layout    = (uint8_t)SET_Global(SET_ID_CLS_LAYOUT);
	SPEC_Cls.PeakColor = (uint8_t)SET_Global(SET_ID_CLS_PEAK_COL);
	SPEC_ClsApply();
}

/* Alev rengi: bloklu cubuk sirasi -> SPEC_SetFirePalette secimi.
   Her blok elle ayarlanmis bir palet; ara ton yok (gercekcilik bozulmasin). */
static const uint8_t FirePal[] = { 1U, 7U, 4U, 2U, 5U, 3U };	/* Ates, Pembe, Mor, Mavi gaz, Buz, Yesil */

static void ApplyFire(uint8_t Mode, int32_t Value)
{
	int32_t i = SET_Global(SET_ID_FIRE_COLOR);
	(void)Mode; (void)Value;
	if(i < 0 || i >= (int32_t)sizeof(FirePal)) i = 0;
	SPEC_SetFirePalette(FirePal[i], 0, 0, 0);
}

static void ApplyMotion(uint8_t Mode, int32_t Value)
{
	(void)Mode; (void)Value;
	SPEC_Speed = (uint8_t)SET_Global(SET_ID_SPEED);
	SPEC_React = (uint8_t)SET_Global(SET_ID_REACT);
}

static void ApplyContrast(uint8_t Mode, int32_t Value)
{
	(void)Mode;
	SPEC_Contrast = (uint8_t)Value;
}

static void ApplyRange(uint8_t Mode, int32_t Value)
{
	(void)Mode;  (void)Value;
	SPEC_SetRange((float)SET_Global(SET_ID_FREQ_LO), (float)SET_Global(SET_ID_FREQ_HI));
}

static void ApplySens(uint8_t Mode, int32_t Value)
{
	(void)Mode;
	SPEC_SetSens((uint8_t)Value);
}

static void ApplyLineColor(uint8_t Mode, int32_t Value)
{
	(void)Mode;
	SPEC_LineSel = (uint8_t)Value;
}

static void ApplyPal(uint8_t Mode, int32_t Value)
{
	(void)Mode;
	SPEC_Pal = (uint8_t)Value;
}

#define FIRE_MASK	((1UL << SPEC_MODE_FIRE) | (1UL << SPEC_MODE_FIRE_BARS) | (1UL << SPEC_MODE_FIRE_BLUE) | \
					 (1UL << SPEC_MODE_BLAST) | (1UL << SPEC_MODE_CBLAST) | (1UL << SPEC_MODE_FIRE_BALL))
#define CLS_MASK	(1UL << SPEC_MODE_CLASSIC)
#define ALL_MASK	((1UL << SPEC_MODE_COUNT) - 1UL)
#define REACT_MASK	(FIRE_MASK | (1UL << SPEC_MODE_RINGS) | (1UL << SPEC_MODE_FOUNTAIN) | \
					 (1UL << SPEC_MODE_SPECWAVE) | (1UL << SPEC_MODE_LIGHTLINE) | \
					 (1UL << SPEC_MODE_LAYERS) | (1UL << SPEC_MODE_WEAVE) | (1UL << SPEC_MODE_AURORA))
#define H			SET_FLAG_HIDDEN
#define A			SET_FLAG_ADVANCED		/* Android'de gizli ince ayar */

/* ==========================================================================
   Parametre tablosu
   ========================================================================== */
static const SET_Param Table[] =
{
	/* Id					Tur				Bayrak	Mask	Min	Max	Adim	Varsayilan */
	{ SET_ID_BRIGHTNESS,	SET_TYPE_INT,	0U,		0U,		5,	100, 5,	(int32_t)CFG_BRIGHTNESS,
	  "Parlakl\304\261k", "Ekran", "%", ApplyBrightness },					/* Parlaklik */

	{ SET_ID_AUTO_SEC,		SET_TYPE_INT,	0U,		0U,		0,	600, 5,	(int32_t)CFG_AUTO_MODE_SEC,
	  "Otomatik mod ge\303\247i\305\237i", "Mod", "sn (0 = kapal\304\261)", 0 },	/* Otomatik mod gecisi, kapali */

	{ SET_ID_FREQ_LO,		SET_TYPE_INT,	A,		0U,		60,	500, 10, (int32_t)CFG_FREQ_LOW,
	  "Alt frekans", "Ses", "Hz", ApplyRange },
	{ SET_ID_FREQ_HI,		SET_TYPE_INT,	A,		0U,		2000, 16000, 500, (int32_t)CFG_FREQ_HIGH,
	  "\303\234st frekans", "Ses", "Hz", ApplyRange },
	{ SET_ID_SENS,			SET_TYPE_INT,	A,		0U,		0,	100, 5,	(int32_t)CFG_SENS,
	  "Vuru\305\237 hassasiyeti", "Ses", "(50 = varsay\304\261lan)", ApplySens },

	/* --- Klasik (her alanin okunur hali yaninda) --- */
	{ SET_ID_CLS_WIDTH, SET_TYPE_INT, SET_FLAG_PER_MODE, CLS_MASK, 1, 4, 1, 3,
	  "Geni\305\237lik", "Bant", "px", ApplyClassic },
	{ SET_ID_CLS_TYPE, SET_TYPE_ENUM, SET_FLAG_PER_MODE | H, CLS_MASK, 0, 1, 1, 0,
	  "\303\207ubuk tipi", "Bant", "D\303\274z|Neon \303\247er\303\247eve", ApplyClassic },
	{ SET_ID_CLS_SHAPE, SET_TYPE_ENUM, SET_FLAG_PER_MODE, CLS_MASK, 0, 2, 1, 0,
	  "\305\236ekil", "Bant", "Yukar\304\261|Sarkan|Simetrik", ApplyClassic },
	{ SET_ID_CLS_COLOR, SET_TYPE_ENUM, SET_FLAG_PER_MODE, CLS_MASK, 0, 3, 1, 1,
	  "Renk", "Renk", "Tek|Hi-Fi|G\303\266kku\305\237a\304\237\304\261|Akan", ApplyClassic },
	{ SET_ID_CLS_BAR_RGB, SET_TYPE_COLOR, SET_FLAG_PER_MODE, CLS_MASK, 0, 0xFFFFFF, 1, 0x00FF00,
	  "S\303\274tun Renk", "Renk", "", ApplyClassic,
	  SET_ID_CLS_COLOR, 0, 1 },
	{ SET_ID_CLS_BLEND, SET_TYPE_ENUM, SET_FLAG_PER_MODE | A, CLS_MASK, 0, 1, 1, 0,
	  "Ge\303\247i\305\237", "Renk", "Yumu\305\237ak|B\303\266lgeli", ApplyClassic,
	  SET_ID_CLS_COLOR, 1, 1 },
	{ SET_ID_CLS_PEAK, SET_TYPE_BOOL, SET_FLAG_PER_MODE, CLS_MASK, 0, 1, 1, 1,
	  "Tepe", "Tepe", "", ApplyClassic },
	{ SET_ID_CLS_PEAK_STYLE, SET_TYPE_ENUM, SET_FLAG_PER_MODE | H, CLS_MASK, 1, 2, 1, 1,
	  "Tepe \305\237ekli", "Tepe", "Nokta|Top", ApplyClassic },		/* Gorunume sabit */
	{ SET_ID_CLS_PEAK_COL, SET_TYPE_ENUM, SET_FLAG_PER_MODE | H, CLS_MASK, 0, 1, 1, 0,
	  "Tepe renk kayna\304\237\304\261", "Tepe", "Sabit renk|Bant rengi", ApplyClassic,	/* Gorunume sabit */
	  SET_ID_CLS_PEAK, 1, 3 },
	{ SET_ID_CLS_PEAK_RGB, SET_TYPE_COLOR, SET_FLAG_PER_MODE, CLS_MASK, 0, 0xFFFFFF, 1, 0xFFFFFF,
	  "Tepe Renk", "Tepe", "", ApplyClassic,
	  SET_ID_CLS_PEAK_COL, 0, 0 },
	{ SET_ID_CLS_LAYOUT, SET_TYPE_ENUM, SET_FLAG_PER_MODE | H, CLS_MASK, 0, 1, 1, 0,
	  "Yerle\305\237im", "Ekstra", "Bas solda|Bas ortada", ApplyClassic },			/* Yerlesim: gorunume sabit (Ayna, Orta Dag) */

	/* --- Alev modlari: bloklu renk cubugu (hazir paletler) --- */
	{ SET_ID_FIRE_COLOR,	SET_TYPE_SWATCH, SET_FLAG_PER_MODE, FIRE_MASK, 0, 5, 1, 0,
	  "Alev Renk", "Alev",
	  "Ate\305\237:FF7A10|Pembe:FF3C8C|Mor:A040FF|Mavi gaz:2C7CFF|Buz:A8E4FF|Ye\305\237il:50E040",	/* Ates ... Yesil */
	  ApplyFire },

	/* --- Tum gorunumler: hareket --- */
	{ SET_ID_SPEED,			SET_TYPE_ENUM,	SET_FLAG_PER_MODE, ALL_MASK, 0, 3, 1, 2,
	  "H\304\261z", "Hareket", "Yava\305\237|Normal|Y\303\274ksek|\303\207ok y\303\274ksek", ApplyMotion },	/* Hiz; hepsi tempoya gore */
	{ SET_ID_REACT,			SET_TYPE_ENUM,	SET_FLAG_PER_MODE, REACT_MASK, 0, 2, 1, 1,
	  "Tepki", "Hareket", "Sakin|Normal|Hareketli", ApplyMotion },

	/* --- Renkli Dalga --- */
	{ SET_ID_CONTRAST,		SET_TYPE_INT,	SET_FLAG_PER_MODE, (1UL << SPEC_MODE_SPECWAVE), 50, 150, 10, 120,
	  "Kontrast", "G\303\266r\303\274n\303\274m", "%", ApplyContrast },

	/* --- Isik Cizgisi: cizgi rengi soldan saga iki renk arasinda gecer --- */
	{ SET_ID_LL_COLOR, SET_TYPE_SWATCH, SET_FLAG_PER_MODE, (1UL << SPEC_MODE_LIGHTLINE), 0, 7, 1, 0,
	  "Renk", "Renk",
	  "K\304\261rm\304\261z\304\261:FF6A00|Turuncu:FFB200|Sar\304\261:E9FF00|Ye\305\237il:00FF55|"
	  "Camg\303\266be\304\237i:0094FF|Mavi:0000FF|Mor:D400FF|Pembe:FF0055", ApplyLineColor },
	{ SET_ID_PAL, SET_TYPE_SWATCH, SET_FLAG_PER_MODE, (1UL << SPEC_MODE_LAYERS) | (1UL << SPEC_MODE_WEAVE), 0, 7, 1, 0,
	  "Renk", "Renk",
	  "Okyanus:0094FF|Ate\305\237:FF3C00|Orman:00C850|G\303\274n bat\304\261m\304\261:FF3C96|"
	  "Buz:3CC8FF|G\303\266kku\305\237a\304\237\304\261:FFB200|Pembe:FF3CA0|Turuncu-Ye\305\237il:FF8C0A", ApplyPal },
};

#define SET_COUNT	(sizeof(Table) / sizeof(Table[0]))

/* ==========================================================================
   Kayit yapisi -- yedek SRAM'de bu sekilde durur
   ========================================================================== */
typedef struct
{
	uint32_t	Magic;
	uint32_t	Layout;							/* Tablo duzeni ozeti			*/
	uint8_t		Mode;
	uint8_t		Res[3];
	char		Name[SET_NAME_LEN + 4U];
	int32_t		Global[SET_GLOBAL_MAX];
	int32_t		PerMode[LOOK_COUNT][SET_MODE_MAX];
	uint32_t	Crc;							/* En sonda						*/
} SET_Store;

_Static_assert(sizeof(SET_Store) <= 4096U, "Ayarlar 4 KB yedek SRAM'e sigmiyor");

#ifdef HOST_SIM
	static SET_Store	HostBkp;
	#define SAVED		(&HostBkp)
#else
	#define SAVED		((SET_Store *)BKPSRAM_BASE)
#endif

static SET_Store	Live;
static uint8_t		Slot[SET_COUNT];			/* Tablo sirasi -> yuva no		*/
static uint32_t		Layout;
static uint8_t		LocalDirty;
static uint32_t		LocalTick;

/* ==========================================================================
   Yardimcilar
   ========================================================================== */
static uint32_t Crc32(const void *Data, uint32_t Len, uint32_t Crc)
{
	const uint8_t *p = (const uint8_t *)Data;

	Crc = ~Crc;
	while(Len--)
	{
		Crc ^= *p++;
		for(uint8_t b = 0; b < 8U; b++)
			Crc = (Crc >> 1) ^ (0xEDB88320UL & (0U - (Crc & 1U)));
	}
	return ~Crc;
}

static int Find(uint16_t Id)
{
	for(uint16_t i = 0; i < SET_COUNT; i++)
		if(Table[i].Id == Id)
			return (int)i;
	return -1;
}

static int32_t *Cell(uint16_t Index, uint8_t Mode)
{
	if(Table[Index].Flags & SET_FLAG_PER_MODE)
		return &Live.PerMode[Mode][Slot[Index]];
	return &Live.Global[Slot[Index]];
}

static uint8_t Available(uint16_t Index, uint8_t Mode)
{
	if(!(Table[Index].Flags & SET_FLAG_PER_MODE))
		return 1U;
	if(!(Table[Index].ModeMask & (1UL << ModeInfo[Mode].Engine)))
		return 0U;
	switch(Table[Index].Id)							/* Neon: 5 px sabit, tek renk tup */
	{
	case SET_ID_CLS_WIDTH: case SET_ID_CLS_COLOR: case SET_ID_CLS_BLEND:
	{
		int t = Find(SET_ID_CLS_TYPE);
		if(t >= 0 && *Cell((uint16_t)t, Mode) == 1)
			return 0U;
		break;
	}
	default: break;
	}
	return 1U;
}

static int32_t Clamp(const SET_Param *p, int32_t v)
{
	if(p->Type == SET_TYPE_COLOR)
		return v & 0x00FFFFFF;
	if(v < p->Min) v = p->Min;
	if(v > p->Max) v = p->Max;
	if(p->Step > 1)
		v = p->Min + ((v - p->Min + p->Step / 2) / p->Step) * p->Step;
	if(v > p->Max) v = p->Max;
	return v;
}

static void ApplyOne(uint16_t Index, uint8_t Mode)
{
	if(Table[Index].Apply)
		Table[Index].Apply(Mode, *Cell(Index, Mode));
}

/* Genel ayarlar + aktif modun ayarlari */
static void ApplyAll(void)
{
	SPEC_SetMode((SPEC_Mode)ModeInfo[Live.Mode].Engine);
	SPEC_ShowNo = Live.Mode;
	for(uint16_t i = 0; i < SET_COUNT; i++)
		if(Available(i, Live.Mode))
			ApplyOne(i, Live.Mode);
}

static void Defaults(SET_Store *s)
{
	memset(s, 0, sizeof(*s));
	s->Magic  = SET_MAGIC;
	s->Layout = Layout;
	s->Mode   = (uint8_t)(CFG_START_MODE % LOOK_COUNT);
	strcpy(s->Name, "Spektrum");
	for(uint16_t i = 0; i < SET_COUNT; i++)
	{
		if(Table[i].Flags & SET_FLAG_PER_MODE)
		{
			for(uint8_t m = 0; m < LOOK_COUNT; m++)
			{
				int32_t v = Table[i].Def;
				for(uint8_t o = 0; o < ModeInfo[m].NOvr; o++)
					if(ModeInfo[m].Ovr[o][0] == Table[i].Id) v = ModeInfo[m].Ovr[o][1];
				s->PerMode[m][Slot[i]] = v;
			}
		}
		else
			s->Global[Slot[i]] = Table[i].Def;
	}
}

static uint8_t Valid(const SET_Store *s)
{
	return (s->Magic == SET_MAGIC && s->Layout == Layout && s->Mode < LOOK_COUNT &&
			s->Crc == Crc32(s, offsetof(SET_Store, Crc), 0U)) ? 1U : 0U;
}

static void BackupEnable(void)
{
#ifndef HOST_SIM
	uint32_t t = 200000U;

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	(void)RCC->APB1ENR;
	PWR->CR      |= PWR_CR_DBP;					/* Yedek alana yazma izni		*/
	RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;
	(void)RCC->AHB1ENR;
	PWR->CSR     |= PWR_CSR_BRE;				/* VBAT'ta SRAM korunsun		*/
	while(!(PWR->CSR & PWR_CSR_BRR) && --t) {}
#endif
}

/* ==========================================================================
   Disari acik fonksiyonlar
   ========================================================================== */
void SET_Init(void)
{
	uint8_t g = 0, m = 0;
	uint32_t Words[2];

	/* Yuvalari dagit, tablo duzeninin ozetini cikar */
	Layout = Crc32("L", 1U, (uint32_t)LOOK_COUNT ^ (uint32_t)sizeof(SET_Store));
	for(uint8_t m = 0; m < LOOK_COUNT; m++)		/* Gorunum varsayilanlari degisince de yenile */
		Layout = Crc32(ModeInfo[m].Ovr ? (const void *)ModeInfo[m].Ovr : (const void *)"", ModeInfo[m].NOvr * 8U, Layout ^ ModeInfo[m].Engine);
	for(uint16_t i = 0; i < SET_COUNT; i++)
	{
		Slot[i] = (Table[i].Flags & SET_FLAG_PER_MODE) ? m++ : g++;
		Words[0] = ((uint32_t)Table[i].Id << 16) | ((uint32_t)Table[i].Type << 8) | Table[i].Flags;
		Words[1] = Table[i].ModeMask;
		Layout   = Crc32(Words, sizeof(Words), Layout);
	}
	/* Yuva tasarsa derleme sirasinda yakalanmaz: calisirken sabitle */
	if(g > SET_GLOBAL_MAX || m > SET_MODE_MAX)
		for(;;) {}

	BackupEnable();
	SET_Load();
}

void SET_Poll(uint32_t Now)
{
	if(LocalDirty && (Now - LocalTick) >= SET_AUTOSAVE_MS)
	{
		LocalDirty = 0;
		SET_Save();
	}
}

void SET_MarkLocalChange(void)
{
	LocalDirty = 1U;
	LocalTick  = HAL_GetTick();
}

uint16_t SET_Count(void)
{
	return (uint16_t)SET_COUNT;
}

const SET_Param *SET_Desc(uint16_t Index)
{
	return (Index < SET_COUNT) ? &Table[Index] : 0;
}

const SET_ModeInfo *SET_Mode(uint8_t Mode)
{
	return (Mode < LOOK_COUNT) ? &ModeInfo[Mode] : 0;
}

uint8_t SET_LookCount(void)
{
	return (uint8_t)LOOK_COUNT;
}

uint8_t SET_Get(uint8_t Mode, uint16_t Id, int32_t *Value)
{
	int i = Find(Id);

	if(i < 0)
		return SET_ERR_PARAM;
	if(Mode == SET_MODE_GLOBAL || Mode >= LOOK_COUNT)
		Mode = Live.Mode;
	if(!Available((uint16_t)i, Mode))
		return SET_ERR_PARAM;
	*Value = *Cell((uint16_t)i, Mode);
	return SET_OK;
}

uint8_t SET_Set(uint8_t Mode, uint16_t Id, int32_t Value, int32_t *Applied)
{
	int i = Find(Id);
	int32_t v;

	if(i < 0)
		return SET_ERR_PARAM;
	if(Table[i].Flags & SET_FLAG_READONLY)
		return SET_ERR_READONLY;
	if(Mode == SET_MODE_GLOBAL || Mode >= LOOK_COUNT)
		Mode = Live.Mode;
	if(!Available((uint16_t)i, Mode))
		return SET_ERR_PARAM;

	v = Clamp(&Table[i], Value);
	*Cell((uint16_t)i, Mode) = v;
	if(!(Table[i].Flags & SET_FLAG_PER_MODE) || Mode == Live.Mode)
		ApplyOne((uint16_t)i, Mode);
	if(Applied)
		*Applied = v;
	/* Adima yuvarlama hata degil; yalnizca sinir disi bildirilir */
	if(Table[i].Type != SET_TYPE_COLOR && (Value < Table[i].Min || Value > Table[i].Max))
		return SET_ERR_RANGE;
	return SET_OK;
}

int32_t SET_Global(uint16_t Id)
{
	int i = Find(Id);
	return (i < 0) ? 0 : *Cell((uint16_t)i, Live.Mode);
}

uint8_t SET_GetCurMode(void)
{
	return Live.Mode;
}

void SET_SetCurMode(uint8_t Mode)
{
	if(Mode >= LOOK_COUNT)
		return;
	Live.Mode = Mode;
	SPEC_SetMode((SPEC_Mode)ModeInfo[Mode].Engine);
	SPEC_ShowNo = Mode;
	for(uint16_t i = 0; i < SET_COUNT; i++)
		if((Table[i].Flags & SET_FLAG_PER_MODE) && Available(i, Mode))
			ApplyOne(i, Mode);
}

const char *SET_GetName(void)
{
	return Live.Name;
}

void SET_SetName(const char *Name, uint8_t Len)
{
	if(Len > SET_NAME_LEN)
		Len = SET_NAME_LEN;
	memset(Live.Name, 0, sizeof(Live.Name));
	memcpy(Live.Name, Name, Len);
}

void SET_Save(void)
{
	Live.Magic  = SET_MAGIC;
	Live.Layout = Layout;
	Live.Crc    = Crc32(&Live, offsetof(SET_Store, Crc), 0U);
	*SAVED      = Live;
}

void SET_Load(void)
{
	if(Valid(SAVED))
	{
		Live = *SAVED;
		/* Tablo araligi degismis olabilir: her hucreyi sinirla */
		for(uint16_t i = 0; i < SET_COUNT; i++)
		{
			if(Table[i].Flags & SET_FLAG_PER_MODE)
				for(uint8_t k = 0; k < LOOK_COUNT; k++)
					Live.PerMode[k][Slot[i]] = Clamp(&Table[i], Live.PerMode[k][Slot[i]]);
			else
				Live.Global[Slot[i]] = Clamp(&Table[i], Live.Global[Slot[i]]);
		}
	}
	else
		Defaults(&Live);
	ApplyAll();
}

void SET_Factory(void)
{
	Defaults(&Live);
	ApplyAll();
}

void SET_SetPower(uint8_t On)
{
	PowerOn = On ? 1U : 0U;
	ApplyBrightness(0, SET_Global(SET_ID_BRIGHTNESS));
}

uint8_t SET_GetPower(void)
{
	return PowerOn;
}
