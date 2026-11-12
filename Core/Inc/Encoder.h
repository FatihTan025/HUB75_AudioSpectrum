/**
  ******************************************************************************
  * @file    Encoder.h
  * @brief   Doner encoder -- PB6/PB7 TIM4 donanim encoder modu, buton PB5
  ******************************************************************************
  * Sayma tamamen donanimda (TIM4): adim kacmaz, CPU yuku yok.
  * Buton aktif LOW, dahili pull-up; yazilimla titresim suzulur.
  ******************************************************************************
  */
#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "main.h"

typedef struct
{
	int8_t		Steps;		/* Bu cagridan beri tik sayisi (+ saat yonu)		*/
	uint8_t		Held;		/* Buton su an basili								*/
	uint8_t		Click;		/* Kisa basma birakildi (basiliyken cevrilmediyse)	*/
	uint8_t		Long;		/* Uzun basma (CFG_ENC_LONG_MS, cevrilmeden), bir kez	*/
} ENC_Event;

void ENC_Init(void);
void ENC_Poll(uint32_t Now, ENC_Event *e);	/* Ana donguden sik sik		*/

#endif /* __ENCODER_H__ */
