/**
  ******************************************************************************
  * @file           : HUB75_DMA.c
  * @brief          : TIM1 Update tetiklemeli DMA2 surucusu
  ******************************************************************************
  * CALISMA PRENSIBI

  *   Cizim alani  ->  HUB75_UpdateDisplay()  ->  RowBuffer  ->  DMA  ->  GPIO
  *
  * RowBuffer icindeki her byte dogrudan veri portunun ODR yazmacina gider.
  * Byte icerigi:
  *     bit0..bit5	R1 G1 B1 R2 G2 B2
  *     bit6			CLK
  *     bit7			kullanilmiyor -- OE pini (PC7) TIM3_CH2 ile ayrica surulur
  *
  * Ayarlar HUB75_Config.h dosyasindadir. Panel secimi HUB75_ACTIVE_PANEL
  * ile yapilir -- bu dosyadaki HUB75_SetRow() ve grup dongusu parametreleri
  * (HUB75_GROUP_COUNT/SIZE) otomatik olarak dogru panele uyarlanir.
  ******************************************************************************
  */
#pragma GCC optimize("O2")		/* NOP gecikmeleri derleme ayarindan bagimsiz */

#include "HUB75.h"
#include "tim.h"

#if(HUB75_USE_GAMMA)
	#include "HUB75_CIE.h"
#endif

/* HUB75.c icinde tanimli panel ic esleme tablolari */
extern const uint16_t UpperRowMap[HUB75_GROUP_COUNT];
extern const uint16_t LowerRowMap[HUB75_GROUP_COUNT];

/* ==========================================================================
   PIN SURME MAKROLARI
   ========================================================================== */
#if(HUB75_OE_TIM_CHANNEL != TIM_CHANNEL_2)
	#error "OE timer kodu TIM_CHANNEL_2 (CCMR1/OC2M, CCR2) icin yazildi"
#endif

#define OE_TIM			(HUB75_OE_TIM_HANDLE.Instance)

/* Sayaci durdur, cikisi ANINDA pasife zorla (Force inactive, OC2M=100) */
#define PANEL_OFF()		do { OE_TIM->CR1 &= ~TIM_CR1_CEN; \
						     OE_TIM->CCMR1 = (OE_TIM->CCMR1 & ~TIM_CCMR1_OC2M) | TIM_CCMR1_OC2M_2; \
						} while(0)

/* ==========================================================================
   RENK ESLEMESI
   ========================================================================== */
#if(HUB75_USE_GAMMA)
	#define MAP_RED(Value)		(CIE_RED[(Value)]   >> (8 - HUB75_BITPLANES))
	#define MAP_GREEN(Value)	(CIE_GREEN[(Value)] >> (8 - HUB75_BITPLANES))
	#define MAP_BLUE(Value)		(CIE_BLUE[(Value)]  >> (8 - HUB75_BITPLANES))
#else
	#define MAP_RED(Value)		((Value) >> (8 - HUB75_BITPLANES))
	#define MAP_GREEN(Value)	MAP_RED(Value)
	#define MAP_BLUE(Value)		MAP_RED(Value)
#endif

/* ==========================================================================
   BUFFER'LAR
   ========================================================================== */
#if(HUB75_COLOR_MODE)
static unsigned char DrawBuffer[HUB75_PIXEL_COUNT][3];	/* Tam renk: R G B	*/
#else
static unsigned char DrawBuffer[HUB75_PIXEL_COUNT];		/* 8 renk: 3 bit	*/
#endif

static unsigned char RowBuffer[PANEL_SCAN][HUB75_BITPLANES][PANEL_PIXELS_PER_ROW * 2];

/* DMA2_Stream5 (TIM1_UP) bayraklari -- HAL'siz kesme icin */
#define HUB75_DMA_ISR_TC	DMA_HISR_TCIF5
#define HUB75_DMA_ALL_CLR	(DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 | \
							 DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5)

/* ==========================================================================
   TARAMA DURUMU
   ========================================================================== */
static unsigned char	CurrentRow		= 0;
static unsigned char	CurrentPlane	= 0;
/* ==========================================================================
   Algisal parlaklik egrisi (CIE 1931 L* -> Y)
   Yuzde degeri gozun algiladigi parlaklik olarak yorumlanir; donen deger
   gercek isik orani, binde (0..1000) cinsinden. Tamsayi, sabit ifade.
     L <= 8 :  Y = L / 903.3
     L >  8 :  Y = ((L + 16) / 116)^3        116^3 = 1560896
   ========================================================================== */
