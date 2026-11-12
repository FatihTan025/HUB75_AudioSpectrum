/**
  ******************************************************************************
  * @file    Uart1.h
  * @brief   USART1 (PA9 = TX, PA10 = RX) -- ESP32 / USB-TTL haberlesme hatti
  ******************************************************************************
  * RX: DMA2_Stream2 (CH4) dairesel tampon, kesme yok.
  * TX: DMA2_Stream7 (CH4), halka tampondan; UART1_Poll ilerletir.
  * Ikisi de dusuk DMA onceligi -- panelin DMA2_Stream5'i etkilenmez.
  ******************************************************************************
  */
#ifndef __UART1_H__
#define __UART1_H__

#include <stdint.h>

#define UART1_BAUD			115200U

void		UART1_Init(uint32_t Baud);

/* Gelen baytlar: en fazla Max kadar kopyalar, kopyalanan sayiyi doner */
uint16_t	UART1_Read(uint8_t *Dst, uint16_t Max);

/* Gonderim kuyruguna ekler. Yer yoksa hic eklemez, 0 doner */
uint8_t		UART1_Write(const uint8_t *Src, uint16_t Len);

/* Ana donguden: biten gonderimi kapatir, siradakini baslatir */
void		UART1_Poll(void);

#endif /* __UART1_H__ */
