/**
  ******************************************************************************
  * @file    Audio_ADC.c
  * @brief   PA5 (ADC1_IN5) ses ornekleme
  ******************************************************************************
  *   TIM2 update -> TRGO -> ADC1 tek donusum -> DMA2_Stream0 (CH0) -> tampon
  *
  *   ADC saati  : PCLK2 90 MHz / 4 = 22.5 MHz
  *   Donusum    : 84 + 12 = 96 cevrim = 4.27 us  (48 kHz'de 20.8 us var)
  *   DMA        : circular 2 x HOP, half + full kesmesi (HOP'ta bir), oncelik HIGH
  *                (HUB75 DMA2_Stream5 VERY HIGH -- tarama bozulmaz)
  ******************************************************************************
  */
/* Debug yapilandirmasi -O0 derler; bu dosya her zaman hizli derlensin
   (UpdateDisplay -O0'da ~15 ms suruyordu, ses kareleri kaciyordu).	*/
#pragma GCC optimize ("O2")

#include "Audio_ADC.h"

static volatile uint16_t	SampleBuffer[AUDIO_HOP_SIZE * 2U];	/* DMA, circular	*/
static uint16_t				Frame[2][AUDIO_FFT_SIZE];			/* Kare tamponlari	*/
static uint8_t				WriteFrame	= 0;
static uint8_t				LastFrame	= 1;
static volatile int8_t		ReadyFrame	= -1;					/* -1 = yok			*/
volatile uint32_t			AUDIO_OverrunCount = 0;
volatile uint32_t			AUDIO_FrameCount   = 0;			/* Yakalanan kare sayaci	*/

const uint16_t *AUDIO_GetReadyBlock(void)
{
	int8_t f = ReadyFrame;

	if(f < 0)
		return 0;

	return Frame[f];
}

void AUDIO_ReleaseBlock(void)
{
	ReadyFrame = -1;
}

/* Onceki kareyi HOP kadar kaydir, yeni HOP ornegi sona ekle.
   NewestHalf: az once dolan DMA yarisi (0 veya 1).				*/
static void CaptureFrame(uint32_t NewestHalf)
{
	uint32_t			i;
	uint16_t		   *Dst = Frame[WriteFrame];
	const uint16_t	   *Old = Frame[LastFrame];
	const volatile uint16_t *New = &SampleBuffer[NewestHalf * AUDIO_HOP_SIZE];

	for(i = 0; i < AUDIO_FFT_SIZE - AUDIO_HOP_SIZE; i++)
		Dst[i] = Old[i + AUDIO_HOP_SIZE];

	for(i = 0; i < AUDIO_HOP_SIZE; i++)
		Dst[AUDIO_FFT_SIZE - AUDIO_HOP_SIZE + i] = New[i];

	if(ReadyFrame >= 0)
		AUDIO_OverrunCount++;

	AUDIO_FrameCount++;
	LastFrame  = WriteFrame;
	ReadyFrame = (int8_t)WriteFrame;
	WriteFrame ^= 1U;
}

/* ==========================================================================
   DMA2_Stream0 kesmesi -- yari / tam doldu
   ========================================================================== */
void DMA2_Stream0_IRQHandler(void)
{
	uint32_t Flags = DMA2->LISR;

	if(Flags & DMA_LISR_HTIF0)
	{
		DMA2->LIFCR = DMA_LIFCR_CHTIF0;
		CaptureFrame(0);
	}

	if(Flags & DMA_LISR_TCIF0)
	{
		DMA2->LIFCR = DMA_LIFCR_CTCIF0;
		CaptureFrame(1);
	}

	/* Hata bayraklari -- temizle, devam et */
	if(Flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0))
		DMA2->LIFCR = DMA_LIFCR_CTEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;
}

/* ==========================================================================
   Baslatma
   ========================================================================== */
void AUDIO_Init(void)
{
	/* --- Saatler --------------------------------------------------- */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	__DSB();

	/* --- PA5 analog ------------------------------------------------- */
	GPIOA->MODER |=  (3U << (5U * 2U));
	GPIOA->PUPDR &= ~(3U << (5U * 2U));

	/* --- DMA2 Stream0, Channel 0 = ADC1 ----------------------------- */
	DMA2_Stream0->CR &= ~DMA_SxCR_EN;
	while(DMA2_Stream0->CR & DMA_SxCR_EN) { }

	DMA2->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 |
	              DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0;

	DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;
	DMA2_Stream0->M0AR = (uint32_t)SampleBuffer;
	DMA2_Stream0->NDTR = AUDIO_HOP_SIZE * 2U;
	DMA2_Stream0->FCR  = 0;									/* Direct mode	*/
	DMA2_Stream0->CR   = (0U << DMA_SxCR_CHSEL_Pos)			/* CH0 = ADC1	*/
	                   | (2U << DMA_SxCR_PL_Pos)				/* HIGH			*/
	                   | (1U << DMA_SxCR_MSIZE_Pos)			/* 16 bit		*/
	                   | (1U << DMA_SxCR_PSIZE_Pos)			/* 16 bit		*/
	                   | DMA_SxCR_MINC | DMA_SxCR_CIRC
	                   | DMA_SxCR_HTIE | DMA_SxCR_TCIE;		/* DIR = P->M	*/

	HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, AUDIO_DMA_IRQ_PRIORITY, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

	DMA2_Stream0->CR |= DMA_SxCR_EN;

	/* --- ADC1 ------------------------------------------------------- */
	ADC->CCR   = (ADC->CCR & ~ADC_CCR_ADCPRE) | (1U << ADC_CCR_ADCPRE_Pos);	/* /4 */
	ADC1->CR1  = 0;											/* 12 bit, tek kanal	*/
	ADC1->SQR1 = 0;											/* L = 1 donusum		*/
	ADC1->SQR3 = 5U;										/* IN5 = PA5			*/
	ADC1->SMPR2 = (ADC1->SMPR2 & ~ADC_SMPR2_SMP5) | (4U << ADC_SMPR2_SMP5_Pos);	/* 84 cyc */
	ADC1->CR2  = (1U << ADC_CR2_EXTEN_Pos)					/* Yukselen kenar		*/
	           | (6U << ADC_CR2_EXTSEL_Pos)					/* TIM2_TRGO			*/
	           | ADC_CR2_DMA | ADC_CR2_DDS
	           | ADC_CR2_ADON;

	/* --- TIM2: Fs tetik --------------------------------------------- */
	TIM2->CR1  = 0;
	TIM2->PSC  = 0;
	TIM2->ARR  = (AUDIO_TIM_CLOCK / AUDIO_SAMPLE_RATE) - 1U;
	TIM2->CR2  = (2U << TIM_CR2_MMS_Pos);					/* TRGO = update		*/
	TIM2->EGR  = TIM_EGR_UG;
	TIM2->CR1  = TIM_CR1_CEN;
}
