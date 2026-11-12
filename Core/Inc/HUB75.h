/**
  ******************************************************************************
  * @file           : HUB75.h
  * @brief          : HUB75 surucusu - tek public header
  ******************************************************************************
  * Ayarlar icin HUB75_Config.h dosyasina bak. Burada parametre yoktur.
  *
  * KULLANIM SIRASI
  *     MX_TIM1_Init();
  *     HUB75_Clear();
  *     HUB75_GetDrawBuffer() ile cizim;
  *     HUB75_UpdateDisplay();		<- her cizimden sonra ZORUNLU
  *     HUB75_DMA_Init();			<- sadece bir kez, en sonda
  *
  * KOORDINAT SISTEMI
  *     Mantiksal alan  PANEL_WIDTH x (PANEL_HEIGHT * PANEL_CHAIN)
  *     PANEL_CHAIN=2 icin 64 genis, 64 yuksek:
  *         Y =  0..31  -> birinci panel
  *         Y = 32..63  -> ikinci panel  (fizikte yatay zincirlidir)
  ******************************************************************************
  */
#ifndef __HUB75_H__
#define __HUB75_H__

#include "main.h"
#include "HUB75_Config.h"

/* --------------------------------------------------------------------------
   SURUCU
   -------------------------------------------------------------------------- */

/* Taramayi baslatir. MX_TIM1_Init() ve ilk HUB75_UpdateDisplay()
   cagrildiktan sonra bir kez calistirilir. Sonrasinda ekran kendi
   kendine yenilenir.													*/
void HUB75_DMA_Init(void);

/* Cizim alanini DMA byte akisina cevirir.
   Cizim yapildiktan sonra cagrilmazsa ekranda hicbir sey degismez.		*/
void HUB75_UpdateDisplay(void);
void HUB75_DMA_IRQHandler(void);			/* DMA2_Stream5_IRQHandler icinden	*/
unsigned char *HUB75_GetDrawBuffer(void);	/* Dogrudan cizim icin tampon		*/

/* Parlaklik ayari. YUZDE olarak verilir: 0 = kapali, 100 = tam.
   Yeni deger bir sonraki HUB75_UpdateDisplay() cagrisinda etkili olur.	*/
void HUB75_SetBrightness(unsigned char Percent);

/* --------------------------------------------------------------------------
   CIZIM
   -------------------------------------------------------------------------- */

/* Cizim alanini siyaha boyar */
void HUB75_Clear(void);

#endif /* __HUB75_H__ */
