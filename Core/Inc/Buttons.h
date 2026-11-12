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

/* Ana donguden cagrilir. Donus: +1 sonraki, -1 onceki, 0 basim yok */
int  BTN_Poll(void);

#endif /* __BUTTONS_H__ */
