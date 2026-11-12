/**
  ******************************************************************************
  * @file           : HUB75.c
  * @brief          : Panel ic eslemesi
  ******************************************************************************
  */
#include "HUB75.h"

/* ==========================================================================
   PANEL IC ESLEMESI  -- panele gore FARKLI mimari

   P5 64x32 (outdoor): zigzag kablolama, satir 32'lik 4 gruba bolunur.
   P4 80x40 (indoor)  : duz kablolama, zigzag yok, tek grup = tam satir.

   Bu tablolar HUB75_DMA.c icindeki donusum fonksiyonunda kullanilir.
   ========================================================================== */
#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P5_64x32_1_8)

	#if(PANEL_CHAIN == 1)
	const uint16_t UpperRowMap[4] = {  512,    0,  544,   32 };
	const uint16_t LowerRowMap[4] = { 1536, 1024, 1568, 1056 };
	#elif(PANEL_CHAIN == 2)
	const uint16_t UpperRowMap[8] = {  512,    0,  544,   32, 2560, 2048, 2592, 2080 };
	const uint16_t LowerRowMap[8] = { 1536, 1024, 1568, 1056, 3584, 3072, 3616, 3104 };
	#else
		#error "PANEL_CHAIN yalnizca 1 veya 2 olabilir (P5 icin)"
	#endif

#elif(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)

	/* Duz/basit mimari: her panel bloğu PANEL_WIDTH*PANEL_HEIGHT piksel.
	   Ust yari = Row, alt yari = Row+PANEL_SCAN (blok icinde). */
	#define HUB75_PANEL_BLOCK_SIZE	(PANEL_WIDTH * PANEL_HEIGHT)

	#if(PANEL_CHAIN == 1)
	const uint16_t UpperRowMap[1] = { 0 };
	const uint16_t LowerRowMap[1] = { PANEL_WIDTH * PANEL_SCAN };
	#elif(PANEL_CHAIN == 2)
	const uint16_t UpperRowMap[2] = { 0, HUB75_PANEL_BLOCK_SIZE };
	const uint16_t LowerRowMap[2] = { PANEL_WIDTH * PANEL_SCAN,
	                                   HUB75_PANEL_BLOCK_SIZE + PANEL_WIDTH * PANEL_SCAN };
	#else
		#error "PANEL_CHAIN yalnizca 1 veya 2 olabilir (P4 icin, 3+ icin HUB75_InitRowMap() gerekir)"
	#endif

#endif
