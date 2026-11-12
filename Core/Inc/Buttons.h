/**
  ******************************************************************************
  * @file    Buttons.h
  * @brief   Mod butonlari -- aktif LOW, dahili pull-up (SpecConfig.h)
  ******************************************************************************
  */
#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include "main.h"

void BTN_Init(void);

/* Kisa basma birakinca bildirilir (uzun basmadan ayirmak icin).
   Kisa: +1 PB3 (sonraki), -1 PB4 (onceki), 0 yok.  Uzun: PB3 CFG_BTN_LONG_MS basili tutuldu (bir kez). */
typedef struct { int8_t Kisa; uint8_t Uzun; } BTN_Olay;

void BTN_Poll(BTN_Olay *o);

#endif /* __BUTTONS_H__ */
