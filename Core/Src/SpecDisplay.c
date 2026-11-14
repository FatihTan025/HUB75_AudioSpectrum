/**
  ******************************************************************************
  * @file    SpecDisplay.c
  * @brief   Muzik gorselleri -- 128x32, 17 mod
  ******************************************************************************
  * Spectrum.c 128 bant uretir (SPEC_Level). Her mod kendi bant sayisini
  * (ModeBands[]) bu 128 banttan gruplayarak alir -> Lvl[0..Nb).
  * Cubuk/tepe dinamigi Lvl uzerinde calisir -> Bar[], Peak[].
  * Tum hiz/sure ayarlari SpecConfig.h'de, saniye cinsinden.
  ******************************************************************************
  */
/* Debug yapilandirmasi -O0 derler: FFT ve cizim 3-5 kat yavaslar, ses
   kareleri kacar. Bu dosya her yapilandirmada hizli derlensin.		*/
#pragma GCC optimize ("O2")

#include "SpecDisplay.h"
#include "HUB75.h"
#include "Audio_ADC.h"
#include "Saat.h"
#include "arialbold24px.h"
#include "bahnbd30.h"
#include "bahncond30.h"
#include "arialbd13.h"
#include "bahnbd16.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define W			SPEC_WIDTH
#define H			SPEC_HEIGHT
static float SpdDT = 1.0f / CFG_FPS;			/* Kare suresi x hiz ayari	*/
#define DT			SpdDT
#define MAXB		128

/* ==========================================================================
   Durum
   ========================================================================== */
/* Cizim dogrudan surucunun tamponuna yapilir (ayri tuval + kopyalama yok,
   12 KB RAM ve ~1 ms/kare kazanc). Surucu 64x64 duzeninde: x >= 64 olan
   yari (ikinci panel) y + 32 satirlarinda durur.						*/
#if(W != 128) || (H != 32)
	#error "Tuval eslemesi 128x32 (2 x 64x32 panel) icin yazildi"
#endif
static uint8_t		(*CanvasBuf)[3];
#define CANVAS_BYTES	((uint32_t)W * (uint32_t)H * 3U)
#define CV(Y, X)		(CanvasBuf[((((uint32_t)(X) >> 6) ^ (uint32_t)CFG_SWAP_HALVES) << 11) + \
						           ((uint32_t)(Y) << 6) + ((uint32_t)(X) & 63U)])

uint8_t				SPEC_Brightness = CFG_BRIGHTNESS;	/* Temel parlaklik % (ayar)	*/
static uint8_t		Nb			= 32;				/* Aktif modun bant sayisi	*/
static float		Lvl[MAXB];						/* Gruplanmis seviye 0..1	*/
static float		PrevLvl[MAXB];
static float		Bar[MAXB];						/* 0..H						*/
static float		Peak[MAXB];
static float		PeakVel[MAXB];
static uint16_t		PeakHold[MAXB];

static SPEC_Mode	Mode		= SPEC_MODE_CLASSIC;
static float		BeatEnv		= 0.0f;				/* 1 = vurus ani, 0'a soner	*/
static float		HuePhase	= 0.0f;				/* 0..768					*/
static float		Time		= 0.0f;
static uint32_t		ModeShowFrames = 0;
static uint8_t		BarSmooth = 0;					/* Cubuk modlarinda yumusatma	*/
static uint32_t		Seed		= 0x12345678U;

/* Moda ozel buyuk alanlar -- ayni anda tek mod calisir */
#define MAX_RINGS		10
#define MAX_PARTICLES	320
#define LT_W			SPEC_WIDTH			/* Yildirim (Katmanli Dag) */
#define LT_H			SPEC_HEIGHT
#define LT_MAX_BR		3U					/* En fazla dal sayisi	*/
#define LT_HLEN			104U				/* Yatay desarj en fazla uzunluk, px	*/
#define LL_STARS		48					/* Yildiz alani: yildiz sayisi (Isik Cizgisi, Orgu Serit) */

typedef struct									/* Ortak 3B yildiz alani (uzayda ilerleme) */
{
	float		X[LL_STARS], Y[LL_STARS], Z[LL_STARS];	/* Uzay konumu, Z = derinlik		*/
	int16_t		Px[LL_STARS], Py[LL_STARS];			/* Onceki ekran konumu (iz icin)	*/
	uint8_t		Fl[LL_STARS];						/* Parlama 0..255 (soner)			*/
} Starfield;

typedef struct { float X, Y, Vx, Vy, Lm, Age, Dur, P; uint32_t Last; uint8_t On; } Meteor;	/* Kayan yildiz */

/* Saatli modlarin ortak saat durumu: rakam maskesi (1 bit/piksel) ve yerlesim.
   Dakika, font ya da yerlesim degisince yeniden kurulur (SaatHazirla). */
typedef struct
{
	uint8_t		M[H][W / 8];					/* Rakam pikseli maskesi			*/
	int16_t		Ix, Iw, Ix2;					/* Iki nokta (fontun ':' glifi): x, genislik; ikincisi (saniye, yoksa -1)	*/
	int16_t		Sag;							/* Saatin sag kenari (isi-nem yazisi bunu gecmez)	*/
	uint8_t		Sa, Dk, Sn, Yazi, Yer, Alt, Isi, Sani, SnVar, Hazir;	/* SnVar: saniye gercekten cizildi */
} SaatDurum;

/* Kar tanesi: tam piksel konum + kesir (dusus Ac/Kd, yana Kx/4096) -> ara piksel cizim.
   Yana kayma serbest: Vx hizi Hd hedefine yavasca doner, Dg karede bir yeni hedef. */
typedef struct
{
	int16_t		X, Y;
	uint16_t	Kd, Ac;							/* Dusme adim suresi ve sayaci (1/16 kare)	*/
	int32_t		Kx;								/* Yana kesir (1/4096 px)				*/
	int16_t		Vx, Hd, En;						/* Yana hiz, hedef hiz, en buyuk (1/4096 px/kare)	*/
	uint16_t	Dg;								/* Yeni hedefe kalan kare				*/
	int16_t		Son;							/* Bu satirda kaybolur (uzaklar havada soner)	*/
	uint8_t		L;								/* Isik %								*/
} SnTn;

typedef struct { int32_t X, Y; int16_t Vy; uint8_t Uz, L, Iri, Hd; } YmDamla;	/* Yagmur damlasi (8.8); Hd: suya dusecegi satir	*/
typedef struct { float X, Y, Vx, Vy, Hd; uint8_t L; } YmZerre;			/* Sicrayan damlacik (Hd: suyun satiri)	*/
typedef struct { float X, Y, R, Hiz; uint8_t Life, Iri; } YmHalka;		/* Suda acilan elips halka	*/
typedef struct { float T, Sure, Eris; float Pr[128]; uint8_t Kp[128]; uint8_t On; } ShYalama;	/* Kiyiya vuran dalga	*/
typedef struct { float Y, V, A, Xm, Yari, Egim, Kv, T; int8_t Pr[128]; uint8_t Ana, On; } ShSirt;	/* Denizdeki dalga sirti (parca)	*/
typedef struct { uint8_t X, Y, Life; } ShKopuk;							/* Denizde kisa omurlu kopuk tepecigi	*/
#define NEON_THROW_H	12.0f				/* Neon: tavana vuran top cerceveden kac satir disari ucar */
#define SWV_BANDS		64				/* Renkli Dalga: ModeBands ile ayni	*/
#define MAX_EMBERS		70
#define SN_UZAK			70				/* Kar: en cok uzak / orta / yakin tane		*/
#define SN_ORTA			35
#define SN_YAKIN		6
#define SN_YILDIZ		16
#define SN_IZ			26				/* Kayan yildiz kuyrugu (nokta, ~28 px)	*/
#define SM_KIV			16				/* Somine: kivilcim sayisi				*/
#define YM_UZAK			90				/* Yagmur: en cok uzak / yakin damla, sicrama	*/
#define YM_YAKIN		18
#define YM_ZERRE		40
#define YM_HALKA		28				/* Suda ayni anda acik halka			*/
#define SH_YALAMA		3				/* Sahil: ayni anda kiyiya vuran dalga	*/
#define SH_SIRT			8				/* Sahil: denizdeki sirt				*/
#define SH_KOPUK		10				/* Sahil: denizdeki kopuk tepecigi		*/

typedef struct { float R, Speed; uint16_t Hue; uint8_t Alive; } Ring;
typedef struct { float X, Y, Vx, Vy; uint16_t Hue; uint8_t Life; } Particle;

static union
{
	struct
	{
		uint8_t		Heat[H + 2][W];					/* +2: gizli yakit satirlari */
		Particle	Embers[MAX_EMBERS];
		struct { float X, Y, Vx, Vy; uint8_t Life; } Frags[120];	/* PATLAMA */
	} Fire;
	Ring		Rings[MAX_RINGS];
	Particle	Parts[MAX_PARTICLES];				/* Halka, Fiskiye */
	struct											/* Renkli Dalga */
	{
		uint8_t		Col[SWV_BANDS][3];
		uint8_t		Lut[256];
		uint8_t		LutC, Ready;
		float		RibAmp[4], Ph[4], Avg[4], Freq, Pump;
	} Swv;
	struct											/* Isik Cizgisi */
	{
		Starfield	St;
		float		Gl[128];							/* Isima: yavas sonen tepe yuksekligi	*/
		float		Pump;
		uint8_t		Col[128][3];
		uint8_t		ColFor;								/* Col hangi renk icin kuruldu (+1; 0 = kurulmadi) */
		uint8_t		Ready;
	} Ll;
	struct												/* Katmanli Dag: golge katmanlar + yildirim */
	{
		float		M[128], S[128], Pump;
		struct
		{
			uint8_t		active, prevBass, strength, nBr;
			uint8_t		glowCol[3], boltCol[3];
			uint32_t	t0, lastEnd;
			uint8_t		bx[LT_H];					/* Ana kanalin her satirdaki x'i	*/
			uint8_t		brY0[LT_MAX_BR], brY1[LT_MAX_BR];
			uint8_t		brX[LT_MAX_BR][LT_H];
			uint8_t		glow[LT_W];					/* Sutun basina yerel isima 0..255	*/
			uint8_t		Gain[LT_W];					/* Karede sutun kazanci			*/
			uint16_t	Tmp[LT_W];					/* Isima haritasi gecici			*/
			uint8_t		ix;							/* Carpma noktasi (sutun)			*/
			uint8_t		hOn, hLen, hX0, hPeak;		/* Nadir yatay desarj				*/
			int8_t		hDir;
			uint8_t		hY[LT_HLEN];
		} Lt;
		uint8_t		Bolt[LT_H][LT_W];				/* Karedeki cizgi parlakligi		*/
	} Ly;
	struct { float Ph, Th, Amp, D0, Avg, Pump; Starfield St; uint8_t Ready; } We;	/* Orgu Serit */
	struct											/* Kar */
	{
		SaatDurum	S;
		SnTn		Uz[SN_UZAK], Or[SN_ORTA], Ya[SN_YAKIN];		/* Uzak / orta / yakin taneler	*/
		float		Sp[SN_YILDIZ];								/* Yildiz parlamasi			*/
		float		Mx, My, Mvx, Mvy, May, MSonra;				/* Kayan yildiz				*/
		int16_t		Ix[SN_IZ], Iy[SN_IZ];
		uint8_t		Sx[SN_YILDIZ], Sy[SN_YILDIZ], MOn, MIz, Ready;
		uint8_t		NOr, NYa;									/* Etkin orta / yakin tane sayisi	*/
	} Sn;
	struct											/* Yagmur */
	{
		SaatDurum	S;
		YmDamla		Uz[YM_UZAK], Ya[YM_YAKIN];
		YmZerre		Zr[YM_ZERRE];
		YmHalka		Hl[YM_HALKA];
		uint8_t		Of[W];							/* Su kenarinin sutun kaymasi		*/
		uint32_t	Kare;
		uint8_t		Ready;
	} Ym;
	struct											/* Sahil */
	{
		SaatDurum	S;
		uint8_t		Islak[H][W];					/* Kumun islakligi (yavasca kurur)	*/
		ShYalama	Yl[SH_YALAMA];
		ShSirt		Sr[SH_SIRT];
		ShKopuk		Kt[SH_KOPUK];
		uint8_t		Of[W];							/* Kipirti bantlarinin sutun kaymasi	*/
		float		Sonra;							/* Siradaki buyuk sirta kalan sure	*/
		uint32_t	Kare;
		uint8_t		Ready;
	} Sh;
	struct											/* Somine */
	{
		SaatDurum	S;
		uint8_t		Heat[H + 2][W];					/* +2: gizli yakit satirlari		*/
		uint8_t		Koz[H][W];						/* Rakam pikseli kozu (isi birikimi)	*/
		float		Yk[W];							/* Sutun yakiti (dil boylari)		*/
		float		Harl, HarlH, HarlSonra;			/* Harlanma: yavas guc degisimi		*/
		Particle	Kv[SM_KIV];
		uint32_t	Kare;
		uint8_t		Ready;
	} Sm;
	struct { float T, Wx, Wd, Wa, Fl; float Gl[128]; uint8_t Pal[64][3];
	         Meteor Me; uint8_t Ready; } Au;	/* Aurora */
} U;

/* Her modun bant sayisi */
#define BANDS_OF(w, g)	((uint8_t)(W / ((w) + (g))))
#define BANDS_FIT(w, g)	((uint8_t)((W + (g)) / ((w) + (g))))		/* son bosluk yok: kenarlara tam oturur */

static const uint8_t ModeBands[SPEC_MODE_COUNT] =
{
	32,												/* CLASSIC: ayardan (SPEC_ClsBands)	*/
	128,											/* FIRE			*/
	32,												/* FIRE_BARS	*/
	64,												/* FIRE_BLUE	*/
	128,											/* BLAST		*/
	16,												/* CBLAST		*/
	128,											/* FIRE_BALL	*/
	64,												/* RINGS		*/
	BANDS_OF(2, 1),									/* FOUNTAIN		*/
	64,												/* WAVE			*/
	64,												/* SPECWAVE (SWV_BANDS ile ayni)	*/
	128,											/* LIGHTLINE: sutun basina bir bant	*/
	128,											/* LAYERS: sutun basina bir bant	*/
	128,											/* WEAVE: spektrum bolgeleri		*/
	128,											/* AURORA: sutun basina bir bant	*/
	32,												/* SNOW: muziksiz (kullanilmaz)	*/
	32,												/* HEARTH: muziksiz (kullanilmaz)	*/
	32,												/* RAIN: muziksiz (kullanilmaz)	*/
	32,												/* SHORE: muziksiz (kullanilmaz)	*/
};

/* ==========================================================================
   Yardimcilar
   ========================================================================== */
static inline uint32_t Rand(void)
{
	Seed ^= Seed << 13; Seed ^= Seed >> 17; Seed ^= Seed << 5;
	return Seed;
}

static inline float RandF(void)					/* 0..1 */
{
	return (float)(Rand() & 0xFFFFU) / 65535.0f;
}

static inline uint8_t Sat(float v)
{
	return (v <= 0.0f) ? 0U : ((v >= 255.0f) ? 255U : (uint8_t)v);
}

static inline int SatI(int v)
{
	return (v < 0) ? 0 : ((v > 255) ? 255 : v);
}

/* Ton 0..767: kirmizi -> yesil -> mavi -> kirmizi */
static void Hue(uint32_t Hu, uint8_t *R, uint8_t *G, uint8_t *B)
{
	uint8_t v;

	Hu %= 768U;
	v = (uint8_t)(Hu & 0xFFU);

	if(Hu < 256U)      { *R = 255U - v; *G = v;        *B = 0;        }
	else if(Hu < 512U) { *R = 0;        *G = 255U - v; *B = v;        }
	else               { *R = v;        *G = 0;        *B = 255U - v; }
}

static inline void Set(int X, int Y, uint8_t R, uint8_t G, uint8_t B)
{
	if((unsigned)X >= W || (unsigned)Y >= H) return;
	{ uint8_t *p = CV(Y, X); p[0] = R; p[1] = G; p[2] = B; }
}

static inline void SetScaled(int X, int Y, uint8_t R, uint8_t G, uint8_t B, uint16_t S)	/* S 0..256 */
{
	Set(X, Y, (uint8_t)((R * S) >> 8), (uint8_t)((G * S) >> 8), (uint8_t)((B * S) >> 8));
}

/* Doygun toplama -- parlama ve iz icin */
static inline void Add(int X, int Y, uint8_t R, uint8_t G, uint8_t B)
{
	uint8_t *p;
	uint16_t v;

	if((unsigned)X >= W || (unsigned)Y >= H) return;
	p = CV(Y, X);
	v = (uint16_t)(p[0] + R); p[0] = (v > 255U) ? 255U : (uint8_t)v;
	v = (uint16_t)(p[1] + G); p[1] = (v > 255U) ? 255U : (uint8_t)v;
	v = (uint16_t)(p[2] + B); p[2] = (v > 255U) ? 255U : (uint8_t)v;
}

static inline void AddHue(int X, int Y, uint32_t Hu, uint16_t Scale)	/* Scale 0..256 */
{
	uint8_t R, G, B;
	Hue(Hu, &R, &G, &B);
	Add(X, Y, (uint8_t)((R * Scale) >> 8), (uint8_t)((G * Scale) >> 8), (uint8_t)((B * Scale) >> 8));
}

static void Clear(void)
{
	memset(CanvasBuf, 0, CANVAS_BYTES);
}

/* Tum tuvali Keep/256 oraninda karart -- iz efekti */
static void Fade(uint16_t Keep)
{
	uint8_t *p = &CanvasBuf[0][0];
	uint32_t i;

	for(i = 0; i < CANVAS_BYTES; i++)
		p[i] = (uint8_t)((p[i] * Keep) >> 8);
}



/* ==========================================================================
   Paletler
   ========================================================================== */
/* Alev paleti: tum alev modlari bunu kullanir. Ayar degisince yeniden
   hesaplanir (768 bayt). 0 = soguk/siyah, 255 = en sicak.			*/
static uint8_t ActPal[256][3];
static uint8_t SPEC_FireAurora;									/* Alev paleti aurora: kivilcimlar renk renk */

static void PalSet(int v, float r, float g, float b)
{
	ActPal[v][0] = (uint8_t)SatI((int)(r * 255.0f));
	ActPal[v][1] = (uint8_t)SatI((int)(g * 255.0f));
	ActPal[v][2] = (uint8_t)SatI((int)(b * 255.0f));
}

/* Sel: 0 = modun kendi rengi, 1 ates, 2 mavi gaz, 3 yesil, 4 mor, 5 buz,
   6 ozel (Kor / Govde / Uc renkleri, 0x00RRGGBB), 7 pembe, 8 aurora		*/
void SPEC_SetFirePalette(uint8_t Sel, uint32_t Kor, uint32_t Govde, uint32_t Uc)
{
	int v;

	if(Sel == 0U)
		Sel = (Mode == SPEC_MODE_FIRE_BLUE) ? 2U : 1U;
	SPEC_FireAurora = (uint8_t)(Sel == 8U);

	for(v = 0; v < 256; v++)
	{
		float f = (float)v / 255.0f, a = f * f * 3.0f, m = (f - 0.35f) * 1.9f, t = (f - 0.80f) * 4.5f;

		switch(Sel)
		{
		case 1:	PalSet(v, a, m, t);										break;	/* siyah-kirmizi-turuncu-sari-beyaz */
		case 2:	PalSet(v, (f - 0.30f) * 0.7f + ((f > 0.85f) ? (f - 0.85f) * 5.0f : 0.0f),
				   (f - 0.45f) * 2.0f, f * 2.2f);						break;	/* lacivert-mavi-camgobegi-beyaz */
		case 3:	PalSet(v, m * 0.55f, a, t);								break;	/* koyu yesil-yesil-sari yesil-beyaz */
		case 4:	PalSet(v, a * 0.85f, t, f * f * 3.3f);					break;	/* mor-eflatun-pembe-beyaz */
		case 5:	PalSet(v, (f - 0.55f) * 2.4f, (f - 0.30f) * 1.6f, f * 2.0f);	break;	/* buz: mavi-beyaz */
		case 7:	PalSet(v, a, (f - 0.70f) * 3.0f, m * 0.9f);				break;	/* pembe: bordo-fusya-pembe-beyaz */
		case 8:														/* aurora: soluk gokkusagi, kuzey isiklari tonlari */
		{
			static const float Dr[7][4] =							/* konum, r, g, b: turkuaz-yesil-camgobegi-mor-pembe-seftali */
			{ { 0.00f, 0.00f, 0.00f, 0.00f }, { 0.18f, 0.00f, 0.22f, 0.26f }, { 0.34f, 0.16f, 0.62f, 0.40f },
			  { 0.50f, 0.22f, 0.68f, 0.76f }, { 0.65f, 0.52f, 0.36f, 0.84f }, { 0.82f, 0.86f, 0.44f, 0.72f },
			  { 1.00f, 0.95f, 0.72f, 0.62f } };
			int k = 0;
			float u;
			while(k < 5 && f > Dr[k + 1][0]) k++;
			u = (f - Dr[k][0]) / (Dr[k + 1][0] - Dr[k][0]);
			PalSet(v, Dr[k][1] + (Dr[k + 1][1] - Dr[k][1]) * u, Dr[k][2] + (Dr[k + 1][2] - Dr[k][2]) * u,
			          Dr[k][3] + (Dr[k + 1][3] - Dr[k][3]) * u);
			break;
		}
		default:
		{
			/* Ozel: siyah -> Kor -> Govde -> Uc (0, 0.35, 0.70, 1.0) */
			uint32_t c0 = 0, c1;
			float    u;
			if(f < 0.35f)      { c1 = Kor;   u = f / 0.35f; }
			else if(f < 0.70f) { c0 = Kor;   c1 = Govde; u = (f - 0.35f) / 0.35f; }
			else               { c0 = Govde; c1 = Uc;    u = (f - 0.70f) / 0.30f; }
			PalSet(v, ((float)((c0 >> 16) & 255U) + ((float)((c1 >> 16) & 255U) - (float)((c0 >> 16) & 255U)) * u) / 255.0f,
			          ((float)((c0 >> 8)  & 255U) + ((float)((c1 >> 8)  & 255U) - (float)((c0 >> 8)  & 255U)) * u) / 255.0f,
			          ((float)( c0        & 255U) + ((float)( c1        & 255U) - (float)( c0        & 255U)) * u) / 255.0f);
			break;
		}
		}
	}
}

/* ==========================================================================
   Seviye gruplama + cubuk / tepe dinamigi
   ========================================================================== */
static void ComputeLevels(void)
{
	uint32_t j, i;

	for(j = 0; j < Nb; j++)
	{
		uint32_t s = j * SPEC_BAND_COUNT / Nb;
		uint32_t e = (j + 1U) * SPEC_BAND_COUNT / Nb;
		float    m = 0.0f;

		if(e <= s) e = s + 1U;
		for(i = s; i < e; i++)
			if(SPEC_Level[i] > m) m = SPEC_Level[i];
		Lvl[j] = m;
	}

	/* Cubuk modlari (>= 32 bant): komsu bantlar 1-2-1 yumusatilir;
	   saf sinus can egrisi gibi gorunur, spektrum zarfi duzgun olur. */
	if(BarSmooth && Nb >= 32U)
	{
		float a = Lvl[0], b;
		for(j = 0; j < Nb; j++)
		{
			float c = (j + 1U < Nb) ? Lvl[j + 1U] : Lvl[j];
			b = Lvl[j];
			Lvl[j] = 0.25f * a + 0.5f * b + 0.25f * c;
			a = b;
		}
	}
}

static void UpdateBars(void)
{
	/* Cubuk ve tepe dinamigi gercek zamanla: Hiz ayari bunlari etkilemez
	   (hizla olceklenince tepe her karede dusup kalkiyor, pirpirlaniyordu). */
	const float    RDT   = 1.0f / CFG_FPS;
	/* Cubuk modlarinda (Klasik, Neon, Orta Dag) Hiz = dusus/tepe hizi (dogrusal),
	   yavasta yukselis de yumusar. Animasyonlarda cubuk dinamigi sabit. */
	static const float KatBar[4] = { 0.2f, 0.4f, 0.8f, 1.0f };	/* Cubuk: Yavas, Normal, Yuksek, Cok yuksek */
	static const float KatAtk[4] = { 0.35f, 0.6f, 1.0f, 1.0f };	/* Yukselis yumusatma */
	const uint8_t  Sp    = (SPEC_Speed < 4U) ? SPEC_Speed : 2U;
	const float    Kb    = (Mode == SPEC_MODE_CLASSIC) ? KatBar[Sp] : 1.0f;
	const float    Ka    = (Mode == SPEC_MODE_CLASSIC) ? KatAtk[Sp] : 1.0f;
	const float    Fall  = CFG_BAR_FALL_PX_S * RDT * Kb;
	const float    Grav  = CFG_PEAK_GRAVITY * RDT * RDT * Kb;
	/* Neon: tavana vuran top cerceveden disari firlar, yercekimiyle geri duser */
	const uint8_t  Throw = (Mode == SPEC_MODE_CLASSIC && SPEC_Cls.Type == 1U &&
	                        SPEC_Cls.Shape == 0U && SPEC_Cls.Layout == 0U);
	const float    V0    = sqrtf(2.0f * Grav * NEON_THROW_H);
	const uint16_t Hold  = (uint16_t)((float)CFG_PEAK_HOLD_MS * CFG_FPS / 1000.0f);
	uint32_t k;

	for(k = 0; k < Nb; k++)
	{
		float Target = Lvl[k] * (float)H;

		if(Target > Bar[k])
			Bar[k] += (Target - Bar[k]) * CFG_ATTACK * Ka;
		else
		{
			Bar[k] -= Fall;
			if(Bar[k] < Target) Bar[k] = Target;
		}

		if(Bar[k] >= Peak[k])
		{
			Peak[k] = Bar[k]; PeakVel[k] = 0.0f; PeakHold[k] = Hold;
			if(Throw && Bar[k] >= (float)H - 1.0f)		/* Tavana vurdu: yukari firlat */
			{
				PeakVel[k]  = -V0;
				PeakHold[k] = 0U;
				Peak[k]    += V0;
			}
		}
		else if(PeakHold[k] > 0U)
			PeakHold[k]--;
		else
		{
			PeakVel[k] += Grav;
			Peak[k]    -= PeakVel[k];
			if(Peak[k] < Bar[k]) Peak[k] = Bar[k];
		}
	}

	/* Vurus zarfi ve renk donmesi */
	if(SPEC_Beat)
		BeatEnv = 1.0f;
	else
	{
		BeatEnv -= RDT / CFG_BEAT_DECAY_S;
		if(BeatEnv < 0.0f) BeatEnv = 0.0f;
	}

	HuePhase += CFG_HUE_SPEED * (768.0f / 360.0f) * DT;
	if(SPEC_Beat) HuePhase += 64.0f;				/* Vurusta renk sicrar */
	while(HuePhase >= 768.0f) HuePhase -= 768.0f;

	Time += DT;
	if(Time > 1000.0f) Time -= 1000.0f;
}

/* ==========================================================================
   MOD -- KLASIK
   Tum gorunum SPEC_Cls ayarlarindan: genislik 1..4 px, tip (duz/neon),
   sekil, yerlesim, renk, tepe.
   ========================================================================== */
uint8_t SPEC_Speed = 2, SPEC_React = 1, SPEC_ShowNo = 0;
uint8_t SPEC_LineSel = 0;						/* Isik Cizgisi renk ailesi (Settings.c yazar)	*/

SPEC_ClsCfg SPEC_Cls = { 3, 0, 0, 1, 0, 1, 0, 0, 0x00FF00UL, 0xFFFFFFUL };

/* Hi-Fi uclu renk: alt = Bar tonu, orta = -60, ust = -120 derece (yesil -> sari -> kirmizi).
   Bar beyaz/soluksa beyaz kademe (griden beyaza). SPEC_ClsApply hesaplar. */
static uint32_t HfLo = 0x00FF00UL, HfMid = 0xFFFF00UL, HfHi = 0xFF0000UL;

static uint32_t TonRenk(int32_t h)						/* Tam doygun, tam parlak ton (derece) */
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

static void HiFiHesapla(uint32_t c)
{
	int32_t r = (int32_t)((c >> 16) & 255U), g = (int32_t)((c >> 8) & 255U), b = (int32_t)(c & 255U);
	int32_t mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn, h;

	if(mx == 0 || d * 4 < mx)							/* Beyaz/soluk: beyaz kademe (soluk gri -> beyaz) */
	{
		HfLo = 0x3C3C3CUL; HfMid = 0xA0A0A0UL; HfHi = 0xFFFFFFUL;
		return;
	}
	if(mx == r)      h = 60 * (g - b) / d;
	else if(mx == g) h = 120 + 60 * (b - r) / d;
	else             h = 240 + 60 * (r - g) / d;
	HfLo = TonRenk(h); HfMid = TonRenk(h - 60); HfHi = TonRenk(h - 120);
}

static uint32_t Mix(uint32_t a, uint32_t b, uint32_t t)		/* t: 0..256 */
{
	uint32_t r = (((a >> 16) & 255U) * (256U - t) + ((b >> 16) & 255U) * t) >> 8;
	uint32_t g = (((a >> 8)  & 255U) * (256U - t) + ((b >> 8)  & 255U) * t) >> 8;
	uint32_t c = (( a        & 255U) * (256U - t) + ( b        & 255U) * t) >> 8;
	return (r << 16) | (g << 8) | c;
}

/* k: bant, Row: alt(ic)tan satir, Area: satir sayisi */
static uint32_t ClsColor(uint32_t k, uint32_t Row, uint32_t Area)
{
	const SPEC_ClsCfg *c = &SPEC_Cls;
	uint8_t R, G, B;

	if(c->Type == 1U) return c->Bar;					/* Neon: tek renk tup, yukseklikle degismez */
	switch(c->ColorMode)
	{
	case 0:	return c->Bar;
	case 1:
	{
		uint32_t Lo = HfLo, Mi = HfMid, Hi = HfHi;
		uint32_t t  = (Area > 1U) ? Row * 256U / (Area - 1U) : 0U;
		if(c->Blend)									/* Bolgeli: %60 / %25 / %15 */
			return (t < 154U) ? Lo : (t < 218U) ? Mi : Hi;
		return (t < 128U) ? Mix(Lo, Mi, t * 2U) : Mix(Mi, Hi, (t - 128U) * 2U);
	}
	case 2:	Hue(k * 768U / Nb, &R, &G, &B);								break;
	default:Hue((uint32_t)HuePhase + k * 768U / Nb, &R, &G, &B);			break;
	}
	return ((uint32_t)R << 16) | ((uint32_t)G << 8) | B;
}

static void PutC(int x, int y, uint32_t c, uint16_t s)
{
	SetScaled(x, y, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c, s);
}

/* Ortadan yerlesim iki sekilde dizilebilir; kenarda daha az bos kalan secilir:
   - Ikiye bolunmus: ortada normal bosluk, iki yana ayna (cift bant)
   - Orta cubuklu:   bas bandi tam ortada tek cubuk, iki yana ayna (tek bant)	*/
static uint8_t	ClsMidBar = 0;					/* 1: orta cubuklu dizilis		*/
static uint32_t	ClsMidX   = 0;					/* Orta cubugun sol kenari		*/

/* Neon tipi her zaman 5 px genis: ortada parlak cekirdek sutunu olsun */
static const uint16_t NeonProf[5] = { 77U, 179U, 256U, 179U, 77U };	/* Enine parlaklik %30-70-100-70-30 */
#define NEON_BALL_WHITE		160U		/* Top merkezi beyaza karisim 0..256 (komsular yarisi) */
#define NEON_CORE_WHITE		50U		/* Orta sutun beyaza karisim 0..256 (102 = %40) */

static uint32_t NeonMix(uint32_t c, uint32_t Wt)		/* Rengi beyaza yaklastir (Wt 0..256): neon cekirdegi */
{
	uint32_t r = (c >> 16) & 255U, g = (c >> 8) & 255U, b = c & 255U;
	r += ((255U - r) * Wt) >> 8;
	g += ((255U - g) * Wt) >> 8;
	b += ((255U - b) * Wt) >> 8;
	return (r << 16) | (g << 8) | b;
}

static uint32_t ClsWidth(void)
{
	if(SPEC_Cls.Type == 1U) return 5U;
	return SPEC_Cls.Width ? SPEC_Cls.Width : 1U;
}

uint8_t SPEC_ClsBands(void)
{
	uint32_t w = ClsWidth(), g = (w > 1U) ? 1U : 0U, P = w + g;
	ClsMidBar = 0U;
	if(SPEC_Cls.Layout == 1U)
	{
		uint32_t Ns = (W / 2U) / P, UsedS = 2U * Ns * P - g;		/* Ikiye bolunmus */
		uint32_t Xc = W / 2U - w / 2U, Kr = (W - Xc - w) / P, Kl = Xc / P;
		uint32_t Kc = (Kr < Kl) ? Kr : Kl, UsedC = w + 2U * Kc * P;	/* Orta cubuklu */
		if(UsedC > UsedS)
		{
			ClsMidBar = 1U;
			ClsMidX   = Xc;
			return (uint8_t)(Kc + 1U);
		}
		return (uint8_t)Ns;
	}
	return BANDS_FIT(w, g);
}

void SPEC_ClsApply(void)
{
	if(SPEC_Cls.Width < 1U) SPEC_Cls.Width = 1U;
	if(SPEC_Cls.Width > 4U) SPEC_Cls.Width = 4U;
	HiFiHesapla(SPEC_Cls.Bar);
	if(Mode == SPEC_MODE_CLASSIC)
		Nb = SPEC_ClsBands();
}

static uint8_t BarH[MAXB];						/* Klasik: kararlastirilmis cubuk boyu (histerezis) */

