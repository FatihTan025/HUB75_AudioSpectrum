/**
  ******************************************************************************
  * @file    Uart1.c
  * @brief   USART1 DMA surucusu -- bkz. Uart1.h
  ******************************************************************************
  */
#pragma GCC optimize("O2")

#include "main.h"
#include "Uart1.h"

#define RX_SIZE			512U		/* 2'nin kuvveti					*/
#define TX_SIZE			1024U		/* 2'nin kuvveti					*/

#define RX_STREAM		DMA2_Stream2
#define TX_STREAM		DMA2_Stream7
#define RX_FLAGS		0x003D0000UL	/* LIFCR: Stream2'nin tum bayraklari	*/
#define TX_FLAGS		0x0F400000UL	/* HIFCR: Stream7'nin tum bayraklari	*/
#define TX_TCIF			DMA_HISR_TCIF7

static uint8_t			RxBuf[RX_SIZE];
static uint16_t			RxTail;

static uint8_t			TxBuf[TX_SIZE];
static volatile uint16_t TxHead, TxTail;	/* Serbest sayaclar (tasarak sarar) */
static uint16_t			TxBusy;			/* DMA'daki bayt sayisi, 0 = bos	*/

void UART1_Init(uint32_t Baud)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	(void)RCC->APB2ENR;

	/* PA9 TX, PA10 RX: AF7 */
	GPIOA->MODER   = (GPIOA->MODER & ~((3U << 18) | (3U << 20))) | (2U << 18) | (2U << 20);
	GPIOA->AFR[1]  = (GPIOA->AFR[1] & ~((0xFU << 4) | (0xFU << 8))) | (7U << 4) | (7U << 8);
	GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(3U << 18)) | (1U << 18);
	GPIOA->PUPDR   = (GPIOA->PUPDR & ~(3U << 20)) | (1U << 20);	/* RX pull-up	*/

	USART1->CR1 = 0;
	USART1->BRR = (HAL_RCC_GetPCLK2Freq() + Baud / 2U) / Baud;
	USART1->CR2 = 0;
	USART1->CR3 = USART_CR3_DMAR | USART_CR3_DMAT;

	/* RX: dairesel, durmadan doldurur */
	RX_STREAM->CR = 0;
	while(RX_STREAM->CR & DMA_SxCR_EN) {}
	DMA2->LIFCR    = RX_FLAGS;
	RX_STREAM->PAR  = (uint32_t)&USART1->DR;
	RX_STREAM->M0AR = (uint32_t)RxBuf;
	RX_STREAM->NDTR = RX_SIZE;
	RX_STREAM->FCR  = 0;
	RX_STREAM->CR   = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC;
	RX_STREAM->CR  |= DMA_SxCR_EN;

	/* TX: her gonderimde yeniden kurulur */
	TX_STREAM->CR = 0;
	while(TX_STREAM->CR & DMA_SxCR_EN) {}
	DMA2->HIFCR    = TX_FLAGS;
	TX_STREAM->PAR = (uint32_t)&USART1->DR;
	TX_STREAM->FCR = 0;
	TX_STREAM->CR  = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_DIR_0;

	RxTail = 0;
	TxHead = TxTail = 0;
	TxBusy = 0;

	USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

uint16_t UART1_Read(uint8_t *Dst, uint16_t Max)
{
	uint16_t Head = (uint16_t)((RX_SIZE - RX_STREAM->NDTR) & (RX_SIZE - 1U));
	uint16_t n = 0;

	while(RxTail != Head && n < Max)
	{
		Dst[n++] = RxBuf[RxTail];
		RxTail   = (uint16_t)((RxTail + 1U) & (RX_SIZE - 1U));
	}
	return n;
}

uint8_t UART1_Write(const uint8_t *Src, uint16_t Len)
{
	uint16_t Used = (uint16_t)(TxHead - TxTail);

	if(Len > (uint16_t)(TX_SIZE - Used))
		return 0;
	for(uint16_t i = 0; i < Len; i++)
		TxBuf[(uint16_t)(TxHead + i) & (TX_SIZE - 1U)] = Src[i];
	TxHead = (uint16_t)(TxHead + Len);
	UART1_Poll();
	return 1;
}

void UART1_Poll(void)
{
	uint16_t Start, Len;

	if(TxBusy)
	{
		if(!(DMA2->HISR & TX_TCIF))
			return;
		DMA2->HIFCR = TX_FLAGS;
		TxTail = (uint16_t)(TxTail + TxBusy);
		TxBusy = 0;
	}
	if(TxHead == TxTail)
		return;

	/* Tamponun sonuna kadar olan bitisik parca */
	Start = (uint16_t)(TxTail & (TX_SIZE - 1U));
	Len   = (uint16_t)(TxHead - TxTail);
	if(Len > (uint16_t)(TX_SIZE - Start))
		Len = (uint16_t)(TX_SIZE - Start);

	TxBusy = Len;
	DMA2->HIFCR     = TX_FLAGS;
	TX_STREAM->M0AR = (uint32_t)&TxBuf[Start];
	TX_STREAM->NDTR = Len;
	TX_STREAM->CR  |= DMA_SxCR_EN;
}
