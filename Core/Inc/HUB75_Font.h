/**
  ******************************************************************************
  * @file           : HUB75_Font.h
  * @brief          : Font veri yapilari - butun fontlar icin ortak
  ******************************************************************************
  * Font header dosyalari bu dosyayi include eder. Yapilar burada bir kez
  * tanimlandigi icin ayni projede istedigin kadar font kullanabilirsin,
  * yeniden tanimlama hatasi olusmaz.
  *
  * FontUret.py ile uretilen fontlar bu yapiya uygundur.
  ******************************************************************************
  */
#ifndef __HUB75_FONT_H__
#define __HUB75_FONT_H__

#include <stdint.h>

/* --------------------------------------------------------------------------
   Karakter tanimlayicisi

   BWidth	Satir basina byte sayisi. Piksel genisliginin 8'e bolunup
			yukari yuvarlanmis halidir. Bitmap icinde bir satirin kac
			byte tuttugunu belirler.
   Width	Karakterin gercek piksel genisligi. Karakterler arasi bosluk
			hesabinda bu deger kullanilir.
   Height	Karakter yuksekligi (piksel).
   OffSet	Bitmap dizisi icindeki baslangic konumu (byte).

   Kullanilmayan karakterlerin tanimlayicisinda BWidth ve Width sifirdir.
   Dizideki yerleri korunur, boylece dizin hesabi bozulmaz.
   -------------------------------------------------------------------------- */
typedef struct
{
	uint8_t		BWidth;
	uint8_t		Width;
	uint8_t		Height;
	uint16_t	OffSet;

} FONT_CHAR_INFO;

/* --------------------------------------------------------------------------
   Font bilgisi

   Height		Butun karakterlerin ortak yuksekligi (piksel).
   StartChar	Tanimlayici dizisinin ilk karakteri. Dizin hesabi
				(Karakter - StartChar) seklinde yapilir.
   EndChar		Son karakter. Sinir kontrolu icin kullanilir.
   Descriptors	Karakter tanimlayici dizisi.
   Bitmap		Piksel verisi. Satir basina BWidth byte, en anlamli
				bitten baslayarak soldan saga.
   -------------------------------------------------------------------------- */
typedef struct
{
	uint8_t					Height;
	char					StartChar;
	char					EndChar;
	const FONT_CHAR_INFO   *Descriptors;
	const uint8_t          *Bitmap;

} FONT_INFO;

#endif /* __HUB75_FONT_H__ */
