/**
  ****************************************************************************
  * @file           : bahnbd16.h
  * @brief          : bahnschrift.ttf  [Bold], 16 piksel yukseklik
  ****************************************************************************
  * HUB75 Araci tarafindan uretilmistir, elle duzenleme gerektirmez.
  *
  * Karakterler    : %0123456789:C
  * Dizi araligi   : '%' .. 'C'
  * Bitmap boyutu  : 384 byte
  *
  * Kullanim: HUB75_Config.h icine su satiri ekle
  *     #include "bahnbd16.h"
  * Cizim sirasinda:
  *     HUB75_PutNumberCentered(0, Text, &bahnbd16FontInfo, GREEN);
  ****************************************************************************
  */
#ifndef __BAHNBD16_H__
#define __BAHNBD16_H__

#include "HUB75_Font.h"

extern const uint8_t        bahnbd16Bitmaps[];
extern const FONT_CHAR_INFO bahnbd16Descriptors[];
extern const FONT_INFO      bahnbd16FontInfo;

#endif /* __BAHNBD16_H__ */