#define HUB75_CIE_PERMILLE(L) \
	(((L) > 8U) ? ((((uint32_t)(L) + 16U) * ((uint32_t)(L) + 16U) * \
	               ((uint32_t)(L) + 16U) * 1000U) / 1560896U) \
	            : (((uint32_t)(L) * 10000U) / 9033U))

/* MSB plane'in OE darbesi, TIM3 tik cinsinden. Alt plane'ler >> ile bulunur.
   (RowBuffer'daki bit7 kullanilmaz; PC7 timer'a baglidir.) */
static uint16_t			OeTickLimit		=
                        (uint16_t)((HUB75_CIE_PERMILLE(HUB75_BRIGHTNESS_INIT) *
                                    HUB75_OE_TIM_ARR) / 1000U);

/* ==========================================================================
   Latch darbesi uretir.
   ========================================================================== */
static inline void HUB75_Latch(void)
{
	int NopCounter;

	HUB75_LATCH_PORT->BSRR = HUB75_LATCH_PIN;

	for(NopCounter = 0; NopCounter < HUB75_LATCH_NOP_COUNT; NopCounter++)
	{
		__NOP();
	}

	HUB75_LATCH_PORT->BSRR = HUB75_LATCH_PIN << 16U;
}

/* ==========================================================================
   Satir adresini ayarlar -- PANELE GORE FARKLI IMPLEMENTASYON

   P5 64x32 (1/8): sadece A,B,C (3 bit), tek port, tek BSRR yazimi.
   P4 80x40 (1/20): A,B,C,D (GPIOB) + E (GPIOA, farkli port) -- iki
                    ayri BSRR yazimi gerekir.
   ========================================================================== */
#if(HUB75_ACTIVE_PANEL == HUB75_PANEL_P5_64x32_1_8)

static inline void HUB75_SetRow(unsigned char Row)
{
	uint32_t SetMask;
	uint32_t ResetMask;

	SetMask   = ((uint32_t)(Row & 0x07U)) << HUB75_ROW_SHIFT;
	ResetMask = (HUB75_ROW_MASK & ~SetMask);

	HUB75_ROW_PORT->BSRR = (ResetMask << 16U) | SetMask;
}

#elif(HUB75_ACTIVE_PANEL == HUB75_PANEL_P4_80x40_1_20)

static inline void HUB75_SetRow(unsigned char Row)
{
	uint32_t SetMaskB;
	uint32_t ResetMaskB;
	uint32_t FullMaskB;

	/* --- GPIOB: A, B, C (bitisik) + D (ayri bit, ayni port) --- */
	SetMaskB = ((uint32_t)(Row & 0x07U)) << HUB75_ROW_SHIFT;

	if(Row & 0x08U)
		SetMaskB |= HUB75_ROW_D_BIT;

	FullMaskB  = HUB75_ROW_MASK | HUB75_ROW_D_BIT;
	ResetMaskB = FullMaskB & ~SetMaskB;

	HUB75_ROW_PORT->BSRR = (ResetMaskB << 16U) | SetMaskB;

	/* --- GPIOA: E -- farkli port, zorunlu ayri BSRR yazimi --- */
	if(Row & 0x10U)
		HUB75_ROW_E_PORT->BSRR = HUB75_ROW_E_BIT;
	else
		HUB75_ROW_E_PORT->BSRR = HUB75_ROW_E_BIT << 16U;
}

#endif

/* ==========================================================================
   Bir satir-plane'in DMA transferini baslatir.
   ========================================================================== */
static void HUB75_StartRowTransfer(void)
{
	__HAL_TIM_DISABLE(&HUB75_TIMER_HANDLE);
	__HAL_TIM_DISABLE_DMA(&HUB75_TIMER_HANDLE, TIM_DMA_UPDATE);
	__HAL_TIM_SET_COUNTER(&HUB75_TIMER_HANDLE, 0);

	/* HAL_DMA_Start_IT yerine dogrudan yazmaclar (saniyede ~20 000 kez
	   cagrilir; HAL kontrolleri ve kilitleri gereksiz yuk).			*/
	{
		DMA_Stream_TypeDef *S = HUB75_DMA_HANDLE.Instance;

		S->CR &= ~DMA_SxCR_EN;
		while(S->CR & DMA_SxCR_EN) { }
		DMA2->HIFCR = HUB75_DMA_ALL_CLR;
		S->PAR  = (uint32_t)&HUB75_DATA_PORT->ODR;
		S->M0AR = (uint32_t)RowBuffer[CurrentRow][CurrentPlane];
		S->NDTR = PANEL_PIXELS_PER_ROW * 2;
		S->CR   = (S->CR & ~(DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE)) | DMA_SxCR_TCIE | DMA_SxCR_EN;
	}

	__HAL_TIM_ENABLE_DMA(&HUB75_TIMER_HANDLE, TIM_DMA_UPDATE);
	__HAL_TIM_ENABLE(&HUB75_TIMER_HANDLE);
}

