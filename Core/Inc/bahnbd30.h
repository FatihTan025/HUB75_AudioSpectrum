/**
  ****************************************************************************
  * @file           : bahnbd30.h
  * @brief          : bahnschrift.ttf  [Bold], 30 piksel yukseklik
  ****************************************************************************
  * HUB75 Araci tarafindan uretilmistir, elle duzenleme gerektirmez.
  *
  * Karakterler    : 0123456789:
  * Dizi araligi   : '0' .. ':'
  * Bitmap boyutu  : 900 byte
  *
  * Kullanim: HUB75_Config.h icine su satiri ekle
  *     #include "bahnbd30.h"
  * Cizim sirasinda:
  *     HUB75_PutNumberCentered(0, Text, &bahnbd30FontInfo, GREEN);
  ****************************************************************************
  */
#ifndef __BAHNBD30_H__
#define __BAHNBD30_H__

#include "HUB75_Font.h"

extern const uint8_t        bahnbd30Bitmaps[];
extern const FONT_CHAR_INFO bahnbd30Descriptors[];
extern const FONT_INFO      bahnbd30FontInfo;

#endif /* __BAHNBD30_H__ */
