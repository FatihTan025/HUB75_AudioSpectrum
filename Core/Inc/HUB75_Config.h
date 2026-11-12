/**
  ******************************************************************************
  * @file           : HUB75_Config.h
  * @brief          : Butun ayarlar bu dosyada
  ******************************************************************************
  * Panel, pin, renk ve zamanlama ayarlarinin tamami burada.
  * Diger dosyalarda degistirilecek parametre yoktur.
  * CubeMX bu dosyaya dokunmaz.
  *
  * BIRDEN FAZLA PANEL DESTEGI
  *   Asagida iki panel tanimlidir. Aktif paneli degistirmek icin
  *   SADECE "HUB75_ACTIVE_PANEL" satirini degistir, baska hicbir
  *   yere dokunma. HUB75.c ve HUB75_DMA.c da ayni makroyu kullanir.
  ******************************************************************************
  */
#ifndef __HUB75_CONFIG_H__
#define __HUB75_CONFIG_H__

/* ==========================================================================
   0) AKTIF PANEL SECIMI  -- SADECE BU SATIRI DEGISTIR
   ========================================================================== */
#define HUB75_PANEL_P5_64x32_1_8	1
#define HUB75_PANEL_P4_80x40_1_20	2

#define HUB75_ACTIVE_PANEL		1


/* ==========================================================================
   1) PANEL OLCULERI VE PIN HARITASI  (panele gore degisir)
   ========================================================================== */
#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P5_64x32_1_8)
	/* ---- P5 64x32, outdoor, 1/8 tarama, 2 panel zincir ---- */
	#define PANEL_WIDTH				64
	#define PANEL_HEIGHT			32
	#define PANEL_CHAIN				2
	#define PANEL_SCAN				8

	#define HUB75_ROW_SHIFT			0		/* A=PB0  B=PB1  C=PB2 */
	/* D, E YOK -- bu panelde sadece 3 adres biti */

#elif(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)
	/* ---- P4-20S-7558-HL, indoor, 80x40, 1/20 tarama, tek panel ---- */
	#define PANEL_WIDTH				80
	#define PANEL_HEIGHT			40
	#define PANEL_CHAIN				1
	#define PANEL_SCAN				20

	#define HUB75_ROW_SHIFT			0		/* A=PB0  B=PB1  C=PB2			*/
	#define HUB75_ROW_D_BIT_NUMBER	10		/* D=PB10 -- A/B/C'ye bitisik degil */
	#define HUB75_ROW_E_PORT		GPIOA	/* E -- FARKLI PORT				*/
	#define HUB75_ROW_E_BIT_NUMBER	7		/* E=PA7							*/

#else
	#error "HUB75_ACTIVE_PANEL gecerli bir deger degil"
#endif

/* ---- Ortak pin tanimlari (her iki panelde de ayni karti kullaniyoruz) ---- */
#define HUB75_DATA_PORT			GPIOC	/* R1 G1 B1 R2 G2 B2 CLK -> bit0..6 (PC7 = OE, TIM3) */
#define HUB75_CLK_BIT_NUMBER	6

#define HUB75_ROW_PORT			GPIOB	/* A, B, C, (D varsa) */

#define HUB75_LATCH_PORT		GPIOB
#define HUB75_LATCH_BIT_NUMBER	12

/* OE polaritesi
     0 = Pin LOW  iken panel yanar	(standart HUB75)
     1 = Pin HIGH iken panel yanar	(arada cevirici varsa)			*/
#define HUB75_OE_ACTIVE_HIGH	0

/* ==========================================================================
   2) RENK
   ========================================================================== */
#define HUB75_COLOR_MODE		1		/* 0=8 renk, 1=tam renk (saat + gece gokyuzu icin sart) */

/* Bit-plane sayisi -- panele gore RAM'e sigacak sekilde secildi.
     P5 64x32 chain=2  -> 8 (RAM bol)
     P4 80x40 chain=1  -> 6 (32KB RAM hedefinde guvenli pay birakir)	*/
#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P5_64x32_1_8)
	#define HUB75_BITPLANES		8
#elif(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)
	#define HUB75_BITPLANES		8
#endif

