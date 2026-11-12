# HUB75 Audio Spectrum – Notlar (4 Kasım 2026)

Yeni oturum: önce bunu oku. Kurallar: Türkçe, kısa cevap. Fatih "başla" demeden kod yazma.
Yazmadan önce cihazdaki dosyayı stage edip diff al (CubeIDE / başka oturum değiştirmiş olabilir).

## GitHub
- Ozel depo: github.com/FatihTan025/HUB75_AudioSpectrum (asil kaynak burasi; ev ve is PC'si buradan ceker).
- Mod resimleri: Tools/Onizleme/uret.sh firmware'in kendi cizim kodunu (Spectrum/SpecDisplay/Settings.c) PC'de
  sentetik muzikle calistirir -> Android/app/src/main/assets/modlar/<ad>.png (kare seridi). Mod eklenince/degisince
  yeniden calistir. Resim moda ADIYLA bagli; resmi olmayan mod uygulamada genel gorunumle cikar.
- Android/ degisince APK otomatik derlenir: Releases > son-apk > Spektrum.apk (telefondan indirip kur).
  Sabit imza Android/app/imza.jks (şifre spektrum) -> yeni APK eskisinin üstüne kurulur; versionCode = GitHub
  derleme no. Simge: Tools/Ikon/ikon_uret.py (siyah zemin, LED bölmeli spektrum, beyaz tepeler).
- Derleme ciktilari (Debug/, build/, __pycache__) depoya girmez (.gitignore).

## Donanım
- STM32F446RC, 180 MHz. Panel: 2×P5 64×32 = 128×32 (sürücü mantıksal 64×64), 1/8 tarama.
- 8 bit-plane (en az 7). RAM ~101 KB / 128 KB. Yığın 0x1000.
- İleride 3–4 panel → GD32F425RGT6 düşünülüyor (192 KB DMA erişimli RAM).

## Pinler
- HUB75: PC0–PC7 (R1 G1 B1 R2 G2 B2 CLK, OE = PC7/TIM3_CH2), A/B/C = PB0–PB2, LAT = PB12.
- Line-in: PA5 (ADC1, TIM2 48 kHz). Ek analog (ileride): PA0–PA3.
- ESP32 UART: PA9/PA10 (USART1, 115200, DMA2 St2/St7).
- ESP32 I2S ses (plan): PC10 CK, PA15 WS, PC12 SD (I2S3).
- Butonlar: PB3 (+), PB4 (−). Encoder (plan): PB6/PB7 TIM4 + buton PB5; IR → PC9 (TIM8) ya da EXTI.
- I2C sensör (AHT20/SHT31): PB8/PB9. SPI flash: PB13–PB15, CS PC8. LSE: PC14/PC15.
- Ses girişi: 4.7µ → 10k/10k bölücü → 1k → PA5, PA5–GND 10n.

## Yazılım yapısı
- Spectrum.c: FFT 1024, 128 log bant 60 Hz–16 kHz, kick/vuruş/tempo, gürültü öğrenme
  (dar bantlı sabit ton gürültü sayılmaz, CFG_NOISE_TONE_SHARE).
- SpecDisplay.c: çizim motorları (SPEC_Mode). Klasik motor tüm çubuk çeşitleri (SPEC_Cls).
- Settings.c: kendini tanıtan ayar tablosu + "görünümler" (ModeInfo[]: motor + varsayılanlar).
  Ayarlar RTC yedek SRAM'de (pil), flash yok. Görünüm başına saklanır.
- Protocol.c: A5 | LEN | SEQ | CMD | PAYLOAD | CRC16. Komut listesi dosya başında.
- PC/SpektrumAraci.py + protokol.py: ayar aracı (sadece Fatih kullanır, gizleme yok).

## Panel menüsü
- Sade OSD menü, ayrıntı MENU_NOTLAR.md. Bant genişliği artık gelişmiş değil (kullanıcı ayarı; menü ve
  Android'de görünür).

## Görünümler (mod listesi)
- Klasik (aile 0): 0 Klasik, 1 Neon, 2 Orta Dağ, 3 Fıskiye, 4 Renkli Dalga, 5 Dalga, 6 Katmanlı Dağ
- Modern (aile 1): 7 Işık Çizgisi, 8 Örgü Şerit, 9 Aurora
- Animasyon (aile 2): 10 Alev, 11 Alev Çubuk, 12 Orta Alev, 13 Alev Halka (açılış), 14 Patlama, 15 Orta Patlama, 16 Halka
- Ortam (aile 3, müziksiz): 17 Kar -- gece gökyüzü, hızlı parlayan yıldızlar, uzak kar saatin arkasında,
  yakın kar önünde; saat soluk gölge (34,40,58). Kar birikmesi yok (32 px'de anlamsız). Geçici font: Font_SaatGecici.h
  (DejaVu Sans Bold 30, 21x22 sabit genişlik); Fatih'in fontu gelince SpecDisplay.c'de SN_FONT değişir, dosya silinir.
- Saat: Saat.c (RTC, LSE, HAL'siz yazmaç). Kristal ~2 s içinde hazır değilse yazılım saati. CMD_TIME 0x40
  Sa Dk Sn [Gün Ay Yıl-2000] ile ayarlanır (yanıt hep 6 bayt; tarih 2000-2099, RTC takvimi). Saat OTOMATİK
  GÖNDERİLMEZ (kullanıcı kararı): Android Ayarlar > "Saat ve tarih" penceresi (telefon saatiyle dolu gelir,
  elle değiştirilebilir), PC aracında "Saati ayarla" düğmesi (PC saati). İnternet gerekmez. Kartta henüz denenmedi.
- Ortam: Android sekmesi aileye değil Kind'a bakar (0 müzikli -> Müzik, 1 müziksiz -> Ortam);
  aile sadece gruplama/sıra (PC'de başlık). Sıra değişince kayıtlı ayarlar kendiliğinden varsayılana döner (Layout CRC).
- Silindi (beğenilmedi): Plazma, Kutup Işığı, Işık Girdabı (akışkan buhar denemesi, 128x32'de ekranı dolduramadı).
- Her görünümde Hız (0 = tempoya göre). Vuruşa tepki verenlerde Tepki (Sakin/Normal/Hareketli).
- Alev rengi: bloklu seçim (Ateş, Pembe, Mor, Mavi gaz, Buz, Yeşil).

## Kararlar
- Birbirinin aynısı mod yok; sadece rengi farklıysa ayrı mod sayılmaz.
- Kullanıcıya mod başına en fazla ~3 ayar. Renk seçimi pencere açmadan (ton çubuğu / bloklar).
- Android asıl arayüz (LED Chord tarzı, resimle mod seçimi). ESP32 köprü: BLE (NUS) + WiFi TCP 5000.
- ESP32 kopru (ESP32/SpektrumKopru, surum 2.0): Wi-Fi AP + ev agi + BLE NUS ayni anda, saydam. 0x7F alt komutlari:
  0 bilgi, 1-3 ev agi, 4-7 yazilim guncelleme (PC aracinda 'ESP32 guncelle'). Arduino IDE ag portu da var (ArduinoOTA).
  Partition: Minimal SPIFFS (1.9MB APP with OTA). Wi-Fi+BLE icin modem uykusu zorunlu (setSleep(true)).
  Kartta denenmedi. Android uygulamasi: Kotlin + Compose (siradaki).
- Android arayuzu (taslak: claude.ai tuvali 'Spektrum Android arayuzu'): LED Chord tarzi, mor zemin + sari vurgu.
  Sekmeler: Muzik / Ortam / Ayarlar (Muzik = Kind 0 muzikli, Ortam = Kind 1 muziksiz; Ortam gorunumleri de birer mod). Spektrum-Animasyon ayrimi YOK; tek kaydirmali mod listesi.
  Mod adi Android'de gosterilmez (sadece resim + sira no). PC aracinda adlar kalir.
  Resim moda ADIYLA baglanir (sira degisirse kaymasin); bilinmeyen mod -> genel resim.
  Mod resimleri: SpecDisplay.c PC'de derlenip ornek muzikle her modun kisa animasyonu uretilecek.
  Ayar satirlari cihazdan (PARAM_DESCRIBE) kurulur: ton=cubuk, blok=renk bloklari, enum=secmeli dugme, bool=kutu.
  Oto dugmesi = SET_ID_AUTO_SEC (main.c'de calisiyor); sure Ayarlar'da. Muzige gore gecis yok (arayuz bogulmasin).
  Ortam sekmesi (saat, sicaklik, kar yagisi gibi muziksiz gorunumler; ad genel kalsin, 'Saat' degil).
  Zincir: muzik -> secili mod; sessizlik X sn -> Ortam'da secili gorunum; daha uzun sessizlik -> uyku;
  muzik baslayinca dogrudan moda donus. 'Muzik durunca gec' suresi Ortam ekraninda (Kapali/30 sn/1 dk/5 dk).
  Kararlar (12 Kas): Oto dongu icin modlara isaret (begenmedigi modu disarida birakir); ses kaynagi secimi
  (Line-in / Telefon ...); surum uyumu (INFO protokol surumu -> 'guncelleyin' uyarisi); acma/kapama saati.
  Ayar yedegi YOK (gerek gorulmedi). STM32 yazilim guncelleme (ESP32 uzerinden BOOT0/NRST + UART bootloader)
  simdilik yok; Ayarlar'da pasif satir. Yeni PCB'de ESP32 -> BOOT0/NRST baglantisi dusunulmeli.
- Android uygulamasi v0.1 (Android/, Kotlin + Compose, paket com.spektrum.kontrol): baglanti (BLE + mDNS + 192.168.4.1),
  Muzik/Ortam (bos kaydirmali kartlar, aile 0+1 / 2), ayar satirlari cihazdan, Oto, parlaklik, Ayarlar (ad, oto sure,
  ev agi, surumler, fabrika). Henuz derlenmedi/denenmedi (Android Studio'da ilk derleme yapilacak).
  Protokol katmani (protokol/*.kt) PC'de derlenip sahte cihazla test edildi (protokol.py ile bayt bayt ayni).
  Firmware: SET_FLAG_ADVANCED 0x10 (Settings.h); isaretli: alt/ust frekans, vurus hassasiyeti, bant genisligi,
  alt/orta/ust renk, gecis -> Android'de gizli, PC aracinda gorunur. Kopru: mDNS adi cihaza ozel (Spektrum-XXXX).
  ONCELIK: once bu cihaz (P5, 2 panel, Line-in) ciksin; temel ileride kokten degisim gerektirmesin, arayuz sik degismesin.
  KURAL: uygulamada sabit liste yok, her sey cihazdan (MODE_LIST, PARAM_DESCRIBE). Panel modeli, ses kaynagi = enum ayar;
  simdilik tek secenek, firmware genisledikce secenek kendiliginden cikar.
  KIT: panel degisebilir (P5 1/8 zigzag, P5 1/16, P4, P2.5 ...). Android Ayarlar > Panel: model listesi (tarama +
  dizilim modelden), panel sayisi 1-4, renk sirasi, test deseni. Yeni PCB'de D ve E adres hatlari SART (su an sadece A/B/C).
  Firmware: surucu + SpecDisplay calisma aninda panel boyutu/tarama/dizilim secebilmeli (panel bagimsizligi isi).
  Hassasiyet (giris kazanci): kartta kisik sesle denenecek; otomatik dengelemiyorsa genel ayara tek cubuk.
- Telefon sesi ESP32'ye A2DP ile gelebilir (I2S → STM32); arayan bilgisi Android uygulamasından.

## Bekleyen işler
- Kartta denenmedi: Hız/Tepki, son düzeltmeler.
- İleride: VFD modu, Şerit, Aurora, Fırtına, Yağmur, 3D efektler (ayar belgesinde liste),
  ortam modları (saat, sıcaklık), encoder + ekran menüsü, IR, ESP32 köprüsü, Android.
- Kod incelemesinden kalanlar: font karakter sınırı (HUB75.c), AcL[96] #error kontrolü,
  protokol hata yanıtları, GET_ALL devamı, EVT_PARAM/EVT_POWER gönderimi.
- Kullanilmayan tanimlar (kalabilir, ileride): SET_TYPE_HUE, SET_TYPE_PALETTE, SET_FLAG_NOSAVE,
  EVT_PARAM/EVT_POWER, PROTO_ERR_BUSY/CRC. CFG_BEAT_FLASH (kullanilmiyor).
- Silindi: VU seridi (cizim + SPEC_Loud hesabi), segmentli cubuk + sonuk segment, tepe "Cizgi",
  CFG_BAR2/3, CFG_VU_*, SPEC_GetMode(). Cubuk tipi: 0 duz, 1 neon. Tepe: 1 nokta, 2 top.
- Yerlesim, Cubuk tipi, Sonuk segmentler, VU seridi ayarlari gizli: moddan secilir (Orta Dag).
- RAM kurali: yeni animasyonun durum verisi SpecDisplay.c U union'ina girer (ayri static dizi yok).
  Degismeyen tablolar flash'ta const. Ornek: SwDistLut (Alev Halka uzaklik, tek ceyrek, 2.7 KB flash,
  4 KB RAM kazanildi, panel sayisindan bagimsiz).
- Panel bagimsizligi: SpecDisplay.c hala 128x32'ye sabit (#error + cok yerde 64 = orta). 3-4 panel icin
  ayrica elden gecirilecek (Orta Alev, Orta Patlama, ayna alev, kivilcim, Halka 64'leri).
- Kural (Fatih): bir gorunum, acik bir ayarla Klasik'ten elde edilebiliyorsa ayri mod olmaz.
- Hiz: 4 kademe (carpanlar SpecDisplay.c Kat[], Fatih deneyerek ayarliyor), tempoya gore. Sadece animasyon
  hareketi + renk donmesi; cubuk/tepe dususu gercek zamanli. Tum cubuk tiplerinde 1 satir histerezis (BarH).
  Alev yayilimi hiza bagli (FirePropagate). Not: Patlama'da hiz degisince goruntu de degisiyor -- simdilik dokunulmayacak.
- Bant genisligi ve Sekil ayar olarak kalir.
- Havai Fisek tamamen silindi (sorunlu calisiyordu, begenilmedi).

## Fikir: Volkan (Animasyon grubu, henuz yazilmadi)
- Sahne: volkan silueti, krater icten kizil; gokyuzunde kul/duman. Alev motoru uzerine.
- Bas (40-100 Hz) vurusu: patlama; lav parcaciklari yercekimiyle yay cizer, dustugu yerde sonerek iz birakir.
  Guc = vurus siddeti (yukseklik, genislik).
- Duman: her patlamada kraterden koyu kul bulutu yukselir, kivrilarak yayilir, yavas soner (toz duman hissi).
- Alt-orta (100-500 Hz): krater/lav parlakligi, lav nehirleri. Orta (500 Hz-4 kHz): duman yogunlugu ve kivrilmasi.
- Tiz (4 kHz+): dumanin icinde kucuk simsekler, kivilcimlar.
- Nakarat/yuksek enerji: surekli lav akisi, yamactan lav nehirleri; sakinde krater icin icin yanar.
- Bantlari tek tek gostermez (animasyon); frekans bolgeleri olaylara baglanir. "Spektrum yelpazesi"
  (parcacik yonu = frekans) dusunuldu ama Fiskiye'ye benzedigi icin elendi.
- RAM: duman izgarasi ~4 KB + parcaciklar ~1 KB, U icinde.
- Alternatif (secilmedi): Kum firtinasi / sok dalgasi.

## Sıradaki: Şömine saat (yeni sohbette başlanacak)
- Sahne: alev panelin altından yanar, saat rakamları alevin içinde durur (şömine).
- Rakamlar katı (odun gibi): alev içinden geçmez; rakam aralarından ve iç boşluklarından (0,4,6,8,9) sızar.
  Rakam olmayan kenarlarda alev daha yükselir (ortada alçak, kenarda yüksek doğal görünüm).
- Köz: alevin değdiği rakam pikseli ısı biriktirir -> koyu kızıl -> parlak turuncu köz, hafif nefes/çıtırtı.
  Alevin ulaşmadığı yerler soğuk/ılık (koyu, kül grisine yakın); ısı düşünce yavaşça bu tona döner.
- Müzik: alev boyu/şiddeti müzikle; bas vuruşunda alev rakamları sarar, közler parlar. Sessizde sakin şömine.
- İki nokta: saniyede bir nefes alan iki küçük köz.
- Saat kaynağı: RTC + LSE kristal (kartta var). ESP32 NTP ile saati alıp işlemciye gönderir (protokole tek komut).
  VBAT pili varsa saat elektrik kesilince de sürer.
- Font: Fatih hazırlayacak, HUB75_Font.h biçiminde; o gelene kadar geçici basit rakam fontu.
  Rakam yüksekliği ~20-24 px önerildi (altta alev için 6-8 satır).
- RAM: köz ısısı ~3 KB, U içinde. .ioc'ye RTC/LSE de işlenecek.

## İleride: Uyku / uyanma ve karakterler
- Uyku: belli süre sessizlikte panel tamamen kararır (LED taraması durur, enerji tasarrufu).
  İşlemci dinlemeye devam eder (mikrofon + FFT).
- Uyanma tetikleri: çift alkış, ıslık, birkaç saniye süren belirgin ses (konuşma/müzik).
  Tek kısa sesler (kapı çarpması) yok sayılır.
- Uyanınca: gece -> uyuyan kedi kafasını kaldırır, esner, etrafına bakar; gündüz -> bir çift göz açılır,
  kırpışır, sese bakar. Birkaç saniye sonra saat; müzik varsa spektrum.
- Tekrar uyku: karakter esneyip gözünü kapatır, ekran yavaşça kararır (süre ayarlı, örn. 1 dk).
- Müzik yokken bekleme animasyonları (spektrum yerine), müzik başlayınca spektruma dönüş.
- Kedi (gece): kıvrılmış, nefes alıp veren, "Zzz", ara sıra kulak oynatma; alkışla uyanır.
- Robot gözleri (gündüz): KENDİ tasarımımız (Cozmo birebir kopyalanmayacak, tescilli tasarım).
  Sessizde etrafa bakar/kırpar/sıkılır, müzikte parlayıp ritimle zıplar, ani basta irkilir,
  uzun sessizlikte uyur. Cihazın "yüzü" olabilir.

## İleride: Ses algılama
- El çırpma: çok kısa (5-20 ms), ani, 1-8 kHz'e düz yayılan (gürültü gibi, spektral düzlük) patlama.
  Müzikteki trampet/zille karışmasın diye ÇİFT alkış deseni (150-400 ms arayla) güvenilir.
- Islık: tek sivri tepe (1-4 kHz), harmoniği az, >= ~200 ms sürer, tepe geri kalana göre çok baskın.
  Yükselen/alçalan ıslık izlenebilir (örn. sonraki/önceki mod). Müzik çalarken daha az güvenilir.
- Kullanım: çift alkış = aç/kapa veya sonraki mod; ıslık = gözler dönüp bakar.
- İki mikrofon (panelin iki ucu, ~60 cm): varış zamanı farkından yön (en fazla ~1.8 ms = ~85 örnek @48 kHz),
  ikinci ADC ile eşzamanlı örnekleme, çapraz korelasyon saniyede birkaç kez. Gözler ani sesin yönüne bakar
  (sürekli müzik sesine takılmaz). Bonus: stereo spektrum.

## İleride: Saat görünümleri, sıcaklık / nem
- Fatih birçok saat fontu hazırlayacak (hepsi HUB75_Font.h biçiminde, her biri flash'ta birkaç KB).
- Saat görünümleri, spektrum "görünüm" (LOOK) mantığıyla: her görünüm = font + alanlar (saat, sıcaklık, nem)
  + konumlar + efekt (şömine, sade, renkli...). Kullanıcı ayarlardan seçer; Python/Android listeyi cihazdan alır.
  Yeni görünüm = tabloya bir satır.
- Sıcaklık/nem: oda içi için I2C sensör (AHT20 / SHT31), dışarısı için ESP32 internetten hava durumu.
  İkisi birlikte olabilir ("içeride 22°, dışarıda 8°").

## İleride: Resim modu (spektrum resme pencere)
- Panelde gerçek bir resim (örn. bir çift göz, manzara). Her sütunun bandı o sütunda resmin ne kadarının
  açılacağını belirler: çubuğun altı resmin gerçek renkleri, üstü karanlık/çok soluk; tepede yumuşak geçiş.
- Bas vuruşunda tüm resim bir an hafif parlar; göz resminde güçlü vuruşta göz bölgesi daha uzun açık kalabilir.
- Resimler 128x32, RGB565 (her biri 8 KB), işlemci flash'ında ayrı bir alanda (örn. 10 resim).
  Python aracı / Android fotoğrafı panele uygun hale getirip Wi-Fi ile yükler; kullanıcı kendi resmini seçer.
- Eski HUB75_Bitmap modülü bu iş için uygun değildi (silindi); maskeleyen küçük yeni kod yazılacak.