static void ModeClassic(void)
{
	const SPEC_ClsCfg *c = &SPEC_Cls;
	const uint32_t w   = ClsWidth(), Gap = (w > 1U) ? 1U : 0U, Pitch = w + Gap;
	const uint32_t Top = 0U, Bot = H;						/* Bot: haric */
	const uint32_t Area = Bot - Top;
	const uint8_t  Mid = (c->Layout == 1U);			/* Bas ortada, iki yana ayna */
	const uint32_t Off = Mid ? (ClsMidBar ? ClsMidX : W / 2U) : (W - Nb * Pitch + Gap) / 2U;
	const uint8_t  Neon = (c->Type == 1U);
	uint32_t k, x, i;

	Clear();

	for(k = 0; k < Nb; k++)
	{
		const int X0 = (int)(Off + k * Pitch);
		const int X1 = ClsMidBar ? (int)ClsMidX - (int)(k * Pitch)			/* Ayna: sol kopya */
		                         : (int)(W / 2U) - (int)((k + 1U) * Pitch);
		const uint8_t Mir = Mid && !(ClsMidBar && k == 0U);			/* Orta cubuk tek cizilir */
		uint32_t h = (uint32_t)(Bar[k] * (float)Area / (float)H + 0.5f);
		int      p = (int)(Peak[k] * (float)Area / (float)H + 0.5f);
		uint32_t Len = (c->Shape == 2U) ? Area / 2U : Area;		/* Simetrik: yarim boy */
		uint32_t hh;
		uint8_t  Floor = (Bar[k] < (float)CFG_BAR_MIN_PX);

		if(h > Area) h = Area;
		if(h < CFG_BAR_MIN_PX) h = CFG_BAR_MIN_PX;
		/* Histerezis (tum tipler): +-1 satir titremesi tepeyi pirpirlatmasin.
		   Yukselis: duzde aninda, neonda en fazla 2 satir/kare. */
		if(h > BarH[k]) BarH[k] = (uint8_t)((Neon && h > BarH[k] + 2U) ? BarH[k] + 2U : h);
		else if(h + 2U <= BarH[k]) BarH[k] = (uint8_t)(h + 1U);
		h = BarH[k];
		hh = (c->Shape == 2U) ? (h + 1U) / 2U : h;

		for(i = 0; i < Len; i++)
		{
			uint8_t  On = (i < hh);
			uint16_t s  = 256U;
			uint32_t col;

			if(!On) break;
			if(Floor && i < CFG_BAR_MIN_PX) s = CFG_BAR_MIN_DIM;
			col = ClsColor(k, i, Len);

			for(x = 0; x < w; x++)
			{
				int y0, y1 = -1;
				uint16_t sx = s;
				uint32_t cx = col;
				if(Neon && hh > 1U)
				{
					/* Neon tup: ortasi en parlak, kenarlara sonen isima; tepe yuvarlak */
					if(i + 1U == hh && (x == 0U || x + 1U == w)) continue;
					sx = (uint16_t)((s * NeonProf[x]) >> 8);
					if(x == w / 2U) cx = NeonMix(col, NEON_CORE_WHITE);
				}
				if(c->Shape == 0U)      y0 = (int)(Bot - 1U - i);
				else if(c->Shape == 1U) y0 = (int)(Top + i);
				else { y0 = (int)(Top + Len - 1U - i); y1 = (int)(Top + Len + i); }
				PutC(X0 + (int)x, y0, cx, sx);
				if(y1 >= 0) PutC(X0 + (int)x, y1, cx, sx);
				if(Mir) { PutC(X1 + (int)x, y0, cx, sx); if(y1 >= 0) PutC(X1 + (int)x, y1, cx, sx); }
			}
		}

		/* Tepe noktasi */
		/* Neon: tepe noktasi 5x5 isiyan top (ortasi en parlak), bar ucunun 1 satir ustunde */
		if(c->Peak && Neon)
		{
			int pn = (int)((Peak[k] * (float)Area / (float)H + 0.5f));
			uint32_t pb, r;
			uint32_t pc;
			if(c->Shape == 2U) pn = (pn + 1) / 2;
			pb = ((uint32_t)pn > hh + 1U) ? (uint32_t)pn : hh + 1U;
			/* Ust kenari asan satirlar cizilmez: top cerceveden cikip geri duser */
			{
				pc = (c->PeakColor == 1U) ? ClsColor(k, (pb < Len) ? pb : Len - 1U, Len) : c->PeakRGB;
				for(r = 0; r < 5U; r++)
					for(x = 0; x < w; x++)
					{
						int y0, y1 = -1;
						uint16_t ps;
						uint32_t pcx;
						if((r == 0U || r == 4U) && (x == 0U || x + 1U == w)) continue;	/* Koseler kapali */
						ps = (NeonProf[x] < NeonProf[r]) ? NeonProf[x] : NeonProf[r];	/* Ortasi en parlak */
						{
							const uint32_t d = ((x > 2U) ? x - 2U : 2U - x) + ((r > 2U) ? r - 2U : 2U - r);	/* Merkeze uzaklik */
							pcx = (d == 0U) ? NeonMix(pc, NEON_BALL_WHITE) :			/* Merkez: en beyaz */
							      (d == 1U) ? NeonMix(pc, NEON_BALL_WHITE / 2U) : pc;	/* 4 komsu: yarisi */
						}
						if(c->Shape == 0U)      y0 = (int)(Bot - 1U - (pb + r));
						else if(c->Shape == 1U) y0 = (int)(Top + pb + r);
						else { y0 = (int)(Top + Len - 1U - (pb + r)); y1 = (int)(Top + Len + (pb + r)); }
						PutC(X0 + (int)x, y0, pcx, ps);
						if(y1 >= 0) PutC(X0 + (int)x, y1, pcx, ps);
						if(Mir) { PutC(X1 + (int)x, y0, pcx, ps); if(y1 >= 0) PutC(X1 + (int)x, y1, pcx, ps); }
					}
			}
		}

		if(c->Peak && !Neon)							/* Duz tipte peak cizgisi */
		{
			uint32_t pc;
			int y, y1 = -1;
			if(p < (int)h) p = (int)h;
			if(c->Shape == 2U) p = (p + 1) / 2;
			if(p < 1) p = 1;
			if(p > (int)Len) p = (int)Len;
			pc = (c->PeakColor == 1U) ? ClsColor(k, (uint32_t)p - 1U, Len) : c->PeakRGB;
			if(c->Shape == 0U)      y = (int)Bot - p;
			else if(c->Shape == 1U) y = (int)Top + p - 1;
			else { y = (int)(Top + Len) - p; y1 = (int)(Top + Len) + p - 1; }

			for(x = 0; x < w; x++)
			{
				uint16_t s = 256U;
				if(c->Peak == 2U && w >= 3U && x != w / 2U) s = 90U;	/* Top: orta parlak */
				PutC(X0 + (int)x, y, pc, s);
				if(y1 >= 0) PutC(X0 + (int)x, y1, pc, s);
				if(Mir) { PutC(X1 + (int)x, y, pc, s); if(y1 >= 0) PutC(X1 + (int)x, y1, pc, s); }
			}
		}
	}

}

/* ==========================================================================
   ALEV MOTORU
   Klasik "sogutma haritasi" alevi: her piksel alttaki 4 komsunun
   ortalamasi eksi sogutma. Sogutma haritasi yumusak gurultudur ve her
   karede yukari kayar -> alev dilleri yukari akar, kopar, titrer.
   Yakit alt iki gizli satira sutun bazinda verilir.
   ========================================================================== */
static uint8_t	CoolMap[H][W];
static uint8_t	CoolOffset = 0;

static void BuildCoolMap(void)
{
	/* Gecici alan: acilista bos olan mod alani (U) kullanilir, ayri 4 KB yok */
	uint8_t (*Tmp)[W] = (uint8_t (*)[W])(void *)&U;
	int x, y, i;

	_Static_assert(sizeof(U) >= (size_t)H * W, "U, CoolMap gecici alani icin kucuk");

	for(y = 0; y < H; y++)
		for(x = 0; x < W; x++)
			Tmp[y][x] = (uint8_t)(Rand() & 0xFFU);

	/* 3 kez yumusat (dikeyde dikey sarmali, yatayda sarmali) */
	for(i = 0; i < 3; i++)
	{
		for(y = 0; y < H; y++)
			for(x = 0; x < W; x++)
			{
				int s = Tmp[y][x] * 2 + Tmp[y][(x + 1) % W] + Tmp[y][(x + W - 1) % W] +
				        Tmp[(y + 1) % H][x] + Tmp[(y + H - 1) % H][x];
				CoolMap[y][x] = (uint8_t)(s / 6);
			}
		memcpy(Tmp, CoolMap, sizeof(CoolMap));
	}

	/* Kontrasti ac: 0..255 */
	{
		int Mn = 255, Mx = 0;
		for(y = 0; y < H; y++) for(x = 0; x < W; x++)
		{ if(Tmp[y][x] < Mn) Mn = Tmp[y][x]; if(Tmp[y][x] > Mx) Mx = Tmp[y][x]; }
		if(Mx <= Mn) Mx = Mn + 1;
		for(y = 0; y < H; y++) for(x = 0; x < W; x++)
			CoolMap[y][x] = (uint8_t)((Tmp[y][x] - Mn) * 255 / (Mx - Mn));
	}
}

static void FireStep(void)
{
	/* Satir basina ortalama soguma -- CFG_FIRE_HEIGHT ile olceklenir */
	const int Cool = (int)(8.0f / CFG_FIRE_HEIGHT + 0.5f);
	int x, y;
#if(CFG_FIRE_WIND)
	/* Yavas salinan ruzgar: alevin ust kismi saga-sola egilir */
	float Wind = sinf(Time * 0.8f) * 0.6f + sinf(Time * 2.1f) * 0.3f;
#else
	float Wind = 0.0f;
#endif

	CoolOffset = (uint8_t)((CoolOffset + 1U) % H);

	for(y = 0; y < H; y++)
	{
		/* Ust satirlarda ruzgar kaymasi daha fazla */
		int Shift = (int)(Wind * (float)(H - y) / (float)H + ((Rand() & 7U) == 0U ? ((Rand() & 1U) ? 1 : -1) : 0));

		for(x = 0; x < W; x++)
		{
			int sx = x - Shift;
			int xl, xr, Sum, c;

			if(sx < 0) sx = 0;
			if(sx >= W) sx = W - 1;
			xl = (sx > 0) ? sx - 1 : sx;
			xr = (sx < W - 1) ? sx + 1 : sx;

			Sum = U.Fire.Heat[y + 1][xl] + U.Fire.Heat[y + 1][sx] * 2 +
			      U.Fire.Heat[y + 1][xr] + U.Fire.Heat[y + 2][sx] * 2;

			c = (CoolMap[(y + CoolOffset) % H][x] * Cool) >> 7;		/* 0..2*Cool */
			U.Fire.Heat[y][x] = (uint8_t)SatI(Sum / 6 - c);
		}
	}
}

/* Alev yayilimi hiza bagli: karede 0, 1 ya da 2 adim (ortalama = hiz carpani) */
static void FirePropagate(void)
{
	static float Acc = 0.0f;
	Acc += SpdDT * CFG_FPS;
	if(Acc > 3.0f) Acc = 3.0f;
	while(Acc >= 1.0f) { FireStep(); Acc -= 1.0f; }
}

/* Kivilcimlar: vurusta alevin icinden yukari ucan noktalar */
static void FireEmbers(const uint8_t Pal[256][3], uint8_t Mirror)
{
#if(CFG_FIRE_SPARKS)
	uint32_t i, n;

	if(SPEC_Beat)
	{
		for(n = 0; n < 10U; n++)
			for(i = 0; i < MAX_EMBERS; i++)
			{
				Particle *p = &U.Fire.Embers[i];
				if(p->Life) continue;
				p->X  = Mirror ? (64.0f + (RandF() - 0.5f) * 70.0f) : RandF() * (float)W;
				p->Y  = (float)H - 2.0f - RandF() * 10.0f;
				p->Vx = (RandF() - 0.5f) * 20.0f;
				p->Vy = -(25.0f + 45.0f * RandF());
				p->Hue = (uint16_t)(90U + Rand() % 166U);		/* Aurora: her kivilcim kendi tonunda */
				p->Life = 255U;
				break;
			}
	}

	for(i = 0; i < MAX_EMBERS; i++)
	{
		Particle *p = &U.Fire.Embers[i];
		uint8_t   v;
		if(!p->Life) continue;

		p->Vy += 70.0f * DT;									/* Hafif yercekimi	*/
		p->X  += (p->Vx + (RandF() - 0.5f) * 30.0f) * DT;		/* Titreyerek ucar	*/
		p->Y  += p->Vy * DT;
		p->Life = (p->Life > 6U) ? (uint8_t)(p->Life - 6U) : 0U;
		if(p->Y < -2.0f || p->Y >= (float)H || p->X < 0.0f || p->X >= (float)W) { p->Life = 0; continue; }

		if(SPEC_FireAurora)										/* Renk renk kivilcim: tonu sabit, omurle soner */
		{
			const uint32_t k = 128U + (p->Life >> 1);
			const uint8_t *c = Pal[p->Hue];
			if(p->Y >= 0.0f) Add((int)p->X, (int)p->Y, (uint8_t)((c[0] * k) >> 8), (uint8_t)((c[1] * k) >> 8), (uint8_t)((c[2] * k) >> 8));
			continue;
		}
		v = (uint8_t)(160U + (p->Life >> 2) + (Rand() & 31U));
		if(p->Y >= 0.0f) Add((int)p->X, (int)p->Y, Pal[v][0], Pal[v][1], Pal[v][2]);
	}
#else
	(void)Pal; (void)Mirror;
#endif
}

static void FireRender(const uint8_t Pal[256][3])
{
	int x, y;

	for(y = 0; y < H; y++)
		for(x = 0; x < W; x++)
		{
			const uint8_t *c = Pal[U.Fire.Heat[y][x]];
			Set(x, y, c[0], c[1], c[2]);
		}
}

/* Yakit: alt iki gizli satir. Seviye alevin boyunu belirler.
   Her sutunun yakiti hizla artar, ~150 ms'de azalir ve titrek verilir:
   ani bas darbesinde duz bir alev cephesi kopup kaymaz, alev "parlar". */
static float FuelLvl[W];

/* Tiz bantlar dar ve kisa: yanyana bulanikta isilari kaybolur, alev
   cikmaz. Bant konumuna gore yakit kazanci (bas 1.0x -> en tiz ~2.0x). */
static inline float FuelGain(int Band, int Count)
{
	float f = (float)Band / (float)Count;
	return 1.0f + CFG_FIRE_TREBLE * f * sqrtf(f);
}

static inline uint8_t FuelAt(int x, float Level)
{
	float n;

	if(Level > FuelLvl[x]) FuelLvl[x] += (Level - FuelLvl[x]) * 0.6f;
	else                   FuelLvl[x] *= expf(-DT / 0.15f);

	n = 0.45f + 0.55f * RandF();							/* Titreme */
	if((Rand() & 15U) == 0U) n *= 0.3f;						/* Arada bogulma */
	return Sat(FuelLvl[x] * 300.0f * n + BeatEnv * 40.0f * n);
}

/* ==========================================================================
   MOD -- ALEV: her sutun kendi bandiyla yanar
   ========================================================================== */
static void ModeFire(void)
{
	int x;

	for(x = 0; x < W; x++)
	{
		uint8_t f = FuelAt(x, Bar[x] / (float)H * FuelGain(x, W));
		U.Fire.Heat[H][x] = f;
		U.Fire.Heat[H + 1][x] = (uint8_t)((f + U.Fire.Heat[H + 1][x]) >> 1);
	}

	FirePropagate();
	FireRender(ActPal);
	FireEmbers(ActPal, 0);
}

/* ==========================================================================
   MOD -- ALEV CUBUK: cubugun govdesi koz, ustunden alev dilleri
   ========================================================================== */
static void ModeFireBars(void)
{
	const uint32_t Pitch = 4U, Width = 3U;
	uint32_t k, x;
	int      y;

	memset(U.Fire.Heat[H], 0, 2U * W);
	FirePropagate();

	for(k = 0; k < Nb; k++)
	{
		int h = (int)(Bar[k] + 0.5f);

		for(y = 0; y < h; y++)
		{
			/* Govde: koz rengi (turuncu-sari), tepeye dogru az soguk + titreme */
			int v = 200 - (y * 40) / H + (int)(BeatEnv * 30.0f);
			for(x = 0; x < Width; x++)
				U.Fire.Heat[H - 1 - y][k * Pitch + x] = (uint8_t)SatI(v - (int)(Rand() & 31U));
		}
	}

	FireRender(ActPal);
	FireEmbers(ActPal, 0);
}

/* ==========================================================================
   MOD -- ORTA ALEV: ortadan simetrik, bas ortada
   ========================================================================== */
static void ModeFireBlue(void)
{
	uint32_t c;

	for(c = 0; c < 64U; c++)
	{
		uint8_t f = FuelAt((int)(64U + c), Bar[c] / (float)H * FuelGain((int)c, 64));
		U.Fire.Heat[H][64U + c] = U.Fire.Heat[H + 1][64U + c] = f;
		f = FuelAt((int)(63U - c), Bar[c] / (float)H * FuelGain((int)c, 64));
		U.Fire.Heat[H][63U - c] = U.Fire.Heat[H + 1][63U - c] = f;
	}

	FirePropagate();
	FireRender(ActPal);
	FireEmbers(ActPal, 1);
}

/* ==========================================================================
   MOD -- PATLAMA
   Zeminde surekli kucuk bir ates yanar (spektrumla). Her vurusta zeminden
   bir patlama: yerde parlayan bir ates topu + yukari ve yanlara savrulan
   alev parcaciklari. Parcaciklar ucarken isi haritasina iz birakir; alev
   motoru bu izleri kaynatip yukari dagitir -> alev etrafa sacilir, soner.
   Vurus ne kadar guclu (bas), patlama o kadar buyuk.
   ========================================================================== */
#define MAX_FRAGS	120

#define FRAGS		(U.Fire.Frags)

static float	GlowX = 64.0f, Glow = 0.0f;

static void BlastSpawn(void)
{
	const float S  = CFG_FIRE_BLAST_SIZE * (0.55f + 0.7f * SPEC_Bass);
	const float Cx = 12.0f + RandF() * 104.0f;
	const float R0 = 5.0f + 10.0f * S;					/* Yer ates topu yaricapi */
	uint32_t i, n, Count = (uint32_t)(25.0f + 45.0f * S);
	int x, y;

	/* Yerde ates topu: alt satirlar + gizli yakit satirlari */
	for(x = (int)(Cx - R0); x <= (int)(Cx + R0); x++)
	{
		float e;
		if(x < 0 || x >= W) continue;
		e = 1.0f - fabsf((float)x - Cx) / R0;
		for(y = H - 4; y < H + 2; y++)
		{
			int v = (int)(150.0f + 105.0f * sqrtf(e)) - (int)(Rand() & 31U);
			if(v > U.Fire.Heat[y][x]) U.Fire.Heat[y][x] = (uint8_t)SatI(v);
		}
	}

	/* Savrulan alev parcaciklari: yukari yarim daire, yataya yakin acilar az */
	for(n = 0; n < Count; n++)
		for(i = 0; i < MAX_FRAGS; i++)
		{
			__typeof__(&FRAGS[0]) f = &FRAGS[i];
			float a, v;
			if(f->Life) continue;
			a = 3.14159f * (0.10f + 0.80f * RandF());
			v = (60.0f + 170.0f * RandF()) * S;
			f->X  = Cx + (RandF() - 0.5f) * R0;
			f->Y  = (float)H - 1.0f;
			f->Vx = cosf(a) * v;
			f->Vy = -sinf(a) * v;
			f->Life = 255U;
			break;
		}

	GlowX = Cx;
	Glow  = 0.6f + 0.4f * S;
}

static void ModeBlast(void)
{
	int x;
	uint32_t i;

	/* Zeminde surekli kucuk ates (spektrumun yarisi kadar) */
	for(x = 0; x < W; x++)
	{
		uint8_t f = FuelAt(x, Bar[x * Nb / W] / (float)H * 0.55f * FuelGain(x, W));
		U.Fire.Heat[H][x] = f;
		U.Fire.Heat[H + 1][x] = (uint8_t)((f + U.Fire.Heat[H + 1][x]) >> 1);
	}

	FirePropagate();
	if(SPEC_Beat) BlastSpawn();

	/* Parcaciklar: yercekimi + hava direnci, ucarken isi izi birakir */
	for(i = 0; i < MAX_FRAGS; i++)
	{
		__typeof__(&FRAGS[0]) f = &FRAGS[i];
		int   px, py, v;
		if(!f->Life) continue;

		f->Vy += 150.0f * DT;
		f->Vx *= expf(-DT * 1.3f);
		f->Vy *= expf(-DT * 1.3f);
		f->X  += f->Vx * DT;
		f->Y  += f->Vy * DT;
		f->Life = (f->Life > 7U) ? (uint8_t)(f->Life - 7U) : 0U;

		px = (int)f->X; py = (int)f->Y;
		if(px < 0 || px >= W || py >= H) { f->Life = 0; continue; }
		if(py < 0) continue;

		v = 110 + (f->Life >> 1) + (int)(Rand() & 15U);
		if(v > U.Fire.Heat[py][px]) U.Fire.Heat[py][px] = (uint8_t)SatI(v);
		if(px > 0     && v - 40 > U.Fire.Heat[py][px - 1]) U.Fire.Heat[py][px - 1] = (uint8_t)SatI(v - 40);
		if(px < W - 1 && v - 40 > U.Fire.Heat[py][px + 1]) U.Fire.Heat[py][px + 1] = (uint8_t)SatI(v - 40);
	}

	FireRender(ActPal);

	/* Yerden yayilan parlama: patlama noktasinda, ~60 ms'de soner */
	if(Glow > 0.02f)
	{
		int y;
		for(y = 0; y < H; y++)
			for(x = 0; x < W; x++)
			{
				float dx = ((float)x - GlowX) * 0.6f, dy = (float)(H - y);
				float d  = 1.0f - sqrtf(dx * dx + dy * dy) / 40.0f;
				if(d > 0.0f)
				{
					uint16_t s = (uint16_t)(d * d * Glow * 150.0f);
					Add(x, y, (uint8_t)((255U * s) >> 8), (uint8_t)((170U * s) >> 8), (uint8_t)((60U * s) >> 8));
				}
			}
	}
	Glow *= expf(-DT / 0.06f);

	FireEmbers(ActPal, 0);
}

/* ==========================================================================
   MOD -- ORTA PATLAMA
   Bas'a bagli. Ortada zeminde bas siddetiyle buyuyen bir ates yanar.
   Her vurusta ortadan iki kenara dogru patlama:
     - zeminde iki yana kosan bir sok dalgasi (alev cephesi)
     - iki yana, hafif yukari savrulan alev parcaciklari
   Kenarlara yaklastikca patlama zayiflar ve yok olur.
   ========================================================================== */
#define MAX_WAVES	4

static struct { float R, V, Pow; } Waves[MAX_WAVES];
static float	CBass = 0.0f;					/* Yumusatilmis bas seviyesi */

/* Merkeze uzakliga gore zayiflama: ortada 1, kenarda 0 */
static inline float EdgeFade(float x)
{
	float d = 1.0f - fabsf(x - 64.0f) / 64.0f;
	return (d < 0.0f) ? 0.0f : sqrtf(d) * d;			/* Kenara yakin hizla soner */
}

static void CenterBlastSpawn(void)
{
	const float S = CFG_FIRE_BLAST_SIZE * (0.5f + 0.8f * CBass);
	uint32_t i, n, Count = (uint32_t)(30.0f + 50.0f * S);

	/* Sok dalgasi */
	for(i = 0; i < MAX_WAVES; i++)
		if(Waves[i].Pow <= 0.0f)
		{
			Waves[i].R   = 2.0f;
			Waves[i].V   = 140.0f + 160.0f * S;			/* px/s */
			Waves[i].Pow = 1.0f;
			break;
		}

	/* Iki yana savrulan parcaciklar: yataya yakin, hafif yukari */
	for(n = 0; n < Count; n++)
		for(i = 0; i < MAX_FRAGS; i++)
		{
			__typeof__(&FRAGS[0]) f = &FRAGS[i];
			float a, v, Dir;
			if(f->Life) continue;
			Dir = (n & 1U) ? 1.0f : -1.0f;
			a = 3.14159f * (0.03f + 0.30f * RandF());	/* 5..60 derece */
			v = (80.0f + 180.0f * RandF()) * S;
			f->X  = 64.0f + Dir * RandF() * 3.0f;
			f->Y  = (float)H - 1.0f - RandF() * 3.0f;
			f->Vx = Dir * cosf(a) * v;
			f->Vy = -sinf(a) * v;
			f->Life = 255U;
			break;
		}

	Glow = 0.5f + 0.5f * S;
	GlowX = 64.0f;
}

static void ModeCenterBlast(void)
{
	int x, y;
	uint32_t i;
	float B;

	/* Bas seviyesi: en dusuk 3 bant (16 bantlik gruplamada ~60-200 Hz) */
	B = Bar[0];
	if(Bar[1] > B) B = Bar[1];
	if(Bar[2] > B) B = Bar[2];
	B /= (float)H;
	if(B > CBass) CBass = B;
	else          CBass += (B - CBass) * (1.0f - expf(-DT / 0.12f));

	/* Ortada bas ile buyuyen ates: genislik ve boy bas ile */
	{
		const float Wd = 6.0f + 40.0f * CBass;
		for(x = 0; x < W; x++)
		{
			float e = 1.0f - fabsf((float)x + 0.5f - 64.0f) / Wd;
			uint8_t f = FuelAt(x, (e > 0.0f) ? CBass * (0.4f + 0.6f * e) : 0.0f);
			U.Fire.Heat[H][x] = f;
			U.Fire.Heat[H + 1][x] = (uint8_t)((f + U.Fire.Heat[H + 1][x]) >> 1);
		}
	}

	FirePropagate();
	if(SPEC_Beat) CenterBlastSpawn();

	/* Sok dalgalari: zeminde iki yana kosan alev cephesi, kenarda soner */
	for(i = 0; i < MAX_WAVES; i++)
	{
		int s;
		if(Waves[i].Pow <= 0.0f) continue;
		Waves[i].R += Waves[i].V * DT;
		Waves[i].V *= expf(-DT * 1.2f);
		if(Waves[i].R > 66.0f) { Waves[i].Pow = 0.0f; continue; }

		for(s = -1; s <= 1; s += 2)
		{
			float Fx = 64.0f + (float)s * Waves[i].R;
			float Pw = EdgeFade(Fx) * Waves[i].Pow;
			for(x = (int)(Fx - 2.0f); x <= (int)(Fx + 2.0f); x++)
			{
				if(x < 0 || x >= W) continue;
				for(y = H - 3; y < H + 2; y++)
				{
					int v = (int)(Pw * (230.0f - (float)(H + 1 - y) * 12.0f)) + (int)(Rand() & 31U);
					if(v > U.Fire.Heat[y][x]) U.Fire.Heat[y][x] = (uint8_t)SatI(v);
				}
			}
		}
	}

	/* Parcaciklar: kenara gittikce isileri azalir */
	for(i = 0; i < MAX_FRAGS; i++)
	{
		__typeof__(&FRAGS[0]) f = &FRAGS[i];
		int   px, py, v;
		float Fd;
		if(!f->Life) continue;

		f->Vy += 120.0f * DT;
		f->Vx *= expf(-DT * 1.0f);
		f->Vy *= expf(-DT * 1.0f);
		f->X  += f->Vx * DT;
		f->Y  += f->Vy * DT;
		f->Life = (f->Life > 6U) ? (uint8_t)(f->Life - 6U) : 0U;

		px = (int)f->X; py = (int)f->Y;
		if(px < 0 || px >= W || py >= H) { f->Life = 0; continue; }
		if(py < 0) continue;

		Fd = EdgeFade(f->X);
		if(Fd < 0.03f) { f->Life = 0; continue; }		/* Kenarda yok olur */

		v = (int)((float)(110 + (f->Life >> 1) + (int)(Rand() & 15U)) * Fd);
		if(v > U.Fire.Heat[py][px]) U.Fire.Heat[py][px] = (uint8_t)SatI(v);
		if(py + 1 < H && v - 50 > U.Fire.Heat[py + 1][px]) U.Fire.Heat[py + 1][px] = (uint8_t)SatI(v - 50);
	}

	FireRender(ActPal);

	/* Ortadan yayilan parlama */
	if(Glow > 0.02f)
	{
		for(y = 0; y < H; y++)
			for(x = 0; x < W; x++)
			{
				float dx = ((float)x - 64.0f) * 0.45f, dy = (float)(H - y) * 1.2f;
				float d  = 1.0f - sqrtf(dx * dx + dy * dy) / 40.0f;
				if(d > 0.0f)
				{
					uint16_t s = (uint16_t)(d * d * Glow * 140.0f);
					Add(x, y, (uint8_t)((255U * s) >> 8), (uint8_t)((160U * s) >> 8), (uint8_t)((50U * s) >> 8));
				}
			}
	}
	Glow *= expf(-DT / 0.07f);

	FireEmbers(ActPal, 1);
}

/* ==========================================================================
   MOD -- ALEV HALKA
   Alev modu zeminde aynen ve her karede tam hizda yanar.
   Her kick ortadan buyuk bir alev halkasi acar (tempo suzgeci yok, vurus
   kacmaz); halka panelden tasinca sagda ve solda yay olarak devam eder.
   Bas muziginde kick dedektorunun kacirdigi bas darbeleri de buyuk halka
   olur; aradaki bas notalari ve orta frekans vuruslari kucuk halka.
   Tiz (>4 kHz agirlikli) vuruslar halka yapmaz. Vurusta ortada kisa sureli
   sicak bir cekirdek (top) parlar.
   Renk palete bagli (simdilik turuncu alev); beyaz ve sari yok.
   ========================================================================== */
#define SW_MAX		8
#define SW_CX		((float)W / 2.0f - 0.5f)	/* Panel ortasi: panel sayisindan bagimsiz */
#define SW_CY		((float)H / 2.0f - 0.5f)
#define SW_HOT		165.0f						/* Isi ust siniri: turuncu */

typedef struct { float Age, Life, Rmax, S, Tau, W0, Wk; float R, Fd, Wd; uint8_t Alive; } SWave;

static SWave	SWaves[SW_MAX];
/* Merkeze uzaklik x3, tek ceyrek (dort ceyrek simetrik), flash'ta.
   [dy][dx]: dx = i + 0.5, dy = j + 0.5 piksel. 85 px ustu hep 255 (kesilir),
   bu yuzden panel sayisi ne olursa olsun tablo yeter. Uretim:
   min(255, int(sqrt((i+.5)^2 + (j+.5)^2) * 3))						*/
