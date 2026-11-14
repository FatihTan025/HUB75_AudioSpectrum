/**
  ****************************************************************************
  * @file           : arialbd13.h
  * @brief          : arialbd.ttf, 14 piksel yukseklik
  ****************************************************************************
  * HUB75 Araci tarafindan uretilmistir, elle duzenleme gerektirmez.
  *
  * Karakterler    : %0123456789:C
  * Dizi araligi   : '%' .. 'C'
  * Bitmap boyutu  : 224 byte
  *
  * Kullanim: HUB75_Config.h icine su satiri ekle
  *     #include "arialbd13.h"
  * Cizim sirasinda:
  *     HUB75_PutNumberCentered(0, Text, &arialbd13FontInfo, GREEN);
  ****************************************************************************
  */
#ifndef __ARIALBD13_H__
#define __ARIALBD13_H__

#include "HUB75_Font.h"

extern const uint8_t        arialbd13Bitmaps[];
extern const FONT_CHAR_INFO arialbd13Descriptors[];
extern const FONT_INFO      arialbd13FontInfo;

#endif /* __ARIALBD13_H__ */
