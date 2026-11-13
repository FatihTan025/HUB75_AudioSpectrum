/* ============================================================================
   Mod onizleme ureteci (PC)
   Firmware'in KENDI cizim kodunu (Spectrum.c, SpecDisplay.c, Settings.c) PC'de
   calistirir, sentetik bir muzikle besler ve her gorunumun karelerini kaydeder.
   Cikti: <cikis>/<no>.rgb  (128x32 RGB888 kareler arka arkaya) + liste.txt
   Kullanim: bkz. uret.sh
   ============================================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "main.h"
#include "HUB75.h"
#include "Spectrum.h"
#include "SpecDisplay.h"
#include "Settings.h"

/* --- Sahte donanim -------------------------------------------------------- */
RCC_TypeDef HostRcc;
PWR_TypeDef HostPwr = { 0, PWR_CSR_BRR };
uint8_t     HostBkpSram[4096];
uint32_t    SystemCoreClock = 180000000U;

static uint8_t  Tuval[64 * 64 * 3];       /* HUB75 cizim tamponu (mantiksal 64x64)  */
static double   SimMs;

uint32_t HAL_GetTick(void)                 { return (uint32_t)SimMs; }
unsigned char *HUB75_GetDrawBuffer(void)   { return Tuval; }
void HUB75_SetBrightness(unsigned char p)  { (void)p; }
void HUB75_UpdateDisplay(void)             { }
void HUB75_Clear(void)                     { memset(Tuval, 0, sizeof(Tuval)); }

/* Saat: onizlemede 21:47'den baslayip simulasyon zamaniyla ilerler (Saat.c kartta) */
#include "Saat.h"
void SAAT_Get(uint8_t *Sa, uint8_t *Dk, uint8_t *Sn)
{
	uint32_t s = (21U * 3600U + 47U * 60U + (uint32_t)(SimMs / 1000.0)) % 86400U;
	*Sa = (uint8_t)(s / 3600U); *Dk = (uint8_t)((s / 60U) % 60U); *Sn = (uint8_t)(s % 60U);
}
uint8_t SAAT_Set(uint8_t Sa, uint8_t Dk, uint8_t Sn) { (void)Sa; (void)Dk; (void)Sn; return 1; }
uint8_t SAAT_SensorOku(int16_t *t, uint8_t *n) { *t = 215; *n = 45; return 1; }	/* Onizleme: 21.5 C, %45 */

/* SpecDisplay.c CV() eslemesinin tersi: 128x32 goruntu */
static void Goruntu(uint8_t *o)
{
	for(int y = 0; y < 32; y++)
		for(int x = 0; x < 128; x++)
		{
			const uint8_t *p = &Tuval[(((((uint32_t)x >> 6) ^ (uint32_t)CFG_SWAP_HALVES) << 11) +
			                            ((uint32_t)y << 6) + ((uint32_t)x & 63U)) * 3U];
			uint8_t *q = &o[(y * 128 + x) * 3];
			q[0] = p[0]; q[1] = p[1]; q[2] = p[2];
		}
}

/* --- Sentetik muzik (124 BPM, 4/4): kick, bas, trampet, hi-hat, akor, arpej --- */
#define FS     ((double)CFG_SAMPLE_RATE)
#define BPM    124.0
static double  Faz[16];
static uint32_t Rnd = 12345;
static double  Gurultu(void) { Rnd = Rnd * 1664525u + 1013904223u; return ((Rnd >> 8) & 0xFFFF) / 32768.0 - 1.0; }
static double  HpOnceki, HpCikis, Hp2Once, Hp2Cikis;

