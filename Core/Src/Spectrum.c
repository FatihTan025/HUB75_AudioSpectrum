/**
  ******************************************************************************
  * @file    Spectrum.c
  * @brief   Pencere + FFT + logaritmik bantlar + AGC + ritim algilama
  ******************************************************************************
  * FFT: kendi radix-2 karmasik FFT'miz (float, FPU). CMSIS-DSP gerekmez.
  * Bant gucu: genis bantta bin TOPLAMI, 2 bin'den dar bantta merkez
  * frekansta komsu bin'lerden interpolasyon (gercek logaritmik eksen).
  * Ritim: bas araligi gucu, kendi kayan ortalamasinin CFG_BEAT_THRESHOLD
  * katini yukselerek gecerse vurus.
  ******************************************************************************
  */
/* Debug yapilandirmasi -O0 derler: FFT ve cizim 3-5 kat yavaslar, ses
   kareleri kacar. Bu dosya her yapilandirmada hizli derlensin.		*/
#pragma GCC optimize ("O2")

#include "Spectrum.h"
#include <math.h>

#define N			AUDIO_FFT_SIZE
#define HALF_N		(N / 2U)
#define PI_F		3.14159265358979f
#define BIN_HZ		((float)AUDIO_SAMPLE_RATE / (float)N)
#define DT_FRAME	(1.0f / CFG_FPS)

#if(N == 2048U)
	#define LOG2N	11U
#else
	#define LOG2N	10U
#endif

float				SPEC_Level[SPEC_BAND_COUNT];
float				SPEC_Volume		= 0.0f;
float				SPEC_Bass		= 0.0f;
float				SPEC_Treble		= 0.0f;
uint8_t				SPEC_Beat		= 0;
uint8_t				SPEC_Kick		= 0;
float				SPEC_KickPower	= 0.0f;
uint8_t				SPEC_Hit		= 0;
float				SPEC_HitPower	= 0.0f;
float				SPEC_HitLow		= 0.0f;
float				SPEC_BassMode	= 0.0f;
uint8_t				SPEC_KickMain	= 0;
float				SPEC_HitHigh	= 0.0f;
uint8_t				SPEC_FrameStep	= 1;
float				SPEC_Punch		= CFG_PUNCH;	/* Mod basina: cubuk modlari dusuk */
float				SPEC_LoudRel	= 1.0f;			/* Muzigin kendi seviyesine gore 0..1 */
float				SPEC_Headroom	= 0.0f;			/* Duz dB olcegin ust payi (mod basina), dB */
float				SPEC_Wave[SPEC_WAVE_POINTS];
float				SPEC_PeakHz		= 0.0f;
uint8_t				SPEC_PeakValid	= 0;
volatile uint32_t	SPEC_ProcessUs	= 0;
volatile uint32_t	SPEC_DrawUs		= 0;			/* main.c olcer: SPEC_Draw		*/
volatile uint32_t	SPEC_UpdateUs	= 0;			/* main.c olcer: HUB75_UpdateDisplay */

static float		Re[HALF_N];						/* Reel FFT: N/2 karmasik nokta yeter */
static float		Im[HALF_N];
static float		HalfWindow[HALF_N];				/* Hann simetrik, yarisi	*/
static float		CosTable[HALF_N];
static float		SinTable[HALF_N];
static uint16_t		BandLo[SPEC_BAND_COUNT];		/* Toplanacak bin [Lo, Hi)	*/
static uint16_t		BandHi[SPEC_BAND_COUNT];
static float		BandCenter[SPEC_BAND_COUNT];	/* Kesirli bin				*/
static float		BandTilt[SPEC_BAND_COUNT];
static float		BandWid[SPEC_BAND_COUNT];		/* Bant genisligi, bin (>= 1)	*/
static uint8_t		BassBands, TrebleFirst;
static uint16_t		BeatLo, BeatHi;
static uint16_t		KickLo, KickHi, MidLo, MidHi;
uint8_t				SPEC_Sens = CFG_SENS;
static float		KickRatio = CFG_KICK_RATIO, KickDbBelow = CFG_KICK_DB_BELOW,	/* SPEC_SetSens kurar */
					KickDom = CFG_KICK_DOM, HitRatio = CFG_HIT_RATIO;
