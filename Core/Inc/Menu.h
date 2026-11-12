/**
  ******************************************************************************
  * @file    Menu.h
  * @brief   Panel menusu: aktif modun ayarlarinda sirayla gezinme (ust bant OSD)
  ******************************************************************************
  * Encoder: kisa bas = ac / sonraki ayar (sondan sonra kapanir), cevir = deger,
  *          uzun bas = kapat. PB3 uzun = ac / sonraki ayar, PB3/PB4 kisa = deger.
  * Liste cihazin ayar tablosundan: modun kendi ayarlari, gizli ve gelismis olmayanlar,
  * o anki degerlere gore gorunenler (Android ve PC araci ile ayni kural).
  * Islem yapilmazsa MENU_ZAMAN_MS sonra kapanir; degisiklikler aninda uygulanir.
  ******************************************************************************
  */
#ifndef __MENU_H__
#define __MENU_H__

#include <stdint.h>

uint8_t MENU_Acik(void);
void    MENU_Ileri(void);					/* Ac / sonraki ayar / (sonda) kapat	*/
void    MENU_Kapat(void);
void    MENU_Degistir(int32_t Adim);		/* Aktif ayarin degerini adim kadar degistir	*/
void    MENU_Poll(uint32_t Now);			/* Zaman asimi, mod degisimi				*/

#endif /* __MENU_H__ */