#define HUB75_USE_GAMMA			1
#define HUB75_BRIGHTNESS_INIT	100

/* ==========================================================================
   3) ZAMANLAMA
   ========================================================================== */
#define HUB75_TIMER_PERIOD		20

#define HUB75_LATCH_NOP_COUNT	2
#define HUB75_ADDRESS_NOP_COUNT	4


/* ==========================================================================
   4) TIMER VE DMA SECIMI
   ========================================================================== */
#define HUB75_TIMER_HANDLE		htim1
#define HUB75_DMA_HANDLE		hdma_tim1_up
#define HUB75_DMA_IRQ_NUMBER	DMA2_Stream5_IRQn
#define HUB75_DMA_IRQ_PRIORITY	0

/* ==========================================================================
   4b) OE SURME -- TIM3_CH2 (PC7)
   OE pini veri byte'inda DEGIL; her latch'ten sonra TIM3 one-pulse
   modunda o plane'in suresi kadar tek darbe uretir (~11 ns cozunurluk).
   ========================================================================== */
#define HUB75_OE_TIM_HANDLE		htim3
#define HUB75_OE_TIM_CHANNEL	TIM_CHANNEL_2

/* En uzun OE darbesi (MSB plane, %100), TIM3 tik cinsinden.
   Satirin DMA suresinin %90'i -- darbe bir sonraki latch'e tasmaz.
     TIM1 (byte saati) 180 MHz, TIM3 (APB1 x2) 90 MHz -> oran 1/2
   P5 64x32 tek panel: 256 * 21 * 9 / 20 = 2419 tik (26.9 us)
   P4 80x40          : 160 * 21 * 9 / 20 = 1512 tik (16.8 us)		*/
#define HUB75_OE_TIM_ARR		((uint32_t)PANEL_PIXELS_PER_ROW * 2U * \
								 ((uint32_t)HUB75_TIMER_PERIOD + 1U) * 9U / 20U)

/* ==========================================================================
   5) FONT
   ========================================================================== */
#include "HUB75_Font.h"			/* Font yapilari (saat fontu icin)		*/


/* ==========================================================================
   TUREMIS SABITLER - bu bolume dokunma
   ========================================================================== */
#if(HUB75_COLOR_MODE == 0)
	#undef  HUB75_BITPLANES
	#define HUB75_BITPLANES		1
	#undef  HUB75_USE_GAMMA
	#define HUB75_USE_GAMMA		0
#endif

/* P5: zigzag mimari, sabit "128" (64 genislik + zigzag'a ozeldi).
   P4: duz mimari, PANEL_WIDTH*PANEL_CHAIN.						*/
#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P5_64x32_1_8)
	#define PANEL_PIXELS_PER_ROW	(128 * PANEL_CHAIN)
#elif(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)
	#define PANEL_PIXELS_PER_ROW	(PANEL_WIDTH * PANEL_CHAIN)
#endif

#define HUB75_PIXEL_COUNT		(PANEL_WIDTH * PANEL_HEIGHT * PANEL_CHAIN)
#define HUB75_MAX_Y				(PANEL_HEIGHT * PANEL_CHAIN)

/* Grup dongusu parametreleri -- HUB75_DMA.c'deki UpdateDisplay bunlari kullanir */
#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P5_64x32_1_8)
	#define HUB75_GROUP_COUNT	(4 * PANEL_CHAIN)
	#define HUB75_GROUP_SIZE	32
#elif(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)
	#define HUB75_GROUP_COUNT	PANEL_CHAIN
	#define HUB75_GROUP_SIZE	PANEL_WIDTH
#endif

#define HUB75_CLK_BIT			(1U << HUB75_CLK_BIT_NUMBER)
#define HUB75_LATCH_PIN			(1U << HUB75_LATCH_BIT_NUMBER)
#define HUB75_ROW_MASK			(0x07U << HUB75_ROW_SHIFT)

#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)
	#define HUB75_ROW_D_BIT		(1U << HUB75_ROW_D_BIT_NUMBER)
	#define HUB75_ROW_E_BIT		(1U << HUB75_ROW_E_BIT_NUMBER)
#endif

#endif /* __HUB75_CONFIG_H__ */