/* ==========================================================================
   DMA transfer tamamlandi kesmesi.
   ========================================================================== */
static void HUB75_TransferComplete(void)
{
	int NopCounter;

	__HAL_TIM_DISABLE(&HUB75_TIMER_HANDLE);
	__HAL_TIM_DISABLE_DMA(&HUB75_TIMER_HANDLE, TIM_DMA_UPDATE);

	PANEL_OFF();
	HUB75_Latch();
	HUB75_SetRow(CurrentRow);

	for(NopCounter = 0; NopCounter < HUB75_ADDRESS_NOP_COUNT; NopCounter++)
	{
		__NOP();
	}

	/* Az once latch'lenen CurrentPlane icin OE darbesi.
	   PWM2, CCR=1, ARR=OnTicks:
	     CNT=0          -> pasif (1 tik)
	     CNT=1..OnTicks -> AKTIF  (OnTicks tik)
	     tasma          -> sayac 0'da durur (OPM), cikis pasif			*/
	{
		uint16_t OnTicks = (uint16_t)(OeTickLimit >> (HUB75_BITPLANES - 1U - CurrentPlane));

		if(OnTicks == 0U)
			OnTicks = 1U;

		OE_TIM->CNT   = 0;
		OE_TIM->CCR2  = 1;
		OE_TIM->ARR   = OnTicks;
		OE_TIM->CCMR1 = (OE_TIM->CCMR1 & ~TIM_CCMR1_OC2M) | TIM_CCMR1_OC2M;	/* PWM2 (111) */
		OE_TIM->CR1  |= TIM_CR1_OPM | TIM_CR1_CEN;
	}

	CurrentPlane++;

	if(CurrentPlane >= HUB75_BITPLANES)
	{
		CurrentPlane = 0;
		CurrentRow++;

		if(CurrentRow >= PANEL_SCAN)
			CurrentRow = 0;
	}
	HUB75_StartRowTransfer();
}

/* ==========================================================================
   Cizim alanini DMA byte akisina cevirir.

   Her piksel BIR KEZ okunur: renk degerleri (gamma dahil) bir kez alinir,
   sonra tum bit-plane'lerin byte'lari ayni anda yazilir.
   Dongu sirasi: satir -> piksel -> plane (plane en icte).

   Grup dongusu HUB75_GROUP_COUNT / HUB75_GROUP_SIZE ile PANELE GORE
   OTOMATIK ayarlanir (bkz. HUB75_Config.h). Baska panel eklenirse bu
   fonksiyona dokunmaya gerek yoktur.
   ========================================================================== */
#define HUB75_PLANE_STRIDE	(PANEL_PIXELS_PER_ROW * 2)	/* Bir plane'den digerine byte farki */

