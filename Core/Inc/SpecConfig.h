/**
  ******************************************************************************
  * @file    SpecConfig.h
  * @brief   AUDIO SPECTRUM -- TUM AYARLAR BU DOSYADA
  ******************************************************************************
  * Degistir -> derle -> yukle. Baska dosyada ayar yoktur.
  * Zaman ayarlari saniye / ms cinsinden; kare hizindan bagimsizdir.
  ******************************************************************************
  */
#ifndef __SPECCONFIG_H__
#define __SPECCONFIG_H__

/* ==========================================================================
   1) ORNEKLEME / FFT
   ========================================================================== */
#define CFG_SAMPLE_RATE			48000U	/* Hz. Nyquist = yarisi					*/
#define CFG_FFT_SIZE			1024U	/* 1024 veya 2048. Buyuk = bas ince,
										   kucuk = darbelere daha hizli tepki	*/
#define CFG_HOP_SIZE			512U	/* Kac yeni ornekte bir kare.
										   512 -> 94 kare/s, 1024 -> 47 kare/s
										   AUDIO_OverrunCount artarsa buyut		*/

/* ==========================================================================
   2) BANTLAR
   ========================================================================== */
#define CFG_BAND_COUNT			128		/* Analiz cozunurlugu -- 128 BIRAK.
										   Her mod kendi bant sayisini buradan
										   uretir (asagida 6. bolum)			*/
#define CFG_FREQ_LOW			60.0f	/* Hz -- muzik icin 50..80				*/
#define CFG_FREQ_HIGH			16000.0f/* Hz -- ustu muzikte cogu zaman bos	*/

/* ==========================================================================
   3) SEVIYE / AGC
   ========================================================================== */
#define CFG_DB_RANGE			34.0f	/* Gosterilen dinamik aralik, dB.
										   Kucuk = daha "dolu" ve sert			*/
#define CFG_DB_FLOOR_TOP		(-38.0f)/* Sessizlik esigi. Sessizde kipirti
										   varsa yukselt (-10)					*/
#define CFG_AGC_DECAY_DB_S		3.0f	/* AGC'nin sessize dogru acilma hizi	*/
#define CFG_TILT_DB_OCT			2.0f	/* Tize ek kazanc, dB/oktav (0..3)		*/

/* ==========================================================================
   3b) RITIM VURGUSU (PUNCH)
   Her bant kendi yakin ortalamasina gore olceklenir: sabit sesler dibe
   coker, vuruslar tavana firlar. Muzigin ritmini hissettiren ayar budur.
   ========================================================================== */
#define CFG_PUNCH				0.85f	/* 0 = duz dB olcek, 1 = tamamen vurus	*/
#define CFG_BARS_HEADROOM_DB	4.0f	/* Cubuk modlari: en guclu bant tavanin bu kadar altinda kalir */
#define CFG_PUNCH_BARS			0.35f	/* Cubuk modlari (Klasik, Renkli Dalga): dusuk = gercek spektrum sekli, sinus can gibi */
#define CFG_PUNCH_FLOOR			0.80f	/* Ortalamanin bu kati = sifir seviye.
										   Buyuk (0.9) = daha sert, daha bos panel */
#define CFG_PUNCH_AVG_SEC		0.60f	/* Ortalama penceresi					*/
#define CFG_PUNCH_MAX_SEC		1.50f	/* Tepe izcinin sonme suresi			*/

/* Bosta parazit bastirma */
#define CFG_SILENCE_DB			(-48.0f)/* Toplam guc bunun altinda = sessiz.
										   Muzik yokken kipirti varsa yukselt (-42) */
#define CFG_NOISE_MARGIN_DB		5.0f	/* Bant, kendi gurultu tabaninin bu kadar
										   ustune cikmadan gosterilmez			*/