#define SW_LUT_W	86
#define SW_LUT_H	32
#if (SPEC_HEIGHT / 2) > SW_LUT_H
#error "SwDistLut: panel yuksekligi tabloyu asiyor"
#endif
static const uint8_t SwDistLut[SW_LUT_H][SW_LUT_W] =
{
	{   2,   4,   7,  10,  13,  16,  19,  22,  25,  28,  31,  34,  37,  40,  43,  46,  49,  52,  55,  58,  61,  64,  67,  70,  73,  76,  79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 127, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 160, 163, 166, 169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255 },
	{   4,   6,   8,  11,  14,  17,  20,  22,  25,  28,  31,  34,  37,  40,  43,  46,  49,  52,  55,  58,  61,  64,  67,  70,  73,  76,  79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 127, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 160, 163, 166, 169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255 },
	{   7,   8,  10,  12,  15,  18,  20,  23,  26,  29,  32,  35,  38,  41,  44,  47,  50,  53,  56,  58,  61,  64,  67,  70,  73,  76,  79,  82,  85,  88,  91,  94,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 127, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 160, 163, 166, 169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255 },
	{  10,  11,  12,  14,  17,  19,  22,  24,  27,  30,  33,  36,  38,  41,  44,  47,  50,  53,  56,  59,  62,  65,  68,  71,  74,  77,  80,  83,  86,  89,  92,  95,  98, 101, 104, 107, 110, 112, 115, 118, 121, 124, 127, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 160, 163, 166, 169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255 },
	{  13,  14,  15,  17,  19,  21,  23,  26,  28,  31,  34,  37,  39,  42,  45,  48,  51,  54,  57,  60,  62,  65,  68,  71,  74,  77,  80,  83,  86,  89,  92,  95,  98, 101, 104, 107, 110, 113, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 176, 179, 182, 184, 187, 190, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255 },
	{  16,  17,  18,  19,  21,  23,  25,  27,  30,  32,  35,  38,  40,  43,  46,  49,  52,  55,  57,  60,  63,  66,  69,  72,  75,  78,  81,  84,  87,  90,  92,  95,  98, 101, 104, 107, 110, 113, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 176, 179, 182, 185, 188, 191, 194, 197, 200, 203, 206, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 254, 255 },
	{  19,  20,  20,  22,  23,  25,  27,  29,  32,  34,  37,  39,  42,  44,  47,  50,  53,  56,  58,  61,  64,  67,  70,  73,  76,  78,  81,  84,  87,  90,  93,  96,  99, 102, 105, 108, 111, 114, 117, 120, 123, 126, 128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 176, 179, 182, 185, 188, 191, 194, 197, 200, 203, 206, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 254, 255 },
	{  22,  22,  23,  24,  26,  27,  29,  31,  34,  36,  38,  41,  43,  46,  48,  51,  54,  57,  59,  62,  65,  68,  71,  74,  76,  79,  82,  85,  88,  91,  94,  97, 100, 102, 105, 108, 111, 114, 117, 120, 123, 126, 129, 132, 135, 138, 141, 144, 147, 150, 153, 156, 159, 162, 165, 168, 170, 173, 176, 179, 182, 185, 188, 191, 194, 197, 200, 203, 206, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 254, 255 },
	{  25,  25,  26,  27,  28,  30,  32,  34,  36,  38,  40,  42,  45,  47,  50,  53,  55,  58,  61,  63,  66,  69,  72,  74,  77,  80,  83,  86,  89,  92,  94,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124, 127, 130, 132, 135, 138, 141, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 189, 192, 195, 198, 201, 204, 207, 210, 213, 216, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 254, 255 },
	{  28,  28,  29,  30,  31,  32,  34,  36,  38,  40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,  78,  81,  84,  87,  90,  92,  95,  98, 101, 104, 107, 110, 113, 116, 118, 121, 124, 127, 130, 133, 136, 139, 142, 145, 148, 151, 154, 157, 160, 163, 165, 168, 171, 174, 177, 180, 183, 186, 189, 192, 195, 198, 201, 204, 207, 210, 213, 216, 219, 222, 225, 228, 231, 234, 237, 240, 243, 246, 249, 252, 255, 255 },
	{  31,  31,  32,  33,  34,  35,  37,  38,  40,  42,  44,  46,  48,  51,  53,  56,  58,  61,  63,  66,  69,  71,  74,  77,  79,  82,  85,  88,  91,  93,  96,  99, 102, 105, 108, 111, 113, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 145, 148, 151, 154, 157, 160, 163, 166, 169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 201, 204, 207, 210, 213, 216, 219, 222, 225, 228, 231, 234, 237, 240, 243, 246, 249, 252, 255, 255 },
	{  34,  34,  35,  36,  37,  38,  39,  41,  42,  44,  46,  48,  50,  53,  55,  57,  60,  62,  65,  67,  70,  73,  75,  78,  81,  83,  86,  89,  92,  94,  97, 100, 103, 106, 109, 111, 114, 117, 120, 123, 126, 129, 132, 134, 137, 140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 240, 243, 246, 249, 252, 255, 255 },
	{  37,  37,  38,  38,  39,  40,  42,  43,  45,  47,  48,  50,  53,  55,  57,  59,  62,  64,  66,  69,  72,  74,  77,  79,  82,  85,  87,  90,  93,  96,  98, 101, 104, 107, 110, 112, 115, 118, 121, 124, 127, 130, 132, 135, 138, 141, 144, 147, 150, 153, 156, 158, 161, 164, 167, 170, 173, 176, 179, 182, 185, 188, 191, 194, 197, 200, 202, 205, 208, 211, 214, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255, 255 },
	{  40,  40,  41,  41,  42,  43,  44,  46,  47,  49,  51,  53,  55,  57,  59,  61,  63,  66,  68,  71,  73,  76,  78,  81,  83,  86,  89,  91,  94,  97, 100, 102, 105, 108, 111, 113, 116, 119, 122, 125, 128, 130, 133, 136, 139, 142, 145, 148, 151, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 185, 188, 191, 194, 197, 200, 203, 206, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 238, 241, 244, 247, 250, 253, 255, 255 },
	{  43,  43,  44,  44,  45,  46,  47,  48,  50,  52,  53,  55,  57,  59,  61,  63,  65,  68,  70,  72,  75,  77,  80,  82,  85,  88,  90,  93,  95,  98, 101, 104, 106, 109, 112, 115, 117, 120, 123, 126, 129, 131, 134, 137, 140, 143, 146, 148, 151, 154, 157, 160, 163, 166, 169, 172, 174, 177, 180, 183, 186, 189, 192, 195, 198, 201, 204, 207, 210, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 254, 255, 255 },
	{  46,  46,  47,  47,  48,  49,  50,  51,  53,  54,  56,  57,  59,  61,  63,  65,  67,  70,  72,  74,  77,  79,  81,  84,  86,  89,  92,  94,  97,  99, 102, 105, 108, 110, 113, 116, 118, 121, 124, 127, 130, 132, 135, 138, 141, 144, 147, 149, 152, 155, 158, 161, 164, 167, 169, 172, 175, 178, 181, 184, 187, 190, 193, 196, 199, 201, 204, 207, 210, 213, 216, 219, 222, 225, 228, 231, 234, 237, 240, 242, 245, 248, 251, 254, 255, 255 },
	{  49,  49,  50,  50,  51,  52,  53,  54,  55,  57,  58,  60,  62,  63,  65,  67,  70,  72,  74,  76,  78,  81,  83,  86,  88,  91,  93,  96,  98, 101, 104, 106, 109, 112, 114, 117, 120, 122, 125, 128, 131, 133, 136, 139, 142, 145, 148, 150, 153, 156, 159, 162, 165, 167, 170, 173, 176, 179, 182, 185, 188, 191, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 223, 225, 228, 231, 234, 237, 240, 243, 246, 249, 252, 255, 255, 255 },
	{  52,  52,  53,  53,  54,  55,  56,  57,  58,  59,  61,  62,  64,  66,  68,  70,  72,  74,  76,  78,  80,  83,  85,  87,  90,  92,  95,  97, 100, 102, 105, 108, 110, 113, 116, 118, 121, 124, 126, 129, 132, 135, 137, 140, 143, 146, 149, 151, 154, 157, 160, 163, 166, 168, 171, 174, 177, 180, 183, 186, 188, 191, 194, 197, 200, 203, 206, 209, 212, 215, 217, 220, 223, 226, 229, 232, 235, 238, 241, 244, 247, 250, 253, 255, 255, 255 },
	{  55,  55,  56,  56,  57,  57,  58,  59,  61,  62,  63,  65,  66,  68,  70,  72,  74,  76,  78,  80,  82,  85,  87,  89,  92,  94,  96,  99, 101, 104, 107, 109, 112, 114, 117, 120, 122, 125, 128, 130, 133, 136, 139, 141, 144, 147, 150, 152, 155, 158, 161, 164, 166, 169, 172, 175, 178, 181, 184, 186, 189, 192, 195, 198, 201, 204, 207, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 241, 244, 247, 250, 253, 255, 255, 255 },
	{  58,  58,  58,  59,  60,  60,  61,  62,  63,  65,  66,  67,  69,  71,  72,  74,  76,  78,  80,  82,  84,  87,  89,  91,  93,  96,  98, 101, 103, 106, 108, 111, 113, 116, 118, 121, 124, 126, 129, 132, 134, 137, 140, 143, 145, 148, 151, 154, 156, 159, 162, 165, 168, 170, 173, 176, 179, 182, 184, 187, 190, 193, 196, 199, 202, 205, 207, 210, 213, 216, 219, 222, 225, 228, 231, 233, 236, 239, 242, 245, 248, 251, 254, 255, 255, 255 },
	{  61,  61,  61,  62,  62,  63,  64,  65,  66,  67,  69,  70,  72,  73,  75,  77,  78,  80,  82,  84,  86,  89,  91,  93,  95,  98, 100, 102, 105, 107, 110, 112, 115, 117, 120, 122, 125, 128, 130, 133, 136, 138, 141, 144, 146, 149, 152, 155, 157, 160, 163, 166, 169, 171, 174, 177, 180, 183, 185, 188, 191, 194, 197, 200, 203, 205, 208, 211, 214, 217, 220, 223, 226, 228, 231, 234, 237, 240, 243, 246, 249, 252, 255, 255, 255, 255 },
	{  64,  64,  64,  65,  65,  66,  67,  68,  69,  70,  71,  73,  74,  76,  77,  79,  81,  83,  85,  87,  89,  91,  93,  95,  97, 100, 102, 104, 107, 109, 111, 114, 116, 119, 121, 124, 127, 129, 132, 134, 137, 140, 142, 145, 148, 150, 153, 156, 159, 161, 164, 167, 170, 172, 175, 178, 181, 184, 186, 189, 192, 195, 198, 201, 203, 206, 209, 212, 215, 218, 221, 223, 226, 229, 232, 235, 238, 241, 244, 247, 249, 252, 255, 255, 255, 255 },
	{  67,  67,  67,  68,  68,  69,  70,  71,  72,  73,  74,  75,  77,  78,  80,  81,  83,  85,  87,  89,  91,  93,  95,  97,  99, 102, 104, 106, 108, 111, 113, 116, 118, 121, 123, 126, 128, 131, 133, 136, 138, 141, 144, 146, 149, 152, 154, 157, 160, 163, 165, 168, 171, 174, 176, 179, 182, 185, 188, 190, 193, 196, 199, 202, 204, 207, 210, 213, 216, 219, 222, 224, 227, 230, 233, 236, 239, 242, 244, 247, 250, 253, 255, 255, 255, 255 },
	{  70,  70,  70,  71,  71,  72,  73,  74,  74,  76,  77,  78,  79,  81,  82,  84,  86,  87,  89,  91,  93,  95,  97,  99, 101, 104, 106, 108, 110, 113, 115, 117, 120, 122, 125, 127, 130, 132, 135, 137, 140, 143, 145, 148, 150, 153, 156, 158, 161, 164, 167, 169, 172, 175, 178, 180, 183, 186, 189, 191, 194, 197, 200, 203, 205, 208, 211, 214, 217, 220, 222, 225, 228, 231, 234, 237, 240, 242, 245, 248, 251, 254, 255, 255, 255, 255 },
	{  73,  73,  73,  74,  74,  75,  76,  76,  77,  78,  79,  81,  82,  83,  85,  86,  88,  90,  92,  93,  95,  97,  99, 101, 103, 106, 108, 110, 112, 115, 117, 119, 122, 124, 126, 129, 131, 134, 136, 139, 142, 144, 147, 149, 152, 155, 157, 160, 163, 165, 168, 171, 173, 176, 179, 182, 184, 187, 190, 193, 195, 198, 201, 204, 206, 209, 212, 215, 218, 221, 223, 226, 229, 232, 235, 238, 240, 243, 246, 249, 252, 255, 255, 255, 255, 255 },
	{  76,  76,  76,  77,  77,  78,  78,  79,  80,  81,  82,  83,  85,  86,  88,  89,  91,  92,  94,  96,  98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 119, 121, 123, 126, 128, 131, 133, 136, 138, 141, 143, 146, 148, 151, 153, 156, 159, 161, 164, 167, 169, 172, 175, 177, 180, 183, 185, 188, 191, 194, 196, 199, 202, 205, 208, 210, 213, 216, 219, 222, 224, 227, 230, 233, 236, 239, 241, 244, 247, 250, 253, 255, 255, 255, 255, 255 },
	{  79,  79,  79,  80,  80,  81,  81,  82,  83,  84,  85,  86,  87,  89,  90,  92,  93,  95,  96,  98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 121, 123, 125, 128, 130, 132, 135, 137, 140, 142, 145, 147, 150, 152, 155, 157, 160, 163, 165, 168, 171, 173, 176, 179, 181, 184, 187, 189, 192, 195, 198, 200, 203, 206, 209, 211, 214, 217, 220, 223, 225, 228, 231, 234, 237, 240, 242, 245, 248, 251, 254, 255, 255, 255, 255, 255 },
	{  82,  82,  82,  83,  83,  84,  84,  85,  86,  87,  88,  89,  90,  91,  93,  94,  96,  97,  99, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 123, 125, 127, 130, 132, 134, 137, 139, 141, 144, 146, 149, 151, 154, 156, 159, 162, 164, 167, 169, 172, 175, 177, 180, 183, 185, 188, 191, 193, 196, 199, 202, 204, 207, 210, 213, 215, 218, 221, 224, 227, 229, 232, 235, 238, 241, 243, 246, 249, 252, 255, 255, 255, 255, 255, 255 },
	{  85,  85,  85,  86,  86,  87,  87,  88,  89,  90,  91,  92,  93,  94,  95,  97,  98, 100, 101, 103, 105, 107, 108, 110, 112, 114, 116, 118, 120, 123, 125, 127, 129, 131, 134, 136, 138, 141, 143, 146, 148, 151, 153, 156, 158, 161, 163, 166, 168, 171, 173, 176, 179, 181, 184, 187, 189, 192, 195, 197, 200, 203, 206, 208, 211, 214, 217, 219, 222, 225, 228, 230, 233, 236, 239, 242, 244, 247, 250, 253, 255, 255, 255, 255, 255, 255 },
	{  88,  88,  88,  89,  89,  90,  90,  91,  92,  92,  93,  94,  96,  97,  98,  99, 101, 102, 104, 106, 107, 109, 111, 113, 115, 116, 118, 120, 123, 125, 127, 129, 131, 133, 136, 138, 140, 143, 145, 147, 150, 152, 155, 157, 160, 162, 165, 167, 170, 172, 175, 178, 180, 183, 185, 188, 191, 193, 196, 199, 201, 204, 207, 210, 212, 215, 218, 220, 223, 226, 229, 232, 234, 237, 240, 243, 245, 248, 251, 254, 255, 255, 255, 255, 255, 255 },
	{  91,  91,  91,  92,  92,  92,  93,  94,  94,  95,  96,  97,  98, 100, 101, 102, 104, 105, 107, 108, 110, 111, 113, 115, 117, 119, 121, 123, 125, 127, 129, 131, 133, 135, 138, 140, 142, 145, 147, 149, 152, 154, 156, 159, 161, 164, 166, 169, 171, 174, 176, 179, 182, 184, 187, 189, 192, 195, 197, 200, 203, 205, 208, 211, 214, 216, 219, 222, 224, 227, 230, 233, 235, 238, 241, 244, 247, 249, 252, 255, 255, 255, 255, 255, 255, 255 },
	{  94,  94,  94,  95,  95,  95,  96,  97,  97,  98,  99, 100, 101, 102, 104, 105, 106, 108, 109, 111, 112, 114, 116, 117, 119, 121, 123, 125, 127, 129, 131, 133, 135, 137, 140, 142, 144, 146, 149, 151, 153, 156, 158, 161, 163, 166, 168, 170, 173, 176, 178, 181, 183, 186, 188, 191, 194, 196, 199, 201, 204, 207, 209, 212, 215, 218, 220, 223, 226, 228, 231, 234, 237, 239, 242, 245, 248, 250, 253, 255, 255, 255, 255, 255, 255, 255 },
};
static float	SwPump  = 0.0f;					/* Cekirdek itisi 0..1 (aninda, ~150 ms) */
static float	SwSinceKick = 9.0f;				/* Son kick'ten beri, s			*/

/* Yeni halka. Rmax: ulasacagi yaricap, Life: omur, Tau: acilma hizi,
   W0/Wk: halka kalinligi (W0 + Wk*R), Pump: cekirdegi ne kadar itsin.	*/
static void SwHit(float S, float Rmax, float Life, float Tau, float W0, float Wk, float Pump)
{
	uint32_t i, Old = 0;

	for(i = 0; i < SW_MAX; i++)
	{
		if(!SWaves[i].Alive) { Old = i; break; }
		if(SWaves[i].Age > SWaves[Old].Age) Old = i;
	}
	SWaves[Old].Alive = 1;
	SWaves[Old].Age   = 0.0f;
	SWaves[Old].S     = S;
	SWaves[Old].Rmax  = Rmax * CFG_RING_REACH;
	SWaves[Old].Life  = Life * CFG_RING_SLOW;
	SWaves[Old].Tau   = Tau  * CFG_RING_SLOW;
	SWaves[Old].W0    = W0;
	SWaves[Old].Wk    = Wk;

	if(Pump > SwPump) SwPump = Pump;			/* Cekirdek aninda ileri */
}

static void ModeFireBall(void)
{
	const uint8_t (*Pal)[3] = ActPal;			/* Renk varyantlari icin tek nokta */
	int   x, y, X0, X1, Y0, Y1;
	uint32_t i;
	float BallR = 0.0f, BallAmt, Ext = 0.0f;
	uint8_t Any = 0;
	const float Dt = DT * (float)SPEC_FrameStep;	/* Kare atlansa da gercek zaman */

	/* 1) Alev moduyla ayni zemin alevi -- her karede, aynen */
	for(x = 0; x < W; x++)
	{
		uint8_t f = FuelAt(x, Bar[x] / (float)H * FuelGain(x, W));
		U.Fire.Heat[H][x] = f;
		U.Fire.Heat[H + 1][x] = (uint8_t)((f + U.Fire.Heat[H + 1][x]) >> 1);
	}
	FirePropagate();
	FireRender(Pal);

	/* 2) Vurus kaynaklari */
	SwSinceKick += Dt;
	if(SPEC_Kick)
	{
		/* Tempo suzgeci yok -- her kick buyuk halka, boyu sadece gucune gore */
		float S = 0.60f + 0.40f * SPEC_KickPower;
		SwSinceKick = 0.0f;
		SwHit(S, 14.0f + 58.0f * S, 0.26f + 0.18f * S, 0.09f, 1.8f, 0.10f, S);
	}
	else if(SPEC_Hit && SPEC_HitHigh < 0.55f && SwSinceKick > 0.08f)	/* kick'in kendi yankisi degil */
	{
		float S = SPEC_HitPower;
		if(SPEC_BassMode >= 0.5f && SPEC_HitLow > 0.35f)
		{
			/* Kick dedektorunun kacirdigi bas darbesi (ani dB sicramasi,
			   agirligi basta) -- yine buyuk halka. Ikinci, bagimsiz yakalama. */
			S = 0.60f + 0.40f * S;
			SwHit(S, 14.0f + 58.0f * S, 0.26f + 0.18f * S, 0.09f, 1.8f, 0.10f, S);
		}
		else if(SPEC_BassMode >= 0.5f)
			SwHit(S, 10.0f + 12.0f * S, 0.26f, 0.07f, 1.5f, 0.06f, 0.45f * S);
		else
			SwHit(S, 12.0f + 50.0f * S * S, 0.35f + 0.15f * S, 0.11f, 1.6f, 0.08f, S);
	}

	/* 3) Cekirdek itisi ~150 ms'de geri doner */
	SwPump *= expf(-Dt / 0.15f);

	BallAmt = SwPump;
	if(BallAmt > 0.12f)
	{
		BallR = 6.0f + 8.0f * SwPump;						/* Cap >= 12 px */
		Ext = BallR + 2.0f;
		Any = 1;
	}

	/* 4) Halkalar: kare sabitleri */
	for(i = 0; i < SW_MAX; i++)
	{
		SWave *w = &SWaves[i];
		if(!w->Alive) continue;
		w->Age += Dt;
		if(w->Age >= w->Life) { w->Alive = 0; continue; }
		w->R  = w->Rmax * (1.0f - expf(-w->Age / w->Tau));
		w->Fd = 1.0f - w->Age / w->Life;
		w->Wd = w->W0 + w->Wk * w->R;							/* Cephe kalinligi */
		if(w->R + 2.0f * w->Wd > Ext) Ext = w->R + 2.0f * w->Wd;
		Any = 1;
	}
	if(!Any) return;

	/* 5) Sadece etki alanindaki pikseller. Merkez sabit: piksel basina
	      tek mesafe hesabi, tum halkalar ayni mesafeyi kullanir.		*/
	X0 = (int)(SW_CX - Ext); X1 = (int)(SW_CX + Ext) + 1;
	Y0 = (int)(SW_CY - Ext); Y1 = (int)(SW_CY + Ext) + 1;
	if(X0 < 0) X0 = 0;
	if(X1 > W - 1) X1 = W - 1;
	if(Y0 < 0) Y0 = 0;
	if(Y1 > H - 1) Y1 = H - 1;

	/* 6) Radyal profil (kare basina) x kayan alev dokusu (piksel basina) */
	{
		static uint8_t BProf[256], WProf[256];
		const int RSx  = (int)(Time * 70.0f);
		const int RSy  = (int)(Time * 40.0f);
		int k, Kmax = (int)(Ext * 3.0f) + 3;
		if(Kmax > 255) Kmax = 255;

		for(k = 0; k <= Kmax; k++)
		{
			float d = (float)k * (1.0f / 3.0f), b = 0.0f, wv = 0.0f;
			if(BallR > 0.0f && d < BallR + 2.0f)
			{
				float q = 1.0f - d / (BallR + 2.0f);			/* 1 merkez, 0 kenar */
				b = SW_HOT * BallAmt * (0.30f + 0.85f * q) * 1.3f;	/* sicak merkez, duz disk degil */
			}
			for(i = 0; i < SW_MAX; i++)
			{
				const SWave *w = &SWaves[i];
				float r, g, v;
				if(!w->Alive || d > w->R + w->Wd) continue;
				r = (d - w->R) / w->Wd;
				g = 1.0f - r * r;										/* Cephe profili */
				if(g < 0.0f) g = 0.0f;
				if(d < w->R)
				{
					float In = (0.15f + 0.40f * d / w->R) * w->Fd * w->Fd;	/* ic hizla soner: halka belirgin */
					if(In > g) g = In;
				}
				v = SW_HOT * 1.2f * g * sqrtf(w->Fd) * (0.85f + 0.30f * w->S);	/* parlak, gec soner */
				if(v > wv) wv = v;
			}
			BProf[k] = (uint8_t)((b  > 255.0f) ? 255.0f : b);
			WProf[k] = (uint8_t)((wv > 255.0f) ? 255.0f : wv);
		}

		for(y = Y0; y <= Y1; y++)
		{
			const int qy = (y < (int)(H / 2U)) ? (int)(H / 2U) - 1 - y : y - (int)(H / 2U);
			const uint8_t *Dr = SwDistLut[qy];

			for(x = X0; x <= X1; x++)
			{
				const int qx = (x < (int)(W / 2U)) ? (int)(W / 2U) - 1 - x : x - (int)(W / 2U);
				int   dk = (qx < SW_LUT_W) ? Dr[qx] : 255;
				float h, n, v;
				if(dk > Kmax) continue;
				if(!(BProf[dk] | WProf[dk])) continue;

				/* Kayan alev dokusu (plazma gibi akar) */
				n = (float)CoolMap[(uint32_t)(y + RSy) % H][(uint32_t)(x + RSx) % W] * (1.0f / 255.0f);
				h = (float)BProf[dk] * (0.75f + 0.35f * n);
				v = (float)WProf[dk] * (0.45f + 0.75f * n);
				if(v > h) h = v;

				if(h > 8.0f)
				{
					int iv = (h > SW_HOT) ? (int)SW_HOT : (int)h;
					uint8_t *cc = CV(y, x);
					if(Pal[iv][0] > cc[0]) cc[0] = Pal[iv][0];
					if(Pal[iv][1] > cc[1]) cc[1] = Pal[iv][1];
					if(Pal[iv][2] > cc[2]) cc[2] = Pal[iv][2];
				}
			}
		}
	}
}

/* ==========================================================================
   MOD -- HALKA
   ========================================================================== */
static void DrawCircle(int Cx, int Cy, int Radius, uint32_t Hu, uint16_t Scale)
{
	int x = Radius, y = 0, Err = 1 - Radius;

	if(Radius <= 0) { AddHue(Cx, Cy, Hu, Scale); return; }

	while(x >= y)
	{
		AddHue(Cx + x, Cy + y, Hu, Scale); AddHue(Cx - x, Cy + y, Hu, Scale);
		AddHue(Cx + x, Cy - y, Hu, Scale); AddHue(Cx - x, Cy - y, Hu, Scale);
		AddHue(Cx + y, Cy + x, Hu, Scale); AddHue(Cx - y, Cy + x, Hu, Scale);
		AddHue(Cx + y, Cy - x, Hu, Scale); AddHue(Cx - y, Cy - x, Hu, Scale);
		y++;
		if(Err < 0) Err += 2 * y + 1;
		else        { x--; Err += 2 * (y - x) + 1; }
	}
}

static void ModeRings(void)
{
	uint32_t i, c;
	int      y, r, Orb;

	Fade(CFG_TRAIL);

	if(SPEC_Beat)
	{
		for(i = 0; i < MAX_RINGS; i++)
		{
			if(!U.Rings[i].Alive)
			{
				U.Rings[i].Alive = 1;
				U.Rings[i].R     = 2.0f;
				U.Rings[i].Speed = 45.0f + SPEC_Bass * 90.0f;
				U.Rings[i].Hue   = (uint16_t)HuePhase;
				break;
			}
		}
	}

	for(i = 0; i < MAX_RINGS; i++)
	{
		Ring *rg = &U.Rings[i];
		float Life;
		if(!rg->Alive) continue;

		rg->R += rg->Speed * DT;
		if(rg->R > 70.0f) { rg->Alive = 0; continue; }

		Life = 1.0f - rg->R / 75.0f;
		DrawCircle(W / 2, H / 2, (int)rg->R,     rg->Hue,       (uint16_t)(256.0f * Life));
		DrawCircle(W / 2, H / 2, (int)rg->R - 1, rg->Hue + 40U, (uint16_t)(120.0f * Life));
	}

	Orb = 1 + (int)(SPEC_Bass * 9.0f + BeatEnv * 3.0f);
	for(r = 0; r <= Orb; r++)
		DrawCircle(W / 2, H / 2, r, (uint32_t)HuePhase + 384U, (uint16_t)(220 - r * 12));

	for(c = 0; c < 64U; c++)
	{
		const int Hh = (int)(Bar[c] * 0.35f);
		for(y = 0; y < Hh; y++)
		{
			/* Cikis noktasi (alt cizgi) parlak, uca dogru karesel sonuk */
			const uint32_t f = 256U - (uint32_t)y * 256U / (uint32_t)(Hh + 2);
			const uint16_t S = (uint16_t)(8U + ((56U * f * f) >> 16));
			AddHue((int)(64U + c), H - 1 - y, (uint32_t)HuePhase + 200U + c * 4U, S);
			AddHue((int)(63U - c), H - 1 - y, (uint32_t)HuePhase + 200U + c * 4U, S);
		}
	}
}

/* ==========================================================================
   MOD -- FISKIYE
   ========================================================================== */
static void Spawn(float X, float Y, float Vy, uint16_t Hu)
{
	uint32_t i;

	for(i = 0; i < MAX_PARTICLES; i++)
	{
		Particle *p = &U.Parts[i];
		if(p->Life) continue;

		p->X = X; p->Y = Y;
		p->Vx = (RandF() - 0.5f) * 30.0f;
		p->Vy = Vy * (0.75f + 0.5f * RandF());
		p->Hue = Hu;
		p->Life = 255U;
		return;
	}
}

static void ModeFountain(void)
{
	const float    Gravity = 140.0f;
	const uint32_t Pitch = 3U;
	uint32_t k, i, Row, x;

	Fade(CFG_TRAIL);

	for(k = 0; k < Nb; k++)
	{
		uint32_t h  = (uint32_t)Bar[k];
		uint32_t Hu = (uint32_t)HuePhase + k * 768U / Nb;
		for(Row = 0; Row < h; Row++)
			for(x = 0; x < 2U; x++)
				AddHue((int)(1U + k * Pitch + x), (int)(H - 1U - Row), Hu, 40U);
	}

	/* Fiskirma sadece vurusta: aralarda sakin, ritim gorunur.
	   Kick = tum cubuklardan guclu puskurme + zeminden yaylim;
	   orta frekans vurusu = sadece yuksek cubuklardan kucuk puskurme. */
	{
		float S = 0.0f, Min = 1.0f;
		if(SPEC_Kick)                                 { S = SPEC_KickPower; Min = 0.20f; }
		else if(SPEC_Hit && SPEC_HitHigh < 0.55f)     { S = 0.6f * SPEC_HitPower; Min = 0.40f; }

		if(S > 0.0f)
		{
			for(k = 0; k < Nb; k++)
			{
				float    Cx = 1.0f + (float)(k * Pitch) + 1.0f;
				uint16_t Hu = (uint16_t)((uint32_t)HuePhase + k * 768U / Nb);
				if(Lvl[k] > Min)
					Spawn(Cx, (float)H - Bar[k], -(40.0f + 110.0f * Lvl[k] * S), Hu);
			}
			if(SPEC_Kick)
				for(i = 0; i < 16U; i++)
					Spawn(RandF() * (float)W, (float)H - 1.0f, -(60.0f + 60.0f * S),
					      (uint16_t)((uint32_t)HuePhase + (Rand() & 127U)));
		}
	}

	for(i = 0; i < MAX_PARTICLES; i++)
	{
		Particle *p = &U.Parts[i];
		if(!p->Life) continue;

		p->Vy += Gravity * DT;
		p->X  += p->Vx * DT;
		p->Y  += p->Vy * DT;
		p->Life = (p->Life > 4U) ? (uint8_t)(p->Life - 4U) : 0U;

		if(p->Y >= (float)H || p->X < 0.0f || p->X >= (float)W) { p->Life = 0; continue; }

		AddHue((int)p->X, (int)p->Y, p->Hue, (uint16_t)(p->Life + 1U));
		Add((int)p->X, (int)p->Y, p->Life >> 2, p->Life >> 2, p->Life >> 2);
	}
}

/* ==========================================================================
   MOD -- DALGA
   ========================================================================== */
static void ModeWave(void)
{
	int      x, y, Prev, Cur, Lo, Hi;
	uint16_t Bright = (uint16_t)(110.0f + 146.0f * SPEC_Volume);
	float    Amp    = 10.0f + 8.0f * SPEC_Volume + 5.0f * BeatEnv;

	Fade(CFG_TRAIL);								/* Zemin siyah: vurus parlamasi yok */

	Prev = H / 2 - (int)(SPEC_Wave[0] * Amp);

	for(x = 0; x < W; x++)
	{
		uint32_t Hu = (uint32_t)HuePhase + (uint32_t)x * 4U;

		Cur = H / 2 - (int)(SPEC_Wave[x] * Amp);
		Lo  = (Cur < Prev) ? Cur : Prev;
		Hi  = (Cur < Prev) ? Prev : Cur;

		for(y = Lo; y <= Hi; y++)
		{
			AddHue(x, y, Hu, Bright);
			if(SPEC_Volume > 0.45f) { AddHue(x, y - 1, Hu, Bright >> 2); AddHue(x, y + 1, Hu, Bright >> 2); }
		}

		AddHue(x, H / 2 + (int)(SPEC_Wave[x] * Amp * 0.6f), Hu + 384U, 60U);
		Prev = Cur;
	}
}

/* Sinus tablosu: 256 adim = 2*pi, genlik 127 (SPEC_Init'te doldurulur) */
static int8_t SinLut[256];

/* ==========================================================================
   Kucuk 3x5 rakamlar -- mod numarasi ve frekans
   ========================================================================== */
