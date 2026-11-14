/**
  ****************************************************************************
  * @file           : arialbold24px.h
  * @brief          : arialnarrow_bold.ttf, 24 piksel yukseklik
  ****************************************************************************
  * HUB75 Araci tarafindan uretilmistir, elle duzenleme gerektirmez.
  *
  * Karakterler    : 0123456789:C
  * Dizi araligi   : '0' .. 'C'
  * Bitmap boyutu  : 552 byte
  *
  * Kullanim: HUB75_Config.h icine su satiri ekle
  *     #include "arialbold24px.h"
  * Cizim sirasinda:
  *     HUB75_PutNumberCentered(0, Text, &arialbold24pxFontInfo, GREEN);
  ****************************************************************************
  */
#ifndef __ARIALBOLD24PX_H__
#define __ARIALBOLD24PX_H__

#include "HUB75_Font.h"

extern const uint8_t        arialbold24pxBitmaps[];
extern const FONT_CHAR_INFO arialbold24pxDescriptors[];
extern const FONT_INFO      arialbold24pxFontInfo;

#endif /* __ARIALBOLD24PX_H__ */