#define CFG_NOISE_RISE_DB_S		3.0f	/* Gurultu tabani ogrenme hizi, 1/s (sadece sessizlikte) */
#define CFG_NOISE_MAX_DB		(-42.0f)	/* Bunun altindaki duragan ses hemen gurultu sayilir */
#define CFG_NOISE_TONE_SHARE	0.60f	/* Gucun bu kadari tek tepede toplaniyorsa ton: gurultu sayilmaz */
#define CFG_NOISE_LEARN_SEC		3.0f	/* Ustundeki duragan ses bu kadar kesintisiz surerse gurultu sayilir */

/* ==========================================================================
   4) TEPKI HIZI
   ========================================================================== */
#define CFG_ATTACK				1.0f	/* Yukselis: 1.0 = aninda, 0.3 = yumusak	*/
#define CFG_BAR_FALL_PX_S		420.0f	/* Cubuk dusus hizi, piksel/s			*/
#define CFG_PEAK_HOLD_MS		80U	/* Tepe noktasi bekleme					*/
#define CFG_PEAK_GRAVITY		900.0f	/* Tepe noktasi dusus ivmesi, px/s^2	*/

#define CFG_BAR_MIN_PX			1U		/* Zemin: her bantta hep yanan piksel	*/
#define CFG_BAR_MIN_DIM			256U	/* Zemin parlakligi 0..256				*/

/* ==========================================================================
   5) RITIM (BEAT) ALGILAMA
   ========================================================================== */
#define CFG_BEAT_FREQ_LOW		40.0f	/* Bas davul/kick araligi, Hz			*/
#define CFG_BEAT_FREQ_HIGH		160.0f
#define CFG_BEAT_THRESHOLD		1.45f	/* Ortalamanin kac kati = vurus.
										   Kucuk = daha cok vurus algilar		*/
#define CFG_BEAT_AVG_SEC		0.8f	/* Ortalama penceresi					*/
#define CFG_BEAT_MIN_MS			180U	/* Iki vurus arasi en az sure			*/
#define CFG_BEAT_DECAY_S		0.25f	/* Vurus parlamasinin sonme suresi		*/

/* Kick (en dusuk bas) dedektoru */
#define CFG_KICK_FREQ_LOW		40.0f	/* Hz									*/
#define CFG_KICK_FREQ_HIGH		100.0f	/* Hz									*/
#define CFG_KICK_RATIO			2.0f	/* Ortalamanin kac kati = darbe. Buyuk = daha secici */
#define CFG_KICK_DB_BELOW		20.0f	/* En guclu banttan en fazla bu kadar dB asagida
										   olmali (kucuk = sadece cok guclu bas)	*/
#define CFG_KICK_DOM			0.5f	/* Bas baskinligi: kick bandi gucu >= alt-orta bandin bu kati
										   (ses/piyano bas sanilmasin; buyuk = daha secici)	*/
#define CFG_MID_FREQ_LOW		150.0f	/* Alt-orta bant (ses, piyano), Hz		*/
#define CFG_MID_FREQ_HIGH		400.0f
#define CFG_KICK_FLOOR_DB		30.0f	/* Kick ortalamasi en guclu bandin bu kadar dB altina inmez
										   (sessiz giriste oran sismesin)		*/
#define CFG_HIT_RATIO			1.8f	/* Genel vurus: akis ortalamasinin kac kati		*/
#define CFG_SENS				50U		/* Vurus hassasiyeti 0..100; 50 = yukaridaki degerler aynen	*/

/* ==========================================================================
   6) GORSEL
   ========================================================================== */
#define CFG_BRIGHTNESS			100U		/* Temel parlaklik, %					*/
#define CFG_BEAT_BRIGHT_BOOST	25U		/* Vurusta ek parlaklik, % (0 = kapali)	*/
#define CFG_HUE_SPEED			40.0f	/* Renk donme hizi, derece/s			*/
#define CFG_TRAIL				200U	/* Iz birakan modlarda iz uzunlugu 0..250 */
#define CFG_START_MODE			14U		/* Acilis modu: 14 = Alev Halka			*/
#define CFG_AUTO_MODE_SEC		0U		/* >0: bu kadar saniyede bir otomatik gec */
#define CFG_SHOW_MODE_MS		800U	/* Mod degisince sol ustte numara		*/
#define CFG_SHOW_FREQ			0		/* 1: sag ustte tepe frekansi (test)	*/
#define CFG_SHOW_OVERRUN		0		/* 1: sag ustte kacan ses karesi sayaci (test) */
#define CFG_SWAP_HALVES			0		/* Panel yarilari ters ise 1			*/