static double Ornek(double t)
{
	const double vurus = 60.0 / BPM;
	const double b  = t / vurus;               /* vurus sayaci */
	const int    bi = (int)b;
	const double bf = (b - bi) * vurus;        /* vurustan beri saniye */
	const int    olcu = bi / 4, ic = bi % 4;
	const double sekiz = fmod(t, vurus / 2.0); /* sekizlikten beri */
	const int    sekizNo = (int)(t / (vurus / 2.0));
	static const double Kok[4] = { 55.0, 43.65, 65.41, 49.0 };            /* A F C G */
	static const double Akor[4][3] = { {220,261.6,329.6}, {174.6,220,261.6}, {261.6,329.6,392}, {196,246.9,293.7} };
	const int    ak = olcu % 4;
	double s = 0.0;

	/* Kick: 150 -> 45 Hz suzulen sinus */
	{
		double f = 45.0 + 105.0 * exp(-bf * 30.0);
		Faz[0] += 2.0 * M_PI * f / FS;
		s += 0.9 * sin(Faz[0]) * exp(-bf * 7.0);
	}
	/* Bas: sekizlikler, testere benzeri */
	{
		double f = Kok[ak] * ((sekizNo % 4 == 3) ? 2.0 : 1.0);
		Faz[1] += 2.0 * M_PI * f / FS;
		double v = sin(Faz[1]) + 0.5 * sin(2 * Faz[1]) + 0.3 * sin(3 * Faz[1]) + 0.2 * sin(4 * Faz[1]);
		s += 0.28 * v * exp(-sekiz * 5.0);
	}
	/* Trampet: 2 ve 4 */
	if(ic == 1 || ic == 3)
	{
		double n = Gurultu();
		HpCikis = 0.6 * (HpCikis + n - HpOnceki); HpOnceki = n;
		Faz[2] += 2.0 * M_PI * 190.0 / FS;
		s += (0.45 * HpCikis + 0.25 * sin(Faz[2])) * exp(-bf * 14.0);
	}
	/* Hi-hat: sekizlik arkalari + onaltilik suslemesi */
	{
		double n = Gurultu();
		Hp2Cikis = 0.2 * (Hp2Cikis + n - Hp2Once); Hp2Once = n;
		double e = exp(-sekiz * ((sekizNo & 1) ? 45.0 : 80.0));
		s += ((sekizNo & 1) ? 0.30 : 0.12) * Hp2Cikis * e;
	}
	/* Akor (pad) */
	for(int i = 0; i < 3; i++)
	{
		Faz[3 + i] += 2.0 * M_PI * Akor[ak][i] / FS;
		s += 0.10 * sin(Faz[3 + i]) * (0.7 + 0.3 * sin(t * 2.0));
	}
	/* Arpej: onaltiliklar (1-2.5 kHz) */
	{
		const int on6 = (int)(t / (vurus / 4.0));
		const double on6f = fmod(t, vurus / 4.0);
		double f = Akor[ak][on6 % 3] * 4.0;
		Faz[6] += 2.0 * M_PI * f / FS;
		s += 0.16 * sin(Faz[6]) * exp(-on6f * 20.0);
		Faz[7] += 2.0 * M_PI * f * 1.5 / FS;
		s += 0.06 * sin(Faz[7]) * exp(-on6f * 30.0);
	}
	/* Giris: ilk 2 olcu daha sakin */
	if(olcu < 2) s *= 0.5;
	return s;
}

/* --- Ana dongu -------------------------------------------------------------- */
static uint16_t Pencere[CFG_FFT_SIZE];
static double   Zaman;

static void Kare(void)                     /* HOP kadar yeni ornek + isle + ciz */
{
	memmove(Pencere, &Pencere[CFG_HOP_SIZE], (CFG_FFT_SIZE - CFG_HOP_SIZE) * sizeof(uint16_t));
	for(uint32_t i = 0; i < CFG_HOP_SIZE; i++)
	{
		double v = 2048.0 + 900.0 * Ornek(Zaman);
		Pencere[CFG_FFT_SIZE - CFG_HOP_SIZE + i] = (uint16_t)(v < 0 ? 0 : (v > 4095 ? 4095 : v));
		Zaman += 1.0 / FS;
	}
	SimMs = Zaman * 1000.0;
	SPEC_FrameStep = 1;
	SPEC_Process(Pencere);
	SPEC_Draw();
}

int main(int argc, char **argv)
{
	const char *dizin = (argc > 1) ? argv[1] : ".";
	const int   kare  = (argc > 2) ? atoi(argv[2]) : 60;     /* kayit karesi        */
	const int   adim  = (argc > 3) ? atoi(argv[3]) : 6;      /* kac karede bir kayit */
	const int   isinma = (int)(CFG_FPS * 2.0);               /* mod basina 2 s      */
	static uint8_t img[128 * 32 * 3];
	char yol[512];

	SPEC_Init();
	SPEC_DisplayInit();
	SET_Init();
	for(int i = 0; i < (int)(CFG_FPS * 6.0); i++) Kare();   /* tempo otursun */

	snprintf(yol, sizeof(yol), "%s/liste.txt", dizin);
	FILE *liste = fopen(yol, "w");
	for(uint8_t m = 0; m < SET_LookCount(); m++)
	{
		const SET_ModeInfo *mi = SET_Mode(m);
		SET_SetCurMode(m);
		for(int i = 0; i < isinma; i++) Kare();
		snprintf(yol, sizeof(yol), "%s/%02u.rgb", dizin, m);
		FILE *f = fopen(yol, "wb");
		for(int k = 0; k < kare; k++)
		{
			for(int j = 0; j < adim; j++) Kare();
			Goruntu(img);
			fwrite(img, 1, sizeof(img), f);
		}
		fclose(f);
		fprintf(liste, "%u\t%u\t%s\n", m, mi->Family, mi->Name);
		printf("%2u %-16s aile %u\n", m, mi->Name, mi->Family);
	}
	fclose(liste);
	return 0;
}