static float		KickAvg = 0.0f, KickPrev = 0.0f, KickSince = 99.0f;
static uint32_t		KickGap = 0;
/* Tempo izci: bas yukselis gecmisi, otokorelasyon ile periyot, tarak ile faz */
#define TEMPO_LEN		256U
static float		OnHist[TEMPO_LEN];
static uint16_t		OnPos = 0, TempoTick = 0;
static float		KickDbPrev = -120.0f;
static float		BeatPeriod = 0.0f;				/* Kare cinsinden, 0 = yok		*/
static float		BeatConf   = 0.0f;				/* 0..1 tempo guveni			*/
static float		BeatNext   = 0.0f;				/* Siradaki beklenen vurusa kare	*/
static float		MainGap    = 9.0f;				/* Son ana vurustan beri, s		*/
static uint8_t		BeatLost   = 0;					/* Ust uste guvensiz tempo sayisi	*/
static float		BandDbPrev[SPEC_BAND_COUNT];
static float		FluxAvg = 0.0f, FluxPrev = 0.0f, FluxMax = 1e-3f;
static uint32_t		HitGap = 0;
static uint8_t		LowBands = 1;					/* <150 Hz bant sayisi	*/
static uint8_t		HighFrom = 255;					/* >4 kHz ilk bant		*/
static float		KickRefDb = -200.0f;			/* Son ana vuruslarin ort. dB	*/
static float		HitRef    = 0.0f;				/* Son vuruslarin ort. akisi	*/
static float		LoudRefDb = -200.0f;			/* Muzigin uzun sureli seviyesi	*/
static float		StatTime  = 0.0f;				/* Kesintisiz duragan ses suresi, s	*/
static float		ToneShare = 0.0f;				/* Gucun tepe banda yakin payi 0..1	*/
static float		NoiseTotDb = -200.0f;			/* Ogrenilmis gurultunun toplami, dB	*/
static float		MusicGate  = 1.0f;				/* 0: sadece gurultu, 1: muzik var	*/
static float		AgcTop		= CFG_DB_FLOOR_TOP;
static float		RefPowerInv;
static float		BeatAvg		= 0.0f;
static float		BeatPrev	= 0.0f;
static uint32_t		BeatGap		= 0;				/* Son vurustan beri kare	*/
static float		WavePeak	= 1.0f;
static float		BandAvg[SPEC_BAND_COUNT];		/* Bant genligi, yavas ort.	*/
static float		BandMax[SPEC_BAND_COUNT];		/* Bant genligi, tepe izci	*/
static float		PowFast[SPEC_BAND_COUNT];		/* ~30 ms ortalama guc (esik icin) */
static float		PowSlow[SPEC_BAND_COUNT];		/* ~0.3 s ortalama guc			*/
static float		NoiseP[SPEC_BAND_COUNT];		/* Gurultu tabani, guc			*/
static float		SilenceGain = 0.0f;				/* Genel sessizlik kapisi	*/
static uint8_t		NoiseInit = 0;
static float		GMinDb = 0.0f;					/* Toplam gucun yavas minimumu	*/
static float		TAvgDb = 0.0f, TVarDb = 10.0f;	/* Toplam gucun ort. ve sapmasi	*/
static uint8_t		Quiet = 1;						/* Su an sessiz mi?				*/

/* Gercek sinyal hilesi: N reel ornek, N/2 noktali karmasik FFT ile cozulur
   (cift ornekler Re, tek ornekler Im). FFT suresi yaklasik yariya iner. */
#define LOG2M		(LOG2N - 1U)

static inline uint32_t BitRev(uint32_t x)
{
#if defined(__ARM_ARCH)
	return __RBIT(x) >> (32U - LOG2M);
#else
	uint32_t r = 0, k;
	for(k = 0; k < LOG2M; k++) if(x & (1U << k)) r |= 1U << (LOG2M - 1U - k);
	return r;
#endif
}

/* ==========================================================================
   Tablolar
   ========================================================================== */
/* Frekans araligi: 128 log bandin sinirlari Lo..Hi Hz arasinda yeniden kurulur.
   Acilista ve ayar degisince cagrilir. */
void SPEC_SetRange(float FLo, float FHi)
{
	uint32_t i;
	float    Ratio;

	if(FLo < 20.0f) FLo = 20.0f;
	if(FHi > 20000.0f) FHi = 20000.0f;
	if(FHi < FLo * 4.0f) FHi = FLo * 4.0f;	/* En az iki oktav */
	Ratio = FHi / FLo;
	LowBands = 1; HighFrom = 255;
	BassBands = 0; TrebleFirst = SPEC_BAND_COUNT;

	for(i = 0; i < SPEC_BAND_COUNT; i++)
	{
		float Lo = FLo * powf(Ratio, (float)i        / (float)SPEC_BAND_COUNT) / BIN_HZ;
		float Hi = FLo * powf(Ratio, (float)(i + 1U) / (float)SPEC_BAND_COUNT) / BIN_HZ;
		uint32_t L = (uint32_t)(Lo + 0.5f);
		uint32_t H = (uint32_t)(Hi + 0.5f);

		if(L < 1U) L = 1U;
		if(H > HALF_N) H = HALF_N;

		BandLo[i]     = (uint16_t)L;
		BandHi[i]     = (uint16_t)H;
		BandCenter[i] = sqrtf(Lo * Hi);
		BandWid[i]    = (Hi - Lo > 1.0f) ? (Hi - Lo) : 1.0f;
		if(BandCenter[i] < 1.0f) BandCenter[i] = 1.0f;

		if(BandCenter[i] * BIN_HZ < 250.0f)  BassBands = (uint8_t)(i + 1U);
		if(BandCenter[i] * BIN_HZ < 150.0f)  LowBands  = (uint8_t)(i + 1U);
		if(BandCenter[i] * BIN_HZ > 4000.0f && HighFrom == 255) HighFrom = (uint8_t)i;
		if(BandCenter[i] * BIN_HZ > 4000.0f && TrebleFirst == SPEC_BAND_COUNT) TrebleFirst = (uint8_t)i;
	}
	if(BassBands == 0U) BassBands = 1U;

	for(i = 0; i < SPEC_BAND_COUNT; i++)
		BandTilt[i] = CFG_TILT_DB_OCT * log2f(BandCenter[i] / BandCenter[0]);

}

/* Vurus hassasiyeti 0..100: Bas = kick algilayici, Tiz = genel vurus (zil, trampet).
   Oranlar geometrik (her adim ayni yuzde), dB sinirlari lineer; 50'de CFG degerleri
   aynen. 0 = secici, 100 = hassas. Mod basina ayar (Settings, Hareket grubu). */