/* Alev ve Patlama modlari */
#define CFG_FIRE_HEIGHT			1.5f	/* Alev boyu carpani (0.6 .. 1.5)		*/
#define CFG_FIRE_WIND			1		/* 0 = duz, 1 = hafif ruzgar/titresim	*/
#define CFG_FIRE_TREBLE			0.4f	/* Tiz bantlarda alev yakiti artisi (0 = yok, 1 = en tizde 2x) */
#define CFG_FIRE_SPARKS			1		/* Vurusta kivilcim						*/
#define CFG_FIRE_BLAST_SIZE		1.0f	/* Patlama, Orta Patlama buyuklugu (0.5 .. 1.5) */
#define CFG_RING_REACH			1.0f	/* Alev Halka: alev halkasinin uzanma carpani */
#define CFG_RING_SLOW			1.35f	/* Alev Halka: halkalarin acilma ve sonme suresi carpani */

/* Modlar (PB3 = +, PB4 = -) -- kullanicinin gordugu "gorunumler" Settings.c
   ModeInfo[] tablosunda. Her biri bir cizim motoru + kendi varsayilanlari:
     0 Klasik  1 Neon  2 Orta Dag  3 Fiskiye  4 Renkli Dalga  5 Isik Cizgisi
     6 Katmanli Dag  7 Orgu Serit  8 Aurora                          (Spektrum)
     9 Alev  10 Alev Cubuk  11 Orta Alev  12 Patlama  13 Orta Patlama
     14 Alev Halka  15 Halka  16 Dalga                                (Animasyon)
   Cizim motorlari: SpecDisplay.h SPEC_Mode.						*/

/* ==========================================================================
   7) BUTONLAR -- aktif LOW, dahili pull-up
   ========================================================================== */
#define CFG_BTN_PORT			GPIOB
#define CFG_BTN_NEXT_PIN		3U		/* PB3 = sonraki mod					*/
#define CFG_BTN_PREV_PIN		4U		/* PB4 = onceki mod						*/
#define CFG_BTN_DEBOUNCE_MS		30U

/* Doner encoder (Encoder.c): PB6/PB7 TIM4, buton PB5 */
#define CFG_ENC_DIR				1		/* Yon ters ise -1						*/
#define CFG_ENC_FILTER			6U		/* TIM4 giris suzgeci 0..15				*/
#define CFG_ENC_LONG_MS			1000U	/* Uzun basma: ekran ac/kapa			*/
#define CFG_ENC_BRIGHT_STEP		5		/* Basili cevirme: parlaklik adimi %	*/

/* ==========================================================================
   Kontroller -- dokunma
   ========================================================================== */
#define CFG_FPS					((float)CFG_SAMPLE_RATE / (float)CFG_HOP_SIZE)

#if((128 % CFG_BAND_COUNT) != 0) || (CFG_BAND_COUNT > 128)
	#error "CFG_BAND_COUNT 16, 32, 64 veya 128 olmali"
#endif
#if(CFG_FFT_SIZE != 1024U) && (CFG_FFT_SIZE != 2048U)
	#error "CFG_FFT_SIZE 1024 veya 2048 olmali"
#endif
#if((CFG_FFT_SIZE % CFG_HOP_SIZE) != 0U) || (CFG_HOP_SIZE > CFG_FFT_SIZE)
	#error "CFG_HOP_SIZE, CFG_FFT_SIZE'i tam bolmeli"
#endif

#endif /* __SPECCONFIG_H__ */