void HUB75_UpdateDisplay(void)
{
	unsigned char	Plane;
	unsigned char	Row;
	unsigned char  *OutPointer;
	int				GroupIndex;
	int				PixelIndex;
	uint16_t		UpperBase;
	uint16_t		LowerBase;
	unsigned char	PixelData;

#if(HUB75_COLOR_MODE)
	const unsigned char *UpperPixel;
	const unsigned char *LowerPixel;
	uint32_t		Red1, Green1, Blue1;	/* Ust yari, gamma uygulanmis	*/
	uint32_t		Red2, Green2, Blue2;	/* Alt yari, gamma uygulanmis	*/
#endif

	for(Row = 0; Row < PANEL_SCAN; Row++)
	{
		OutPointer = RowBuffer[Row][0];

		for(GroupIndex = 0; GroupIndex < HUB75_GROUP_COUNT; GroupIndex++)
		{
			UpperBase = (uint16_t)(Row * PANEL_WIDTH) + UpperRowMap[GroupIndex];
			LowerBase = (uint16_t)(Row * PANEL_WIDTH) + LowerRowMap[GroupIndex];

			for(PixelIndex = 0; PixelIndex < HUB75_GROUP_SIZE; PixelIndex++)
			{
				unsigned char *PlanePointer = OutPointer;

#if(HUB75_COLOR_MODE)
				UpperPixel = DrawBuffer[UpperBase + PixelIndex];
				LowerPixel = DrawBuffer[LowerBase + PixelIndex];

				Red1   = MAP_RED(UpperPixel[0]);
				Green1 = MAP_GREEN(UpperPixel[1]);
				Blue1  = MAP_BLUE(UpperPixel[2]);
				Red2   = MAP_RED(LowerPixel[0]);
				Green2 = MAP_GREEN(LowerPixel[1]);
				Blue2  = MAP_BLUE(LowerPixel[2]);

				for(Plane = 0; Plane < HUB75_BITPLANES; Plane++)
				{
					/* Her degerin en dusuk biti = bu plane'in biti */
					PixelData = (unsigned char)
					            ( (Red1   & 1U)       | ((Green1 & 1U) << 1) | ((Blue1 & 1U) << 2) |
					             ((Red2   & 1U) << 3) | ((Green2 & 1U) << 4) | ((Blue2 & 1U) << 5));

					PlanePointer[0] = PixelData;
					PlanePointer[1] = PixelData | HUB75_CLK_BIT;
					PlanePointer   += HUB75_PLANE_STRIDE;

					Red1 >>= 1;  Green1 >>= 1;  Blue1 >>= 1;
					Red2 >>= 1;  Green2 >>= 1;  Blue2 >>= 1;
				}
#else
				/* 8 renk modunda maske kullanilmaz, her plane ayni veriyi alir */
				PixelData = (unsigned char)((DrawBuffer[UpperBase + PixelIndex] & 0x07U) |
				                            ((DrawBuffer[LowerBase + PixelIndex] & 0x07U) << 3));

				for(Plane = 0; Plane < HUB75_BITPLANES; Plane++)
				{
					PlanePointer[0] = PixelData;
					PlanePointer[1] = PixelData | HUB75_CLK_BIT;
					PlanePointer   += HUB75_PLANE_STRIDE;
				}
#endif

				OutPointer += 2;
			}
		}
	}
}

/* ==========================================================================
   Cizim alanini siyaha boyar
   ========================================================================== */
void HUB75_Clear(void)
{
	uint16_t Coord;

	for(Coord = 0; Coord < HUB75_PIXEL_COUNT; Coord++)
	{
#if(HUB75_COLOR_MODE)
		DrawBuffer[Coord][0] = 0;
		DrawBuffer[Coord][1] = 0;
		DrawBuffer[Coord][2] = 0;
#else
		DrawBuffer[Coord] = 0;
#endif
	}
}

/* ==========================================================================
   DMA2_Stream5 kesmesi (stm32f4xx_it.c cagirir) -- HAL'siz.
   ========================================================================== */
void HUB75_DMA_IRQHandler(void)
{
	uint32_t Flags = DMA2->HISR;

	DMA2->HIFCR = HUB75_DMA_ALL_CLR;			/* hata bayraklari dahil: temizle, devam */
	if(Flags & HUB75_DMA_ISR_TC)
		HUB75_TransferComplete();
}

/* Surucunun cizim tamponu (tam renk: piksel basina R,G,B; satir = PANEL_WIDTH).
   Uygulama dogrudan buraya cizebilir -- ayri bir tuval gerekmez.		*/
unsigned char *HUB75_GetDrawBuffer(void)
{
#if(HUB75_COLOR_MODE)
	return &DrawBuffer[0][0];
#else
	return &DrawBuffer[0];
#endif
}

/* ==========================================================================
   Parlaklik ayari.
   ========================================================================== */
void HUB75_SetBrightness(unsigned char Percent)
{
	if(Percent > 100U)
		Percent = 100U;

	OeTickLimit  = (uint16_t)((HUB75_CIE_PERMILLE(Percent) * HUB75_OE_TIM_ARR) / 1000U);
}

/* ==========================================================================
   Taramayi baslatir.
   ========================================================================== */
void HUB75_DMA_Init(void)
{
	/* LAT pini: gpio.c (CubeMX) LOW hizda kuruyor -- kisa darbe icin en yuksek hiz */
	HUB75_LATCH_PORT->OSPEEDR |= (3U << (HUB75_LATCH_BIT_NUMBER * 2U));

	CurrentRow   = 0;
	CurrentPlane = 0;

	__HAL_TIM_SET_AUTORELOAD(&HUB75_TIMER_HANDLE, HUB75_TIMER_PERIOD);
	HUB75_TIMER_HANDLE.Instance->PSC = 0;
	HUB75_TIMER_HANDLE.Instance->EGR = TIM_EGR_UG;

	HAL_NVIC_SetPriority(HUB75_DMA_IRQ_NUMBER, HUB75_DMA_IRQ_PRIORITY, 0);
	HAL_NVIC_EnableIRQ(HUB75_DMA_IRQ_NUMBER);

	PANEL_OFF();
	HUB75_SetRow(0);
	HUB75_StartRowTransfer();
}