void SPEC_SetSens(uint8_t Bas, uint8_t Tiz)
{
	const float t = (float)((Bas > 100U) ? 100U : Bas) / 100.0f;
	const float u = (float)((Tiz > 100U) ? 100U : Tiz) / 100.0f;

	SPEC_Sens   = (Bas > 100U) ? 100U : Bas;
	KickRatio   = CFG_KICK_RATIO * 1.5f * powf(4.0f / 9.0f, t);	/* 3.0 > 2.0 > 1.33		*/
	KickDbBelow = CFG_KICK_DB_BELOW + 16.0f * (t - 0.5f);	/* 12 > 20 > 28 dB		*/
	KickDom     = CFG_KICK_DOM * 2.0f * powf(0.25f, t);		/* 1.0 > 0.5 > 0.25		*/
	HitRatio    = CFG_HIT_RATIO * 1.5f * powf(4.0f / 9.0f, u);	/* 2.7 > 1.8 > 1.2		*/
}

void SPEC_Init(void)
{
	uint32_t i;

	for(i = 0; i < HALF_N; i++)
	{
		HalfWindow[i] = 0.5f - 0.5f * cosf(2.0f * PI_F * (float)i / (float)(N - 1U));
		CosTable[i]   =  cosf(2.0f * PI_F * (float)i / (float)N);
		SinTable[i]   = -sinf(2.0f * PI_F * (float)i / (float)N);
	}

	SPEC_SetRange(CFG_FREQ_LOW, CFG_FREQ_HIGH);

	BeatLo = (uint16_t)(CFG_BEAT_FREQ_LOW  / BIN_HZ + 0.5f);
	BeatHi = (uint16_t)(CFG_BEAT_FREQ_HIGH / BIN_HZ + 0.5f);
	if(BeatLo < 1U) BeatLo = 1U;
	if(BeatHi <= BeatLo) BeatHi = BeatLo + 1U;

	/* Kick araligi: en dusuk bas (bin merkezleri 40..100 Hz) */
	KickLo = (uint16_t)(CFG_KICK_FREQ_LOW  / BIN_HZ + 0.5f);
	KickHi = (uint16_t)(CFG_KICK_FREQ_HIGH / BIN_HZ + 0.5f) + 1U;
	if(KickLo < 1U) KickLo = 1U;
	if(KickHi <= KickLo) KickHi = KickLo + 1U;

	/* Alt-orta bant: bas baskinligi icin (ses, piyano) */
	MidLo = (uint16_t)(CFG_MID_FREQ_LOW  / BIN_HZ + 0.5f);
	MidHi = (uint16_t)(CFG_MID_FREQ_HIGH / BIN_HZ + 0.5f) + 1U;
	if(MidLo < KickHi) MidLo = KickHi;
	if(MidHi <= MidLo) MidHi = MidLo + 1U;
	SPEC_SetSens(CFG_SENS, CFG_SENS);

	/* 0 dB referansi: tam olcek sinus (genlik 2048), Hann kazanci 0.5 */
	{
		float RefAmp = 2048.0f * (float)HALF_N * 0.5f;
		RefPowerInv  = 1.0f / (RefAmp * RefAmp);
	}

	for(i = 0; i < SPEC_BAND_COUNT; i++)  SPEC_Level[i] = 0.0f;
	for(i = 0; i < SPEC_WAVE_POINTS; i++) SPEC_Wave[i]  = 0.0f;

#if defined(__ARM_ARCH)
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

/* ==========================================================================
   Yerinde radix-2 FFT (Re/Im bit-ters sirada)
   ========================================================================== */
static void FFT_Run(void)
{
	uint32_t Size, HalfSize, Step, Start, j;

	for(Size = 2; Size <= HALF_N; Size <<= 1)			/* N/2 noktali karmasik FFT */
	{
		HalfSize = Size >> 1;
		Step     = N / Size;							/* W_Size^j = W_N^(j*N/Size) */

		for(Start = 0; Start < HALF_N; Start += Size)
		{
			for(j = 0; j < HalfSize; j++)
			{
				uint32_t a = Start + j;
				uint32_t b = a + HalfSize;
				float Wr = CosTable[j * Step];
				float Wi = SinTable[j * Step];
				float Tr = Re[b] * Wr - Im[b] * Wi;
				float Ti = Re[b] * Wi + Im[b] * Wr;

				Re[b] = Re[a] - Tr;
				Im[b] = Im[a] - Ti;
				Re[a] += Tr;
				Im[a] += Ti;
			}
		}
	}
}

/* ==========================================================================
   Kare isleme
   ========================================================================== */
void SPEC_Process(const uint16_t *Samples)
{
	uint32_t	i, k;
	float		Mean = 0.0f;
	float		Db[SPEC_BAND_COUNT];
	float		BandPow[SPEC_BAND_COUNT];
	float		MaxDb = -200.0f;
	float		Bottom, Sum;
	float		Flux = 0.0f, FluxLow = 0.0f, FluxHigh = 0.0f;
#if defined(__ARM_ARCH)
	uint32_t	StartCycle = DWT->CYCCNT;
#endif
	uint32_t	PeakBin = 1;
	float		PeakPow = 0.0f;

	/* DC (VDD/2 ofseti) */
	for(i = 0; i < N; i++)
		Mean += (float)Samples[i];
	Mean /= (float)N;

	/* Dalga formu: son ornekler, 4'te bir, otomatik kazanc */
	{
		float m = 0.0f;
		const uint16_t *W = &Samples[N - SPEC_WAVE_POINTS * 4U];

		for(i = 0; i < SPEC_WAVE_POINTS; i++)
		{
			float v = (float)W[i * 4U] - Mean;
			SPEC_Wave[i] = v;
			if(fabsf(v) > m) m = fabsf(v);
		}

		/* Tepe aninda yukselir, yavas duser; 40 LSB altina inmez */
		if(m > WavePeak) WavePeak = m;
		else             WavePeak += (m - WavePeak) * 0.02f;
		if(WavePeak < 40.0f) WavePeak = 40.0f;

		for(i = 0; i < SPEC_WAVE_POINTS; i++)
		{
			float v = SPEC_Wave[i] / WavePeak;
			SPEC_Wave[i] = (v > 1.0f) ? 1.0f : ((v < -1.0f) ? -1.0f : v);
		}
	}

	/* Pencere + paketleme: z[n] = x[2n] + j x[2n+1], bit-ters yerlesim */
	for(i = 0; i < HALF_N; i++)
	{
		uint32_t r  = BitRev(i);
		uint32_t i0 = 2U * i, i1 = i0 + 1U;
		float    w0 = HalfWindow[(i0 < HALF_N) ? i0 : (N - 1U - i0)];
		float    w1 = HalfWindow[(i1 < HALF_N) ? i1 : (N - 1U - i1)];
		Re[r] = ((float)Samples[i0] - Mean) * w0;
		Im[r] = ((float)Samples[i1] - Mean) * w1;
	}

	FFT_Run();

	/* Ayristirma: X[k] = E[k] + W^k O[k]; k ve N/2-k birlikte, guc Re[] uzerine.
	   Sonuc N noktali reel FFT ile ayni (olcek dahil).					*/
	for(i = 1; i <= HALF_N / 2U; i++)
	{
		uint32_t m  = HALF_N - i;
		float    a  = Re[i], b = Im[i], c = Re[m], d = Im[m];
		float    Er, Ei, Or, Oi, Xr, Xi;

		Er = 0.5f * (a + c); Ei = 0.5f * (b - d);
		Or = 0.5f * (b + d); Oi = -0.5f * (a - c);
		Xr = Er + CosTable[i] * Or - SinTable[i] * Oi;
		Xi = Ei + CosTable[i] * Oi + SinTable[i] * Or;
		Re[i] = Xr * Xr + Xi * Xi;

		if(m != i)
		{
			Er = 0.5f * (c + a); Ei = 0.5f * (d - b);
			Or = 0.5f * (d + b); Oi = -0.5f * (c - a);
			Xr = Er + CosTable[m] * Or - SinTable[m] * Oi;
			Xi = Ei + CosTable[m] * Oi + SinTable[m] * Or;
			Re[m] = Xr * Xr + Xi * Xi;
		}
	}

	/* En guclu bin */
	for(i = 1; i < HALF_N; i++)
		if(Re[i] > PeakPow) { PeakPow = Re[i]; PeakBin = i; }

	/* Bant gucleri -> dB */
	for(k = 0; k < SPEC_BAND_COUNT; k++)
	{
		float Power = 0.0f;

		/* Guc = bin basina ortalama guc x gercek bant genisligi. Dar (ara
		   degerli) ve genis (toplanan) bantlar arasinda basamak olusmaz. */
		if(BandHi[k] >= BandLo[k] + 2U)
		{
			for(i = BandLo[k]; i < BandHi[k]; i++)
				Power += Re[i];
			Power *= BandWid[k] / (float)(BandHi[k] - BandLo[k]);
		}
		else
		{
			uint32_t i0 = (uint32_t)BandCenter[k];
			float    t  = BandCenter[k] - (float)i0;

			if(i0 >= HALF_N - 1U) i0 = HALF_N - 2U;
			Power = ((1.0f - t) * Re[i0] + t * Re[i0 + 1U]) * BandWid[k];
		}

		BandPow[k] = Power * RefPowerInv;
		Db[k] = 10.0f * log10f(Power * RefPowerInv + 1e-12f) + BandTilt[k];
		if(Db[k] > MaxDb) MaxDb = Db[k];
	}

	/* AGC: tepe aninda yukselir, yavas duser */
	if(MaxDb > AgcTop) AgcTop = MaxDb;
	else               AgcTop -= CFG_AGC_DECAY_DB_S * DT_FRAME * (float)SPEC_FrameStep;
	if(AgcTop < CFG_DB_FLOOR_TOP) AgcTop = CFG_DB_FLOOR_TOP;

	Bottom = AgcTop - CFG_DB_RANGE;

	/* Seviye = PUNCH * vurus seviyesi + (1 - PUNCH) * duz dB seviyesi.
	   Vurus seviyesi: bandin genligi kendi yakin ortalamasinin ne kadar
	   ustunde? Sabit sesler dibe coker, darbeler tavana firlar.
	   Gurultu esiginin (Bottom) altindaki bantlar her durumda 0.		*/
	{
		const float AvgA = 1.0f - expf(-DT_FRAME / CFG_PUNCH_AVG_SEC);
		Flux = 0.0f; FluxLow = 0.0f; FluxHigh = 0.0f;
		const float MaxD = expf(-DT_FRAME / CFG_PUNCH_MAX_SEC);

		/* Genel sessizlik: toplam guc mutlak esigin altindaysa her sey soner.
		   Muzik durunca tizde gurultu kipirtisi kalmaz.					*/
		{
			float Tot = 0.0f, TDb, G;
			for(k = 0; k < SPEC_BAND_COUNT; k++) Tot += BandPow[k];
			TDb = 10.0f * log10f(Tot + 1e-12f);
			G   = (TDb - CFG_SILENCE_DB) / 6.0f;
			G   = (G < 0.0f) ? 0.0f : ((G > 1.0f) ? 1.0f : G);
			SilenceGain += (G - SilenceGain) * ((G > SilenceGain) ? 0.5f : 0.15f);

			/* Sessizlik tespiti: toplam guc kendi minimumuna yakinsa sessiz.
			   Gurultu tabani SADECE bu durumda ogrenilir -> muzik gurultu
			   sayilmaz, tizler kapanmaz.									*/
			/* Gurultu duragandir (toplam guc neredeyse sabit), muzik dalgalanir.
			   Sessiz = sapma kucuk VE seviye kendi minimumuna yakin.		*/
			if(!NoiseInit) { GMinDb = TDb; TAvgDb = TDb; }
			if(TDb < GMinDb && TDb > -90.0f) GMinDb = TDb;		/* Bos tampon (acilis) sayilmaz */
			else                             GMinDb += 0.2f * DT_FRAME;
			TAvgDb += (TDb - TAvgDb) * (1.0f - expf(-DT_FRAME / 0.5f));
			TVarDb += (fabsf(TDb - TAvgDb) - TVarDb) * (1.0f - expf(-DT_FRAME / 0.3f));

			/* Mutlak ses referansi: sarkinin genel seviyesi (aninda yukselir,
			   1 dB/s iner). Kisik bolumde LoudRel duser -> vuruslar zayiflar,
			   ~18 dB alti tamamen susar. Sessizlikte patlama olmaz.		*/
			if(TAvgDb > LoudRefDb) LoudRefDb = TAvgDb;
			else                   LoudRefDb -= 1.0f * DT_FRAME * (float)SPEC_FrameStep;
			{
				float Lr = (TAvgDb - (LoudRefDb - 18.0f)) / 12.0f;
				/* Muzik kapisi: toplam ses ogrenilmis gurultunun 4..8 dB ustunde
				   degilse hicbir vurus / patlama tetiklenmez.				*/
				float Mg = (TAvgDb - NoiseTotDb - 4.0f) / 4.0f;
				Mg = (Mg < 0.0f) ? 0.0f : ((Mg > 1.0f) ? 1.0f : Mg);
				MusicGate = Mg;
				SPEC_LoudRel = ((Lr < 0.0f) ? 0.0f : ((Lr > 1.0f) ? 1.0f : Lr)) * SilenceGain * Mg;
			}
			/* Ayrica mutlak seviye dusuk olmali: surekli bir sinus ya da uzun
			   notali klasik muzik de duragandir, ama gurultu kadar kisik degildir. */
			/* Yuksek seviyeli duragan ses (bos kablo ugultusu, durmus muzigin
			   hisirtisi) ancak CFG_NOISE_LEARN_SEC boyunca kesintisiz kalirsa
			   gurultu sayilir. Muzik bu kadar sabit kalmaz.				*/
			{
				uint8_t Low = (TVarDb < 1.5f && TDb < GMinDb + 4.0f && TDb < CFG_NOISE_MAX_DB &&
				               BeatGap > (uint32_t)(1.5f * CFG_FPS)) ? 1U : 0U;
				/* Ton mu gurultu mu: gucun ne kadari en guclu bandin +-2/3 oktav
				   cevresinde. Sinus / sabit nota ~1, hisirti ve ugultu dagilir.
				   Dar bantli duragan ses gurultu sayilmaz, ekranda kalir.		*/
				{
					const int Wn = (int)(0.667f * (float)(SPEC_BAND_COUNT - 1) /
					                     log2f(CFG_FREQ_HIGH / CFG_FREQ_LOW) + 0.5f);
					float Mx = 0.0f, Win = 0.0f;
					int Kp = 0, a, b;
					for(k = 0; k < SPEC_BAND_COUNT; k++) if(BandPow[k] > Mx) { Mx = BandPow[k]; Kp = k; }
					a = (Kp - Wn < 0) ? 0 : Kp - Wn;
					b = (Kp + Wn >= SPEC_BAND_COUNT) ? SPEC_BAND_COUNT - 1 : Kp + Wn;
					for(k = a; k <= b; k++) Win += BandPow[k];
					ToneShare += (Win / (Tot + 1e-12f) - ToneShare) * (1.0f - expf(-DT_FRAME / 0.5f));
				}
				StatTime = (TVarDb < 1.0f && ToneShare < CFG_NOISE_TONE_SHARE) ?
				           StatTime + DT_FRAME * (float)SPEC_FrameStep : 0.0f;
				Quiet = (Low || StatTime > CFG_NOISE_LEARN_SEC) ? 1U : 0U;
			}
		}

		const float NoiseFade = powf(10.0f, -0.1f * DT_FRAME * (float)SPEC_FrameStep);
		float       NoiseSum  = 0.0f;

		if(!NoiseInit)
		{
			for(k = 0; k < SPEC_BAND_COUNT; k++) { NoiseP[k] = 1e-12f; PowSlow[k] = PowFast[k] = BandPow[k]; }
			NoiseInit = 1;
		}

		for(k = 0; k < SPEC_BAND_COUNT; k++)
		{
			float Lp = (Db[k] - Bottom - SPEC_Headroom) / CFG_DB_RANGE;	/* Duz seviye	*/
			float Gate, GateN, A, Fl, Lt;

			/* Bant gurultu tabani: asagi aninda iner, yukari cok yavas cikar.
			   Bant, tabaninin CFG_NOISE_MARGIN_DB ustune cikmadikca susar.	*/
			/* Gurultu tabani GUC olarak tutulur (dB ortalamasi gurultuyu oldugundan
			   dusuk gosterir) ve SADECE sessizlikte ogrenilir. Muzik calarken
			   hic degismez -> muzigin surekli tizleri gurultu sayilmaz.		*/
			PowSlow[k] += (BandPow[k] - PowSlow[k]) * (1.0f - expf(-DT_FRAME / 0.3f));
			PowFast[k] += (BandPow[k] - PowFast[k]) * (1.0f - expf(-DT_FRAME / 0.03f));
			if(Quiet)                  NoiseP[k] += (PowSlow[k] - NoiseP[k]) *
			                                        (1.0f - expf(-DT_FRAME * CFG_NOISE_RISE_DB_S));
			else                       NoiseP[k] *= NoiseFade;		/* muzikte 1 dB/s unutulur */
			NoiseSum += NoiseP[k];
			Lp    = (Lp < 0.0f) ? 0.0f : ((Lp > 1.0f) ? 1.0f : Lp);
			Gate  = (Db[k] - Bottom) / 6.0f;				/* Esikte yumusak gecis */
			Gate  = (Gate < 0.0f) ? 0.0f : ((Gate > 1.0f) ? 1.0f : Gate);
			/* Esik karari ~30 ms'lik ortalama guce gore: gurultunun tek karelik
			   sicramalari elenir, muzik darbeleri gecikmeden gecer.			*/
			GateN = (10.0f * log10f((PowFast[k] + 1e-12f) / (NoiseP[k] + 1e-12f)) - CFG_NOISE_MARGIN_DB) / 3.0f;
			GateN = (GateN < 0.0f) ? 0.0f : ((GateN > 1.0f) ? 1.0f : GateN);
			Gate *= GateN * SilenceGain;

			A = sqrtf(BandPow[k]);							/* Dogrusal genlik	*/
			BandAvg[k] += (A - BandAvg[k]) * AvgA;
			BandMax[k]  = (A > BandMax[k]) ? A : BandMax[k] * MaxD;
			if(BandMax[k] < BandAvg[k] * 1.15f + 1e-3f) BandMax[k] = BandAvg[k] * 1.15f + 1e-3f;

			Fl = CFG_PUNCH_FLOOR * BandAvg[k];
			Lt = (A - Fl) / (BandMax[k] - Fl);
			Lt = (Lt < 0.0f) ? 0.0f : ((Lt > 1.0f) ? 1.0f : Lt);

			SPEC_Level[k] = (SPEC_Punch * Lt + (1.0f - SPEC_Punch) * Lp) * Gate;

			/* Spektral aki: banttaki dB artisi (sadece esigi gecen bantlar) */
			{
				float Up = (Db[k] - BandDbPrev[k]) * Gate;
				BandDbPrev[k] = Db[k];
				if(Up > 0.0f)
				{
					Flux += Up;
					if(k < LowBands) FluxLow += Up;
					if(k >= HighFrom) FluxHigh += Up;
				}
			}
		}
		NoiseTotDb = 10.0f * log10f(NoiseSum + 1e-12f);
	}

	/* Toplu seviyeler */
	Sum = 0.0f; for(k = 0; k < SPEC_BAND_COUNT; k++) Sum += SPEC_Level[k];
	SPEC_Volume = Sum / (float)SPEC_BAND_COUNT;
	Sum = 0.0f; for(k = 0; k < BassBands; k++) Sum += SPEC_Level[k];
	SPEC_Bass = Sum / (float)BassBands;
	Sum = 0.0f; for(k = TrebleFirst; k < SPEC_BAND_COUNT; k++) Sum += SPEC_Level[k];
	SPEC_Treble = (TrebleFirst < SPEC_BAND_COUNT) ? Sum / (float)(SPEC_BAND_COUNT - TrebleFirst) : 0.0f;

	/* Ritim algilama */
	{
		float Bass = 0.0f, BassDb;
		const float Alpha = 1.0f / (CFG_BEAT_AVG_SEC * CFG_FPS);
		const uint32_t MinGap = (uint32_t)((float)CFG_BEAT_MIN_MS * CFG_FPS / 1000.0f);

		for(i = BeatLo; i < BeatHi; i++) Bass += Re[i];
		BassDb = 10.0f * log10f(Bass * RefPowerInv + 1e-12f);

		SPEC_Beat = 0;
		if(BeatGap < 0xFFFFU) BeatGap++;

		if(Bass > BeatAvg * CFG_BEAT_THRESHOLD &&
		   Bass > BeatPrev &&
		   BassDb > Bottom + 6.0f && SPEC_LoudRel > 0.3f &&
		   BeatGap >= MinGap)
		{
			SPEC_Beat = 1;
			BeatGap   = 0;
		}

		BeatAvg += (Bass - BeatAvg) * Alpha;
		BeatPrev = Bass;
	}

	/* Kick dedektoru: en dusuk bas araliginin gucu
	     - kendi 0.35 s ortalamasinin CFG_KICK_RATIO katina ani sicrama
	     - mutlak seviye: muzigin en guclu bandindan en fazla
	       CFG_KICK_DB_BELOW dB asagida (gercekten yuksek bas)
	     - genel sessizlik kapisi acik, en az 90 ms arayla			*/
	{
		float P = 0.0f, Pm = 0.0f, PDb, AvgF;
		const float Al = 1.0f - expf(-DT_FRAME / 0.35f);

		for(i = KickLo; i < KickHi; i++) P  += Re[i];
		for(i = MidLo;  i < MidHi;  i++) Pm += Re[i];
		P  *= RefPowerInv;
		Pm *= RefPowerInv;
		PDb = 10.0f * log10f(P + 1e-12f);

		/* Ortalamaya taban: sessiz giriste kucuk bas sizintisi "2 kat" sayilmasin */
		AvgF = powf(10.0f, (AgcTop - CFG_KICK_FLOOR_DB) * 0.1f);
		if(AvgF < KickAvg) AvgF = KickAvg;

		SPEC_Kick = 0;
		if(KickGap < 0xFFFFU) KickGap += SPEC_FrameStep;

		/* Tempo izci: bas yukselisini kaydet; ~0.34 s'de bir periyot (0.3-0.8 s) ve faz bul.
		   Atlanan kareler (islemci yetismezse) bos kayit olarak eklenir, zaman kaymaz. */
		{
			uint32_t Stp = SPEC_FrameStep ? SPEC_FrameStep : 1U, q;
			float O = (PDb > AgcTop - KickDbBelow - 6.0f) ? (PDb - KickDbPrev) : 0.0f;
			if(O < 0.0f) O = 0.0f;
			if(O > 20.0f) O = 20.0f;
			for(q = 1; q < Stp; q++)
			{
				OnHist[OnPos] = 0.0f;
				OnPos = (uint16_t)((OnPos + 1U) % TEMPO_LEN);
			}
			OnHist[OnPos] = O * SilenceGain;
			OnPos = (uint16_t)((OnPos + 1U) % TEMPO_LEN);
			KickDbPrev = PDb;
			BeatNext  -= (float)Stp;
			TempoTick  = (uint16_t)(TempoTick + Stp);

			if(TempoTick >= 32U)
			{
				const int Lmin = (int)(0.30f * CFG_FPS), Lmax = (int)(0.80f * CFG_FPS);
				static float AcL[96];
				static float Lin[TEMPO_LEN];				/* Eskiden yeniye, ortalamasi cikmis */
				float Mean = 0.0f, Ac0 = 0.0f, Best = 0.0f;
				int   L, j, BestL = 0;
				TempoTick = 0;
				for(j = 0; j < (int)TEMPO_LEN; j++) Mean += OnHist[j];
				Mean /= (float)TEMPO_LEN;
				for(j = 0; j < (int)TEMPO_LEN; j++)
				{
					float a = OnHist[(OnPos + (unsigned)j) & (TEMPO_LEN - 1U)] - Mean;
					Lin[j] = a; Ac0 += a * a;
				}
				for(L = Lmin; L <= Lmax; L++)
				{
					float Ac = 0.0f;
					const float *p = &Lin[L], *q = &Lin[0];
					for(j = L; j < (int)TEMPO_LEN; j++)
						Ac += *p++ * *q++;
					Ac *= (float)TEMPO_LEN / (float)(TEMPO_LEN - L);
					AcL[L - Lmin] = Ac;
					if(Ac > Best) { Best = Ac; BestL = L; }
				}
				BeatConf = (Ac0 > 1e-6f) ? Best / Ac0 : 0.0f;
				if(BestL > 0 && BeatConf > 0.25f)
				{
					/* Kesirli periyot: tepe etrafinda parabol (1 kare = 10.7 ms; tam sayi kayar) */
					float Per = (float)BestL;
					if(BestL > Lmin && BestL < Lmax)
					{
						float y0 = AcL[BestL - 1 - Lmin], y1 = AcL[BestL - Lmin], y2 = AcL[BestL + 1 - Lmin];
						float Dn = y0 - 2.0f * y1 + y2;
						if(Dn < -1e-9f) { float o = 0.5f * (y0 - y2) / Dn; if(o > -0.5f && o < 0.5f) Per += o; }
					}

					/* Faz: son periyotlarin ayni noktalarindaki toplam yukselis (+-1 kare).
					   Histerezis: mevcut faz belirgin sekilde kotu degilse korunur.		*/
					{
						int Ph, BestPh = 0, CurPh = -1;
						float BestSum = -1.0f, CurSum = 0.0f;
						int   Pi = (int)(Per + 0.5f);
						uint8_t Had = (BeatPeriod > 0.0f && fabsf(BeatPeriod - Per) < 2.0f);
						if(Had)
						{
							CurPh = (int)(Per - BeatNext + 0.5f);		/* son beklenen vurustan bu yana */
							while(CurPh < 0)   CurPh += Pi;
							while(CurPh >= Pi) CurPh -= Pi;
						}
						for(Ph = 0; Ph < Pi; Ph++)
						{
							float Sum = 0.0f; int m, d;
							for(m = 0; m < 6; m++)
								for(d = -1; d <= 1; d++)
								{
									int Back = Ph + (int)(m * Per + 0.5f) + d;
									if(Back < 0 || Back >= (int)TEMPO_LEN) continue;
									Sum += OnHist[(OnPos + TEMPO_LEN - 1U - (unsigned)Back) % TEMPO_LEN] * (d ? 0.5f : 1.0f);
								}
							if(Sum > BestSum) { BestSum = Sum; BestPh = Ph; }
							if(Ph == CurPh) CurSum = Sum;
						}
						BeatPeriod = Per;
						if(!Had || BestSum > 1.5f * CurSum)
							BeatNext = Per - (float)BestPh;			/* yeni faz */
						BeatLost = 0;
					}
				}
				else if(++BeatLost >= 4U) BeatPeriod = 0.0f;	/* ~1.4 s guvensizse birak */
			}
			while(BeatPeriod > 0.0f && BeatNext < -0.5f * BeatPeriod) BeatNext += BeatPeriod;
		}

		SPEC_KickMain = 0;
		if(P > AvgF * KickRatio && P > KickPrev && P >= Pm * KickDom &&
		   PDb > AgcTop - KickDbBelow && SilenceGain > 0.5f && SPEC_LoudRel > 0.3f &&
		   KickGap > (uint32_t)(0.09f * CFG_FPS))
		{
			/* Guc kulaga gore (dB): son ana vuruslarin ortalamasi = tam guc.
			   3 dB zayif %85, 6 dB %70, 12 dB %40. Tek sert vurus sonrakileri sondurmez. */
			float S;
			if(KickRefDb < -150.0f) KickRefDb = PDb;
			S = 1.0f + (PDb - KickRefDb) / 20.0f;
			SPEC_Kick      = 1;
			SPEC_KickPower = ((S > 1.0f) ? 1.0f : ((S < 0.15f) ? 0.15f : S)) * (0.4f + 0.6f * SPEC_LoudRel);
			KickGap        = 0;
			KickSince      = 0.0f;

			/* Ana vurus: tempo yoksa her kick; varsa beklenen zamana yakin olan */
			if(BeatPeriod <= 0.0f)
			{
				SPEC_KickMain = 1;
				MainGap = 0.0f;
			}
			else
			{
				const float Tol = 0.07f * CFG_FPS;
				float Dn = BeatNext, Dp = BeatNext + BeatPeriod;	/* yakindaki iki aday */
				if(Dn > 0.5f * BeatPeriod) Dn -= BeatPeriod;
				if(fabsf(Dn) <= Tol || fabsf(Dp) <= Tol)
				{
					float E = (fabsf(Dn) <= Tol) ? Dn : Dp;		/* + : vurus erken geldi */
					SPEC_KickMain = 1;
					BeatNext = BeatPeriod + 0.5f * E;			/* fazi yumusak kilitle */
					MainGap  = 0.0f;
				}
			}
		}

		/* Referans: ana vuruslarin dB ortalamasi (~6 vurus); uzun sessizlikte asagi kayar */
		if(SPEC_Kick && SPEC_KickMain) KickRefDb += (PDb - KickRefDb) * 0.2f;
		else if(KickRefDb > -150.0f)   KickRefDb -= 1.5f * DT_FRAME * (float)SPEC_FrameStep;

		/* Bas modu: son 2 s'de kick geldiyse 1, yoksa ~0.5 s'de 0'a */
		KickSince += DT_FRAME * (float)SPEC_FrameStep;
		MainGap   += DT_FRAME * (float)SPEC_FrameStep;
		{
			float Tg = (KickSince < 2.0f) ? 1.0f : 0.0f;
			SPEC_BassMode += (Tg - SPEC_BassMode) * (1.0f - expf(-DT_FRAME / 0.5f));
		}

		KickAvg += (P - KickAvg) * Al;
		KickPrev = P;
	}

	/* Genel vurus (spektral aki): her frekansta ani yukselis.
	   Guc son ~4 s'nin en guclu vurusuna gore; HitLow vurusun bas payi. */
	{
		SPEC_Hit = 0;
		if(HitGap < 0xFFFFU) HitGap++;

		if(Flux > FluxMax) FluxMax = Flux;
		else               FluxMax *= expf(-DT_FRAME / 4.0f);
		if(FluxMax < 1e-3f) FluxMax = 1e-3f;

		if(Flux > FluxAvg * HitRatio && Flux > FluxPrev && Flux > 0.15f * FluxMax &&
		   SilenceGain > 0.5f && SPEC_LoudRel > 0.3f && HitGap > (uint32_t)(0.10f * CFG_FPS))
		{
			/* Guc: son vuruslarin ortalama akisina gore (akis zaten dB toplami) */
			float S;
			if(HitRef <= 0.0f) HitRef = Flux;
			S = 0.25f + 0.75f * Flux / (HitRef * 1.15f);
			HitRef += (Flux - HitRef) * 0.15f;
			SPEC_Hit      = 1;
			SPEC_HitPower = ((S > 1.0f) ? 1.0f : S) * (0.4f + 0.6f * SPEC_LoudRel);
			SPEC_HitLow   = (Flux > 0.0f) ? FluxLow / Flux : 0.0f;
			SPEC_HitHigh  = (Flux > 0.0f) ? FluxHigh / Flux : 0.0f;
			HitGap        = 0;
		}
		FluxAvg += (Flux - FluxAvg) * (1.0f - expf(-DT_FRAME / 0.5f));
		FluxPrev = Flux;
	}

	/* Tepe frekansi: komsu bin'lerle parabolik interpolasyon */
	{
		float PeakDb = 10.0f * log10f(PeakPow * RefPowerInv + 1e-12f);

		SPEC_PeakValid = (PeakDb > Bottom + 10.0f) ? 1U : 0U;

		if(PeakBin > 1U && PeakBin < HALF_N - 1U)
		{
			float a = logf(Re[PeakBin - 1U] + 1e-9f);
			float b = logf(Re[PeakBin]      + 1e-9f);
			float c = logf(Re[PeakBin + 1U] + 1e-9f);
			float d = a - 2.0f * b + c;
			float Delta = (d < 0.0f) ? 0.5f * (a - c) / d : 0.0f;
			SPEC_PeakHz = ((float)PeakBin + Delta) * BIN_HZ;
		}
		else
			SPEC_PeakHz = (float)PeakBin * BIN_HZ;
	}

#if defined(__ARM_ARCH)
	SPEC_ProcessUs = (DWT->CYCCNT - StartCycle) / (SystemCoreClock / 1000000U);
#endif
}

/* Olculen tempo, BPM. 0 = tempo yok (ritimsiz / sessiz) */
float SPEC_GetBpm(void)
{
	return (BeatPeriod > 0.0f) ? 60.0f * CFG_FPS / BeatPeriod : 0.0f;
}
