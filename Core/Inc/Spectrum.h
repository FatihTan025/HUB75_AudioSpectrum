/**
  ******************************************************************************
  * @file    Spectrum.h
  * @brief   Pencere + FFT + logaritmik bantlar + AGC + ritim algilama
  ******************************************************************************
  * Ayarlar SpecConfig.h icinde.
  ******************************************************************************
  */
#ifndef __SPECTRUM_H__
#define __SPECTRUM_H__

#include "main.h"
#include "SpecConfig.h"
#include "Audio_ADC.h"

#define SPEC_BAND_COUNT		CFG_BAND_COUNT
#define SPEC_WAVE_POINTS	128

/* Bant seviyeleri 0..1 (bas -> tiz), AGC uygulanmis */
extern float			SPEC_Level[SPEC_BAND_COUNT];

/* Toplu degerler, 0..1 */
extern float			SPEC_Volume;		/* Tum bantlarin ortalamasi		*/
extern float			SPEC_Bass;			/* ~250 Hz alti ortalama		*/
extern float			SPEC_Treble;		/* ~4 kHz ustu ortalama			*/

/* Kick (en dusuk bas, ~40-100 Hz) darbesi: 1 kare boyunca 1.
   Sadece gercek, yuksek bir bas darbesinde tetiklenir; muzikte bas
   yoksa hic tetiklenmez. SPEC_KickPower: darbenin gucu 0..1.		*/
extern uint8_t			SPEC_Kick;
extern float			SPEC_KickPower;		/* Son ~4 s'nin en guclu kick'ine gore	*/

/* Genel vurus (her frekansta, spektral aki): muzikte bas yoksa kullanilir.
   SPEC_HitPower: son ~4 s'nin en guclu vurusuna gore 0..1
   SPEC_HitLow  : vurusun ne kadari bastan geldi 0..1 (0 = tiz vurus)	*/
extern uint8_t			SPEC_Hit;
extern float			SPEC_HitPower;
extern float			SPEC_HitLow;
extern float			SPEC_HitHigh;		/* Vurusun >4 kHz payi (tiz)	*/
extern float			SPEC_Punch;			/* Seviye hesabindaki vurus payi (mod basina) */
extern float			SPEC_LoudRel;		/* Muzigin kendi seviyesine gore 0..1	*/
extern float			SPEC_Headroom;		/* Cubuk modlarinda en guclu bant tavana yapismaz */

/* Muzigin bas agirligi (SPEC_BassMode): son ~2 s icinde kick geldiyse 1, yoksa 0'a iner */
extern uint8_t		SPEC_FrameStep;	/* Son islenen kareden beri gecen kare (main.c yazar) */
extern uint8_t		SPEC_KickMain;	/* Kick tempoya uyuyor: ana vurus		*/
extern float			SPEC_BassMode;

/* Bu karede vurus algilandi mi (1 kare boyunca 1) */
extern uint8_t			SPEC_Beat;

/* Dalga formu, -1..1, son ~SPEC_WAVE_POINTS*4 ornek (otomatik kazanc) */
extern float			SPEC_Wave[SPEC_WAVE_POINTS];

/* En guclu tonun frekansi, Hz. SPEC_PeakValid = 0 ise sessiz. */
extern float			SPEC_PeakHz;
extern uint8_t			SPEC_PeakValid;

/* Son SPEC_Process suresi, us (Live Expressions ile izle) */
extern volatile uint32_t SPEC_ProcessUs;
extern volatile uint32_t	SPEC_DrawUs;
extern volatile uint32_t	SPEC_UpdateUs;

void SPEC_Init(void);
void SPEC_SetRange(float FLo, float FHi);	/* Frekans araligi, Hz */
void SPEC_SetSens(uint8_t Bas, uint8_t Tiz);	/* Vurus hassasiyeti 0..100 (50 = CFG degerleri): bas (kick), tiz (genel vurus) */
extern uint8_t		SPEC_Sens;					/* Bas hassasiyeti */
void SPEC_Process(const uint16_t *Samples);

float SPEC_GetBpm(void);			/* Olculen tempo, 0 = yok			*/

#endif /* __SPECTRUM_H__ */