#if(CFG_SHOW_FREQ || CFG_SHOW_OVERRUN)
static const uint8_t Digit3x5[10][5] =
{
	{7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
	{7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};
#endif

#if(CFG_SHOW_FREQ)
static void DrawNumber(int X0, uint32_t Value, uint8_t R, uint8_t G, uint8_t B)
{
	char     Tmp[6];
	int      Len = 0, i, Row, Col;

	do { Tmp[Len++] = (char)('0' + Value % 10U); Value /= 10U; } while(Value && Len < 5);

	for(Row = 0; Row < 7; Row++)
		for(Col = 0; Col < Len * 4 + 1; Col++)
			Set(X0 - 1 + Col, Row, 0, 0, 0);

	for(i = 0; i < Len; i++)
		for(Row = 0; Row < 5; Row++)
			for(Col = 0; Col < 3; Col++)
				if(Digit3x5[Tmp[Len - 1 - i] - '0'][Row] & (4U >> Col))
					Set(X0 + i * 4 + Col, 1 + Row, R, G, B);
}
#endif

/* 5x7 rakamlar (mod numarasi) -- satir basina 5 bit, MSB solda */
static const uint8_t Digit5x7[10][7] =
{
	{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
	{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
	{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
	{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
	{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}
};

/* Saydam beyaz 5x7 sayi: arka plan silinmez, alttaki goruntuyle karisir */
static void DrawNumber57T(int X0, int Y0, uint32_t Value)
{
	char Tmp[6];
	int  Len = 0, i, Row, Col;

	do { Tmp[Len++] = (char)('0' + Value % 10U); Value /= 10U; } while(Value && Len < 5);

	for(i = 0; i < Len; i++)
		for(Row = 0; Row < 7; Row++)
			for(Col = 0; Col < 5; Col++)
				if(Digit5x7[Tmp[Len - 1 - i] - '0'][Row] & (0x10U >> Col))
				{
					int x = X0 + i * 6 + Col, y = Y0 + Row, k;
					if(x < 0 || x >= W || y < 0 || y >= H) continue;
					for(k = 0; k < 3; k++)
						CV(y, x)[k] = (uint8_t)(CV(y, x)[k] + (((255 - CV(y, x)[k]) * 150) >> 8));
				}
}

#if(CFG_SHOW_OVERRUN)
/* Saydam beyaz rakam: arka plan silinmez, alttaki goruntuyle %55 karisir */
static void DrawNumberT(int X0, int Y0, uint32_t Value)
{
	char     Tmp[10];
	int      Len = 0, i, Row, Col;

	do { Tmp[Len++] = (char)('0' + Value % 10U); Value /= 10U; } while(Value && Len < 9);

	for(i = 0; i < Len; i++)
		for(Row = 0; Row < 5; Row++)
			for(Col = 0; Col < 3; Col++)
				if(Digit3x5[Tmp[Len - 1 - i] - '0'][Row] & (4U >> Col))
				{
					int x = X0 + i * 4 + Col, y = Y0 + Row, k;
					if(x < 0 || x >= W) continue;
					for(k = 0; k < 3; k++)
						CV(y, x)[k] = (uint8_t)(CV(y, x)[k] + (((255 - CV(y, x)[k]) * 140) >> 8));
				}
}
#endif


/* ==========================================================================
   MOD -- RENKLI DALGA
   Eksene gore simetrik ince cubuklar (spektrum tepecikleri) + 4 sinus seridi.
   Renk soldan saga sabit gradyan: yesil > camgobegi > mavi > mor > pembe >
   kirmizi > turuncu > sari. Seritler 4 frekans bolgesini izler (bas, alt orta,
   ust orta, tiz); genlik bolgenin kendi ortalamasina gore olceklenir, serit
   duzlesince sonuklesir. Kontrast ayari parlaklik egrisini degistirir.
   Durum U.Swv icinde (ortak RAM).
   ========================================================================== */
#if(SWV_BANDS > MAXB) || (SWV_BANDS * 2 > W)
	#error "SWV_BANDS tuvale / bant dizisine sigmiyor"
#endif

/* Gradyan duraklari: { konum 0..255, R, G, B } -- referans resimden */
static const uint8_t SwvStops[9][4] =
{
	{   0,   0, 190, 110 },		/* koyu zumrut	*/
	{  43,   0, 235, 130 },		/* yesil		*/
	{  71,   0, 215, 205 },		/* camgobegi	*/
	{  97,  35, 115, 255 },		/* mavi			*/
	{ 122, 150, 105, 255 },		/* mor			*/
	{ 148, 255,  45, 170 },		/* pembe		*/
	{ 173, 255,  60,  75 },		/* kirmizi		*/
	{ 204, 255, 140,  20 },		/* turuncu		*/
	{ 255, 255, 210,  35 }		/* sari			*/
};

static void SwvBuildColors(void)
{
	uint32_t k, s, c;

	for(k = 0; k < SWV_BANDS; k++)
	{
		const uint32_t Pos = k * 255U / (SWV_BANDS - 1U);
		const uint8_t *a, *b;
		uint32_t Span, u;

		for(s = 1U; s < 8U && Pos > SwvStops[s][0]; s++) { }
		a = SwvStops[s - 1U];
		b = SwvStops[s];
		Span = (uint32_t)b[0] - a[0];
		u    = Pos - a[0];
		for(c = 0; c < 3U; c++)
			U.Swv.Col[k][c] = (uint8_t)(((uint32_t)a[1U + c] * (Span - u) + (uint32_t)b[1U + c] * u) / Span);
	}
	U.Swv.Ready = 1;
}

/* Renkli Dalga doluluk egrisi (sabit %150): zayif bolgeleri de aydinlatir */
#define SWV_DOLULUK		150U
static void SwvBuildLut(uint8_t Contrast)
{
	const float Inv = 100.0f / (float)Contrast;
	uint32_t i;

	for(i = 0; i < 256U; i++)
		U.Swv.Lut[i] = (uint8_t)(255.0f * powf((float)i / 255.0f, Inv) + 0.5f);
	U.Swv.LutC = Contrast;
}

static void SwvReset(void)					/* SPEC_SetMode, U sifirlandiktan sonra */
{
	uint32_t r;

	for(r = 0; r < 4U; r++)
	{
		U.Swv.Ph[r]  = 0.25f * (float)r;
		U.Swv.Avg[r] = 0.30f;
	}
	U.Swv.Freq = 1.0f;
}

static void ModeSpecWave(void)
{
	/* Seritler: tur sayisi (panel genisligi boyunca), aci hizi rad/s, genlik carpani,
	   parlaklik 0..256. Serit 0 = bas ... 3 = tiz */
	static const struct { float Cyc, Omega, Gain; uint16_t Lum; } Rib[4] =
	{
		{ 1.5f,  1.9f, 1.00f, 200U },
		{ 2.0f, -1.4f, 0.90f, 170U },
		{ 2.6f,  2.6f, 0.85f, 140U },
		{ 3.3f, -2.2f, 0.80f, 120U }
	};
	const uint32_t Qn = SPEC_BAND_COUNT / 4U;
	float    Hh[SWV_BANDS], Lev[4], Imp = 0.0f, Spd, FreqT;
	uint32_t k, d, x, r;
	uint16_t Gain;

	if(!U.Swv.Ready)					SwvBuildColors();
	if(U.Swv.LutC != SWV_DOLULUK)		SwvBuildLut(SWV_DOLULUK);

	/* Vurus zarfi: Tepki ayari SPEC_Draw'da Kick/Hit'i zaten suzdu */
	if(SPEC_Kick)		Imp = SPEC_KickPower;
	else if(SPEC_Hit)	Imp = 0.6f * SPEC_HitPower;
	U.Swv.Pump -= DT / CFG_BEAT_DECAY_S;
	if(U.Swv.Pump < 0.0f) U.Swv.Pump = 0.0f;
	if(Imp > U.Swv.Pump)  U.Swv.Pump = Imp;

	Gain = (uint16_t)(230.0f + 26.0f * U.Swv.Pump);			/* <= 256 */

	/* Yarim yukseklik: Bar[] 1-2-1 yumusatilir, %25 uzatilir -> panelin ust/alt kenarina ulasir */
	for(k = 0; k < SWV_BANDS; k++)
	{
		const float a = Bar[(k > 0U) ? k - 1U : 0U];
		const float c = Bar[(k + 1U < SWV_BANDS) ? k + 1U : k];
		float h = (0.25f * a + 0.5f * Bar[k] + 0.25f * c) * 0.5f * 1.25f;
		Hh[k] = (h < 1.0f) ? 1.0f : (h > 16.0f) ? 16.0f : h;
	}

	Clear();

	/* Cubuklar: eksenden uca, parlaklik karesel soner, Kontrast egrisinden gecer */
	for(k = 0; k < SWV_BANDS; k++)
	{
		const float    hh = Hh[k];
		const uint32_t n  = (uint32_t)(hh + 0.999f);
		const int      X  = (int)(k * 2U);

		for(d = 0; d < n && d < (H / 2U); d++)
		{
			const float    f  = 1.0f - (float)d / (hh + 4.0f);
			const uint32_t bi = (uint32_t)(255.0f * f * (0.35f + 0.65f * f));
			const uint32_t S  = ((uint32_t)Gain * U.Swv.Lut[bi]) >> 8;
			const uint8_t  R  = (uint8_t)((U.Swv.Col[k][0] * S) >> 8);
			const uint8_t  G  = (uint8_t)((U.Swv.Col[k][1] * S) >> 8);
			const uint8_t  B  = (uint8_t)((U.Swv.Col[k][2] * S) >> 8);

			Set(X, (int)(H / 2U) - 1 - (int)d, R, G, B);
			Set(X, (int)(H / 2U) + (int)d,     R, G, B);
		}
	}

	/* Bolge seviyeleri (bas / alt orta / ust orta / tiz) */
	for(r = 0; r < 4U; r++)
	{
		float Sum = 0.0f;
		for(k = r * Qn; k < (r + 1U) * Qn; k++) Sum += SPEC_Level[k];
		Lev[r] = Sum / (float)Qn;
	}

	Spd   = 0.7f + 1.8f * SPEC_Volume;						/* Yuksek ses = hizli */
	FreqT = 0.8f + 0.8f * SPEC_Treble;						/* Tiz cok = sik dalga */
	if(FreqT > 1.6f) FreqT = 1.6f;
	U.Swv.Freq += (FreqT - U.Swv.Freq) * 0.03f;

	for(r = 0; r < 4U; r++)
	{
		float    Rel, Lv, Tgt, Amp, Q;
		uint16_t Lum;
		int      yPrev = -1;

		/* Genlik: bolgenin kendi ortalamasina gore (vurgusuz muzikte de salinir) */
		U.Swv.Avg[r] += (Lev[r] - U.Swv.Avg[r]) * 0.004f;
		Rel = Lev[r] / (2.0f * U.Swv.Avg[r] + 0.05f);
		if(Rel > 1.0f) Rel = 1.0f;
		Lv  = 0.4f * ((Lev[r] > 1.0f) ? 1.0f : Lev[r]) + 0.6f * Rel;
		Tgt = 4.0f + 11.5f * Lv + 3.0f * U.Swv.Pump;
		if(Tgt > 15.5f) Tgt = 15.5f;
		U.Swv.RibAmp[r] += (Tgt - U.Swv.RibAmp[r]) * ((Tgt > U.Swv.RibAmp[r]) ? 0.35f : 0.12f);
		Amp = U.Swv.RibAmp[r] * Rib[r].Gain *
		      (0.88f + 0.12f * (float)SinLut[(uint32_t)((Time * 0.15f + 0.25f * (float)r) * 256.0f) & 255U] * (1.0f / 127.0f));

		/* Parlaklik genlige bagli: duz serit sonuk, salinan parlak */
		Q = (Amp - 3.0f) * (1.0f / 9.0f);
		if(Q < 0.0f) Q = 0.0f;
		if(Q > 1.0f) Q = 1.0f;
		Lum = (uint16_t)((float)Rib[r].Lum * (0.45f + 0.55f * Q) + 56.0f * U.Swv.Pump);
		if(Lum > 256U) Lum = 256U;

		U.Swv.Ph[r] += Rib[r].Omega * (1.0f / 6.2832f) * DT * Spd;
		if(Imp > 0.0f)										/* Vurusta faz sicrar */
			U.Swv.Ph[r] += ((Rib[r].Omega > 0.0f) ? 0.18f : -0.18f) * Imp;
		U.Swv.Ph[r] -= floorf(U.Swv.Ph[r]);

		for(x = 0; x < W; x++)
		{
			const float    Env = 0.30f + 0.70f * (float)SinLut[(x * 128U) / (W - 1U)] * (1.0f / 127.0f);
			const uint32_t Idx = (uint32_t)(int32_t)((U.Swv.Ph[r] + Rib[r].Cyc * U.Swv.Freq * (float)x / (float)W) * 256.0f) & 255U;
			const float    yc  = ((float)H - 1.0f) * 0.5f + Amp * Env * (float)SinLut[Idx] * (1.0f / 127.0f);
			const int      y   = (int)(yc + 0.5f);
			const uint8_t  R   = (uint8_t)((U.Swv.Col[x >> 1][0] * Lum) >> 8);
			const uint8_t  G   = (uint8_t)((U.Swv.Col[x >> 1][1] * Lum) >> 8);
			const uint8_t  B   = (uint8_t)((U.Swv.Col[x >> 1][2] * Lum) >> 8);
			int y0, y1, yy;

			if(yPrev < 0) yPrev = y;
			y0 = (y < yPrev) ? y : yPrev;					/* Dik kisimda bosluk kalmasin */
			y1 = (y < yPrev) ? yPrev : y;
			for(yy = y0; yy <= y1; yy++)
				Add((int)x, yy, R, G, B);
			yPrev = y;
		}
	}
}

/* ==========================================================================
   MOD -- ISIK CIZGISI
   Uzay boslugu: ortada ince bir isik cizgisi (renk: arayuzden sol/sag renk). Ses
   gelince bantlar eksenden iki yana dar, parlak tepeler olarak firlar, yavas
   sonen bir isima birakir. Arkada 3B yildiz alani: yildizlar ortadan dogar,
   yaklastikca hizlanip parlayarak disa acilir (ileri ucus). Vurus/bas ucusu
   hizlandirir, hizliyken yildizlar kisa iz birakir. Hiz ayari ucus hizini
   da etkiler. Durum U.Ll icinde (ortak RAM).
   ========================================================================== */
#define LL_STAR_LUM		170U		/* Yakindaki yildizin en yuksek parlakligi 0..255 */
#define LL_FOCAL		16.0f		/* Perspektif: Z = 1'de yildizin ortadan uzakligi, px */
#define LL_FLY			0.35f		/* Temel ucus hizi, derinlik/s (Hiz ayariyla carpilir) */
#define LL_FLY_BOOST	2.5f		/* Vurusta ek hiz carpani */

/* Cizgi rengi: orijinal kirmizi > turuncu > sari gecisinin ton kaymasi
   (+0/+16/+35/+56 derece, uc biraz acik) secilen renk ailesinden baslar.
   Kirmizi (0) = birebir orijinal. */
static const uint16_t LlBaseHue[8] = { 5U, 22U, 45U, 120U, 185U, 220U, 270U, 320U };
static const uint8_t  LlStops[4][4] =			/* { konum 0..255, ton kaymasi, doygunluk, parlaklik } */
{
	{   0,  0, 255, 255 },
	{  90, 16, 255, 255 },
	{ 180, 35, 255, 255 },
	{ 255, 56, 234, 230 }
};

static void LlHsv(uint32_t h, uint32_t s, uint32_t v, uint8_t *o)	/* h 0..359, s/v 0..255 */
{
	const uint32_t f = (h % 60U) * 255U / 60U;
	const uint8_t  p = (uint8_t)(v * (255U - s) / 255U);
	const uint8_t  q = (uint8_t)(v * (255U - s * f / 255U) / 255U);
	const uint8_t  t = (uint8_t)(v * (255U - s * (255U - f) / 255U) / 255U);
	switch((h / 60U) % 6U)
	{
	case 0:  o[0] = (uint8_t)v; o[1] = t; o[2] = p; break;
	case 1:  o[0] = q; o[1] = (uint8_t)v; o[2] = p; break;
	case 2:  o[0] = p; o[1] = (uint8_t)v; o[2] = t; break;
	case 3:  o[0] = p; o[1] = q; o[2] = (uint8_t)v; break;
	case 4:  o[0] = t; o[1] = p; o[2] = (uint8_t)v; break;
	default: o[0] = (uint8_t)v; o[1] = p; o[2] = q; break;
	}
}

static void LlBuildColors(void)
{
	const uint8_t  Sel = (SPEC_LineSel < 8U) ? SPEC_LineSel : 0U;
	uint32_t k, s;

	for(k = 0; k < 128U; k++)
	{
		const uint32_t Pos = k * 255U / 127U;
		const uint8_t *a, *b;
		uint32_t Span, u, dh, sa, va;
		for(s = 1U; s < 3U && Pos > LlStops[s][0]; s++) { }
		a = LlStops[s - 1U];
		b = LlStops[s];
		Span = (uint32_t)b[0] - a[0];
		u    = Pos - a[0];
		dh = (a[1] * (Span - u) + b[1] * u) / Span;
		sa = (a[2] * (Span - u) + b[2] * u) / Span;
		va = (a[3] * (Span - u) + b[3] * u) / Span;
		LlHsv((LlBaseHue[Sel] + dh) % 360U, sa, va, U.Ll.Col[k]);
	}
	U.Ll.ColFor = (uint8_t)(Sel + 1U);
}

/* --------------------------------------------------------------------------
   ORTAK YILDIZ ALANI (Isik Cizgisi, Orgu Serit)
   Yildizlar ortadan dogar, yaklastikca hizlanip parlayarak disa acilir.
   Ara sira (tiz vurusta daha sik) bir yildiz bir an mavimsi beyaz parlar,
   guclu parlamada arti seklinde isir, sonra solar.
   -------------------------------------------------------------------------- */
#define STAR_FLARE_PER_S	3.0f		/* Kendiliginden parlama sayisi / s			*/
#define STAR_FLARE_HIT		2U			/* Tiz vurusta ek parlayan yildiz			*/
#define STAR_FLARE_DECAY	0.90f		/* Kare basina sonme (0.90 ~ 0.25 s)		*/
#define STAR_FLARE_CROSS	150U		/* Bu parlamanin ustunde arti isimasi		*/
#define STAR_MIN_LUM		25U			/* Uzak yildizin taban parlakligi			*/
static const uint8_t StarFlareCol[3] = { 180U, 210U, 255U };	/* Mavimsi beyaz */

/* Yildizi uzak duzlemde yeniden dogur. X genis, Y dar: panel 4:1 */
static void StarSpawn(Starfield *S, uint32_t i, float Z)
{
	float x, y;
	do { x = 2.0f * RandF() - 1.0f; y = RandF() - 0.5f; }
	while(x * x + y * y < 0.01f);					/* Tam merkezde takili kalmasin */
	S->X[i]  = x;
	S->Y[i]  = y;
	S->Z[i]  = Z;
	S->Px[i] = -1;
	S->Fl[i] = 0;
}

static void StarInit(Starfield *S)
{
	uint32_t i;
	for(i = 0; i < LL_STARS; i++)
		StarSpawn(S, i, 0.1f + 0.9f * RandF());		/* Ilk karede derinlige yayilmis */
}

static void StarFlare(Starfield *S)					/* Yakin bir yildizi parlat */
{
	uint32_t i = Rand() % LL_STARS, n;
	for(n = 0; n < 8U; n++, i = (i + 7U) % LL_STARS)
		if(S->Fl[i] == 0U && S->Z[i] < 0.8f) { S->Fl[i] = 255U; return; }
}

/* Ilerlet + ciz. Pump: vurus zarfi (ucusu hizlandirir) */
static void StarDraw(Starfield *S, float Pump)
{
	const float Fly = LL_FLY * (1.0f + LL_FLY_BOOST * Pump) * DT;
	uint32_t i;

	if(RandF() < STAR_FLARE_PER_S / CFG_FPS) StarFlare(S);
	if(SPEC_Hit) for(i = 0; i < STAR_FLARE_HIT; i++) StarFlare(S);

	for(i = 0; i < LL_STARS; i++)
	{
		float    Z = S->Z[i] - Fly, q;
		int      sx, sy, px, py, j, m;
		uint32_t Lum, R, G, B, f;

		if(Z < 0.04f) { StarSpawn(S, i, 1.0f); continue; }
		S->Z[i] = Z;
		sx = (int)((float)W * 0.5f + S->X[i] * LL_FOCAL / Z);
		sy = (int)((float)H * 0.5f + S->Y[i] * LL_FOCAL / Z);
		if(sx < 0 || sx >= (int)W || sy < 0 || sy >= (int)H) { StarSpawn(S, i, 1.0f); continue; }

		q   = 1.0f - Z;									/* Yakinlik 0..1 */
		Lum = STAR_MIN_LUM + (uint32_t)((float)LL_STAR_LUM * q * q);
		f   = S->Fl[i];
		Lum += ((255U - Lum) * f) >> 8;					/* Parlama: tam parlakliga dogru */
		R = (Lum * (204U + (((StarFlareCol[0] - 204) * (int32_t)f) >> 8))) >> 8;
		G = (Lum * (224U + (((StarFlareCol[1] - 224) * (int32_t)f) >> 8))) >> 8;
		B = (Lum * (255U + (((StarFlareCol[2] - 255) * (int32_t)f) >> 8))) >> 8;
		px  = S->Px[i];
		py  = S->Py[i];

		/* Iz: onceki konumdan simdikine, geriye dogru sonerek (en fazla 6 px) */
		if(px >= 0)
		{
			int dx = sx - px, dy = sy - py;
			m = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
			if(m > 6) m = 6;
			for(j = 1; j <= m; j++)
			{
				const uint32_t k = (uint32_t)(m + 1 - j) * 256U / (uint32_t)(m + 2);
				Add(sx - dx * j / m, sy - dy * j / m, (uint8_t)((R * k) >> 8), (uint8_t)((G * k) >> 8), (uint8_t)((B * k) >> 8));
			}
		}
		Add(sx, sy, (uint8_t)R, (uint8_t)G, (uint8_t)B);
		if(f > STAR_FLARE_CROSS)						/* Guclu parlama: arti isima */
		{
			const uint8_t r2 = (uint8_t)(R >> 1), g2 = (uint8_t)(G >> 1), b2 = (uint8_t)(B >> 1);
			Add(sx - 1, sy, r2, g2, b2); Add(sx + 1, sy, r2, g2, b2);
			Add(sx, sy - 1, r2, g2, b2); Add(sx, sy + 1, r2, g2, b2);
		}
		S->Fl[i] = (uint8_t)((float)f * STAR_FLARE_DECAY);
		S->Px[i] = (int16_t)sx;
		S->Py[i] = (int16_t)sy;
	}
}

static void LlInit(void)
{
	StarInit(&U.Ll.St);
	U.Ll.Ready = 1;
}

static inline void LlPut(int x, int y, const uint8_t *c, uint32_t S)
{
	Add(x, y, (uint8_t)((c[0] * S) >> 8), (uint8_t)((c[1] * S) >> 8), (uint8_t)((c[2] * S) >> 8));
}

static void ModeLightLine(void)
{
	float    Imp = 0.0f;
	uint32_t k, d, n;
	uint16_t Base;

	if(!U.Ll.Ready)
		LlInit();

	/* Cizgi rengi: secim degisince bir kez kurulur */
	if(U.Ll.ColFor != (uint8_t)(SPEC_LineSel + 1U))
		LlBuildColors();

	/* Vurus zarfi: Tepki ayari SPEC_Draw'da Kick/Hit'i zaten suzdu */
	if(SPEC_Kick)		Imp = SPEC_KickPower;
	else if(SPEC_Hit)	Imp = 0.6f * SPEC_HitPower;
	U.Ll.Pump -= DT / CFG_BEAT_DECAY_S;
	if(U.Ll.Pump < 0.0f) U.Ll.Pump = 0.0f;
	if(Imp > U.Ll.Pump)  U.Ll.Pump = Imp;

	/* Yavas sonen isima yuksekligi */
	for(k = 0; k < 128U; k++)
	{
		U.Ll.Gl[k] = (Bar[k] > U.Ll.Gl[k]) ? Bar[k] : U.Ll.Gl[k] * 0.93f;
	}

	Clear();
	StarDraw(&U.Ll.St, U.Ll.Pump);				/* Ortak yildiz alani */

	/* Isik cizgisi + tepeler: soluk isima, ustune dar parlak cekirdek */
	Base = (uint16_t)(130.0f + 100.0f * U.Ll.Pump);
	for(k = 0; k < 128U; k++)
	{
		const float a  = U.Ll.Gl[(k > 0U) ? k - 1U : 0U];
		const float c  = U.Ll.Gl[(k < 127U) ? k + 1U : k];
		const float hg = (0.25f * a + 0.5f * U.Ll.Gl[k] + 0.25f * c) * 0.5f * 1.15f;
		const float hc = Bar[k] * 0.5f * 1.15f;
		const uint8_t *col = U.Ll.Col[k];

		n = (uint32_t)(hg + 0.999f);
		for(d = 0; d < n && d < (H / 2U); d++)
		{
			const float f = 1.0f - (float)d / (hg + 2.0f);
			const uint32_t S = (uint32_t)(110.0f * f * f);
			LlPut((int)k, (int)(H / 2U) - 1 - (int)d, col, S);
			LlPut((int)k, (int)(H / 2U) + (int)d,     col, S);
		}
		n = (uint32_t)(hc + 0.999f);
		for(d = 0; d < n && d < (H / 2U); d++)
		{
			const float f = 1.0f - (float)d / (hc + 2.0f);
			const uint32_t S = (uint32_t)(255.0f * f * (0.4f + 0.6f * f));
			LlPut((int)k, (int)(H / 2U) - 1 - (int)d, col, S);
			LlPut((int)k, (int)(H / 2U) + (int)d,     col, S);
		}
		LlPut((int)k, (int)(H / 2U) - 1, col, Base);
		LlPut((int)k, (int)(H / 2U),     col, Base);
	}
}

/* ==========================================================================
   YILDIRIM (Katmanli Dag)
   Yuksek bass (kick) vurusunda: kirikli ana kanal, 0..3 dal, geri vurusu
   (restrike) olan duzensiz titreme zarfi ve cizgi cevresinde yerel gokyuzu
   isimasi. Renk bass bolgesinden gelir. Daglardan ONCE cizilir (gokyuzu
   aydinlanir, daglar siluet kalir). Durum U.Ly.Lt / U.Ly.Bolt icinde (ortak RAM).
   Kaynak: Lightning.c / Lightning.h -- projeye ayrica eklenmez, buraya islendi.
   ========================================================================== */
#define LT_STEPS		13U
#define LT_BOLT_STEPS	8U			/* Bu adimdan sonra cizgi soner, isima surer	*/
#define LT_GLOW_R		56			/* Cizgi cevresindeki yerel isima yaricapi, px (buyuk = daha genis aydinlanma)	*/
/* Bas vurusu esigi (0..255): Vurus hassasiyeti 0 > 110, 50 > 70, 100 > 30; kucuk = daha cok cakar */
#define LT_BASS_THR		((uint32_t)(110U - (SPEC_Sens * 4U) / 5U))
#define LT_RISE			40U			/* Esik ustundeyken tekrar cakmak icin ani artis	*/
#define LT_RETRIG_MS	60U			/* Iki yildirim arasi en az sure; pespese vurusta yenisi eskinin ustune cakar	*/
#define LT_STEP_MS		30U			/* Parlaklik zarfinin adim suresi (titreme hizi)	*/
#define LT_AMB			115U			/* Cizgiden uzak yerlerin temel aydinlanmasi (0..255)	*/
#define LT_GLOW_GAIN	235U		/* Gokyuzu parlaklik carpani (0..255)			*/
#define LT_GLOW_WHITE	70U			/* Gokyuzu isiginin beyaza kaymasi (0..255)		*/
#define LT_BOLT_WHITE	200U		/* Yildirim cekirdeginin beyaza kaymasi (0..255)	*/

/* Parlaklik zarfi: guclu ilk vurus, sonra duzensiz titreme ve sonum.
   Ikinci tepe (235) gercek yildirimdaki geri vurusu taklit eder. */
static const uint8_t LtEnv[LT_STEPS] = { 255, 110, 235, 70, 170, 95, 60, 40, 26, 16, 9, 4, 0 };

static uint32_t LtRng = 2463534242U;

static uint32_t LtRnd(void)
{
	LtRng ^= LtRng << 13;
	LtRng ^= LtRng >> 17;
	LtRng ^= LtRng << 5;
	return LtRng;
}

static uint8_t LtClampX(int x)
{
	if(x < 2)				return 2U;
	if(x > LT_W - 3)		return (uint8_t)(LT_W - 3);
	return (uint8_t)x;
}

static uint8_t LtMix(uint8_t c, uint8_t white)
{
	return (uint8_t)(c + ((uint32_t)(255U - c) * white) / 255U);
}

#define LT_BR_GROW_MS	35U				/* Dallar ana kanaldan bu surede uzar (0 = aninda); ana kanal vurusta aninda	*/
#define LT_HIT_MS		140U			/* Carpma: dag sirti aydinlanma suresi			*/
#define LT_HIT_R		10				/* Carpma: aydinlanan sirt yari genisligi, px	*/

static int LyCrestN(uint32_t k);			/* Dag yuksekligi (satir), Katmanli Dag bolumunde */

/* Carpma noktasi + nadiren yatay desarj (uzak bulut ya da uzak tepe) */
static void LtExtras(void)
{
	int y, c;

	U.Ly.Lt.hOn = 0U;

	/* Carpma noktasi: ana kanalin dag tepesine ilk degdigi sutun */
	U.Ly.Lt.ix = U.Ly.Lt.bx[LT_H - 1];
	for(y = 0; y < LT_H; y++)
	{
		c = U.Ly.Lt.bx[y];
		if(y >= LT_H - LyCrestN((uint32_t)c)) { U.Ly.Lt.ix = (uint8_t)c; break; }
	}

	/* Nadir (~5te 1): ana kanaldan yatay yonde uzak bir noktaya kirikli ince dal */
	if((LtRnd() % 5U) == 0U)
	{
		const int yo = 2 + (int)(LtRnd() % 8U);
		const int x0 = U.Ly.Lt.bx[yo];
		int dir = (x0 < LT_W / 2) ? 1 : -1;
		int room, len, xt, yt, peak, bestTop, bi;
		uint32_t i;

		if((LtRnd() % 4U) == 0U) dir = -dir;					/* Bazen yakin tarafa */
		room = (dir > 0) ? (LT_W - 3 - x0) : (x0 - 2);
		len  = 40 + (int)(LtRnd() % 51U);						/* 40..90 px */
		if(len > room) len = room;

		if(len >= 16)
		{
			peak = (int)(LtRnd() & 1U);
			xt   = x0 + dir * (len - 1);
			yt   = yo + (int)(LtRnd() % 3U) - 1;
			if(peak)												/* Uzak tepe: o bolgedeki en yuksek dag */
			{
				bestTop = LT_H;  bi = xt;
				for(c = xt - 10; c <= xt + 10; c++)
				{
					const int cc  = (c < 2) ? 2 : (c > LT_W - 3) ? LT_W - 3 : c;
					const int top = LT_H - LyCrestN((uint32_t)cc);
					if(top < bestTop) { bestTop = top; bi = cc; }
				}
				if(bestTop < LT_H - 2 && (bi - x0) * dir >= 12 && (bi - x0) * dir + 1 < (int)LT_HLEN)
				{
					xt = bi;  yt = bestTop;  len = (xt - x0) * dir + 1;
				}
				else peak = 0;
			}
			if(yt < 0) yt = 0;
			if(yt > LT_H - 1) yt = LT_H - 1;

			/* Yol: 3..8 px'lik egik parcalar, parca basinda 1..4 satir kirilma.
			   Genelde hedefe (yt) cekilir, son parca tam hedefte biter. Y x16 sabit nokta. */
			{
				int yf = yo * 16, st = 0, n = 0, rem, cur, dy;
				for(i = 0U; i < (uint32_t)len; i++)
				{
					rem = len - 1 - (int)i;
					y   = (yf + 8) >> 4;
					if(y < 0) y = 0;
					if(y > LT_H - 1) y = LT_H - 1;
					U.Ly.Lt.hY[i] = (uint8_t)y;
					if(n == 0 && rem > 0)
					{
						cur = y;
						n   = 3 + (int)(LtRnd() % 6U);
						if(rem <= n + 2)									/* Son parca: hedefe */
						{
							n  = rem;
							dy = yt - cur;
							if(dy == 0 && rem > 6) { n = rem / 2; dy = (LtRnd() & 1U) ? 2 : -2; }	/* Duz bitmesin */
						}
						else
						{
							dy = 1 + (int)(LtRnd() % 4U);
							if((yt - cur > 4) || (cur - yt > 4))			/* Uzaklasti: yarisinda hedefe don */
								dy = ((LtRnd() & 1U) ? ((yt > cur) ? dy : -dy) : ((LtRnd() & 1U) ? dy : -dy));
							else if(LtRnd() & 1U) dy = -dy;
							if(cur + dy < 0 || cur + dy > LT_H - 1) dy = -dy;	/* Kenarda yon degistir */
						}
						yf = cur * 16;
						st = dy * 16 / n;
					}
					yf += st;
					n--;
				}
			}
			U.Ly.Lt.hY[len - 1] = (uint8_t)yt;
			U.Ly.Lt.hOn   = 1U;
			U.Ly.Lt.hX0   = (uint8_t)x0;
			U.Ly.Lt.hDir  = (int8_t)dir;
			U.Ly.Lt.hLen  = (uint8_t)len;
			U.Ly.Lt.hPeak = (uint8_t)peak;
		}
	}
}

/* Zikzak kanal: 2..5 satirlik duz parcalar, parca basinda sert kirilma.
   Br = 0: ana kanal (dik iner, iki yana kirilir); Br = 1: dal (Dir yonune acilir,
   ara sira geri kirilir). out[y0..y1] doldurulur. */
static void LtZig(uint8_t *out, int x, uint8_t y0, uint8_t y1, int Dir, uint8_t Br)
{
	static const int8_t MainS[8] = { -2, -1, -1, 0, 0, 1, 1, 2 };	/* Ana kanal egimleri, px/satir	*/
	static const int8_t BrS[8]   = {  3,  2,  2, 1, 1, 1, 0, -1 };	/* Dal egimleri (Dir yonunde)	*/
	int      sl = 0, prev = 0x7F, n = 0;
	uint32_t y, q;

	for(y = y0; y <= y1; y++)
	{
		out[y] = LtClampX(x);
		if(n == 0)											/* Yeni parca: egim degissin */
		{
			q  = LtRnd();
			sl = Br ? BrS[q & 7U] * Dir : MainS[q & 7U] + ((((q >> 3) & 3U) == 0U) ? Dir : 0);
			if(sl == prev) sl = (sl == 0) ? (((q >> 5) & 1U) ? 1 : -1) : 0;
			prev = sl;
			n    = (int)(2U + ((q >> 6) % (Br ? 3U : 4U)));
		}
		x = (int)LtClampX(x + sl);
		n--;
	}
}

static void LtStrike(uint8_t bass, uint32_t now, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t  y, k, y0, y1, len;
	int      x, bias, dx, xx, dir;

	U.Ly.Lt.active   = 1U;
	U.Ly.Lt.t0       = now;
	U.Ly.Lt.strength = (uint8_t)(120U + ((uint32_t)(bass - LT_BASS_THR) * 135U) / (255U - LT_BASS_THR));

	U.Ly.Lt.glowCol[0] = LtMix(r, LT_GLOW_WHITE);
	U.Ly.Lt.glowCol[1] = LtMix(g, LT_GLOW_WHITE);
	U.Ly.Lt.glowCol[2] = LtMix(b, LT_GLOW_WHITE);
	U.Ly.Lt.boltCol[0] = LtMix(r, LT_BOLT_WHITE);
	U.Ly.Lt.boltCol[1] = LtMix(g, LT_BOLT_WHITE);
	U.Ly.Lt.boltCol[2] = LtMix(b, LT_BOLT_WHITE);

	/* Ana kanal: yukaridan asagi zikzak, vurusa ozgu hafif yon egilimi */
	x    = 20 + (int)(LtRnd() % (LT_W - 40U));
	bias = (int)(LtRnd() % 3U) - 1;
	LtZig(U.Ly.Lt.bx, x, 0U, (uint8_t)(LT_H - 1), bias, 0U);

	/* Dallar: siddete gore 0..3, ana kanaldan yana dogru zikzakla iner */
	U.Ly.Lt.nBr = (U.Ly.Lt.strength > 235U) ? 3U : (U.Ly.Lt.strength > 190U) ? 2U :
	              (U.Ly.Lt.strength > 150U) ? 1U : 0U;
	for(k = 0U; k < U.Ly.Lt.nBr; k++)
	{
		y0  = (uint8_t)(4U + LtRnd() % 14U);
		dir = (LtRnd() & 1U) ? 1 : -1;
		len = (uint8_t)(6U + LtRnd() % 7U);
		y1  = (uint8_t)(y0 + len);
		if(y1 > LT_H - 1) y1 = (uint8_t)(LT_H - 1);
		LtZig(U.Ly.Lt.brX[k], U.Ly.Lt.bx[y0], y0, y1, dir, 1U);
		U.Ly.Lt.brY0[k] = y0;
		U.Ly.Lt.brY1[k] = y1;
	}

	LtExtras();

	/* Yerel isima haritasi: cizgiye (ve yatay dala) yakin sutunlar daha parlak */
	memset(U.Ly.Lt.Tmp, 0, sizeof(U.Ly.Lt.Tmp));
	for(y = 0U; y < LT_H; y++)
	{
		for(dx = -LT_GLOW_R; dx <= LT_GLOW_R; dx++)
		{
			xx = (int)U.Ly.Lt.bx[y] + dx;
			if(xx < 0 || xx >= LT_W) continue;
			U.Ly.Lt.Tmp[xx] = (uint16_t)(U.Ly.Lt.Tmp[xx] + (LT_GLOW_R + 1 - (dx < 0 ? -dx : dx)));
		}
	}
	if(U.Ly.Lt.hOn)
		for(k = 0U; k < U.Ly.Lt.hLen; k += 3U)
		{
			const int cx = (int)U.Ly.Lt.hX0 + (int)U.Ly.Lt.hDir * (int)k;
			for(dx = -LT_GLOW_R; dx <= LT_GLOW_R; dx++)
			{
				xx = cx + dx;
				if(xx < 0 || xx >= LT_W) continue;
				U.Ly.Lt.Tmp[xx] = (uint16_t)(U.Ly.Lt.Tmp[xx] + (LT_GLOW_R + 1 - (dx < 0 ? -dx : dx)));
			}
		}
	for(x = 0; x < LT_W; x++)
	{
		/* Normalle: dikey cizgide cizgi dibinde ~255, uzaklastikca dogrusal azalir */
		const uint32_t v = (uint32_t)U.Ly.Lt.Tmp[x] * 330U / (LT_H * (LT_GLOW_R + 1U));
		U.Ly.Lt.glow[x] = (v > 255U) ? 255U : (uint8_t)v;
	}
}

/* Tetikleme: bass esigi yukari keserse (veya esik ustunde ani sicrarsa) */
static void LtUpdate(uint8_t bass, uint32_t nowMs, uint8_t r, uint8_t g, uint8_t b)
{
	const uint8_t prev = U.Ly.Lt.prevBass;
	const uint8_t rise = (bass > prev) ? (uint8_t)(bass - prev) : 0U;

	U.Ly.Lt.prevBass = bass;
	LtRng ^= nowMs;										/* Her vurus farkli olsun */
	if(LtRng == 0U) LtRng = 1U;

	if((uint32_t)(nowMs - U.Ly.Lt.t0) < LT_RETRIG_MS)		return;
	if(bass < LT_BASS_THR)									return;
	if(prev >= LT_BASS_THR && rise < LT_RISE)				return;

	LtStrike(bass, nowMs, r, g, b);
}

static void LtMaxSet(uint8_t y, uint8_t x, uint8_t v)
{
	if(U.Ly.Bolt[y][x] < v) U.Ly.Bolt[y][x] = v;
}

/* Bir satirda xa..xb arasini doldurur (satirlar arasi kopukluk olmasin) */
static void LtSeg(uint8_t y, uint8_t xa, uint8_t xb, uint8_t core, uint8_t halo)
{
	const uint8_t lo = (xa < xb) ? xa : xb;
	const uint8_t hi = (xa < xb) ? xb : xa;
	uint8_t x;

	for(x = lo; x <= hi; x++) LtMaxSet(y, x, core);
	if(lo > 0U)			LtMaxSet(y, (uint8_t)(lo - 1U), halo);
	if(hi < LT_W - 1)	LtMaxSet(y, (uint8_t)(hi + 1U), halo);
}

/* Cizim: arka plan temizlendikten sonra, daglardan once cagrilir.
   Yildirim aktif degilse hicbir sey yapmaz (sifir maliyet). */
static void LtRender(uint32_t nowMs)
{
	uint32_t step, e, I, eb, vert, L, bm, r, g, b;
	uint8_t  drawBolt, k, ya, xb;
	int      x, y;

	if(!U.Ly.Lt.active) return;

	step = (nowMs - U.Ly.Lt.t0) / LT_STEP_MS;
	if(step >= LT_STEPS - 1U)
	{
		U.Ly.Lt.active  = 0U;
		U.Ly.Lt.lastEnd = nowMs;
		return;
	}

	/* Zarf + her karede hafif titreme (0.85..1.0) */
	e  = LtEnv[step];
	e  = (e * (216U + (LtRnd() % 40U))) / 255U;
	I  = (e * U.Ly.Lt.strength) / 255U;
	eb = (I * 5U) / 4U;
	if(eb > 255U) eb = 255U;

	drawBolt = (step < LT_BOLT_STEPS) ? 1U : 0U;

	if(drawBolt)
	{
		memset(U.Ly.Bolt, 0, sizeof(U.Ly.Bolt));

		for(ya = 0U; ya < LT_H; ya++)					/* Ana kanal */
		{
			xb = (ya < LT_H - 1) ? U.Ly.Lt.bx[ya + 1U] : U.Ly.Lt.bx[ya];
			LtSeg(ya, U.Ly.Lt.bx[ya], xb, 255U, 95U);
		}
		for(k = 0U; k < U.Ly.Lt.nBr; k++)				/* Dallar: uca dogru incelir, ilk karelerde uzar */
		{
			const uint8_t  y0  = U.Ly.Lt.brY0[k], y1 = U.Ly.Lt.brY1[k];
			const uint32_t age = nowMs - U.Ly.Lt.t0;
			uint8_t        ye  = y1;
			if(age < LT_BR_GROW_MS)
				ye = (uint8_t)(y0 + 1U + ((uint32_t)(y1 - y0 - 1U) * age) / LT_BR_GROW_MS);
			for(ya = y0; ya < ye; ya++)
			{
				const uint8_t core = (uint8_t)(40U + (110U * (uint32_t)(y1 - ya)) / (uint32_t)(y1 - y0));
				LtSeg(ya, U.Ly.Lt.brX[k][ya], U.Ly.Lt.brX[k][ya + 1U], core, (uint8_t)((core * 35U) / 100U));
			}
		}
		if(U.Ly.Lt.hOn)										/* Nadir yatay desarj: ince, uca dogru sonuk */
		{
			for(ya = 0U; (uint8_t)(ya + 1U) < U.Ly.Lt.hLen; ya++)
			{
				const uint8_t hx   = (uint8_t)((int)U.Ly.Lt.hX0 + (int)U.Ly.Lt.hDir * (int)ya);
				const uint8_t core = (uint8_t)(120U - (60U * ya) / U.Ly.Lt.hLen);
				uint8_t ya0 = U.Ly.Lt.hY[ya], ya1 = U.Ly.Lt.hY[ya + 1U], yy;
				if(ya0 > ya1) { yy = ya0; ya0 = ya1; ya1 = yy; }
				for(yy = ya0; yy <= ya1; yy++) LtMaxSet(yy, hx, core);
			}
			if(!U.Ly.Lt.hPeak)								/* Bulutta biten dal: ucta yumusak parlama */
			{
				const int tx = (int)U.Ly.Lt.hX0 + (int)U.Ly.Lt.hDir * ((int)U.Ly.Lt.hLen - 1);
				const int ty = U.Ly.Lt.hY[U.Ly.Lt.hLen - 1U];
				LtMaxSet((uint8_t)ty, (uint8_t)tx, 100U);
				if(tx > 0)				LtMaxSet((uint8_t)ty, (uint8_t)(tx - 1), 50U);
				if(tx < LT_W - 1)		LtMaxSet((uint8_t)ty, (uint8_t)(tx + 1), 50U);
				if(ty > 0)				LtMaxSet((uint8_t)(ty - 1), (uint8_t)tx, 50U);
				if(ty < LT_H - 1)		LtMaxSet((uint8_t)(ty + 1), (uint8_t)tx, 50U);
			}
		}
	}

	/* Sutun kazanci: temel aydinlanma + cizgiye yakinlik */
	for(x = 0; x < LT_W; x++)
	{
		const uint32_t gx = LT_AMB + ((255U - LT_AMB) * U.Ly.Lt.glow[x]) / 255U;
		U.Ly.Lt.Gain[x] = (uint8_t)((I * gx) >> 8);
	}

	for(y = 0; y < LT_H; y++)
	{
		vert = 255U - ((uint32_t)y * 110U) / LT_H;			/* Ustte daha parlak (bulutlar) */

		for(x = 0; x < LT_W; x++)
		{
			L = ((uint32_t)U.Ly.Lt.Gain[x] * vert) >> 8;
			L = (L * LT_GLOW_GAIN) >> 8;

			r = (L * U.Ly.Lt.glowCol[0]) >> 8;
			g = (L * U.Ly.Lt.glowCol[1]) >> 8;
			b = (L * U.Ly.Lt.glowCol[2]) >> 8;

			if(drawBolt && U.Ly.Bolt[y][x])
			{
				bm = ((uint32_t)U.Ly.Bolt[y][x] * eb) >> 8;
				r += (bm * U.Ly.Lt.boltCol[0]) >> 8;
				g += (bm * U.Ly.Lt.boltCol[1]) >> 8;
				b += (bm * U.Ly.Lt.boltCol[2]) >> 8;
			}

			if((r | g | b) != 0U)
				Add(x, y, (uint8_t)((r > 255U) ? 255U : r), (uint8_t)((g > 255U) ? 255U : g), (uint8_t)((b > 255U) ? 255U : b));
		}
	}
}

/* Sirt aydinlanmasi: (x0) cevresinde on dagin GUNCEL tepe cizgisi beyaza yakin
   parlar, yanlara ve zamanla soner. Amp: 256 = tam. */
static void LtRidge(int x0, int R, uint32_t Amp)
{
	int x;
	for(x = x0 - R; x <= x0 + R; x++)
	{
		const int d = (x < x0) ? x0 - x : x - x0;
		const int n = (x >= 0 && x < LT_W) ? LyCrestN((uint32_t)x) : 0;
		uint32_t  w;
		if(n <= 0) continue;
		w = Amp * (uint32_t)(R + 1 - d) / (uint32_t)(R + 1);
		w = (w * w) >> 8;										/* Ortada dar, yumusak kenar */
		Add(x, LT_H - n, (uint8_t)((255U * w) >> 8), (uint8_t)((245U * w) >> 8), (uint8_t)((230U * w) >> 8));
		if(n > 1)
			Add(x, LT_H - n + 1, (uint8_t)((255U * w) >> 9), (uint8_t)((245U * w) >> 9), (uint8_t)((230U * w) >> 9));
	}
}

/* Carpma: yildirimin dustugu sirt (ve uzak tepedeki yatay desarj ucu) bir an aydinlanir */
static void LtImpact(uint32_t nowMs)
{
	const uint32_t age = nowMs - U.Ly.Lt.t0;
	uint32_t f;

	if(age >= LT_HIT_MS) return;
	f = (LT_HIT_MS - age) * 256U / LT_HIT_MS;
	f = (f * f) >> 8;										/* Hizli dusus, yumusak kuyruk */
	LtRidge(U.Ly.Lt.ix, LT_HIT_R, f * U.Ly.Lt.strength / 255U);
	if(U.Ly.Lt.hOn && U.Ly.Lt.hPeak)
		LtRidge((int)U.Ly.Lt.hX0 + (int)U.Ly.Lt.hDir * ((int)U.Ly.Lt.hLen - 1), LT_HIT_R / 2, f / 2U);
}

/* ==========================================================================
   MOD -- KATMANLI DAG
   Uc ust uste dag silueti: onde o anki spektrum (dar, parlak), ortada ve
   arkada onun yavas sonen golgesi (arka daha genis ve daha yavas). Alttan dolgu soluk,
   tepe cizgisi parlak; renk yuksekligin fonksiyonu (guclu tepe camgobegine
   cikar). Katmanlar toplamali cizilir, kesistikleri yerde renkler karisir.
   Vurusta yukseklik ve parlaklik artar. Durum U.Ly icinde (ortak RAM).
   ========================================================================== */
/* Katmanli Dag / Orgu Serit: 8 sabit renk ailesi (Python arayuzunden secilir),
   her biri 4 durak (koyu > acik). Settings.c'deki secenek metniyle ayni sirada. */
uint8_t SPEC_Pal = 0;

static const uint8_t PalFam[8][4][3] =
{
	{ { 150,  40, 255 }, {  40,  80, 255 }, {   0, 170, 255 }, {   0, 230, 200 } },	/* 0 Okyanus		*/
	{ { 200,   0,  20 }, { 255,  60,   0 }, { 255, 140,   0 }, { 255, 220,  40 } },	/* 1 Ates			*/
	{ {   0, 120,  60 }, {   0, 200,  80 }, { 120, 230,  40 }, { 230, 240,  60 } },	/* 2 Orman			*/
	{ { 120,  30, 200 }, { 255,  40, 150 }, { 255, 110,  40 }, { 255, 200,  60 } },	/* 3 Gun batimi		*/
	{ {  20,  40, 200 }, {  30, 120, 255 }, {  80, 200, 255 }, { 100, 230, 255 } },	/* 4 Buz			*/
	{ { 255,   0, 120 }, { 255, 160,   0 }, {   0, 220,  90 }, {   0, 160, 255 } },	/* 5 Gokkusagi		*/
	{ { 100,  20, 160 }, { 200,  20, 160 }, { 255,  60, 150 }, { 255, 130, 190 } },	/* 6 Pembe			*/
	{ { 255, 110,  10 }, { 255, 210,  40 }, {  60, 230, 110 }, {  40, 210, 160 } }	/* 7 Turuncu-Yesil	*/
};

/* Ailenin 4 duragi arasinda t (0..768) konumundaki renk */
static void PalColor(uint8_t Fam, uint32_t t, uint8_t *c)
{
	const uint8_t (*p)[3] = PalFam[(Fam < 8U) ? Fam : 0U];
	uint32_t seg, f, i;

	if(t > 768U) t = 768U;
	seg = t / 256U;
	if(seg > 2U) seg = 2U;
	f = t - seg * 256U;
	for(i = 0; i < 3U; i++)
		c[i] = (uint8_t)((((uint32_t)p[seg][i] * (256U - f)) + ((uint32_t)p[seg + 1U][i] * f)) >> 8);
}

static const uint32_t LyFill[3] = { 70U, 90U, 110U };	/* Dolgu parlakligi 0..255 */

/* Bir sutun: alttan h piksel, dikeyde renk gecisi, en ustteki piksel parlak */
static void LyColumn(int x, float h, const uint8_t Pal[2][3], uint32_t Fill, uint32_t Gain)
{
	const int n = (int)(h + 0.5f);
	int i;

	for(i = 0; i < n && i < (int)H; i++)
	{
		uint32_t t = (uint32_t)i * 256U / 28U;					/* Mutlak yukseklik: 0..256 */
		const uint32_t S = (i == n - 1) ? 255U : Fill;			/* Tepe cizgisi parlak */
		uint32_t R, G, B;

		if(t > 256U) t = 256U;
		R = ((uint32_t)Pal[0][0] * (256U - t) + (uint32_t)Pal[1][0] * t) >> 8;
		G = ((uint32_t)Pal[0][1] * (256U - t) + (uint32_t)Pal[1][1] * t) >> 8;
		B = ((uint32_t)Pal[0][2] * (256U - t) + (uint32_t)Pal[1][2] * t) >> 8;
		Add(x, (int)H - 1 - i, (uint8_t)((R * S * Gain) >> 16), (uint8_t)((G * S * Gain) >> 16), (uint8_t)((B * S * Gain) >> 16));
	}
}

/* On katmanin yuksekligi (1-2-1 yumusatilmis spektrum) */
static inline float LyFrontH(uint32_t k)
{
	const float a = Bar[(k > 0U) ? k - 1U : 0U];
	const float c = Bar[(k < 127U) ? k + 1U : k];
	return (0.25f * a + 0.5f * Bar[k] + 0.25f * c) * 0.95f;
}

/* Bir sutunda dagin (en yuksek katman) kapladigi satir sayisi */
static int LyCrestN(uint32_t k)
{
	float hm = U.Ly.S[k];

	if(U.Ly.M[k] > hm)		hm = U.Ly.M[k];
	if(LyFrontH(k) > hm)	hm = LyFrontH(k);
	return (hm > (float)LT_H) ? LT_H : (int)(hm + 0.5f);
}

static void ModeLayers(void)
{
	float    Imp = 0.0f, Boost;
	uint32_t k, Gain;
	int      j;
	uint8_t  Pal[3][2][3];

	/* Vurus zarfi: Tepki ayari SPEC_Draw'da Kick/Hit'i zaten suzdu */
	if(SPEC_Kick)		Imp = SPEC_KickPower;
	else if(SPEC_Hit)	Imp = 0.6f * SPEC_HitPower;
	U.Ly.Pump -= DT / CFG_BEAT_DECAY_S;
	if(U.Ly.Pump < 0.0f) U.Ly.Pump = 0.0f;
	if(Imp > U.Ly.Pump)  U.Ly.Pump = Imp;
	Boost = 1.0f + 0.12f * U.Ly.Pump;
	for(k = 0; k < 3U; k++)											/* Katman renkleri: ailenin 4 duragi */
	{
		PalColor(SPEC_Pal, k * 256U, Pal[k][0]);
		PalColor(SPEC_Pal, (k + 1U) * 256U, Pal[k][1]);
	}
	Gain  = (uint32_t)(230.0f + 26.0f * U.Ly.Pump);			/* <= 256 */

	/* Orta (5 nokta) ve arka (9 nokta) katman: yatay yumusatma + HAYALET: yukselis aninda,
	   dusus yavas. Onun ustunde kalan kisim ayri bir golge siluet olarak gorunur. */
	for(k = 0; k < 128U; k++)
	{
		float tm = 0.0f, ts = 0.0f;
		const float Fr = Bar[k] * 0.95f;							/* On katmanin yuksekligi (yaklasik) */

		for(j = -4; j <= 4; j++)
		{
			int q = (int)k + j;
			if(q < 0)   q = 0;
			if(q > 127) q = 127;
			ts += Bar[q];
			if(j >= -2 && j <= 2) tm += Bar[q];
		}
		tm *= 0.2f;
		ts *= (1.0f / 9.0f);
		if(tm < Fr) tm = Fr;										/* Hayalet onden alcak olmaz */
		if(ts < Fr) ts = Fr;
		U.Ly.M[k] = (tm > U.Ly.M[k] * 0.955f) ? tm : U.Ly.M[k] * 0.955f;	/* ~0.16 sn'de yariya */
		U.Ly.S[k] = (ts > U.Ly.S[k] * 0.975f) ? ts : U.Ly.S[k] * 0.975f;	/* ~0.3 sn'de yariya */
	}

	Clear();

	/* Yildirim: renk bass bolgesinin (sol sutunlar) on katman tepe rengi; gokyuzunu
	   aydinlatir, daglardan once cizilir */
	{
		const uint32_t Now = HAL_GetTick();
		float    bh = 0.0f;
		uint32_t ti, r, g, b;
		uint8_t  bass = 0U;

		for(k = 0; k < 16U; k++)
			if(Bar[k] > bh) bh = Bar[k];
		ti = (uint32_t)(bh * 0.95f * (256.0f / 28.0f));
		if(ti > 256U) ti = 256U;
		r = ((uint32_t)Pal[2][0][0] * (256U - ti) + (uint32_t)Pal[2][1][0] * ti) >> 8;
		g = ((uint32_t)Pal[2][0][1] * (256U - ti) + (uint32_t)Pal[2][1][1] * ti) >> 8;
		b = ((uint32_t)Pal[2][0][2] * (256U - ti) + (uint32_t)Pal[2][1][2] * ti) >> 8;
		if(SPEC_Kick)
			bass = (uint8_t)(255.0f * ((SPEC_KickPower > 1.0f) ? 1.0f : SPEC_KickPower));

		LtUpdate(bass, Now, (uint8_t)r, (uint8_t)g, (uint8_t)b);
		LtRender(Now);
	}

	/* Yildirim aktifken daglarin arkasini karart: gokyuzu parlak, daglar koyu siluet */
	if(U.Ly.Lt.active)
	{
		for(k = 0; k < 128U; k++)
		{
			float hm = U.Ly.S[k];
			int   n, i;

			if(U.Ly.M[k] > hm)			hm = U.Ly.M[k];
			if(LyFrontH(k) > hm)		hm = LyFrontH(k);
			n = (int)(hm * Boost + 0.5f);
			for(i = 0; i < n && i < (int)H; i++)
				Set((int)k, (int)H - 1 - i, 0, 0, 0);
		}
	}

	/* Arkadan one: arka, orta, on */
	for(k = 0; k < 128U; k++)
		LyColumn((int)k, U.Ly.S[k] * Boost, Pal[0], LyFill[0], Gain);
	for(k = 0; k < 128U; k++)
		LyColumn((int)k, U.Ly.M[k] * Boost, Pal[1], LyFill[1], Gain);
	for(k = 0; k < 128U; k++)
		LyColumn((int)k, LyFrontH(k) * Boost, Pal[2], LyFill[2], Gain);

	/* Carpma: dag cizildikten sonra, sirtin ustune */
	LtImpact(HAL_GetTick());
}

/* ==========================================================================
   MOD -- AURORA
   Karanlik, yildizli gokte kutup isigi perdesi. Perdenin alt kenari ve isin
   dokusu fraktal (3 kat sinus) ile yavasca kivrilir. Her sutunun isin boyu ve
   parlakligi o sutunun bandi (bas solda). Renk dikey: dipte yesil, uca dogru
   camgobegi > mor. Bas vurusu perde boyunca kosan bir dalga, tiz vurus isinlarda
   kisa titreme. Guclu tiz / ana vurusta seyrek kayan yildiz. Durum U.Au icinde.
   ========================================================================== */
#define AU_METEOR_MS	6000U			/* Iki kayan yildiz arasi en az sure			*/
#define AU_METEOR_PCT	30U				/* Uygun vurusta cikma ihtimali, %			*/
#define AU_PAL_MS		30000U			/* Renk takimi suresi						*/
#define AU_FADE_MS		4000U			/* Takimlar arasi yumusak gecis				*/

/* Aurora renk takimlari: (indeks 0..63, R, G, B) duraklari. 34 = ana renk, 63 = uc parlak */
static const uint8_t AuSets[4][6][4] =
{
	{ { 0,  3,  6, 28 }, { 20,   0, 120,  80 }, { 34,  40, 255, 130 }, { 46,  30, 200, 230 }, { 56, 150,  60, 230 }, { 63, 255,  90, 200 } },	/* Yesil - mor		*/
	{ { 0,  8,  2, 20 }, { 20,  90,   0,  40 }, { 34, 255,  40,  60 }, { 46, 220,  40, 140 }, { 56, 150,  40, 230 }, { 63, 255, 140, 200 } },	/* Kizil - mor		*/
	{ { 0,  2,  6, 30 }, { 20,   0,  60, 140 }, { 34,  30, 170, 255 }, { 46,  40, 240, 230 }, { 56, 120, 120, 255 }, { 63, 220, 240, 255 } },	/* Mavi - camgobegi	*/
	{ { 0, 10,  4, 24 }, { 20, 120,  20,  90 }, { 34, 255,  80, 170 }, { 46, 255, 150, 120 }, { 56, 255, 210,  80 }, { 63, 255, 240, 190 } },	/* Pembe - altin	*/
};

static void AuSetColor(uint32_t Set, uint32_t i, uint32_t *c)
{
	uint32_t s = 0, k;
	while(s < 4U && i > AuSets[Set][s + 1][0]) s++;
	{
		const uint32_t a = AuSets[Set][s][0], b = AuSets[Set][s + 1][0], t = ((i - a) * 256U) / (b - a);
		for(k = 0; k < 3U; k++) c[k] = (AuSets[Set][s][k + 1] * (256U - t) + AuSets[Set][s + 1][k + 1] * t) >> 8;
	}
}

/* Paleti zamana gore kur: her takim AU_PAL_MS surer, sonunda bir sonrakine yumusak gecer */
static void AuPalette(uint8_t Pal[64][3])
{
	const uint32_t Now = HAL_GetTick(), Ph = Now % AU_PAL_MS;
	const uint32_t A = (Now / AU_PAL_MS) % 4U, B = (A + 1U) % 4U;
	const uint32_t f = (Ph > AU_PAL_MS - AU_FADE_MS) ? ((Ph - (AU_PAL_MS - AU_FADE_MS)) * 256U) / AU_FADE_MS : 0U;
	uint32_t i, k, ca[3], cb[3];

	for(i = 0; i < 64U; i++)
	{
		AuSetColor(A, i, ca);
		AuSetColor(B, i, cb);
		for(k = 0; k < 3U; k++) Pal[i][k] = (uint8_t)((ca[k] * (256U - f) + cb[k] * f) >> 8);
	}
}

static inline float AuSin(float Turn)		/* sin(2*pi*Turn), SinLut ile */
{
	return (float)SinLut[(uint8_t)(int32_t)(Turn * 256.0f)] * (1.0f / 127.0f);
}

/* Kayan yildiz: guclu tiz ya da guclu ana vurusta, seyrek. Aci 20..55 derece, yon
   rastgele; hafif hizlanir, kuyruk once uzar, sonunda bas bir an parlayip soner.
   Gectigi yerde alttaki aurora hafifce aydinlanir. */
static void MeteorStep(Meteor *M)
{
	const uint32_t Now = HAL_GetTick();
	float k, Hb, Ln, ux, uy, Sp;
	int   j, dx, dy;

	if(!M->On && (Now - M->Last) > AU_METEOR_MS &&
	   ((SPEC_Hit && SPEC_HitHigh > 0.4f && SPEC_HitPower > 0.6f) || (SPEC_KickMain && SPEC_KickPower > 0.8f)))
	{
		M->Last = Now;										/* Sans tutmasa da bekleme yeniden baslar */
		if((Rand() % 100U) < AU_METEOR_PCT)
		{
			const float a  = (20.0f + 35.0f * RandF()) / 360.0f;	/* Tur */
			const float d  = (Rand() & 1U) ? 1.0f : -1.0f;
			M->P   = SPEC_Hit ? SPEC_HitPower : SPEC_KickPower;
			Sp     = 70.0f + 40.0f * M->P;
			M->Vx  = d * Sp * AuSin(a + 0.25f);
			M->Vy  = Sp * AuSin(a);
			M->X   = (d > 0.0f) ? (float)(Rand() % 80U) : (float)(48U + Rand() % 80U);
			M->Y   = (float)(Rand() % 8U);
			M->Lm  = 6.0f + 8.0f * M->P;
			M->Dur = 0.55f + 0.3f * RandF();
			M->Age = 0.0f;
			M->On  = 1;
		}
	}
	if(!M->On) return;

	M->Age += DT;
	if(M->Age >= M->Dur || M->X < -M->Lm || M->X >= (float)W + M->Lm || M->Y >= (float)H) { M->On = 0; return; }
	M->X  += M->Vx * DT;
	M->Y  += M->Vy * DT;
	M->Vx *= 1.0f + 0.8f * DT;							/* Hafif hizlanma */
	M->Vy *= 1.0f + 0.8f * DT;

	k  = M->Age / M->Dur;								/* 0..1 */
	Ln = M->Lm * ((k < 0.35f) ? k / 0.35f : 1.0f);		/* Kuyruk uzar */
	Hb = (0.6f + 0.4f * M->P) * ((k < 0.8f) ? 1.0f : (k < 0.9f) ? 1.0f + 15.0f * (k - 0.8f)	/* Sonda parlar	*/
	                                            : 25.0f * (1.0f - k));							/* ve soner		*/
	Sp = sqrtf(M->Vx * M->Vx + M->Vy * M->Vy);
	ux = M->Vx / Sp;
	uy = M->Vy / Sp;

	/* Alttaki auroraya hafif isik: basin cevresinde 4 px */
	for(dy = -3; dy <= 3; dy++)
		for(dx = -4; dx <= 4; dx++)
		{
			const int x = (int)M->X + dx, y = (int)M->Y + dy;
			const float r = sqrtf((float)(dx * dx) + (float)(dy * dy) * 2.0f);
			if(x < 0 || x >= (int)W || y < 0 || y >= (int)H || r >= 4.0f) continue;
			{
				uint8_t *p = CV(y, x);
				const uint32_t m = 256U + (uint32_t)(150.0f * Hb * (1.0f - r / 4.0f));
				uint32_t c, v;
				for(c = 0; c < 3U; c++) { v = (p[c] * m) >> 8; p[c] = (uint8_t)((v > 255U) ? 255U : v); }
			}
		}

	for(j = 0; j < (int)Ln; j++)
	{
		const float    q = 1.0f - (float)j / Ln;
		const float    b = (j == 0) ? Hb : (0.6f + 0.4f * M->P) * q * q;
		const uint32_t L = (b > 1.0f) ? 255U : (uint32_t)(255.0f * b);
		Add((int)(M->X - ux * (float)j), (int)(M->Y - uy * (float)j),
		    (uint8_t)((L * 220U) >> 8), (uint8_t)((L * 235U) >> 8), (uint8_t)L);
	}
	if(Hb > 1.2f)										/* Son parlama: arti */
	{
		const uint8_t L = (uint8_t)((Hb > 2.0f) ? 200U : (uint32_t)(100.0f * Hb));
		Add((int)M->X - 1, (int)M->Y, L, L, L); Add((int)M->X + 1, (int)M->Y, L, L, L);
		Add((int)M->X, (int)M->Y - 1, L, L, L); Add((int)M->X, (int)M->Y + 1, L, L, L);
	}
}

/* Aurora gogu: kuzeye bakan sabit gorunum (40 derece enlem, gercek konumlardan
   hesaplandi): Buyuk Ayi batida, Kutup Yildizi ortada, Kassiopeia doguda +
   sonuk yildizlar. Hareket yok; parildama ve renk anlari var. x'e gore sirali:
   { x, y, parlaklik, tur }  tur: 0 beyaz, 1 ara sira mavimsi, 2 ara sira turuncu
   + hizli parildama, 3 sicak beyaz. Perdenin alt kenari ufuktur: altinda yildiz
   yok, ufka yaklastikca soner. Flash'ta. */
static const uint8_t AuSky[74][4] =
{
	{  1, 4, 34,0}, {  3,15, 60,0}, { 10, 3, 99,1}, { 11,17, 60,1}, { 12, 2, 60,0}, { 13,14, 60,0}, { 14,29,
	40,0}, { 15, 6, 60,0}, { 15,18, 40,0}, { 16, 1, 48,0}, { 18, 5, 82,0}, { 19,24, 40,2}, { 21, 9,104,0}, { 23,
	8, 34,0}, { 23,17, 74,0}, { 24,11, 40,0}, { 25,13, 47,0}, { 26, 0, 40,0}, { 30,16, 34,0}, { 30,18, 30,0}, {
	30,22, 76,0}, { 31, 7, 48,1}, { 34, 9, 34,0}, { 35,18,102,2}, { 38,17, 22,0}, { 38,29, 34,0}, { 42,24, 30,0},
	{ 46, 3, 40,0}, { 46, 8, 30,0}, { 46,22, 60,2}, { 48,11, 22,0}, { 52,15, 48,0}, { 54, 1, 22,0}, { 59, 4,
	22,0}, { 59,21, 26,0}, { 61, 2, 40,0}, { 63,12, 34,1}, { 65, 4, 93,3}, { 71,22, 34,0}, { 71,28, 26,0}, { 73,
	4, 48,0}, { 73,19, 22,0}, { 76,16, 34,0}, { 79,20, 40,0}, { 80,10, 48,2}, { 82, 4, 34,0}, { 87,19, 22,0}, {
	87,23, 34,0}, { 88, 0, 34,0}, { 89,19, 34,0}, { 90, 5, 40,0}, { 91,21, 34,1}, { 92, 9, 26,0}, { 92,17, 46,0},
	{ 94,19, 40,0}, { 97,16, 65,0}, { 98,28, 48,0}, { 99,13, 72,1}, {101, 1, 26,0}, {102,12, 22,0}, {103, 8,
	80,0}, {104,13, 81,2}, {108, 1, 60,2}, {109,24, 30,0}, {110,27, 40,0}, {111,13, 22,0}, {112, 5, 22,0}, {114,
	9, 48,0}, {114,12, 40,0}, {116, 2, 60,1}, {119,18, 34,0}, {120,29, 89,2}, {123,20, 34,0}, {127, 2, 26,0}
};

/* Renk ani: her yildizda 20 s'de bir; 0.5 s renge gecer, 1.5 s tutar, 0.5 s doner */
static inline float AuTint(float T, float Ph)
{
	float c = T * 0.05f + Ph;
	c -= floorf(c);
	return (c < 0.025f) ? c * 40.0f : (c < 0.1f) ? 1.0f : (c < 0.125f) ? (0.125f - c) * 40.0f : 0.0f;
}

/* Bir yildiz: ufuk (yb) uzakligina gore sonerek */
static void AuStar(uint32_t i, int yb, float T)
{
	const int   y  = AuSky[i][1];
	const float ph = (float)i * 0.53f;
	float L = (float)AuSky[i][2] * (1.0f / 255.0f), tn, r = 245.0f, g = 245.0f, b = 255.0f;

	if(y >= yb - 1) return;									/* Ufkun alti */
	if(y > yb - 6) L *= (float)(yb - 1 - y) * 0.2f;			/* Ufka yakin: soluk */
	switch(AuSky[i][3])
	{
		case 1:												/* Ara sira mavimsi */
			tn = AuTint(T, ph);
			L *= (0.8f + 0.3f * RandF()) * (1.0f + 0.4f * tn);
			r = 245.0f - 135.0f * tn;  g = 245.0f - 85.0f * tn;
			break;
		case 2:												/* Hizli parildama, ara sira turuncu */
			tn = AuTint(T, ph);
			L *= (0.6f + 0.5f * RandF()) * (1.0f + 0.4f * tn);
			r = 255.0f;  g = 240.0f - 100.0f * tn;  b = 225.0f - 185.0f * tn;
			break;
		case 3:  r = 255.0f; g = 245.0f; b = 225.0f; L *= 0.9f + 0.1f * AuSin(T * 0.4f + ph); break;
		default: L *= 0.9f + 0.1f * AuSin(T * 0.4f + ph); break;
	}
	if(L > 1.0f) L = 1.0f;
	Add(AuSky[i][0], y, (uint8_t)(r * L), (uint8_t)(g * L), (uint8_t)(b * L));
}

static void ModeAurora(void)
{
	const float Ts = (float)HAL_GetTick() * 0.001f;		/* Parildama gercek zamanda */
	float    Imp = 0.0f;
	uint32_t x, si = 0;
	int      j;

	/* Vurus: bas > kosan dalga, tiz > isin titremesi */
	if(SPEC_Kick)
	{
		Imp = SPEC_KickPower;
		if(Imp > U.Au.Wa) { U.Au.Wa = Imp; U.Au.Wx = (float)(Rand() % W); U.Au.Wd = (Rand() & 1U) ? 1.0f : -1.0f; }
	}
	if(SPEC_Hit && SPEC_HitHigh > 0.3f && SPEC_HitPower > U.Au.Fl) U.Au.Fl = SPEC_HitPower;
	U.Au.Wx += U.Au.Wd * 90.0f * DT;
	U.Au.Wa *= 0.94f;
	U.Au.Fl *= 0.85f;
	U.Au.T  += DT;

	AuPalette(U.Au.Pal);
	Clear();

	/* Perde */
	for(x = 0; x < W; x++)
	{
		const float fx = (float)x / (float)W, T = U.Au.T;
		float  Fr, Yb, Hh, Ry, Dw, Lv;
		int    yb;

		Lv = Bar[x];
		U.Au.Gl[x] = (Lv > U.Au.Gl[x]) ? U.Au.Gl[x] + (Lv - U.Au.Gl[x]) * 0.5f : U.Au.Gl[x] * 0.93f;

		/* Fraktal alt kenar (3 kat) */
		Fr = AuSin(fx * 1.3f + T * 0.05f) + 0.5f * AuSin(fx * 3.1f - T * 0.09f) + 0.25f * AuSin(fx * 7.3f + T * 0.17f);
		Dw = (float)x - U.Au.Wx;
		Dw = (Dw < 0.0f) ? -Dw : Dw;
		Dw = (Dw < 14.0f) ? U.Au.Wa * (1.0f - Dw / 14.0f) : 0.0f;	/* Kosan dalga */
		Yb = 27.0f + 3.0f * Fr - 4.0f * Dw;
		Hh = 6.0f + U.Au.Gl[x] * 1.1f + 3.0f * Dw;				/* Isin boyu: bant */

		/* Isin dokusu: suzulen dikey seritler (+ tiz titremesi) */
		Ry = 0.55f + 0.45f * AuSin(fx * 11.0f + T * 0.23f + 0.5f * AuSin(fx * 29.0f - T * 0.11f));
		if(U.Au.Fl > 0.05f) Ry *= 1.0f - 0.5f * U.Au.Fl * RandF() + 0.4f * U.Au.Fl;
		Ry *= 0.35f + 0.65f * ((U.Au.Gl[x] > (float)H * 0.6f) ? 1.0f : U.Au.Gl[x] / ((float)H * 0.6f)) + 0.3f * Dw;

		yb = (int)(Yb + 0.5f);
		for(; si < 74U && AuSky[si][0] == x; si++) AuStar(si, yb, Ts);	/* Bu sutunun yildizlari */
		for(j = -1; j < (int)Hh; j++)
		{
			const int   y = yb - j;
			const float t = (j < 0) ? 0.0f : (float)j / Hh;		/* 0 dip > 1 uc */
			float       I = Ry * (1.0f - t) * sqrtf(1.0f - t) * ((j < 0) ? 0.35f : 1.0f);	/* Yavas soner */
			uint32_t    r, g, b, L;
			if(y < 0 || y >= (int)H) continue;
			if(I > 1.0f) I = 1.0f;
			L = (uint32_t)(255.0f * I);
			{ const uint8_t *c = U.Au.Pal[34U + (uint32_t)(t * 29.0f)]; r = c[0]; g = c[1]; b = c[2]; }	/* Dip ana renk > uc */
			Add((int)x, y, (uint8_t)((r * L) >> 8), (uint8_t)((g * L) >> 8), (uint8_t)((b * L) >> 8));
		}
	}

	MeteorStep(&U.Au.Me);						/* Perdenin ustunde: isigi perdeye vurur */
}

/* ==========================================================================
   ORTAM MODLARI -- ortak saat (Kar, Somine, Yagmur, Sahil)
   Rakamlar secilen fonttan 1 bit/piksel maskeye cizilir; mod bu maskeyi
   kendi tarzinda boyar (Kar: soluk golge, Somine: odun/koz). Yerlesim hazir
   duzenlerden secilir; sicaklik / nem CMD_SENSOR ile gelir (5x7 kucuk yazi).
   ========================================================================== */
uint8_t SPEC_SnIsik = 60U, SPEC_SnYogun = 1U;
uint8_t SPEC_SaatYazi = 0U, SPEC_SaatYer = 1U;
uint8_t SPEC_SmBoy = 1U;
uint8_t SPEC_YmSiddet = 1U, SPEC_YmRuzgar = 1U;
uint8_t SPEC_ShDalga = 1U, SPEC_ShIsik = 0U;
uint8_t  SPEC_SaatAlt = 0U, SPEC_SaatIsi = 0U, SPEC_SaatSn = 0U;							/* Saat: alta, isi-nem goster	*/
uint8_t  SPEC_SaatSaydam = 0U;									/* Saat saydamligi % (0 = kati)	*/
uint32_t SPEC_SaatRgb = 0x0A0D13UL;						/* Saat rengi 0xRRGGBB (Settings: renk x parlaklik)	*/

/* Saat fontlari: Fatih'in fontlari geldikce buraya eklenir (Settings.c SAAT_YAZI
   secenek listesi ve Max ile ayni sirada). Bicim: FONT_INFO, 0-9.		*/
static const FONT_INFO *const SaatYazilari[] = { &arialbold24pxFontInfo, &bahncond30FontInfo, &bahnbd30FontInfo };	/* Kucukten buyuge */
#define SAAT_YAZI_SAYISI	(sizeof(SaatYazilari) / sizeof(SaatYazilari[0]))

uint8_t SPEC_SaatYaziSayisi(void) { return (uint8_t)SAAT_YAZI_SAYISI; }

#define SAAT_ARA		2					/* Rakam ve iki nokta arasi (px)	*/


static inline uint8_t SaatPx(const SaatDurum *S, int x, int y)
{
	return (x >= 0 && x < (int)W && y >= 0 && y < (int)H) ? (uint8_t)(S->M[y][x >> 3] & (0x80U >> (x & 7))) : 0U;
}

/* Karakterin tanimlayici sirasi (fontun baslangic karakterine gore: '0' ya da '%' ...) */
static uint8_t SaatGlif(const FONT_INFO *F, char c) { return (uint8_t)(c - F->StartChar); }
static int SaatRakamGen(const FONT_INFO *F, uint8_t g) { return (int)F->Descriptors[g].Width; }

/* Rakam yuvasi: en genis rakam kadar (oransal fontta dakika degisince saat kaymasin) */
static int SaatYuva(const FONT_INFO *F)
{
	int r, g = 0;
	for(r = 0; r < 10; r++) if(SaatRakamGen(F, SaatGlif(F, (char)('0' + r))) > g) g = SaatRakamGen(F, SaatGlif(F, (char)('0' + r)));
	return g;
}

/* Glifin gercekten yanan ilk ve son satiri */
static void SaatSatirlar(const FONT_INFO *F, uint8_t g, int *Ust, int *Alt)
{
	const FONT_CHAR_INFO *d = &F->Descriptors[g];
	const uint8_t *b = &F->Bitmap[d->OffSet];
	int y, k;
	*Ust = (int)d->Height; *Alt = -1;
	for(y = 0; y < (int)d->Height; y++)
		for(k = 0; k < (int)d->BWidth; k++)
			if(b[y * d->BWidth + k]) { if(y < *Ust) *Ust = y; *Alt = y; break; }
	if(*Alt < 0) { *Ust = 0; *Alt = (int)d->Height - 1; }
}

static void SaatRakamMaske(SaatDurum *S, const FONT_INFO *F, int X0, int Y0, uint8_t r)
{
	const FONT_CHAR_INFO *d = &F->Descriptors[r];
	const uint8_t *b = &F->Bitmap[d->OffSet];
	int x, y;

	for(y = 0; y < (int)d->Height; y++)
		for(x = 0; x < (int)d->Width; x++)
			if((b[y * d->BWidth + (x >> 3)] & (0x80U >> (x & 7))) &&
			   X0 + x >= 0 && X0 + x < (int)W && Y0 + y >= 0 && Y0 + y < (int)H)
				S->M[Y0 + y][(X0 + x) >> 3] |= (uint8_t)(0x80U >> ((X0 + x) & 7));
}

/* Dakika, font ya da yerlesim degistiyse maskeyi ve konumlari yeniden kurar.
   Iki nokta fontun kendi ':' glifidir (her fontta tanimli).
   Yerlesim: Sol / Orta (yatay), Alt (dikey), Isi-Nem (saat solda, en sagda
   sicaklik ve nem; sensor verisi yoksa uygulanmaz, saat Sol/Orta'ya gore durur). */
static void SaatHazirla(SaatDurum *S)
{
	const uint8_t Yazi = (SPEC_SaatYazi < SAAT_YAZI_SAYISI) ? SPEC_SaatYazi : 0U;
	const FONT_INFO *F = SaatYazilari[Yazi];
	const FONT_INFO *K = &arialbd13FontInfo;					/* Sigmayan saniye icin kucuk font */
	const int fh = (int)F->Height;
	const uint8_t Ik = SaatGlif(F, ':');
	const int Yv = SaatYuva(F), Wc = SaatRakamGen(F, Ik);
	int16_t T; uint8_t N;
	const uint8_t Isi = (uint8_t)(SPEC_SaatIsi && SAAT_SensorOku(&T, &N));
	uint8_t Sani = SPEC_SaatSn;
	uint8_t Sa, Dk, Sn, r[6], Buyuk = 0;
	int tw, x0, y0, yi, i, x, n = 4;

	SAAT_Get(&Sa, &Dk, &Sn);
	if(!Sani) Sn = 0;											/* Saniye kapaliysa dakikada bir kurulur */
	if(S->Hazir && S->Sa == Sa && S->Dk == Dk && S->Sn == Sn && S->Yazi == Yazi && S->Yer == SPEC_SaatYer &&
	   S->Alt == SPEC_SaatAlt && S->Isi == Isi && S->Sani == Sani) return;
	S->Sa = Sa; S->Dk = Dk; S->Sn = Sn; S->Yazi = Yazi; S->Yer = SPEC_SaatYer; S->Alt = SPEC_SaatAlt;
	S->Isi = Isi; S->Sani = Sani; S->Hazir = 1U;
	memset(S->M, 0, sizeof(S->M));

	r[0] = (uint8_t)(Sa / 10U); r[1] = (uint8_t)(Sa % 10U); r[2] = (uint8_t)(Dk / 10U); r[3] = (uint8_t)(Dk % 10U);
	r[4] = (uint8_t)(Sn / 10U); r[5] = (uint8_t)(Sn % 10U);
	tw = 4 * Yv + Wc + 4 * SAAT_ARA;
	if(Sani)
	{
		const int Yer = (int)W - 1 - (Isi ? 35 : 0);				/* Isi-nem yazisina pay */
		const int Buy = 6 * Yv + 2 * Wc + 7 * SAAT_ARA;
		const int Kuc = tw + SAAT_ARA + SaatRakamGen(K, SaatGlif(K, (char)('0' + r[4]))) + 1 + SaatRakamGen(K, SaatGlif(K, (char)('0' + r[5])));
		if(Buy <= Yer)      { tw = Buy; Buyuk = 1U; n = 6; }		/* Saniye ayni buyuk fontla */
		else if(Kuc <= Yer) tw = Kuc;								/* Kucuk saniye */
		else                Sani = 0;								/* Hic yer yok (genis font + isi-nem): saniye yok */
	}

	x0 = Isi ? 0 : (S->Yer == 0U) ? 1 : ((int)W - tw) / 2;			/* Isi-Nem: yer dar, en sola */
	y0 = S->Alt ? (int)H - fh - 1 : ((int)H - fh) / 2;
	if(y0 < 0) y0 = 0;
	S->Sag = (int16_t)(x0 + tw);
	S->Ix2 = -1;
	S->SnVar = Sani;

	{																/* Iki nokta rakamlarin dikey ortasinda (fontta asagida olabilir) */
		int du, da, iu, ia;
		SaatSatirlar(F, SaatGlif(F, '0'), &du, &da);
		SaatSatirlar(F, Ik, &iu, &ia);
		yi = y0 + ((du + da) - (iu + ia)) / 2;
	}
	x = x0;
	for(i = 0; i < n; i++)
	{
		const uint8_t g = SaatGlif(F, (char)('0' + r[i]));
		SaatRakamMaske(S, F, x + (Yv - SaatRakamGen(F, g)) / 2, y0, g);	/* Yuvada ortali */
		x += Yv + SAAT_ARA;
		if(i == 1 || (i == 3 && Buyuk))
		{
			SaatRakamMaske(S, F, x, yi, Ik);
			if(i == 1) { S->Ix = (int16_t)x; S->Iw = (int16_t)Wc; } else S->Ix2 = (int16_t)x;
			x += Wc + SAAT_ARA;
		}
	}
	if(Sani && !Buyuk)											/* Kucuk saniye: dakikanin sagina, alt hizada */
	{
		const int yk = y0 + fh - (int)K->Height;
		for(i = 4; i < 6; i++)
		{
			const uint8_t g = SaatGlif(K, (char)('0' + r[i]));
			SaatRakamMaske(S, K, x, yk, g);
			x += SaatRakamGen(K, g) + 1;
		}
	}
	if(Isi || S->Yer == 0U)										/* Sola yasli: ilk yanan piksel panelden 2 px iceride */
	{
		int mx = (int)W, kay, yy, xx;
		for(yy = 0; yy < (int)H; yy++)
			for(xx = 0; xx < mx; xx++)
				if(SaatPx(S, xx, yy)) { mx = xx; break; }
		kay = 2 - mx;
		if(mx < (int)W && kay)
		{
			uint8_t M2[H][W / 8];
			memset(M2, 0, sizeof(M2));
			for(yy = 0; yy < (int)H; yy++)
				for(xx = 0; xx < (int)W; xx++)
					if(SaatPx(S, xx, yy) && xx + kay >= 0 && xx + kay < (int)W)
						M2[yy][(xx + kay) >> 3] |= (uint8_t)(0x80U >> ((xx + kay) & 7));
			memcpy(S->M, M2, sizeof(M2));
			S->Ix = (int16_t)(S->Ix + kay); S->Sag = (int16_t)(S->Sag + kay);
			if(S->Ix2 >= 0) S->Ix2 = (int16_t)(S->Ix2 + kay);
		}
	}
}

/* Saat pikseli. Saydamlik 0: kati. Saydamlikta renkli cam: arka plan saat renginin
   tonundan gecer gibi boyanir (iki renk ust uste binip yeni renk olur), kati renk azalir. */
static void SaatSet(int x, int y, uint8_t R, uint8_t G, uint8_t B)
{
	const uint32_t s = (uint32_t)SPEC_SaatSaydam * 256U / 100U;	/* 0..205 */
	uint32_t m, k, r, g, b;
	uint8_t *p;
	if(!s) { Set(x, y, R, G, B); return; }
	if((unsigned)x >= W || (unsigned)y >= H) return;
	p = CV(y, x);
	m = R; if(G > m) m = G; if(B > m) m = B;
	if(!m) m = 1;
	k = (s * 2U > 256U) ? 256U : s * 2U;							/* Camin gecirgenligi */
	r = (R * (256U - s) >> 8) + ((p[0] * R / m) * k >> 8);
	g = (G * (256U - s) >> 8) + ((p[1] * G / m) * k >> 8);
	b = (B * (256U - s) >> 8) + ((p[2] * B / m) * k >> 8);
	p[0] = (uint8_t)((r > 255U) ? 255U : r);
	p[1] = (uint8_t)((g > 255U) ? 255U : g);
	p[2] = (uint8_t)((b > 255U) ? 255U : b);
}

/* Iki nokta: saniye gosterilirken sabit, yoksa saniyede bir yanip soner */
static uint8_t SaatNoktaYanik(const SaatDurum *S) { return (uint8_t)(S->SnVar || (HAL_GetTick() % 1000U) < 500U); }

static uint8_t SaatNokta(const SaatDurum *S, int x)
{
	return (uint8_t)((x >= S->Ix && x < S->Ix + S->Iw) || (S->Ix2 >= 0 && x >= S->Ix2 && x < S->Ix2 + S->Iw));
}

/* Oransal font ile yazi (1 px aralik); Ciz 0 ise yalnizca genisligi dondurur */
static int FontYazi(const FONT_INFO *F, int X, int Y, const char *s, uint8_t R, uint8_t G, uint8_t B, uint8_t Ciz)
{
	int w = 0;
	for(; *s; s++)
	{
		if(*s == '\x01')											/* Derece halkasi 3x3 */
		{
			static const uint8_t Hl[3] = { 2, 5, 2 };
			int a, c;
			if(Ciz) for(a = 0; a < 3; a++) for(c = 0; c < 3; c++) if(Hl[a] & (4U >> c)) Set(X + w + c, Y + 1 + a, R, G, B);
			w += 4; continue;
		}
		if(*s == '-')												/* Eksi: fontlarda yok, kisa cizgi */
		{
			int c;
			if(Ciz) for(c = 0; c < 3; c++) Set(X + w + c, Y + (int)F->Height / 2, R, G, B);
			w += 4; continue;
		}
		if(*s < (char)F->StartChar || *s > (char)F->EndChar) continue;
		{
			const FONT_CHAR_INFO *d = &F->Descriptors[(uint8_t)(*s - F->StartChar)];
			const uint8_t *b = &F->Bitmap[d->OffSet];
			int x, y;
			if(Ciz)
				for(y = 0; y < (int)d->Height; y++)
					for(x = 0; x < (int)d->Width; x++)
						if(b[y * d->BWidth + (x >> 3)] & (0x80U >> (x & 7))) Set(X + w + x, Y + y, R, G, B);
			w += (int)d->Width + 1;
		}
	}
	return (w > 0) ? w - 1 : 0;
}

/* Sicaklik ve nem: en sagda. Nem varsa iki satir (Arial 13), yalniz sicaklik
   varsa tek satir, daha buyuk (Bahnschrift 16). Saate yer kalmazsa 'C' dusurulur. */
static void SaatIsiCiz(const SaatDurum *S, uint8_t R, uint8_t G, uint8_t B)
{
	char t[10], n[6];
	int16_t T; uint8_t N;
	int v, k = 0;
	const FONT_INFO *F;

	if(!S->Isi || !SAAT_SensorOku(&T, &N)) return;
	v = (T >= 0) ? (T + 5) / 10 : (T - 5) / 10;
	if(v < 0) { t[k++] = '-'; v = -v; }
	if(v >= 10) t[k++] = (char)('0' + v / 10 % 10);
	t[k++] = (char)('0' + v % 10); t[k++] = '\x01'; t[k++] = 'C'; t[k] = 0;

	F = (N <= 100U) ? &arialbd13FontInfo : &bahnbd16FontInfo;
	if((int)W - FontYazi(F, 0, 0, t, 0, 0, 0, 0) < S->Sag + 2) t[--k] = 0;	/* Sigmazsa 'C' yok */
	if(N <= 100U)
	{
		n[0] = '%'; n[1] = (char)('0' + N / 10 % 10); n[2] = (char)('0' + N % 10); n[3] = 0;	/* Turkce: %45 */
		FontYazi(F, (int)W - FontYazi(F, 0, 0, t, 0, 0, 0, 0), 2, t, R, G, B, 1);
		FontYazi(F, (int)W - FontYazi(F, 0, 0, n, 0, 0, 0, 0), 17, n, R, G, B, 1);
	}
	else FontYazi(F, (int)W - FontYazi(F, 0, 0, t, 0, 0, 0, 0), ((int)H - (int)F->Height) / 2, t, R, G, B, 1);
}

/* Ortam modlarinin ortak dokusu: 64 adimli yumusak dalga, 0..255 (Kar, Yagmur, Sahil) */
static const uint8_t ShDoku[64] =
{
	128,140,153,165,177,188,199,209,218,226,234,240,245,250,253,254,255,254,253,250,245,240,234,226,218,209,199,188,177,165,153,140,
	128,115,102, 90, 78, 67, 56, 46, 37, 29, 21, 15, 10,  5,  2,  1,  0,  1,  2,  5, 10, 15, 21, 29, 37, 46, 56, 67, 78, 90,102,115
};

/* Kenar / doku kaymasi: 16 px'de bir kontrol noktasi, yumusak gecis (duz cizgi olmasin) */
static void OrtamKayma(uint8_t *Of, float Gen)
{
	float c[W / 16U + 2U];
	uint32_t k;
	int x;
	for(k = 0; k < W / 16U + 2U; k++) c[k] = RandF() * Gen;
	for(x = 0; x < (int)W; x++)
	{
		const float u = (float)(x & 15) / 16.0f, sm = u * u * (3.0f - 2.0f * u);
		Of[x] = (uint8_t)(c[x >> 4] + (c[(x >> 4) + 1] - c[x >> 4]) * sm);
	}
}

/* ==========================================================================
   MOD -- KAR (Ortam, muziksiz)
   Gece pencereden kar: siyah gok, ara sira parlayan yildizlar, arada yay
   cizerek inen kayan yildiz. Uc katman, derinlik hiz ve parlaklik farkindan:
     uzak : tek sonuk piksel, cok yavas, dumduz; havada rastgele yukseklikte
            sonup kaybolur (hepsi ayni cizgiye "dusmez"), saatin arkasinda
     orta : 3x3 yumusak tane, orta hiz, yana en cok ~1.5 px/s
     yakin: 3x3 yumusak tane, hizli, saatin onunde, yana en cok ~3 px/s
   Yerde biriken kar: mod acilinca bos; orta ve yakin taneler dustukleri yere
   eklenir, 3-4 dakikada 12-13 px'e kadar yukselir ve orada dengelenir. Ust
   kenari tumsek ve cukurlu (komsusundan cok yukselen sutun yana kayar),
   ici doku ve derinlik golgeli, asla parlak degil; saatin arkasinda kalir.
   Salinim (gidip gelme) yok: tane bir yone kayar, birkac saniyede bir yeni
   yon secip oraya yavasca doner (bazen sola, bazen saga, bazen duz). 3x3
   taneler birbirinin dibinde dogmaz, yaklasinca birbirinden uzaklasir.
   Taneler donmez. Konum kesirli tutulur (dusus Ac/Kd, yana Kx) ve tane
   ara piksel ile cizilir: isik komsu 2x2 piksele, parlaklik (karesi) korunacak
   sekilde bolusturulur; yavas tane bir satirdan otekine akar, adim adim
   ziplamaz. Durum U.Sn icinde.
   ========================================================================== */
#define SN_TEPE		120U					/* Kar Isigi %100'de en parlak tane	*/

/* Desen: '#' orta (tam), '+' %30, '-' %18 */
static const char *const SnYumusak[3] = { "-+-", "+#+", "-+-" };

/* Ara piksel agirliklari: tanenin kesirli konumu (Fx, Fy: 0..256, bir sonraki
   piksele dogru) -> 2x2 komsuya karekok agirlik (0..256). Panel isigi
   algiya gore (CIE) cevirdigi icin isigin karesi korunur: tane iki satirin
   arasindayken sonuklesmez, butun yolda ayni parlaklikta gorunur. */
typedef struct { uint16_t A, B, C, D; } SnAg;			/* (x,y) (x+1,y) (x,y+1) (x+1,y+1) */

static SnAg SnAgirlik(uint32_t Fx, uint32_t Fy)
{
	const float fx = (float)Fx / 256.0f, fy = (float)Fy / 256.0f;
	SnAg g;
	g.A = (uint16_t)(256.0f * sqrtf((1.0f - fx) * (1.0f - fy)) + 0.5f);
	g.B = (uint16_t)(256.0f * sqrtf(fx * (1.0f - fy)) + 0.5f);
	g.C = (uint16_t)(256.0f * sqrtf((1.0f - fx) * fy) + 0.5f);
	g.D = (uint16_t)(256.0f * sqrtf(fx * fy) + 0.5f);
	return g;
}

static void SnNokta(int x, int y, const SnAg *g, uint32_t v)
{
	uint32_t k;
	if((k = (v * g->A) >> 8) != 0U) Add(x,     y,     (uint8_t)k, (uint8_t)k, (uint8_t)k);
	if((k = (v * g->B) >> 8) != 0U) Add(x + 1, y,     (uint8_t)k, (uint8_t)k, (uint8_t)k);
	if((k = (v * g->C) >> 8) != 0U) Add(x,     y + 1, (uint8_t)k, (uint8_t)k, (uint8_t)k);
	if((k = (v * g->D) >> 8) != 0U) Add(x + 1, y + 1, (uint8_t)k, (uint8_t)k, (uint8_t)k);
}

/* Tanenin cizim konumu: tam kisim (X0, Y0) + ara piksel agirliklari */
static SnAg SnKonum(const SnTn *t, int *X0, int *Y0)
{
	const int32_t Yan = t->Kx >> 4;								/* 1/256 px (aritmetik kaydirma: asagi yuvarlar) */
	*X0 = t->X + (int)(Yan >> 8);
	*Y0 = t->Y;
	return SnAgirlik((uint32_t)(Yan & 255), (uint32_t)t->Ac * 256U / t->Kd);
}

static void SnDesen(const SnTn *t, const char *const *Sat, int n, uint32_t L)
{
	int X, Y, x, y;
	const SnAg g = SnKonum(t, &X, &Y);
	for(y = 0; y < n; y++)
		for(x = 0; x < n; x++)
		{
			const char c = Sat[y][x];
			uint32_t v;
			v = (c == '#') ? L : (c == '+') ? (L * 77U) >> 8 : (L * 46U) >> 8;
			if(v) SnNokta(X + x - n / 2, Y + y - n / 2, &g, v);
		}
}

/* Yana kayma hedefi: bazen sola, bazen saga, bazen duz; birkac saniye surer */
static void SnYon(SnTn *t)
{
	t->Hd = ((Rand() % 10U) < 3U) ? 0 : (int16_t)((int32_t)(Rand() % (2U * (uint32_t)t->En + 1U)) - t->En);
	t->Dg = (uint16_t)(CFG_FPS * (1.6f + 3.2f * RandF()));
}

/* 3x3 tanenin (orta / yakin) yakininda baska 3x3 tane var mi */
static uint8_t SnDolu(const SnTn *t, int x, int y, int Ara)
{
	uint32_t i;
	const uint32_t NOr = U.Sn.NOr, NYa = U.Sn.NYa;
	for(i = 0; i < NOr + NYa; i++)
	{
		const SnTn *o = (i < NOr) ? &U.Sn.Or[i] : &U.Sn.Ya[i - NOr];
		if(o == t) continue;
		if(abs(o->X - x) < Ara && abs(o->Y - y) < Ara) return 1U;
	}
	return 0U;
}

/* Tane hiz (px/s) ve turuyle yeniden dogar. Tur: 0 uzak (duz iner), 1 orta
   (en cok ~1.5 px/s yana), 2 yakin (en cok ~3 px/s yana). Orta ve yakin taneler
   baska 3x3 tanenin dibinde dogmaz. Basta = ekranin her yerinde.
   Son: uzak tanenin sonup kaybolacagi satir. */
static void SnDog(SnTn *t, float Hiz, uint8_t Tur, uint8_t L, uint8_t Basta)
{
	int k;
	t->Son = (Tur == 0U) ? (int16_t)(H / 3U + Rand() % (H * 2U / 3U)) : (int16_t)(H + 3U);
	for(k = 0; k < 8; k++)										/* Bos yer ara */
	{
		t->X = (int16_t)(Rand() % W);
		t->Y = Basta ? (int16_t)(Rand() % (uint32_t)t->Son) : (int16_t)(-1 - (int)(Rand() % 6U));
		if(Tur == 0U || !SnDolu(t, t->X, t->Y, 5)) break;
	}
	t->Kd = (uint16_t)(16.0f * CFG_FPS / Hiz);
	t->Ac = (uint16_t)(Rand() % t->Kd);
	t->Kx = 0;
	t->L  = L;
	t->En = (Tur == 2U) ? (int16_t)(3.0f * 4096.0f / CFG_FPS) : (Tur == 1U) ? (int16_t)(1.5f * 4096.0f / CFG_FPS) : 0;
	if(t->En) { SnYon(t); t->Vx = t->Hd; } else { t->Hd = t->Vx = 0; t->Dg = 0xFFFFU; }
}

/* Duzenli adim: Adim 1/16 kare biriminde (Hiz ayari dahil) */
static void SnIn(SnTn *t, uint16_t Adim)
{
	t->Ac = (uint16_t)(t->Ac + Adim);
	while(t->Ac >= t->Kd) { t->Ac = (uint16_t)(t->Ac - t->Kd); t->Y++; }
	if(!t->En) return;
	if(t->Dg) t->Dg--; else SnYon(t);
	if(t->Vx < t->Hd) t->Vx++;									/* Yeni yone yavasca doner (~2-3 s) */
	else if(t->Vx > t->Hd) t->Vx--;
	t->Kx += (int32_t)t->Vx * Adim / 16;
	while(t->Kx >= 4096)  { t->Kx -= 4096; t->X++; }
	while(t->Kx <= -4096) { t->Kx += 4096; t->X--; }
	if(t->X < -2)              t->X = (int16_t)(W + 1U);		/* Kenardan cikan karsidan girer */
	else if(t->X > (int)W + 1) t->X = -2;
}

/* Iki 3x3 tane cok yaklasirsa birbirinden uzaklasirlar (ust uste binip leke olmasinlar) */
static void SnAyir(void)
{
	const uint32_t NOr = U.Sn.NOr, NYa = U.Sn.NYa;
	uint32_t i, j;
	for(i = 0; i < NOr + NYa; i++)
	{
		SnTn *a = (i < NOr) ? &U.Sn.Or[i] : &U.Sn.Ya[i - NOr];
		if(a->Y < -2) continue;
		for(j = i + 1; j < NOr + NYa; j++)
		{
			SnTn *b = (j < NOr) ? &U.Sn.Or[j] : &U.Sn.Ya[j - NOr];
			int dx;
			if(abs(a->Y - b->Y) >= 4) continue;
			dx = a->X - b->X;
			if(abs(dx) >= 4) continue;
			if(dx == 0) dx = (Rand() & 1U) ? 1 : -1;
			a->Hd = (int16_t)((dx > 0) ? a->En : -a->En);			/* Soldaki sola, sagdaki saga */
			b->Hd = (int16_t)((dx > 0) ? -b->En : b->En);
			a->Dg = b->Dg = (uint16_t)CFG_FPS;
		}
	}
}

/* Kayan yildiz: ust kosenin birinden girer, yay cizerek (yercekimi gibi) karsi
   kenarin alt tarafina iner; parlak bas + sonen kuyruk. */
static void SnMeteor(float Gdt)
{
	int i;

	if(!U.Sn.MOn)
	{
		U.Sn.MSonra -= Gdt;
		if(U.Sn.MSonra > 0.0f) return;
		{
			const float d = (Rand() & 1U) ? 1.0f : -1.0f;
			U.Sn.Mx  = (d > 0.0f) ? -1.0f : (float)W;
			U.Sn.My  = RandF() * 3.0f;
			U.Sn.Mvx = d * (95.0f + 25.0f * RandF());
			U.Sn.Mvy = 3.0f + 4.0f * RandF();
			U.Sn.May = 22.0f + 10.0f * RandF();
			U.Sn.MIz = 0; U.Sn.MOn = 1;
		}
	}

	if(U.Sn.MIz < SN_IZ) U.Sn.MIz++;
	for(i = (int)U.Sn.MIz - 1; i > 0; i--) { U.Sn.Ix[i] = U.Sn.Ix[i - 1]; U.Sn.Iy[i] = U.Sn.Iy[i - 1]; }
	U.Sn.Mx += U.Sn.Mvx * Gdt; U.Sn.Mvy += U.Sn.May * Gdt; U.Sn.My += U.Sn.Mvy * Gdt;
	U.Sn.Ix[0] = (int16_t)U.Sn.Mx; U.Sn.Iy[0] = (int16_t)U.Sn.My;

	if(U.Sn.Mx < -(float)SN_IZ || U.Sn.Mx > (float)(W + SN_IZ) || U.Sn.My > (float)H + 2.0f)
	{
		U.Sn.MOn = 0;
		U.Sn.MSonra = 6.0f + 6.0f * RandF();
		return;
	}
	for(i = (int)U.Sn.MIz - 1; i > 0; i--)							/* Kuyruk: 1 px, basa yakin parlak, ucta soner */
	{
		const uint32_t k = (uint32_t)(SN_IZ - i), v = 130U * k * k / ((uint32_t)SN_IZ * SN_IZ);
		Add(U.Sn.Ix[i], U.Sn.Iy[i], (uint8_t)v, (uint8_t)((v * 240U) >> 8), (uint8_t)((v * 210U) >> 8));
	}
	{																/* Bas: 3x3 yumusak, ara piksel */
		const float fx = U.Sn.Mx - 1.0f, fy = U.Sn.My - 1.0f;		/* Desen merkezi Mx, My */
		const int   X0 = (int)floorf(fx), Y0 = (int)floorf(fy);
		const SnAg  g  = SnAgirlik((uint32_t)((fx - (float)X0) * 256.0f), (uint32_t)((fy - (float)Y0) * 256.0f));
		int x, y;
		for(y = 0; y < 3; y++)
			for(x = 0; x < 3; x++)
			{
				const char c = SnYumusak[y][x];
				const uint32_t L = (c == '#') ? 170U : (c == '+') ? 70U : 36U;
				const uint32_t a[4] = { (L * g.A) >> 8, (L * g.B) >> 8, (L * g.C) >> 8, (L * g.D) >> 8 };
				int k;
				for(k = 0; k < 4; k++)
					if(a[k]) Add(X0 + x + (k & 1), Y0 + y + (k >> 1), (uint8_t)a[k], (uint8_t)((a[k] * 240U) >> 8), (uint8_t)((a[k] * 210U) >> 8));
			}
	}
}

/* Saatli modlarin ortak saat cizimi: maske tek renk + yanip sonen iki nokta */
static void SaatCiz(const SaatDurum *S, uint8_t R, uint8_t G, uint8_t B)
{
	const uint8_t Yanik = SaatNoktaYanik(S);
	int x, y;
	for(y = 0; y < (int)H; y++)
		for(x = 0; x < (int)W; x++)
			if(SaatPx(S, x, y) && (Yanik || !SaatNokta(S, x))) SaatSet(x, y, R, G, B);
}

/* Saat secilen renkte; sicaklik / nem biraz daha acik */
static void SaatCizRgb(const SaatDurum *S)
{
	const uint32_t r = (SPEC_SaatRgb >> 16) & 0xFFU, g = (SPEC_SaatRgb >> 8) & 0xFFU, b = SPEC_SaatRgb & 0xFFU;
	SaatCiz(S, (uint8_t)r, (uint8_t)g, (uint8_t)b);
	SaatIsiCiz(S, (uint8_t)((r * 3U / 2U > 255U) ? 255U : r * 3U / 2U), (uint8_t)((g * 3U / 2U > 255U) ? 255U : g * 3U / 2U),
	              (uint8_t)((b * 3U / 2U > 255U) ? 255U : b * 3U / 2U));
}

static void ModeSnow(void)
{
	static const uint8_t NUz[3] = { 30, 50, 70 }, NOr[3] = { 15, 25, 35 }, NYa[3] = { 3, 5, 6 };
	const float    Gdt  = 1.0f / CFG_FPS;					/* Yildiz, kayan yildiz: gercek zaman	*/
	const uint16_t Adim = (uint16_t)(SpdDT * CFG_FPS * 16.0f + 0.5f);	/* Kar: Hiz ayari ile		*/
	const uint32_t Tepe = SN_TEPE * SPEC_SnIsik / 100U;
	const uint8_t  Yo   = (SPEC_SnYogun < 3U) ? SPEC_SnYogun : 1U;
	uint32_t i;
	int x, y;

	if(!U.Sn.Ready)
	{
		for(i = 0; i < SN_UZAK; i++)  SnDog(&U.Sn.Uz[i], 2.0f + 1.2f * RandF(), 0, (uint8_t)(30U + Rand() % 16U), 1);
		U.Sn.NOr = U.Sn.NYa = 0;									/* Yerlestikce bosluk denetimine girer */
		for(i = 0; i < SN_ORTA; i++)  { SnDog(&U.Sn.Or[i], 5.0f + 2.0f * RandF(), 1, 60, 1); U.Sn.NOr = (uint8_t)(i + 1U); }
		for(i = 0; i < SN_YAKIN; i++) { SnDog(&U.Sn.Ya[i], 14.0f + 6.0f * RandF(), 2, 100, 1); U.Sn.NYa = (uint8_t)(i + 1U); }
		for(i = 0; i < SN_YILDIZ; i++)
		{
			U.Sn.Sx[i] = (uint8_t)(Rand() % W);
			U.Sn.Sy[i] = (uint8_t)(Rand() % (H * 2U / 3U));
		}
		U.Sn.MSonra = 5.0f + 10.0f * RandF();
		U.Sn.Ready = 1;
	}
	SaatHazirla(&U.Sn.S);

	/* Gok: siyah (gece odayi aydinlatmasin) */
	for(y = 0; y < (int)H; y++)
		for(x = 0; x < (int)W; x++)
			Set(x, y, 0, 0, 0);

	/* Yildizlar: sonuk durur; ara sira parlar (en cok ~160), ~0.3 s'de soner */
	for(i = 0; i < SN_YILDIZ; i++)
	{
		uint32_t L;
		if(RandF() < Gdt / 3.0f) U.Sn.Sp[i] = 1.0f;
		U.Sn.Sp[i] *= 0.90f;
		L = 30U + (uint32_t)(130.0f * U.Sn.Sp[i]);
		Add(U.Sn.Sx[i], U.Sn.Sy[i], (uint8_t)L, (uint8_t)((L * 240U) >> 8), (uint8_t)((L * 200U) >> 8));	/* Sicak beyaz */
	}
	SnMeteor(Gdt);
	U.Sn.NOr = NOr[Yo]; U.Sn.NYa = NYa[Yo];
	SnAyir();

	/* Uzak kar: tek sonuk piksel; son 6 satirda sonup havada kaybolur */
	for(i = 0; i < NUz[Yo]; i++)
	{
		SnTn *t = &U.Sn.Uz[i];
		uint32_t v = Tepe * t->L / 100U;
		SnIn(t, Adim);
		if(t->Y >= t->Son) { SnDog(t, 2.0f + 1.2f * RandF(), 0, (uint8_t)(30U + Rand() % 16U), 0); continue; }
		if(t->Son - t->Y < 6) v = v * (uint32_t)(t->Son - t->Y) / 6U;
		{
			int X0, Y0;
			const SnAg g = SnKonum(t, &X0, &Y0);
			SnNokta(X0, Y0, &g, v);
		}
	}

	SaatCizRgb(&U.Sn.S);

	/* Orta kar: 3x3 yumusak, orta hiz */
	for(i = 0; i < NOr[Yo]; i++)
	{
		SnTn *t = &U.Sn.Or[i];
		SnIn(t, Adim);
		if(t->Y >= t->Son) SnDog(t, 5.0f + 2.0f * RandF(), 1, 60, 0);
		SnDesen(t, SnYumusak, 3, Tepe * t->L / 100U);
	}

	/* Yakin kar: hizli (saatin onunde) */
	for(i = 0; i < NYa[Yo]; i++)
	{
		SnTn *t = &U.Sn.Ya[i];
		SnIn(t, Adim);
		if(t->Y >= t->Son) SnDog(t, 14.0f + 6.0f * RandF(), 2, 100, 0);
		SnDesen(t, SnYumusak, 3, Tepe);
	}
}

/* ==========================================================================
   MOD -- SOMINE (Ortam, muziksiz)
   Kisik, gercekci alev: taban koyu kizil, orta turuncu-sari, uclar kopuk
   diller. Alev kenarlardan yukselir, ortada alcak kalir; saat one cikar.
   Saat rakamlari alevin icinde KATI: alev icinden gecmez, aralardan sizar.
   Rakamlarin ici secilen saat renginde, alevin degdigi kenarlar koz gibi parlar.
   Alev kendi dogal duzensizligiyle yanar (arada harlanir). Ara sira
   kivilcim yukselir. Alev motoru (tamsayi + sogutma haritasi). Durum U.Sm.
   ========================================================================== */

/* Yavas nefes: 0..255 (kare ve piksele gore), tablo ile */
static uint8_t SmNefes(uint32_t Kare, int x, int y)
{
	static const uint8_t S[32] = { 128,152,176,198,218,234,245,253,255,253,245,234,218,198,176,152,
	                               128,103, 79, 57, 37, 21, 10,  2,  0,  2, 10, 21, 37, 57, 79,103 };
	return S[((Kare >> 3) + (uint32_t)(x * 7 + y * 13)) & 31U];
}

/* Koz rengi (kenar): koyu kizil -> turuncu -> parlak turuncu-sari */
static void SmKozRenk(uint32_t k, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if(k < 160U) { *r = (uint8_t)(90U + k * 9U / 10U); *g = (uint8_t)(6U + k / 3U); *b = 4U; }
	else         { *r = 235U + (uint8_t)((k - 160U) / 5U); *g = (uint8_t)(59U + (k - 160U)); *b = (uint8_t)(4U + (k - 160U) / 3U); }
}

/* Rakamlar alev icin engel; Saydamlik acikken alev rakamlarin icinden de yanar */
static inline uint8_t SmEngel(const SaatDurum *S, int x, int y) { return (uint8_t)(SPEC_SaatSaydam == 0U && SaatPx(S, x, y)); }

static void ModeHearth(void)
{
	static const float Boy[3] = { 0.38f, 0.48f, 0.60f };	/* Yuksek = eski Alcak */
	const float Gdt = 1.0f / CFG_FPS;
	const float B   = Boy[(SPEC_SmBoy < 3U) ? SPEC_SmBoy : 1U];
	const int   Cool = (int)(8.0f / CFG_FIRE_HEIGHT + 0.5f);
	SaatDurum  *S = &U.Sm.S;
	uint32_t i;
	uint8_t nr, ng, nb;
	int x, y;

	if(!U.Sm.Ready)
	{
		for(x = 0; x < (int)W; x++) U.Sm.Yk[x] = 0.5f + 0.5f * RandF();
		U.Sm.Harl = U.Sm.HarlH = 1.0f;
		U.Sm.HarlSonra = 4.0f;
		SPEC_SetFirePalette(6U, 0xA02800UL, 0xFF8A18UL, 0xFFD060UL);	/* Gercek alev: kirmizi yalniz serin uclarda, govde turuncu, sicak yer altin sari (beyaz yok) */
		U.Sm.Ready = 1;
	}
	U.Sm.Kare++;
	SaatHazirla(S);

	/* Harlanma: 4-10 s'de bir yeni hedef (0.85-1.15), seyrek daha guclu harlama */
	U.Sm.HarlSonra -= Gdt;
	if(U.Sm.HarlSonra <= 0.0f)
	{
		U.Sm.HarlH = ((Rand() % 5U) == 0U) ? 1.25f : 0.85f + 0.3f * RandF();
		U.Sm.HarlSonra = 4.0f + 6.0f * RandF();
	}
	U.Sm.Harl += (U.Sm.HarlH - U.Sm.Harl) * 0.01f;

	/* Yakit: sutun hedefleri arada degisir, komsularla yumusar -> farkli boyda diller.
	   Kenarlarda tam, ortaya dogru azalir (ortada ~1/3): alev saati yanlardan cerceveler. */
	for(i = 0; i < 4U; i++) U.Sm.Yk[Rand() % W] = 0.45f + 0.55f * RandF();
	for(x = 1; x < (int)W - 1; x++) U.Sm.Yk[x] = (U.Sm.Yk[x - 1] + U.Sm.Yk[x] * 2.0f + U.Sm.Yk[x + 1]) * 0.25f;
	for(x = 0; x < (int)W; x++)
	{
		const float d = fabsf((float)x - (float)(W - 1) * 0.5f) / ((float)W * 0.5f);	/* 0 orta .. 1 kenar */
		const float Kenar = 0.33f + 0.67f * d * d;
		float n = 0.6f + 0.4f * RandF();
		uint8_t f;
		if((Rand() & 15U) == 0U) n *= 0.4f;					/* Arada bogulma */
		f = Sat(620.0f * B * Kenar * U.Sm.Harl * (0.25f + 0.75f * U.Sm.Yk[x] * U.Sm.Yk[x]) * n);
		U.Sm.Heat[H][x] = f;
		U.Sm.Heat[H + 1][x] = (uint8_t)((f + U.Sm.Heat[H + 1][x]) >> 1);
	}

	/* Alev adimi: rakam pikselleri kati (isi 0, alev gecmez, aralardan sizar) */
	CoolOffset = (uint8_t)((CoolOffset + 1U) % H);
	{
		const float Wind = sinf(U.Sm.Kare * Gdt * 0.7f) * 0.5f;
		for(y = 0; y < (int)H; y++)
		{
			const int Shift = (int)(Wind * (float)(H - y) / (float)H + ((Rand() & 7U) == 0U ? ((Rand() & 1U) ? 1 : -1) : 0));
			for(x = 0; x < (int)W; x++)
			{
				int sx = x - Shift, xl, xr, Sum, c;
				if(SmEngel(S, x, y)) { U.Sm.Heat[y][x] = 0; continue; }
				if(sx < 0) sx = 0;
				if(sx >= (int)W) sx = W - 1;
				xl = (sx > 0) ? sx - 1 : sx;
				xr = (sx < (int)W - 1) ? sx + 1 : sx;
				/* Rakam hucreleri ornekten cikar: alev rakamin kenarini yalar, etrafinda karanlik hale kalmaz */
				{
					int Ag = 0;
					Sum = 0;
					if(!SmEngel(S, xl, y + 1)) { Sum += U.Sm.Heat[y + 1][xl];     Ag += 1; }
					if(!SmEngel(S, sx, y + 1)) { Sum += U.Sm.Heat[y + 1][sx] * 2; Ag += 2; }
					if(!SmEngel(S, xr, y + 1)) { Sum += U.Sm.Heat[y + 1][xr];     Ag += 1; }
					if(!SmEngel(S, sx, y + 2)) { Sum += U.Sm.Heat[y + 2][sx] * 2; Ag += 2; }
					if(!SmEngel(S, x - 1, y))  { Sum += U.Sm.Heat[y][(x > 0) ? x - 1 : x];          Ag += 1; }	/* Yandan da beslenir */
					if(!SmEngel(S, x + 1, y))  { Sum += U.Sm.Heat[y][(x < (int)W - 1) ? x + 1 : x]; Ag += 1; }
					Sum = Ag ? Sum * 6 / Ag : 0;
				}
				c = (CoolMap[(y + CoolOffset) % H][x] * Cool) >> 7;
				U.Sm.Heat[y][x] = (uint8_t)SatI(Sum / 6 - c);
			}
		}
	}

	/* Ciz: alev */
	for(y = 0; y < (int)H; y++)
		for(x = 0; x < (int)W; x++)
		{
			const uint8_t *c = ActPal[U.Sm.Heat[y][x]];
			Set(x, y, c[0], c[1], c[2]);
		}

	/* Rakamlar: ici saat rengi (okunur), alevin degdigi kenar koz gibi parlar.
	   Iki nokta: saat renginde, diger modlardaki gibi yanip soner (saniyede sabit). */
	nr = (uint8_t)((SPEC_SaatRgb >> 16) & 0xFFU); ng = (uint8_t)((SPEC_SaatRgb >> 8) & 0xFFU); nb = (uint8_t)(SPEC_SaatRgb & 0xFFU);
	for(y = 0; y < (int)H; y++)
		for(x = 0; x < (int)W; x++)
		{
			uint32_t h = 0, k;
			uint8_t r, g, b;
			if(!SaatPx(S, x, y)) continue;
			if(SaatNokta(S, x)) { if(SaatNoktaYanik(S)) SaatSet(x, y, nr, ng, nb); continue; }
			if(SPEC_SaatSaydam)									/* Saydam: kor yok, cam rengi */
			{
				SaatSet(x, y, (uint8_t)((SPEC_SaatRgb >> 16) & 0xFFU), (uint8_t)((SPEC_SaatRgb >> 8) & 0xFFU), (uint8_t)(SPEC_SaatRgb & 0xFFU));
				continue;
			}
			if(y > 0          && U.Sm.Heat[y - 1][x] > h) h = U.Sm.Heat[y - 1][x];
			if(y < (int)H - 1 && U.Sm.Heat[y + 1][x] > h) h = U.Sm.Heat[y + 1][x];
			if(x > 0          && U.Sm.Heat[y][x - 1] > h) h = U.Sm.Heat[y][x - 1];
			if(x < (int)W - 1 && U.Sm.Heat[y][x + 1] > h) h = U.Sm.Heat[y][x + 1];
			k = U.Sm.Koz[y][x];
			if(h > k)                              k += (h - k + 15U) >> 4;
			else if(k > 0U && (U.Sm.Kare & 3U) == 0U) k--;
			U.Sm.Koz[y][x] = (uint8_t)k;
			k = (k * (360U + (SmNefes(U.Sm.Kare, x, y) >> 1))) >> 8;	/* Kazanc + hafif nefes */
			if(k > 255U) k = 255U;
			SmKozRenk(k, &r, &g, &b);
			{													/* Koz rengi, kozun gucu kadar saat renginin uzerine */
				const uint32_t a = k, sr = (SPEC_SaatRgb >> 16) & 0xFFU, sg = (SPEC_SaatRgb >> 8) & 0xFFU, sb = SPEC_SaatRgb & 0xFFU;
				r = (uint8_t)((sr * (255U - a) + r * a) / 255U);
				g = (uint8_t)((sg * (255U - a) + g * a) / 255U);
				b = (uint8_t)((sb * (255U - a) + b * a) / 255U);
			}
			SaatSet(x, y, r, g, b);
		}

	SaatIsiCiz(S, (uint8_t)((SPEC_SaatRgb >> 16) & 0xFFU), (uint8_t)((SPEC_SaatRgb >> 8) & 0xFFU), (uint8_t)(SPEC_SaatRgb & 0xFFU));

	/* Kivilcim: ortalama 6 karede bir alevin tabanindan (cogunlukla kenarlardan) yukselip soner */
	if((Rand() % 6U) == 0U)
		for(i = 0; i < SM_KIV; i++)
			if(!U.Sm.Kv[i].Life)
			{
				Particle *p = &U.Sm.Kv[i];
				const float u = RandF() * 0.35f;
				p->X = (Rand() & 1U) ? u * (float)W : (1.0f - u) * (float)W;
				p->Y = (float)H - 2.0f - RandF() * 5.0f;
				p->Vx = (RandF() - 0.5f) * 10.0f; p->Vy = -(15.0f + 18.0f * RandF());
				p->Life = 255U;
				break;
			}
	for(i = 0; i < SM_KIV; i++)
	{
		Particle *p = &U.Sm.Kv[i];
		if(!p->Life) continue;
		p->X += (p->Vx + (RandF() - 0.5f) * 20.0f) * Gdt;
		p->Y += p->Vy * Gdt;
		p->Vy += 12.0f * Gdt;
		p->Life = (p->Life > 5U) ? (uint8_t)(p->Life - 5U) : 0U;
		if(p->Y < 0.0f || p->X < 0.0f || p->X >= (float)W) { p->Life = 0; continue; }
		{
			const uint8_t *c = ActPal[150U + (p->Life >> 2) + (Rand() & 15U) - 8U];
			Add((int)p->X, (int)p->Y, c[0], c[1], c[2]);
		}
	}
}

/* ==========================================================================
   MOD -- YAGMUR (Ortam, muziksiz)
   Gece yagmuru, uc derinlik:
     uzak : ince, sonuk, kuyrugu sonen cizgiler; sik; saatin arkasinda
            (tek tek secilmez, kipirdayan bir yagmur perdesi)
     yakin: saatin onunde. Iri damlalar: 3 px yuvarlak bas (ortasi tam,
            yanlari yari isikli, alt ucu burun) + 1 px sonen kuyruk.
            Kucukler: 1 px bas + kisa kuyruk.
   Altta saydam su (gol): 10-12 px, Saganakta 15'e kadar. Ust kenari asla duz
   degil: sutun sutun kontrol noktali, yavasca kayan, vurusla kabaran, 2 px'te
   yumusakca beliren bir kenar. Perspektif: suyun ustu uzak, alti yakin; uzak
   damlalar uste, yakinlar alta duser. Her damla dustugu yerde yassi elips
   halkalar acar (uzakta kucuk ve basik, yakinda genis), halkanin hizi ve
   buyuklugu damlanin hizina bagli; yakin damla damlacik sicratir. Su saati
   ortmez: altindaki gorunur, koyu mavi tonlu ve 1 px dalgalanarak.
   Ruzgar egimi ayarli, simsek yok. Konum 8.8 sabit nokta. Durum U.Ym.
   ========================================================================== */
#define YM_DERIN(Si)	(((Si) == 2U) ? 15U : ((Si) == 0U) ? 10U : 12U)	/* Su derinligi (satir) */
#define YM_RENK(v)	(uint8_t)(((v) * 200U) >> 8), (uint8_t)(((v) * 215U) >> 8), (uint8_t)(v)	/* Soguk gri-mavi */

/* Damla: X,Y 8.8 (Y = burun satiri); Vy px/kare*256; Uz = kuyruk; L = isik */
static void YmDog(YmDamla *d, uint8_t Tur, uint8_t Basta)	/* Tur: 0 uzak, 1 yakin kucuk, 2 yakin iri */
{
	static const float HizA[3] = { 55.0f, 90.0f, 70.0f }, HizB[3] = { 20.0f, 20.0f, 20.0f };
	const float Hiz = HizA[Tur] + HizB[Tur] * RandF();				/* px/s */
	d->Vy  = (int16_t)(Hiz * 256.0f / CFG_FPS);
	d->X   = (int32_t)(Rand() % (W + 24U)) * 256 - 12 * 256;		/* Egimle kenardan da girsin */
	d->Y   = Basta ? (int32_t)(Rand() % H) * 256 : -(int32_t)(2U + Rand() % 12U) * 256;
	d->Iri = (uint8_t)(Tur == 2U);
	{														/* Dusecegi satir: uzak -> suyun ust (uzak) kismi, yakin -> alt kismi */
		const uint32_t Ys = H - YM_DERIN(SPEC_YmSiddet);
		const uint32_t D  = H - Ys;
		d->Hd = (Tur == 0U) ? (uint8_t)(Ys + 1U + Rand() % (D * 2U / 5U)) : (uint8_t)(Ys + D / 2U + Rand() % (D - D / 2U));
		if(d->Hd >= H) d->Hd = (uint8_t)(H - 1U);
	}
	d->Uz  = (Tur == 2U) ? (uint8_t)(5U + Rand() % 4U) : (Tur == 1U) ? (uint8_t)(3U + Rand() % 2U) : (uint8_t)(3U + Rand() % 3U);
	d->L   = (Tur == 2U) ? (uint8_t)(120U + Rand() % 25U) : (Tur == 1U) ? (uint8_t)(80U + Rand() % 20U) : (uint8_t)(22U + Rand() % 16U);
}

static void YmCiz(const YmDamla *d, int32_t Vx)
{
	const int x0 = (int)(d->X >> 8), y0 = (int)(d->Y >> 8);
	const uint32_t L = d->L;
	int k, n;
	#define YM_X(k)	(x0 - (int)(Vx * (k) / d->Vy))					/* Egim: kuyruk ruzgarin tersine kayar */
	if(d->Iri)
	{
		Add(YM_X(0), y0, YM_RENK((L * 180U) >> 8));					/* Burun */
		Add(YM_X(1), y0 - 1, YM_RENK(L));								/* Bas: en genis yer */
		Add(YM_X(1) - 1, y0 - 1, YM_RENK((L * 128U) >> 8));
		Add(YM_X(1) + 1, y0 - 1, YM_RENK((L * 128U) >> 8));
		Add(YM_X(2), y0 - 2, YM_RENK(L));								/* Bas ustu */
		Add(YM_X(2) - 1, y0 - 2, YM_RENK((L * 70U) >> 8));
		Add(YM_X(2) + 1, y0 - 2, YM_RENK((L * 70U) >> 8));
		n = 3;
	}
	else if(L > 60U)													/* Yakin kucuk: burun + bas */
	{
		Add(YM_X(0), y0, YM_RENK((L * 150U) >> 8));
		Add(YM_X(1), y0 - 1, YM_RENK(L));
		n = 2;
	}
	else n = 0;															/* Uzak: yalniz iz */
	for(k = 0; k < (int)d->Uz; k++)										/* Kuyruk: basa yakin parlak, ucta soner */
	{
		const uint32_t v = (L * (uint32_t)(d->Uz - k) * (n ? 170U : 255U)) / ((uint32_t)(d->Uz + 1U) * 256U);
		if(v) Add(YM_X(n + k), y0 - n - k, YM_RENK(v));
	}
	#undef YM_X
}

/* Suya degme: halka (hizi damla hizina bagli) + yakin damlada damlaciklar + kenar kabarmasi */
static void YmSicrat(const YmDamla *d, uint8_t Yakin)
{
	const float x = (float)(d->X >> 8) + 0.5f, y = (float)d->Hd;
	const float Hiz = (float)d->Vy * CFG_FPS / 256.0f;			/* Damla hizi px/s */
	uint32_t i, j;
	for(j = 0; j < YM_HALKA; j++)
		if(!U.Ym.Hl[j].Life)
		{
			YmHalka *h = &U.Ym.Hl[j];
			h->X = x; h->Y = y; h->R = 0.5f; h->Iri = d->Iri;
			h->Hiz  = Hiz * (Yakin ? 0.15f : 0.10f) * (d->Iri ? 1.3f : 1.0f);	/* Hizli damla hizli halka (px/s) */
			h->Life = Yakin ? 255U : 150U;
			break;
		}
	if(!Yakin) return;
	{
		const uint32_t Adet = d->Iri ? 3U + Rand() % 3U : 1U + Rand() % 2U;
		for(i = 0, j = 0; i < Adet && j < YM_ZERRE; j++)
		{
			YmZerre *z = &U.Ym.Zr[j];
			if(z->L) continue;
			z->X  = x; z->Y = y - 0.5f; z->Hd = y;
			z->Vx = ((Rand() & 1U) ? 1.0f : -1.0f) * (d->Iri ? 12.0f + 26.0f * RandF() : 8.0f + 14.0f * RandF());
			z->Vy = -(d->Iri ? 22.0f + 22.0f * RandF() : 12.0f + 12.0f * RandF());
			z->L  = d->Iri ? 115U : 75U;
			i++;
		}
	}
}

#define YM_SU(v)	(uint8_t)(((v) * 70U) >> 8), (uint8_t)(((v) * 150U) >> 8), (uint8_t)(((v) * 210U) >> 8)	/* Acik su tonu (beyaz degil) */

/* Elips halka: uzakta basik (perspektif), genisledikce soner */
static void YmHalkaCiz(const YmHalka *h, float Ys0)
{
	static const int8_t Cs[16][2] = { {64,0},{59,24},{45,45},{24,59},{0,64},{-24,59},{-45,45},{-59,24},
	                                  {-64,0},{-59,-24},{-45,-45},{-24,-59},{0,-64},{24,-59},{45,-45},{59,-24} };
	const float z = (h->Y - Ys0) / ((float)H - Ys0 + 0.01f);		/* 0 uzak .. 1 yakin */
	const float ry = h->R * (0.22f + 0.30f * z);
	const uint32_t v = (uint32_t)h->Life * (h->Iri ? 90U : 60U) >> 8;
	int k;
	if(!v) return;
	for(k = 0; k < 16; k++)
	{
		const int x = (int)(h->X + h->R * (float)Cs[k][0] / 64.0f);
		const int y = (int)(h->Y + ry * (float)Cs[k][1] / 64.0f + 0.5f);
		if(y >= (int)Ys0 - 1 && y < (int)H) Add(x, y, YM_SU(v));
	}
}

static void ModeRain(void)
{
	static const uint8_t NUz[3] = { 30, 60, 90 }, NYa[3] = { 4, 9, 14 }, NIri[3] = { 2, 4, 6 };
	static const uint8_t Egim[3] = { 0, 40, 100 };				/* Vx = Vy * Egim / 256 */
	const float   Gdt = 1.0f / CFG_FPS;
	const uint8_t Si  = (SPEC_YmSiddet < 3U) ? SPEC_YmSiddet : 1U;
	const uint8_t Ru  = (SPEC_YmRuzgar < 3U) ? SPEC_YmRuzgar : 1U;
	const float   Ys0 = (float)(H - YM_DERIN(Si));				/* Suyun ortalama ust kenari */
	float   Ys[W];
	uint32_t i;
	int x, y;

	if(!U.Ym.Ready)
	{
		OrtamKayma(U.Ym.Of, 64.0f);
		for(i = 0; i < YM_UZAK; i++)  YmDog(&U.Ym.Uz[i], 0, 1);
		for(i = 0; i < YM_YAKIN; i++) YmDog(&U.Ym.Ya[i], (i < NIri[2]) ? 2U : 1U, 1);
		U.Ym.Ready = 1;
	}
	U.Ym.Kare++;
	SaatHazirla(&U.Ym.S);

	/* Su kenari: sabit, duzensiz kiyi cizgisi (kipirdamaz, duz de degil) */
	for(x = 0; x < (int)W; x++)
		Ys[x] = Ys0 + ((float)ShDoku[U.Ym.Of[x] & 63U] - 128.0f) * (1.5f / 128.0f)
		            + ((float)ShDoku[(U.Ym.Of[(x * 5) & (W - 1)] / 2U + 32U) & 63U] - 128.0f) * (0.7f / 128.0f);

	for(y = 0; y < (int)H; y++)
		for(x = 0; x < (int)W; x++)
			Set(x, y, 0, 0, 0);

	/* Uzak yagmur: saatin arkasinda, suyun uzak kismina duser */
	for(i = 0; i < NUz[Si]; i++)
	{
		YmDamla *d = &U.Ym.Uz[i];
		const int32_t Vx = (int32_t)d->Vy * Egim[Ru] / 256;
		d->Y += d->Vy; d->X += Vx;
		if((d->Y >> 8) >= (int32_t)d->Hd) { YmSicrat(d, 0); YmDog(d, 0, 0); continue; }
		YmCiz(d, Vx);
	}

	SaatCizRgb(&U.Ym.S);

	/* Su: saydam; altindaki (saat, damla) koyu mavi tonla ve hafif dalgalanarak gorunur.
	   Ust 2 px'te yumusakca belirir (keskin cizgi yok). */
	for(y = (int)Ys0 - 4; y < (int)H; y++)
	{
		uint8_t Sat[W][3];
		const float z = ((float)y - Ys0) / ((float)H - Ys0);			/* 0 uzak .. 1 yakin */
		const float zz = (z > 0.0f) ? z : 0.0f;
		const float Tr = 6.0f + 6.0f * zz, Tg = 14.0f + 10.0f * zz, Tb = 28.0f + 16.0f * zz;
		const float Ka = 6.2832f * 2.6f * sqrtf(zz + 0.04f), Za = (float)U.Ym.Kare * Gdt;	/* Uzakta sik, yakinda genis */
		if(y < 0) continue;
		for(x = 0; x < (int)W; x++) { const uint8_t *c = CV(y, x); Sat[x][0] = c[0]; Sat[x][1] = c[1]; Sat[x][2] = c[2]; }
		for(x = 0; x < (int)W; x++)
		{
			const float a = ((float)y + 1.0f - Ys[x]) * 0.5f;			/* Kenarda 0 -> 2 px'te 1 */
			uint32_t A, r, g, b, m, tr, tg, tb;
			float w;
			int sx;
			if(a <= 0.0f) continue;
			A  = (a >= 1.0f) ? 256U : (uint32_t)(a * 256.0f);
			sx = x + ((ShDoku[((uint32_t)y * 9U + U.Ym.Of[x] + U.Ym.Kare / 3U) & 63U] > 215U) ? 1 : 0);	/* Kirilma */
			if(sx >= (int)W) sx = (int)W - 1;
			m  = 150U + (ShDoku[((uint32_t)y * 13U + U.Ym.Of[x] / 2U + 64U - (U.Ym.Kare / 4U)) & 63U] >> 2);	/* Su ici kipirti */
			/* Katman katman dalga: suyun kendi tonu (tepe acik, cukur koyu); iki takim, egik ve kivrimli */
			w  = 0.45f * sinf(Ka + (float)U.Ym.Of[x] * 0.03f - 1.6f * Za)
			   + 0.25f * sinf(Ka * 0.7f + (float)x * 0.09f + (float)U.Ym.Of[(x * 3) & (W - 1)] * 0.02f - 1.1f * Za);
			tr = (uint32_t)(Tr * (1.0f + w * 0.8f)); tg = (uint32_t)(Tg * (1.0f + w)); tb = (uint32_t)(Tb * (1.0f + w));
			r  = ((Sat[sx][0] * m) >> 8) + tr; g = ((Sat[sx][1] * m) >> 8) + tg; b = ((Sat[sx][2] * m) >> 8) + tb;
			{
				uint8_t *c = CV(y, x);
				c[0] = (uint8_t)((Sat[x][0] * (256U - A) + ((r > 255U) ? 255U : r) * A) >> 8);
				c[1] = (uint8_t)((Sat[x][1] * (256U - A) + ((g > 255U) ? 255U : g) * A) >> 8);
				c[2] = (uint8_t)((Sat[x][2] * (256U - A) + ((b > 255U) ? 255U : b) * A) >> 8);
			}
		}
	}

	/* Halkalar: hizlari damlanin hizina gore genisler, soner */
	for(i = 0; i < YM_HALKA; i++)
	{
		YmHalka *h = &U.Ym.Hl[i];
		if(!h->Life) continue;
		h->R += h->Hiz * Gdt;
		h->Life = (h->Life > 5U) ? (uint8_t)(h->Life - 5U) : 0U;
		YmHalkaCiz(h, Ys0);
	}

	/* Yakin yagmur: saatin ve suyun onunde; ilk NIri tanesi iri. Suya degince sicrar. */
	for(i = 0; i < NYa[Si]; i++)
	{
		YmDamla *d = &U.Ym.Ya[i];
		const int32_t Vx = (int32_t)d->Vy * Egim[Ru] / 256;
		d->Y += d->Vy; d->X += Vx;
		if((d->Y >> 8) >= (int32_t)d->Hd)
		{
			YmSicrat(d, 1);
			YmDog(d, (i < NIri[Si]) ? 2U : 1U, 0);
			continue;
		}
		YmCiz(d, Vx);
	}

	/* Damlaciklar: yay cizip suya geri duser */
	for(i = 0; i < YM_ZERRE; i++)
	{
		YmZerre *z = &U.Ym.Zr[i];
		if(!z->L) continue;
		z->X += z->Vx * Gdt; z->Y += z->Vy * Gdt; z->Vy += 260.0f * Gdt;
		if((z->Vy > 0.0f && z->Y >= z->Hd) || z->X < 0.0f || z->X >= (float)W) { z->L = 0; continue; }
		Add((int)z->X, (int)z->Y, YM_SU((uint32_t)z->L * 3U / 4U));
		z->L = (z->L > 2U) ? (uint8_t)(z->L - 2U) : 0U;
	}
}

/* ==========================================================================
   MOD -- SAHIL (Ortam, muziksiz)
   Kusbakisi kiyi: ustte acik deniz, altta kum.
     Deniz katmanlari: derin (koyu) -> kiyiya yakin (acik) renk gecisi;
       kumun uzerine yayilan su denizin bittigi renkten baslar, ucuna dogru
       sig renge acilir, en ucta ince kalip altindaki kum gorunur (renk
       siniri yok); icinde onceki dalgalarin kopuk cizgileri ve isik kipirtisi.
     Dalga sirtlari: paralel degil. Her sirt bir parca (uzunlugu farkli,
       uclari soner), egik gelir; sirt boyunca hiz farkli oldugu icin bir
       kismi one gecer, kavsi ilerledikce degisir. Onunde hafif cukur golgesi,
       kiyiya yaklastikca guclenir. Buyuk sirtlar kiyiya varinca kirilir ve
       kumun uzerine yayilan dalgayi baslatir; kucukler kiyida soner. Arada
       kisa omurlu beyaz kopuk tepecikleri.
     Kiyiya vuran dalga: hizli gelir, daha yavas cekilir. Ucu duz degil: kirilan
       sirtin hizasinda one uzanir, yer yer dil gibi ilerler, egik gelebilir;
       ucunda duzensiz kopuk + kabarcik. Onceki dalga cekilirken yenisi gelir
       (ayni anda en cok 3); cekilenin kopuk cizgisi yeni suyun icinde gorunur.
       Ara sira buyuk dalga panelin altina kadar gelir (Sakin: hic, Normal:
       ~10 dalgada bir, Dalgali: ~5 dalgada bir).
     Kum: suyun degdigi yer islanir, yeni acilan islak kum hafif parlar,
       yavasca kurur; cekilen dalga yer yer kopuk birakir.
   Dalga ayari: Sakin / Normal / Dalgali (hiz ve erisim). Isik: Gece / Gunduz
   (parlaklik ve soluklugu). Piksel basina sinf yok. Durum U.Sh.
   ========================================================================== */
typedef struct { uint8_t Dz1[3], Dz2[3], Sig[3], Sirt[3], Kp[3], Kum[3], Islak[3]; } ShRenk;
static const ShRenk ShRenkler[2] =
{	/*   derin           kiyi            sig su          sirt            kopuk           kuru kum        islak kum	*/
	{ {  2,  6, 24 }, {  6, 32, 62 }, { 14, 48, 72 }, { 40, 78,116 }, {110,120,140 }, { 42, 34, 24 }, { 28, 23, 18 } },	/* Gece		*/
	{ {  0, 38, 92 }, {  0,105,150 }, { 40,170,172 }, {125,205,225 }, {228,242,242 }, {190,160,110 }, {118, 94, 60 } },	/* Gunduz	*/
};
#define SH_TABAN	9.0f						/* Denizin kiyisi (dalga buraya cekilir)	*/

static const float ShSure[3] = { 3.4f, 2.8f, 2.2f };		/* Kiyiya vuran dalganin omru (s)		*/
static const float ShEris[3] = { 11.0f, 14.0f, 16.0f };		/* Kumun uzerinde en cok kac satir		*/

static void ShRenkKat(uint32_t *r, uint32_t *g, uint32_t *b, const uint8_t *c, uint32_t a)	/* a: 0..256 */
{
	*r = (*r * (256U - a) + c[0] * a) >> 8;
	*g = (*g * (256U - a) + c[1] * a) >> 8;
	*b = (*b * (256U - a) + c[2] * a) >> 8;
}

/* Kiyiya vuran yeni dalga (bos yuva varsa). Sk: kirilan sirt (yayilma sekli icin) */
static void ShYalamaBaslat(uint8_t Dl, const ShSirt *Sk)
{
	static const uint8_t Alta[3] = { 0, 10, 5 };					/* Kac dalgada bir panelin altina kadar (0 = hic) */
	uint32_t i;
	int x, k;
	for(i = 0; i < SH_YALAMA; i++)
	{
		ShYalama *y = &U.Sh.Yl[i];
		const float Xm = Sk ? Sk->Xm : (float)W * 0.5f, Yari = Sk ? Sk->Yari + 20.0f : (float)W;
		const float Eg = Sk ? Sk->Egim * 1.5f : 0.0f;
		if(y->On) continue;
		y->On   = 1U;
		y->T    = 0.0f;
		y->Sure = ShSure[Dl] * (0.85f + 0.3f * RandF());
		y->Eris = ShEris[Dl] * (0.6f + 0.4f * RandF());
		if(Alta[Dl] && (Rand() % Alta[Dl]) == 0U)					/* Buyuk dalga: kumu gecip alta kadar */
		{
			y->Eris = (float)H - SH_TABAN + 3.0f;
			y->Sure *= 1.35f;
		}
		{															/* Genis diller: 16 px'de bir kontrol noktasi, yumusak gecis */
			float c[W / 16U + 2U];
			for(k = 0; k < (int)(W / 16U + 2U); k++) c[k] = (RandF() - 0.5f) * 9.0f;
			for(x = 0; x < (int)W; x++)
			{
				const float u = (float)(x & 15) / 16.0f, s = u * u * (3.0f - 2.0f * u);
				y->Pr[x] = c[x >> 4] + (c[(x >> 4) + 1] - c[x >> 4]) * s + (RandF() - 0.5f) * 1.2f;
			}
			for(x = 1; x < (int)W - 1; x++) y->Pr[x] = (y->Pr[x - 1] + y->Pr[x] * 2.0f + y->Pr[x + 1]) * 0.25f;
		}
		for(x = 0; x < (int)W; x++)									/* Sirtin hizasinda one uzanir, egik gelir */
		{
			float u = ((float)x - Xm) / Yari;
			if(u < -1.0f) u = -1.0f;
			if(u > 1.0f)  u = 1.0f;
			y->Pr[x] = y->Pr[x] * (1.0f + 0.4f * (1.0f - u * u)) - y->Eris * 0.35f * u * u + Eg * u;
		}
		for(x = 0; x < (int)W; x++) y->Kp[x] = (uint8_t)(Rand() & 0xFFU);	/* Kopuk: yer yer kalin */
		for(k = 0; k < 2; k++)
			for(x = 1; x < (int)W - 1; x++) y->Kp[x] = (uint8_t)((y->Kp[x - 1] + y->Kp[x] * 2U + y->Kp[x + 1]) >> 2);
		return;
	}
}

/* Yeni dalga sirti (parca): Ana = kiyiya varinca kirilir */
static void ShSirtBaslat(uint8_t Ana)
{
	uint32_t i;
	int x, k;
	for(i = 0; i < SH_SIRT; i++)
	{
		ShSirt *s = &U.Sh.Sr[i];
		if(s->On) continue;
		s->On   = 1U; s->Ana = Ana; s->T = 0.0f;
		s->Y    = -3.0f;
		s->V    = Ana ? 10.0f + 4.0f * RandF() : 11.0f + 9.0f * RandF();	/* px/s */
		s->A    = Ana ? 1.0f : 0.35f + 0.25f * RandF();
		s->Xm   = RandF() * (float)W;
		s->Yari = Ana ? 40.0f + 40.0f * RandF() : 8.0f + 18.0f * RandF();
		s->Egim = (RandF() - 0.5f) * (Ana ? 8.0f : 5.0f);					/* Uclar arasi satir farki / 2 */
		s->Kv   = (RandF() - 0.5f) * 6.0f;									/* Orta kisim one / geri (px/s) */
		{															/* Kavis: kiyidaki dalga ucu gibi, 16 px'de bir kontrol noktasi */
			float c[W / 16U + 2U];
			for(k = 0; k < (int)(W / 16U + 2U); k++) c[k] = (RandF() - 0.5f) * 5.0f;
			for(x = 0; x < (int)W; x++)
			{
				const float u = (float)(x & 15) / 16.0f, sm = u * u * (3.0f - 2.0f * u);
				s->Pr[x] = (int8_t)((c[x >> 4] + (c[(x >> 4) + 1] - c[x >> 4]) * sm) * 16.0f);	/* 1/16 px */
			}
		}
		return;
	}
}

/* Dalga zarfi 0..1: hizli gelir (yavaslayarak, ilk %40), daha yavas cekilir */
static float ShZarf(float u)
{
	float f;
	if(u < 0.4f) { f = 1.0f - u / 0.4f; return 1.0f - f * f; }
	f = (u - 0.4f) / 0.6f;
	return 1.0f - f * f * (3.0f - 2.0f * f);
}

static void ModeShore(void)
{
	const ShRenk *R  = &ShRenkler[(SPEC_ShIsik < 2U) ? SPEC_ShIsik : 0U];
	const uint8_t Dl = (SPEC_ShDalga < 3U) ? SPEC_ShDalga : 1U;
	const float  Gdt = 1.0f / CFG_FPS;
	float   Zf[SH_YALAMA];
	uint32_t i;
	int x, y;

	if(!U.Sh.Ready)
	{
		memset(U.Sh.Islak, 0, sizeof(U.Sh.Islak));
		OrtamKayma(U.Sh.Of, 40.0f);							/* Bantlar paralel olmasin */
		ShYalamaBaslat(Dl, 0);
		U.Sh.Sonra = 0.3f;
		U.Sh.Ready = 1;
	}
	U.Sh.Kare++;
	SaatHazirla(&U.Sh.S);

	/* Zamanlama: buyuk sirt araligi = dalga omrunun ~%60'i (onceki cekilirken yenisi gelir) */
	U.Sh.Sonra -= Gdt;
	if(U.Sh.Sonra <= 0.0f)
	{
		ShSirtBaslat(1U);
		U.Sh.Sonra = ShSure[Dl] * 0.62f * (0.85f + 0.35f * RandF());
	}
	{																		/* Kucuk, kisa sirtlar: rastgele + denizde hep en az 3 */
		uint32_t k, Say = 0;
		for(k = 0; k < SH_SIRT; k++) Say += (U.Sh.Sr[k].On && !U.Sh.Sr[k].Ana) ? 1U : 0U;
		if(Say < 3U || RandF() < Gdt / 0.25f) ShSirtBaslat(0U);
	}
	if(RandF() < Gdt / 0.3f)												/* Kopuk tepecigi */
		for(i = 0; i < SH_KOPUK; i++)
			if(!U.Sh.Kt[i].Life)
			{
				U.Sh.Kt[i].X = (uint8_t)(Rand() % W); U.Sh.Kt[i].Y = (uint8_t)(2U + Rand() % 6U);
				U.Sh.Kt[i].Life = (uint8_t)(20U + Rand() % 30U);
				break;
			}

	for(i = 0; i < SH_SIRT; i++)
	{
		ShSirt *s = &U.Sh.Sr[i];
		if(!s->On) continue;
		s->Y += s->V * Gdt; s->T += Gdt;
		if(s->Y + s->Kv * s->T >= SH_TABAN - 0.5f)						/* Orta kismi kiyiya vardi */
		{
			if(s->Ana) ShYalamaBaslat(Dl, s);								/* Kirilir, kumun uzerine yayilir */
			s->On = 0U;
		}
	}
	for(i = 0; i < SH_KOPUK; i++) if(U.Sh.Kt[i].Life) U.Sh.Kt[i].Life--;
	for(i = 0; i < SH_YALAMA; i++)
	{
		ShYalama *yl = &U.Sh.Yl[i];
		Zf[i] = 0.0f;
		if(!yl->On) continue;
		yl->T += Gdt;
		if(yl->T >= yl->Sure) { yl->On = 0U; continue; }
		Zf[i] = ShZarf(yl->T / yl->Sure);
	}

	for(x = 0; x < (int)W; x++)
	{
		float Ke[SH_YALAMA], Su = SH_TABAN;									/* Her dalganin ucu, suyun bittigi satir */
		int32_t Akis = 1;															/* Kumdaki su: +1 kiyiya akar, -1 cekilir */
		for(i = 0; i < SH_YALAMA; i++)
		{
			Ke[i] = SH_TABAN + (U.Sh.Yl[i].Eris + U.Sh.Yl[i].Pr[x]) * Zf[i];
			if(U.Sh.Yl[i].On && Ke[i] > Su) { Su = Ke[i]; Akis = (U.Sh.Yl[i].T < 0.4f * U.Sh.Yl[i].Sure) ? 1 : -1; }
		}
		for(y = 0; y < (int)H; y++)
		{
			uint32_t r, g, b;
			if((float)y < SH_TABAN)											/* Acik deniz: derinden kiyiya gecis + sirtlar */
			{
				const uint32_t t = (uint32_t)((float)y * 256.0f / SH_TABAN);
				r = R->Dz1[0]; g = R->Dz1[1]; b = R->Dz1[2];
				ShRenkKat(&r, &g, &b, R->Dz2, t);
				{													/* Surekli kipirti: kiyiya akan yumusak bantlar */
					const uint32_t m = 200U + (ShDoku[((uint32_t)y * 10U + U.Sh.Of[x] + 64U - ((U.Sh.Kare >> 2) & 63U)) & 63U] * 112U >> 8);	/* +-%22 */
					r = (r * m) >> 8; g = (g * m) >> 8; b = (b * m) >> 8;
					if((Rand() % 700U) == 0U) { r += r >> 1; g += g >> 1; b += b >> 1; }	/* Seyrek parilti */
				}
				for(i = 0; i < SH_SIRT; i++)
				{
					const ShSirt *s = &U.Sh.Sr[i];
					float d, Gu;
					if(!s->On) continue;
					{
						const float u = ((float)x - s->Xm) / s->Yari;			/* -1..1: sirt boyunca */
						if(u <= -1.0f || u >= 1.0f) continue;					/* Parcanin disi */
						d  = (float)y - (s->Y + s->Egim * u + s->Kv * s->T * (1.0f - u * u) + (float)s->Pr[x] * (1.0f / 16.0f));	/* d > 0: sirtin onu */
						Gu = s->A * (1.0f - u * u) * (0.35f + 0.65f * (float)t / 256.0f);	/* Uclar soner, kiyida guclenir */
					}
					if(d > -1.3f && d < 1.3f)
					{
						uint32_t a = (uint32_t)(Gu * (1.0f - fabsf(d) / 1.3f) * 200.0f);
						if(s->Ana && s->Y > SH_TABAN - 3.0f) { ShRenkKat(&r, &g, &b, R->Kp, a); }	/* Kirilmak uzere: kopuklenir */
						else                                 { ShRenkKat(&r, &g, &b, R->Sirt, a); }
						if((Rand() % 90U) == 0U) { r += r >> 1; g += g >> 1; b += b >> 1; }			/* Isik kipirtisi */
					}
					else if(d >= 1.3f && d < 3.0f)							/* Sirtin onu: hafif cukur golgesi */
					{
						const uint32_t a = (uint32_t)(Gu * 60.0f);
						r -= (r * a) >> 8; g -= (g * a) >> 8; b -= (b * a) >> 8;
					}
				}
				for(i = 0; i < SH_KOPUK; i++)								/* Kopuk tepecigi: 1-2 px, soner */
				{
					const ShKopuk *k = &U.Sh.Kt[i];
					if(k->Life && k->Y == (uint8_t)y && (k->X == (uint8_t)x || (k->X + 1U == (uint8_t)x && (k->Life & 1U))))
						ShRenkKat(&r, &g, &b, R->Kp, (uint32_t)k->Life * 4U);
				}
			}
			else if((float)y < Su)											/* Kumun uzerindeki su: denizden kesintisiz */
			{
				const float Der = Su - (float)y;								/* Uca uzaklik */
				const uint32_t t = (uint32_t)(((float)y - SH_TABAN) * 256.0f / (Su - SH_TABAN));	/* 0 deniz kiyisi .. 256 uc */
				r = R->Dz2[0]; g = R->Dz2[1]; b = R->Dz2[2];
				ShRenkKat(&r, &g, &b, R->Sig, t);							/* Ucuna dogru sig renge acilir */
				if(Der < 4.0f) ShRenkKat(&r, &g, &b, R->Islak, (uint32_t)((4.0f - Der) * 48.0f));	/* Uc ince: kum gorunur */
				{												/* Kivrimlar: ucun seklini izler, gelirken kiyiya, cekilirken denize akar */
					const uint32_t d = ShDoku[((uint32_t)(Der * 9.0f) + U.Sh.Of[x] + (uint32_t)(Akis * (int32_t)(U.Sh.Kare >> 1)) + 1024U) & 63U];
					const uint32_t m = 200U + (d * 112U >> 8);
					r = (r * m) >> 8; g = (g * m) >> 8; b = (b * m) >> 8;
					if(d > 235U && (Rand() % 6U) == 0U) ShRenkKat(&r, &g, &b, R->Kp, 90U);	/* Kopuk izi */
				}
				U.Sh.Islak[y][x] = 255U;
				if((Rand() % 160U) == 0U) { r += r >> 1; g += g >> 1; b += b >> 1; }
			}
			else															/* Kum: kuru <-> islak; yeni acilan islak kum parlar */
			{
				const uint32_t w = U.Sh.Islak[y][x];
				r = R->Kum[0]; g = R->Kum[1]; b = R->Kum[2];
				ShRenkKat(&r, &g, &b, R->Islak, w);
				if(w > 225U) ShRenkKat(&r, &g, &b, R->Sig, (w - 225U) * 2U);
				if(w && (U.Sh.Kare % 3U) == 0U) U.Sh.Islak[y][x] = (uint8_t)(w - 1U);	/* Yavasca kurur */
			}

			/* Kopuk: her dalganin ucunda (cekilenin cizgisi yeni suyun icinde de gorunur) */
			for(i = 0; i < SH_YALAMA; i++)
			{
				const ShYalama *yl = &U.Sh.Yl[i];
				const int k = (int)Ke[i];
				const uint8_t Gel = (uint8_t)(yl->T < 0.4f * yl->Sure);
				const float   q   = Gel ? 0.0f : (yl->T / yl->Sure - 0.4f) / 0.6f;	/* Cekilme ilerlemesi 0..1 */
				uint32_t a = 0;
				if(!yl->On || Zf[i] < 0.03f || (float)y < SH_TABAN - 1.0f) continue;
				if(y == k - 1)                                           a = 150U + (Rand() & 63U);
				else if(y == k - 2 && yl->Kp[x] > 110U)                  a = 90U + (Rand() & 63U);
				else if(y == k - 3 && yl->Kp[x] > 150U && (Rand() & 3U)) a = 60U;
				else if(y >= k - 6 && y < k && (Rand() % 40U) == 0U)     a = 70U;	/* Kabarcik */
				else if(!Gel && (y == k || y == k + 1) && (Rand() % 5U) == 0U) a = 110U;	/* Cekilirken kalan */
				if(a && !Gel)											/* Cekilirken kademeli: soner + delikler yavasca buyur */
				{
					const uint32_t h = ((uint32_t)yl->Kp[x] * 7U + (uint32_t)x * 13U + (uint32_t)y * 29U) & 255U;	/* Sabit: titremez */
					if((float)h < q * 240.0f) a = 0;
					else a = (uint32_t)((float)a * (1.0f - q * q));
				}
				if(a) ShRenkKat(&r, &g, &b, R->Kp, a);
			}
			Set(x, y, (uint8_t)((r > 255U) ? 255U : r), (uint8_t)((g > 255U) ? 255U : g), (uint8_t)((b > 255U) ? 255U : b));
		}
	}

	SaatCizRgb(&U.Sh.S);
}

/* ==========================================================================
   MOD -- ORGU SERIT
   12 ince cizgiden dolanan serit: komsu cizgilerin faz farki x boyunca
   isaret degistirir, farkin sifir oldugu yerde cizgiler tek noktada birlesir
   (dugum), sonra yelpaze gibi acilir. Dugumler zamanla kayar.
   Renk cizgiden cizgiye turuncu > sari > yesil, toplamali cizilir
   (kesisimler parlar). Her cizgi bir spektrum bolgesini izler; genlik sesin
   kendi ortalamasina gore olceklenir (vurgusuz muzikte de salinir), tiz arttikca
   serit daha cok acilir, vurusta faz ve genlik sicrar. Durum U.We icinde.
   ========================================================================== */
#define WV_LINES	12


static void ModeWeave(void)
{
	float    Lev[WV_LINES], Imp = 0.0f, Lv, Rel, Tgt, D0t, Q;
	uint32_t i, x, Lum;
	const uint32_t Gs = SPEC_BAND_COUNT / WV_LINES;

	/* Vurus zarfi: Tepki ayari SPEC_Draw'da Kick/Hit'i zaten suzdu */
	if(SPEC_Kick)		Imp = SPEC_KickPower;
	else if(SPEC_Hit)	Imp = 0.6f * SPEC_HitPower;
	U.We.Pump -= DT / CFG_BEAT_DECAY_S;
	if(U.We.Pump < 0.0f) U.We.Pump = 0.0f;
	if(Imp > U.We.Pump)  U.We.Pump = Imp;

	/* Cizgi basina spektrum bolgesi seviyesi */
	for(i = 0; i < WV_LINES; i++)
	{
		float Sum = 0.0f;
		uint32_t k;
		for(k = i * Gs; k < (i + 1U) * Gs; k++) Sum += SPEC_Level[k];
		Lev[i] = Sum / (float)Gs;
	}

	/* Genlik: sesin kendi ortalamasina gore (vurgusuz muzikte de salinir) */
	Lv = 0.5f * SPEC_Volume + 0.5f * SPEC_Bass;
	U.We.Avg += (Lv - U.We.Avg) * 0.004f;
	Rel = Lv / (2.0f * U.We.Avg + 0.05f);
	if(Rel > 1.0f) Rel = 1.0f;
	Tgt = 4.0f + 11.0f * (0.4f * ((Lv > 1.0f) ? 1.0f : Lv) + 0.6f * Rel) + 3.0f * U.We.Pump;
	if(Tgt > 15.0f) Tgt = 15.0f;
	U.We.Amp += (Tgt - U.We.Amp) * ((Tgt > U.We.Amp) ? 0.35f : 0.12f);

	/* Serit acikligi: tiz arttikca daha cok acilir */
	D0t = 0.05f + 0.06f * ((SPEC_Treble > 1.0f) ? 1.0f : SPEC_Treble) + 0.03f * U.We.Pump;
	U.We.D0 += (D0t - U.We.D0) * 0.03f;

	/* Faz ve dugum konumu: hiz sese bagli, vurusta sicrar */
	U.We.Ph += 0.20f * DT * (0.5f + 1.6f * SPEC_Volume) + 0.15f * Imp;
	U.We.Th += 0.05f * DT + 0.05f * Imp;
	U.We.Ph -= floorf(U.We.Ph);
	U.We.Th -= floorf(U.We.Th);

	Q = (U.We.Amp - 3.0f) * (1.0f / 9.0f);
	if(Q < 0.0f) Q = 0.0f;
	if(Q > 1.0f) Q = 1.0f;
	Lum = (uint32_t)(55.0f + 100.0f * Q + 40.0f * U.We.Pump);
	if(Lum > 256U) Lum = 256U;

	Clear();
	if(!U.We.Ready) { StarInit(&U.We.St); U.We.Ready = 1; }
	StarDraw(&U.We.St, U.We.Pump);				/* Arkada uzay: ortak yildiz alani */

	for(i = 0; i < WV_LINES; i++)
	{
		const float    Ai = U.We.Amp * (0.8f + 0.4f * ((Lev[i] > 1.0f) ? 1.0f : Lev[i]));
		const float    Fi = (float)i - 0.5f * (float)(WV_LINES - 1);
		uint8_t        pc[3];
		uint32_t       R, G, B;
		int yPrev = -1;

		PalColor(SPEC_Pal, i * 768U / (WV_LINES - 1U), pc);			/* Cizgi rengi: ailenin 4 duragi */
		R = ((uint32_t)pc[0] * Lum) >> 8;
		G = ((uint32_t)pc[1] * Lum) >> 8;
		B = ((uint32_t)pc[2] * Lum) >> 8;

		for(x = 0; x < W; x++)
		{
			const float    u    = (float)x / (float)W;
			const uint32_t di   = (uint32_t)((1.2f * u + U.We.Th) * 256.0f) & 255U;
			const float    diff = U.We.D0 * (float)SinLut[di] * (1.0f / 127.0f);	/* Komsu faz farki */
			const float    ph   = 1.4f * u + U.We.Ph + Fi * diff;
			const uint32_t pi   = (uint32_t)(int32_t)(ph * 256.0f) & 255U;
			const float    Env  = 0.25f + 0.75f * (float)SinLut[(x * 128U) / (W - 1U)] * (1.0f / 127.0f);
			const float    yc   = ((float)H - 1.0f) * 0.5f + Ai * Env * (float)SinLut[pi] * (1.0f / 127.0f);
			const int      y    = (int)(yc + 0.5f);
			int y0, y1, yy;

			if(yPrev < 0) yPrev = y;
			y0 = (y < yPrev) ? y : yPrev;						/* Dik kisimda bosluk kalmasin */
			y1 = (y < yPrev) ? yPrev : y;
			for(yy = y0; yy <= y1; yy++)
				Add((int)x, yy, (uint8_t)R, (uint8_t)G, (uint8_t)B);
			yPrev = y;
		}
	}
}

/* ==========================================================================
   Public
   ========================================================================== */
void SPEC_DisplayInit(void)
{
	uint32_t i;

	CanvasBuf = (uint8_t (*)[3])(void *)HUB75_GetDrawBuffer();

	for(i = 0; i < 256U; i++)
		SinLut[i] = (int8_t)(127.0f * sinf(2.0f * 3.14159265f * (float)i / 256.0f));

	SPEC_SetFirePalette(0, 0, 0, 0);
	BuildCoolMap();

	SPEC_SetMode(SPEC_MODE_CLASSIC);			/* Gercek acilis modu: Settings */
}

void SPEC_SetMode(SPEC_Mode NewMode)
{
	Mode = (SPEC_Mode)((uint32_t)NewMode % SPEC_MODE_COUNT);
	SPEC_Speed = 2U;							/* Gorunum kendi degerini sonra yazar */
	SPEC_React = 1U;
	Nb   = (Mode == SPEC_MODE_CLASSIC) ? SPEC_ClsBands() : ModeBands[Mode];

	/* Cubuk modlari gercek spektrum sekli (dusuk punch, ust pay, yumusatma);
	   alev ve efekt modlari ritim odakli (yuksek punch).				*/
	switch(Mode)
	{
		case SPEC_MODE_CLASSIC:
		case SPEC_MODE_SPECWAVE:
		case SPEC_MODE_LAYERS:
		case SPEC_MODE_WEAVE:
		case SPEC_MODE_AURORA:
			SPEC_Punch    = CFG_PUNCH_BARS;
			SPEC_Headroom = CFG_BARS_HEADROOM_DB;
			BarSmooth     = 1;
			break;
		default:
			SPEC_Punch    = CFG_PUNCH;
			SPEC_Headroom = 0.0f;
			BarSmooth     = 0;
			break;
	}

	memset(&U, 0, sizeof(U));
	memset(Bar, 0, sizeof(Bar));
	memset(Peak, 0, sizeof(Peak));
	memset(PeakVel, 0, sizeof(PeakVel));
	memset(PeakHold, 0, sizeof(PeakHold));
	memset(PrevLvl, 0, sizeof(PrevLvl));
	memset(BarH, 0, sizeof(BarH));
	Clear();

	Glow = 0.0f;
	CBass = 0.0f;
	memset(SWaves, 0, sizeof(SWaves));
	SwvReset();
	SwPump = 0.0f;
	memset(Waves, 0, sizeof(Waves));
	memset(FuelLvl, 0, sizeof(FuelLvl));

	ModeShowFrames = (uint32_t)((float)CFG_SHOW_MODE_MS * CFG_FPS / 1000.0f);
}

/* ==========================================================================
   OSD -- ust bant (panel menusu). 5x7 buyuk harf font (Turkce), koyu zemin.
   Mod cizildikten sonra ust 9 (renkte 12) satir karartilip uzerine yazilir.
   ========================================================================== */
typedef struct { uint16_t Kod; uint8_t S[7]; } Harf57;

static const Harf57 Font57[] =
{
	{' ',{0,0,0,0,0,0,0}},
	{'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
	{'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}}, {'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
	{'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
	{'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}}, {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
	{'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
	{'A',{0x0E,0x11,0x11,0x11,0x1F,0x11,0x11}}, {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
	{'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}}, {'D',{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},
	{'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
	{'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}}, {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
	{'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}}, {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
	{'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
	{'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, {'N',{0x11,0x11,0x19,0x15,0x13,0x11,0x11}},
	{'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
	{'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
	{'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
	{'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
	{'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0A}}, {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
	{'Y',{0x11,0x11,0x11,0x0A,0x04,0x04,0x04}}, {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
	{0x00C7,{0x0E,0x11,0x10,0x10,0x11,0x0E,0x04}},	/* C cedilla */
	{0x011E,{0x0E,0x00,0x0F,0x10,0x17,0x11,0x0F}},	/* G breve */
	{0x0130,{0x04,0x00,0x0E,0x04,0x04,0x04,0x0E}},	/* I noktali */
	{0x00D6,{0x0A,0x00,0x0E,0x11,0x11,0x11,0x0E}},	/* O umlaut */
	{0x015E,{0x0F,0x10,0x0E,0x01,0x1E,0x04,0x0C}},	/* S cedilla */
	{0x00DC,{0x0A,0x00,0x11,0x11,0x11,0x11,0x0E}},	/* U umlaut */
	{':',{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}}, {'-',{0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
	{'.',{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}}, {',',{0x00,0x00,0x00,0x00,0x0C,0x04,0x08}},
	{'%',{0x18,0x19,0x02,0x04,0x08,0x13,0x03}}, {'/',{0x00,0x01,0x02,0x04,0x08,0x10,0x00}},
	{'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}}, {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
	{'=',{0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}}, {0x00B0,{0x06,0x09,0x09,0x06,0x00,0x00,0x00}},	/* derece */
	{'?',{0x0E,0x11,0x01,0x02,0x04,0x00,0x04}},
};

static char		OsdMetin[48];
static int16_t	OsdTon = -1;
static uint32_t	OsdKare;

/* UTF-8 -> kod noktasi, Turkce buyuk harf (i -> I noktali, dotless i -> I) */
static uint16_t OsdHarf(const char **s)
{
	const uint8_t *p = (const uint8_t *)*s;
	uint16_t c;

	if(p[0] < 0x80U)                        { c = p[0]; *s += 1; }
	else if((p[0] & 0xE0U) == 0xC0U && p[1]) { c = (uint16_t)(((p[0] & 0x1FU) << 6) | (p[1] & 0x3FU)); *s += 2; }
	else                                    { c = '?'; *s += 1; while((**s & 0xC0) == 0x80) (*s)++; }

	if(c == 'i')                 return 0x0130;
	if(c >= 'a' && c <= 'z')     return (uint16_t)(c - 32U);
	switch(c)
	{
	case 0x0131: return 'I';	case 0x00E7: return 0x00C7;	case 0x011F: return 0x011E;
	case 0x00F6: return 0x00D6;	case 0x015F: return 0x015E;	case 0x00FC: return 0x00DC;
	default:     return c;
	}
}

static const uint8_t *OsdGlif(uint16_t c)
{
	for(uint32_t i = 0; i < sizeof(Font57) / sizeof(Font57[0]); i++)
		if(Font57[i].Kod == c) return Font57[i].S;
	return Font57[sizeof(Font57) / sizeof(Font57[0]) - 1U].S;	/* '?' */
}

uint16_t SPEC_OsdGenislik(const char *Metin)
{
	uint16_t n = 0;
	while(*Metin) { (void)OsdHarf(&Metin); n++; }
	return (uint16_t)(n ? n * 6U - 1U : 0U);
}

void SPEC_Osd(const char *Metin, int16_t Ton, uint16_t Ms)
{
	strncpy(OsdMetin, Metin, sizeof(OsdMetin) - 1U);
	OsdMetin[sizeof(OsdMetin) - 1U] = 0;
	OsdTon  = Ton;
	OsdKare = (uint32_t)((float)Ms * CFG_FPS / 1000.0f);
	ModeShowFrames = 0;							/* Mod numarasi ile cakismasin */
}

static void OsdCiz(void)
{
	const int Bant = (OsdTon >= 0) ? 13 : 9;
	const char *s = OsdMetin;
	int x, y, x0;

	if(!OsdKare) return;
	OsdKare--;

	for(y = 0; y < Bant; y++)							/* Zemin: animasyonu karart */
		for(x = 0; x < (int)W; x++)
		{
			uint8_t *p = CV(y, x);
			p[0] >>= 3; p[1] >>= 3; p[2] >>= 3;
		}

	x0 = ((int)W - (int)SPEC_OsdGenislik(OsdMetin)) / 2;
	if(x0 < 0) x0 = 0;
	while(*s && x0 < (int)W)
	{
		const uint8_t *g = OsdGlif(OsdHarf(&s));
		for(y = 0; y < 7; y++)
			for(x = 0; x < 5; x++)
				if(g[y] & (0x10U >> x)) Set(x0 + x, 1 + y, 220, 225, 235);
		x0 += 6;
	}

	if(OsdTon >= 0)										/* Renk: beyaz + gokkusagi seridi + imlec */
	{
		const int Bz = 6;								/* Bastaki beyaz bolum (px) */
		const int ix = (OsdTon >= SPEC_OSD_BEYAZ) ? Bz / 2 : Bz + (int)((uint32_t)OsdTon * (W - 1U - Bz) / 359U);
		for(x = 0; x < (int)W; x++)
		{
			uint8_t r = 255, g = 255, b = 255;
			if(x >= Bz) Hue((uint32_t)(x - Bz) * 767U / (W - 1U - Bz), &r, &g, &b);
			Set(x, 10, r >> 1, g >> 1, b >> 1);
			Set(x, 11, r >> 1, g >> 1, b >> 1);
		}
		for(y = 9; y < 13; y++) Set(ix, y, 255, 255, 255);
		Set(ix - 1, 12, 255, 255, 255); Set(ix + 1, 12, 255, 255, 255);
	}
}

static uint8_t	LevelVal = 0;
static uint16_t	LevelShowFrames = 0;

void SPEC_ShowLevel(uint8_t Percent)
{
	LevelVal        = Percent;
	LevelShowFrames = (uint16_t)(1.5f * CFG_FPS);
	ModeShowFrames  = 0;
}

void SPEC_Draw(void)
{
	/* Hiz: tempo carpani (120 BPM = 1) x kademe. Sadece animasyon hareketi ve
	   renk donmesi; cubuk/tepe dusus hizi gercek zamanli (UpdateBars).	*/
	{
		static const float Kat[4] = { 0.5f, 0.7f, 1.0f, 1.3f };	/* Animasyon: Yavas, Normal, Yuksek, Cok yuksek */
		static float Kt = 1.0f;
		float Bpm = SPEC_GetBpm();
		float k = (Bpm > 0.0f) ? Bpm / 120.0f : 1.0f;
		k = (k < 0.75f) ? 0.75f : (k > 1.6f) ? 1.6f : k;
		Kt += (k - Kt) * 0.02f;						/* tempo sicramasin	*/
		SpdDT = Kt * Kat[(SPEC_Speed < 4U) ? SPEC_Speed : 2U] / CFG_FPS;
	}

	/* Tepki: hangi vuruslar efekt tetikler */
	if(SPEC_React == 0U)
	{
		SPEC_Hit = 0;
		if(SPEC_Kick && SPEC_KickPower < 0.45f) SPEC_Kick = 0;
	}
	else if(SPEC_React == 2U && SPEC_Hit && !SPEC_Kick && SPEC_HitPower > 0.3f)
	{
		SPEC_Kick = 1;
		if(SPEC_KickPower < SPEC_HitPower) SPEC_KickPower = SPEC_HitPower;
	}

	ComputeLevels();
	UpdateBars();

	switch(Mode)
	{
		case SPEC_MODE_FIRE:		ModeFire();			break;
		case SPEC_MODE_FIRE_BARS:	ModeFireBars();		break;
		case SPEC_MODE_FIRE_BLUE:	ModeFireBlue();		break;
		case SPEC_MODE_BLAST:		ModeBlast();		break;
		case SPEC_MODE_CBLAST:		ModeCenterBlast();	break;
		case SPEC_MODE_FIRE_BALL:	ModeFireBall();		break;
		case SPEC_MODE_SPECWAVE:	ModeSpecWave();		break;
		case SPEC_MODE_LIGHTLINE:	ModeLightLine();	break;
		case SPEC_MODE_LAYERS:		ModeLayers();		break;
		case SPEC_MODE_WEAVE:		ModeWeave();		break;
		case SPEC_MODE_AURORA:		ModeAurora();		break;
		case SPEC_MODE_SNOW:		ModeSnow();			break;
		case SPEC_MODE_HEARTH:		ModeHearth();		break;
		case SPEC_MODE_RAIN:		ModeRain();			break;
		case SPEC_MODE_SHORE:		ModeShore();		break;
		case SPEC_MODE_RINGS:		ModeRings();		break;
		case SPEC_MODE_FOUNTAIN:	ModeFountain();		break;
		case SPEC_MODE_WAVE:		ModeWave();			break;
		case SPEC_MODE_CLASSIC:
		default:					ModeClassic();		break;
	}

	memcpy(PrevLvl, Lvl, sizeof(PrevLvl));
	OsdCiz();

#if(CFG_SHOW_FREQ)
	if(SPEC_PeakValid)
	{
		uint32_t Hz = (uint32_t)(SPEC_PeakHz + 0.5f);
		uint32_t Digits = (Hz >= 10000U) ? 5U : (Hz >= 1000U) ? 4U : (Hz >= 100U) ? 3U : (Hz >= 10U) ? 2U : 1U;
		DrawNumber((int)(W - Digits * 4U), Hz, 0, 255, 255);
	}
#endif

#if(CFG_SHOW_OVERRUN)
	{
		/* Test satirlari (sag ust, son 1 s'nin en buyugu):
		   1) kacan ses karesi sayaci  2) SPEC_Process us
		   3) SPEC_Draw us              4) HUB75_UpdateDisplay us
		   Butce: 2+3+4 toplami < 10600 us olmali.					*/
		static uint32_t Mx[3], Show[3], Cnt = 0;
		uint32_t Val[4], j;
		uint32_t Now[3] = { SPEC_ProcessUs, SPEC_DrawUs, SPEC_UpdateUs };
		for(j = 0; j < 3U; j++) if(Now[j] > Mx[j]) Mx[j] = Now[j];
		if(++Cnt >= 94U) { Cnt = 0; for(j = 0; j < 3U; j++) { Show[j] = Mx[j]; Mx[j] = 0; } }
		Val[0] = AUDIO_OverrunCount; Val[1] = Show[0]; Val[2] = Show[1]; Val[3] = Show[2];
		for(j = 0; j < 4U; j++)
		{
			uint32_t d = 1U, t = Val[j];
			while(t >= 10U) { t /= 10U; d++; }
			DrawNumberT((int)(W - d * 4U), 1 + (int)j * 7, Val[j]);
		}
	}
#endif

	if(ModeShowFrames)
	{
		ModeShowFrames--;
		DrawNumber57T(1, 1, (uint32_t)SPEC_ShowNo);
	}
	if(LevelShowFrames)								/* Hizli ayar: alt kenarda seviye cubugu */
	{
		const uint32_t n = (uint32_t)LevelVal * W / 100U;
		uint32_t x;
		LevelShowFrames--;
		for(x = 0; x < W; x++)
		{
			const uint8_t On = (x < n);
			Set((int)x, H - 2, 0, On ? 200U : 25U, On ? 255U : 30U);
			Set((int)x, H - 1, 0, On ? 140U : 18U, On ? 180U : 22U);
		}
		DrawNumber57T(1, 1, (uint32_t)LevelVal);
	}


	/* Vurusta panel parlakligi pompalanir */
	{
		uint32_t Br = SPEC_Brightness +
		              (uint32_t)((float)(CFG_BEAT_BRIGHT_BOOST * SPEC_Brightness) / 100.0f * BeatEnv);
		HUB75_SetBrightness((unsigned char)((Br > 100U) ? 100U : Br));
	}
}
