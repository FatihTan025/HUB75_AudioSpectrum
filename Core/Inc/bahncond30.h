/**
  ****************************************************************************
  * @file           : bahncond30.h
  * @brief          : bahnschrift.ttf  [Condensed], 30 piksel yukseklik
  ****************************************************************************
  * HUB75 Araci tarafindan uretilmistir, elle duzenleme gerektirmez.
  *
  * Karakterler    : 0123456789:
  * Dizi araligi   : '0' .. ':'
  * Bitmap boyutu  : 600 byte
  *
  * Kullanim: HUB75_Config.h icine su satiri ekle
  *     #include "bahncond30.h"
  * Cizim sirasinda:
  *     HUB75_PutNumberCentered(0, Text, &bahncond30FontInfo, GREEN);
  ****************************************************************************
  */
#ifndef __BAHNCOND30_H__
#define __BAHNCOND30_H__

#include "HUB75_Font.h"

extern const uint8_t        bahncond30Bitmaps[];
extern const FONT_CHAR_INFO bahncond30Descriptors[];
extern const FONT_INFO      bahncond30FontInfo;

#endif /* __BAHNCOND30_H__ */
