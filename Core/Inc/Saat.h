/**
  ******************************************************************************
  * @file    Saat.h
  * @brief   Saat: RTC (LSE 32.768 kHz kristal, VBAT pili ile elektrik kesilince de surer)
  ******************************************************************************
  * Kristal hazir olana kadar (en fazla ~2 s) ve kristal yoksa saat HAL_GetTick ile
  * yazilimda yurur; kristal hazir olunca RTC'ye gecer. Saat protokolle (CMD_TIME)
  * ayarlanir: Android uygulamasi baglaninca telefonun saatini gonderir.
  ******************************************************************************
  */
#ifndef __SAAT_H__
#define __SAAT_H__

#include <stdint.h>

void SAAT_Init(void);										/* SET_Init'ten sonra bir kez	*/
void SAAT_Poll(void);										/* Ana donguden					*/
void SAAT_Get(uint8_t *Sa, uint8_t *Dk, uint8_t *Sn);
void SAAT_Set(uint8_t Sa, uint8_t Dk, uint8_t Sn);

#endif /* __SAAT_H__ */
