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

## Android bağlantı mimarisi (13 Kasım)
- AgSecici.kt: her Wi-Fi soketi kendi ağında açılır (Network.socketFactory), Android'in tahminine bırakılmaz.
  Sıra: ev ağı -> kendi ağı (telefon zaten bağlıysa) -> Bluetooth -> kendi ağı, uygulama bağlanır
  (Android 10+ WifiNetworkSpecifier "yalnız yerel": telefonun interneti kesilmez, sistem bir kez sorar;
  Android 7-9 enableNetwork + 192.168.4.x ağı beklenir, soket o ağa bağlanır -> J4/Huidu sorunu olmaz).
  Ağ bulunamazsa düz sokete düşer (eski davranış). Keşif: BLE adı = AP adı (ssid), Wi-Fi taraması; 192.168.4.1
  TCP yoklaması yalnız telefon internetsiz Wi-Fi'dayken (ev ağında PC aracının bağlantısını düşürmesin).
  Kartta/telefonda denenmedi.

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
- HUB75 D satırı: PB10 (HUB75_Config.h) — boş DEĞİL.
- I2C sensör (AHT20/SHT31): PB8/PB9 (gpio.c PB9'u CubeMX artığı çıkış yapıyor, sensörle kaldırılacak). SPI flash: PB13–PB15, CS PC8. LSE: PC14/PC15.
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
- Animasyon (aile 2): 10 Alev, 11 Alev Çubuk, 12 Orta Alev, 13 Alev Halka, 14 Patlama, 15 Orta Patlama, 16 Halka
- Ortam (aile 3, müziksiz): 17 Kar -- gece gökyüzü, hızlı parlayan yıldızlar, uzak kar saatin arkasında,
  yakın kar önünde; saat soluk gölge (34,40,58). Kar birikmesi yok (32 px'de anlamsız). Geçici font: Font_SaatGecici.h
  (DejaVu Sans Bold 30, 21x22 sabit genişlik); Fatih'in fontu gelince SpecDisplay.c'de SN_FONT değişir, dosya silinir.
- Saat: Saat.c (RTC, LSE, HAL'siz yazmaç). Kristal ~2 s içinde hazır değilse yazılım saati. CMD_TIME 0x40
  Sa Dk Sn [Gün Ay Yıl-2000] ile ayarlanır (yanıt hep 6 bayt; tarih 2000-2099, RTC takvimi). Saat OTOMATİK
  GÖNDERİLMEZ (kullanıcı kararı): Android Ayarlar > "Saati ayarla" tek buton (telefonun saati+tarihi, pencere/geri
  okuma yok), PC aracında "Saati ayarla" düğmesi. İnternet gerekmez. Kartta henüz denenmedi.
- Ortam: Android sekmesi aileye değil Kind'a bakar (0 müzikli -> Müzik, 1 müziksiz -> Ortam);
  aile sadece gruplama/sıra (PC'de başlık). Sıra değişince kayıtlı ayarlar kendiliğinden varsayılana döner (Layout CRC).
- Silindi (beğenilmedi): Plazma, Kutup Işığı, Işık Girdabı (akışkan buhar denemesi, 128x32'de ekranı dolduramadı).
- Her görünümde Hız (0 = tempoya göre). Vuruşa tepki verenlerde Tepki (Sakin/Normal/Hareketli).
- Alev rengi: bloklu seçim (Ateş, Pembe, Mor, Mavi gaz, Buz, Yeşil).

## Kararlar
- Birbirinin aynısı mod yok; sadece rengi farklıysa ayrı mod sayılmaz.
- Favoriler: gizli genel ayar SET_ID_FAV (bit n = mod n); Android mod resminde kalp (yalnız müzikli). Oto geçiş
  SET_NextAuto: yalnız müzikli modlar, favori varsa yalnız favoriler (eskiden Ortam modlarına da geçiyordu).
- Kar: zemin siyah (gece odayı aydınlatmasın); taneler saf beyaz, uzak 1 sönük px, yakın 9 tek px (100-130) + 7 "+"/"×"
  (orta 110-140, uçlar ~%35; dönüyormuş gibi), ara sıra parlama; yıldızlar sıcak beyaz. Rüzgâr = beyaz perde: hayalet
  gibi soluk beyaz sis, ~90 px boy, 40-50 px kalın, iki katman (tepe 35 + 22), ~7.5 s'de geçer, 25-45 s'de bir;
  geçtiği yerdeki taneleri hafifçe iter (SnPerdeYogun).
- Alev rengi: Buz kaldırıldı (Mavi gaz ile aynıydı). Katmanlı Dağ/Örgü Şerit renk daireleri ailenin ilk-son rengiyle
  geçişli ("Ad:RRGGBB,RRGGBB", protokol 256 B sınırı: PAL tanımı 229 B). Android Saat satırında kabarık "Ayarla" düğmesi.
- Renkli Dalga "Kontrast" ayarı kaldırıldı, sabit %150 (SWV_DOLULUK). Açılış/fabrika modu 0 = Klasik.
- Klasik renk: Tek | Hi-Fi | Gökkuşağı | Akan (Renk Seti ve hazır setler kaldırıldı, kullanıcı kararı).
  Hi-Fi: alt = Sütun Renk tonu, orta -60°, üst -120° (yeşil -> klasik yeşil-sarı-kırmızı; beyaz = griden beyaza kademe).
- Android: bağlantı koparsa / açılışta son cihaza otomatik bağlanır ("Bırak" deyince yapmaz).
  Samsung'da pil "Optimize" arka planda ağı kesiyordu -> "Kısıtlanmamış" ile düzeldi.
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
- 14 Kasım (16, kartta denenmedi): SAAT KAYMA ÖĞRENME (Saat.c başında ayrıntı). Android her bağlanışta telefonun
  saatini tam saniye başında sessizce gönderir (önceki "otomatik gönderilmez" kararının yerine, Fatih). CMD_TIME 7.
  bayt Güvenilir (1 = telefonun otomatik ağ saati açık); yalnız güvenilir tam esitlemede (SAAT_Esitle) kayma
  ölçülür. Referans (son öğrenme anı) + biriken fark RTC yedek yazmaçlarında (BKP0-7, sihir + sağlama; Settings'in
  yedek SRAM'ine dokunmaz). Öğrenme: süre >= 6 sa ve birikim >= 3 sn; kayma <= 250 ppm; güven çarpanı fark/(fark+2);
  adım <= 60 ppm, toplam <= 300 ppm; RTC CALR (~0.95 ppm adım), yazılım saati için ayrı değer. Ayar değişikliği
  sayılan (öğrenilmez, referans yenilenir): tek esitlemede fark > 30 sn + süre x 250 ppm (en çok 30 dk), kaynak
  RTC<->yazılım değişti, açılışta saat yeniden başladı, elle saat/tarih (PC aracı 6 bayt = güvenilmez).
  PC'de sahte RTC ile denendi (+4.6/-23/+80/-150 ppm): 1-4 öğrenmede kalan ~1-2 ppm (günde ~0.1-0.2 sn).
  Yan düzeltme: yazılım saati HAL_GetTick 49 günde taşınca geri sıçrıyordu -> 64 bit düzeltmeli ms sayacı.
  BEKLİYOR: ESP32 NTP (ev ağı varsa) -- saat dilimi gerektirir, ayrıca konuşulacak.
- 14 Kasım (15, kartta denenmedi): Direk en üste kadar, kol 2 px kalın kuğu boynu (direkle tümleşik), cam 4 px
  (orta 2 tam, yanlar ~%40). Koni dik, iki yana eşit ~30 derece (tan30), direk ışığın önünde (aydınlanmaz).
  Karışık renk düzeltmesi: sudaki leke ve şerit su rengini ışık rengine KARIŞTIRIR (eklemez), konideki damlalar
  ışığın rengini alır, YM_ISIK en yakına yuvarlar. Direkler (ve ışıkları) yalnız saat kenarlardan YM_KENAR (7 px)
  içerideyken ve Isı-Nem yokken çizilir (YmLamba; sola yaslı / Isı-Nem / taşan geniş fontta yok). Ayar yok (Fatih).
- 14 Kasım (14, kartta denenmedi): Direk tek renk (58,36,18), yeşil kapak kalktı (kapak da direk renginde), yalnız
  iki boğaz açık (104,68,34). Lamba ışığı (cam, koni, sudaki leke ve şerit) saat renginin tam tonu (YmIr/Ig/Ib,
  her kare SPEC_SaatRgb'den; parlaklık ayarından bağımsız).
- 14 Kasım (13, kartta denenmedi): Direk 3 px (gölge/orta/açık), ayak 3-6 px kavisli, bilezik 4 px, direk ucunda
  topuz, kol ve lamba 1 px içeride (YM_LX 8, cam x7-10). Yağmur fabrika saat rengi krem FFD9A0 %42 (gri-mavi suda
  seçilmiyordu); kayıtlı ayar varsa Android'de ↺ ile gelir.
- 14 Kasım (12, kartta denenmedi): Lamba yenilendi (Fatih): lamba en üstte (yeşil bakır kapak y0-1, cam y2), kuğu
  boynu kol, kahverengi döküm direk (koyu 34,20,10 / ışığa bakan yüz 72,44,22), y15 ve alttan 7. satırda bilezik,
  dibi kavisle genişleyen ayak. Işık sıcak sarı (YM_ISIK 255/222/130, G yukarı yuvarlanır: sönükte kızıllaşmasın).
  Koni geniş ~50 derece, içe 15 derece eğik, dış kenarı direğin dibine iner; hava 34, sudaki leke 95, şerit 75/28.
- 14 Kasım (11, kartta denenmedi): Yağmur sokağa döndü (Fatih: su deniz gibiydi). Panelin en sol/sağında yarısı
  kesik lamba direkleri (YmDirek: direk x 0-1, kol y3, gövde y4 x5-9, cam y5 x6-8; sağ aynalı). Dar koni (YmKoni:
  ~26 derece, içe ~18 derece eğik, gün ışığı tonu YM_ISIK 255/245/230): havayı hafif aydınlatır (26), içinden geçen
  damla 4 kata kadar parlar, suda dalgayla kırılan ışık lekesi (70) + lambanın altında titreyen dikey yansıma
  şeridi (orta 60, yan 22). Su sığlaştı: Hafif 6, Normal 8, Sağanak 10 px. Direk saatin arkasında.
- 14 Kasım (10, kartta denenmedi): YAPILACAKLAR listesi yazıldı.
  Yağmur: su artık 128x16 tam sayı dalga denklemi (U.Ym.Dl[2][16][128] int16, Ym union'ı ~11.5 KB -> union
  ~3 KB büyüdü). Dalga 2 karede bir adım (YmDalga), yatay YM_KX 64, dikey uzakta ~%35 (basık halka), sönüm >>7,
  üst/alt 2 satır yutar (yansıyıp bulamaçlaşmasın). Damla suya değince iter (YmIt, güç = hız; uzakların 1/8'i iz
  bırakır, yakın 18, iri 30); halka kendiliğinden yayılır, girişim var. YmHalka ve kırılma (sx kaydırma) silindi;
  su katmanında sinf yok. Su tonu açıldı (R 14-24, G 34-52, B 62-86), eğimle ±24 tepe/çukur. Saatin ters sönük
  yansıması (eğim + satır satır kıpırtı), kısa yatay ışık çizgileri (uzakta 3 px sık, yakında 12 px seyrek, yavaş
  kayar, eğimle bükülür). Saat suyun ÖNÜNDE: dalga rakamların üstüne binmez, sudaki kısmı hafif mavimsi.
  Ayar noktaları (kartta bakılacak): Ib >> 6 (dalga kontrastı), YmIt güçleri, sönüm >>7, YM_KX.
  Şömine: ':' alev için engel değil (sönünce yeri kara kalmıyor). Alt 10 satırda ısı palete +%0..94 kayar
  (SM_SARI, taban sarı), üst aynı.
  Bütün modlar: saydam saat artık ekran karışımı (SaatSet): saat rengi zeminle karışır, zemin parlaksa saat de parlak.
  Android: Saat > Renk iki çubuk (SaatRenk): ton + açıklık (sol koyu %15, orta renk, sağ pastel beyaza %80);
  kart tek RGB, iki değer RGB'den geri hesaplanır; ↺ fabrika rengi. APK güncellemesi gerekir.
  Kar: zemindeki kar örtüsü kaldırıldı, kar eski haline döndü (Fatih onayladı, öyle kalsın).
  Not: PC'ye kopyalarken her seferinde dosya boyutunu kontrol et (aynı adla tekrar gönderilen dosya eski
  kopya olarak gitti).
- 14 Kasım (20, kartta denenmedi): Saydamlık alfa karışımı (SaatSet; ekran karışımında parlak saatte etkisi
  görünmüyordu). Yağmur hüzmesi kademeleri 10/17/24 (kenar soğuk beyaza kaymasın). Şiddet'e "Oto" seçeneği (max 3,
  YENİ AYAR YOK: Layout CRC id/tip/bayrak/maske + ModeInfo varsayılanlarından; seçenek sayısı girmez): şiddet
  (0..2 sürekli) ve rüzgâr eğimi (0..110) 20-30 sn'de bir yeni hedef, ~4 sn'de yumuşak geçiş; Oto'da Rüzgâr ayarı
  yok sayılır, su derinliği Yağmur'unki. Önizlemede diğer bütün modların çıktısı bayt bayt aynı kaldı.
  DİKKAT: ModeInfo varsayılanı (ör. fabrika saat rengi) değişirse Layout değişir, kayıtlı ayarlar fabrikaya döner.
- 14 Kasım (19, kartta denenmedi): Yağmur hava hüzmesi biraz parlak: kademeler panel çıkışında en parlak kanal
  6/10/14 basamak (Lv; önce 1/2/3 idi, sönükte renk kayıyordu). Sudaki ışık aynı (suyu boğmasın, Fatih).
  Kök sebep (ileride, tüm modlar): düşük genel parlaklıkta alt bitplane'ler 1 tikte sabit (HUB75_DMA.c OnTicks).
- 14 Kasım (18): Android ayarYaz debounce (60 ms bekle + iptal) yerine akış: yolda en çok bir gönderim, arada gelenlerden
  yalnız sonuncusu (bekleyen) cevap gelince gider. Kaydırırken panel hemen güncellenir (renk, parlaklık, frekans...).
- 14 Kasım (17, kartta denenmedi): Lamba hava hüzmesi kademeli ve çok sönük (YM_KADEME 4: sönük + 3 kademe).
  Kademe renkleri her karede saat renginden (YmHzKur): panel çıkışında (CIE tablosu + bitplane) en parlak kanal
  1/2/3 basamak, diğerleri aynı oranda; önemli bir kanal sıfıra düşerse o kademe sönük (yeşil/kırmızı piksel yok).
  Sudaki ışık ve yansıma DEĞİŞMEDİ (Fatih beğendi). Boğazlar (80,52,26).
  Alev ailesi Aurora (14 Patlama, 15 Orta Patlama ve diğer alevler): soluk aurora paleti yerine doygun gökkuşağı
  (GkTon 0..1535); ton panel boyunca ~3/4 tur ve ~25 sn'de bir tur kayar (GK_TUR_SN), parlaklık ısıdan, en sıcakta
  hafif beyaz. Havai fişek: her patlamanın parçaları kendi tonunda (PtTonSec: arkadaki alevden 120-210 derece öte,
  bir öncekinden farklı; Frags.Ton), parçalar kendi renginde çizilir (PtParcaCiz), parlama da o renkte; hareket,
  şekil, boyut aynı. Kıvılcımlar rastgele gökkuşağı tonu. Android'deki Aurora bloğu (28AA6E,8C5AD2) aynı kaldı.
- 14 Kasım (21, kartta denenmedi): Hız 3 kademe (Yavaş|Normal|Yüksek, varsayılan Normal): animasyon Kat {0.7,1.0,1.3},
  Kar KatKar {1.0,1.3,1.7}, Klasik çubuk KatBar {0.4,0.8,1.0} / KatAtk {0.6,1.0,1.0} (eski Normal/Yüksek/Çok yüksek);
  SPEC_SetMode varsayılanı 1. Kayıtlı 3 -> 2'ye sınırlanır. NOT: kayıtlı eski değerler (Def CRC'ye girmez) bir kademe
  hızlı kalabilir; Android'den yeniden seçilir. Yağmur hüzmesi Lv {0,8,13,19}. Kar kayan yıldız kuyruğu damla
  (KyKuyruk: baş tarafında 3 px, yarıya kadar yavaşça incelir). Yağmur'a nadir küçük kayan yıldız (YmKayan: 25-50 sn,
  ~0.5-0.9 sn, 12 px kuyruk, YM_KY_ALT 8. satırın altına inmez, uzak yağmurun arkasında). Önizlemede 0-16 aynı;
  17-20 farkı Kar hızı ve rastgele sayı sırasından (Kar hızı eşitlenince Şömine de bayt bayt aynı).
- 14 Kasım (24, kartta denenmedi): SAYDAMLIK TAMAMEN KALDIRILDI (ayar 0x030E, SPEC_SaatSaydam, SaatSet -> Set,
  SmEngel/Şömine saydam dalları, Android satırı ve AyarId). Ayar tablosu değişti -> kayıtlı ayarlar bir kez fabrikaya.
  Yağmur lambası sabit krem (255,217,160), saat renginden bağımsız; YmHzKur yalnız açılışta. Önizlemede 0-18 ve 20
  bayt bayt aynı, Yağmur'da lamba tonu en çok 1 basamak farklı (eski yuvarlama).
- 14 Kasım (23, kartta denenmedi): Yağmur Oto ayrı BOOL ayar (SET_ID_YM_OTO 0x0312, "Oto"); Şiddet yine 3 seçenek.
  Yeni ayar -> kayıtlı ayarlar bir kez fabrikaya döner (Fatih: sorun değil). Android: kısa seçim + yanındaki BOOL aynı
  satırda (Rüzgâr | Oto), APK güncellemesi gerekir.
- 14 Kasım (22, kartta denenmedi): Saydamlık: saatin ışığı düşmez (SaatSet: c + (max(c,zemin)-c)*s; koyu zeminde saat
  aynen, parlak zemin rakamın içinde görünür). Yağmur hüzmesi Lv {0,4,7,10}.
- **YAPILACAK (14 Kasım 16:12, Fatih "not al, yazma"; akşam Android görsel düzenlemeleriyle birlikte):**
  Android: Rüzgâr | Oto satırı eşit bölündüğü için Rüzgâr seçenekleri sıkıştı, Oto ince uzun oldu; ayrıca çift satırda
  (ust = true) ad üste çıkıp düğmeler altına iniyor -- İSTENMİYOR. Olması gereken: normal tek satır (ad solda, seçenekler
  yanında, eskisi gibi) + satırın en sağında KÜÇÜK "Oto" düğmesi: şimdiki seçenek düğmesi boyunun YARISINDAN biraz küçük.
  (Bilesenler.kt AyarListesi'ndeki "kısa seçim + BOOL" çift kuralı kalkar; yerine bu özel satır.)
  Android HATA: otomatik geçişte mod bazen bir önceki moda dönüp sonra tekrar ileri gidiyor (kalpli favorilerde).
  Sebep (Ekranlar.kt ~261): kart modu değiştirip bildirince uygulama sayfayı animateScrollToPage ile kaydırıyor;
  LaunchedEffect(settledPage) programatik kaydırmayı kullanıcı kaydırması sanıp modSec gönderiyor -- animasyon yarıda
  kesilirse ya da sayfa ara/eski sayfada durursa kartı YANLIŞ moda geri yolluyor. Çözüm: yalnız kullanıcının
  kaydırması (sürükleme) mod göndersin; kartın bildirdiği mod için sayfa kayarken gönderim yok.
  ANDROID KURALI (Fatih): arayüzde simetri ve düzen bozulmayacak -- satırlar hizalı, aynı türden düğmeler aynı boyda,
  ad sütunu ve kenar boşlukları her satırda aynı; değişiklikten sonra komşu satırlarla hizayı kontrol et. Düğme boyu içindeki
  yazıdan çok büyük olmayacak: düğme yazıyı az bir iç boşlukla sarar (ör. "Oto" düğmesi yazısı kadar, ince kenar payı). Akşam: Android arayüzünde başka görsel düzeltmeler gelecek.
- **YAPILACAK (14 Kasım 17:19, Fatih "not al"):** Şömine: alevin değdiği rakam kenarlarının köz gibi ısınıp renk
  değiştirmesi KALKSIN (saat rengine göre rakamı bozuyor). Rakamlar düz saat renginde; köz kodu (U.Sm.Koz, SmKozRenk,
  SmNefes'in köz kullanımı) tamamen silinsin, alev yine rakamlardan geçmez.
  Şömine: rakamlar alev için ENGEL OLMASIN (ör. 2'nin alt çizgisinin üstünde alev kesiliyor). Alev hiçbir şeye
  takılmadan yanar, saat alevin ÖNÜNDE düz renkte çizilir; rakamların iç boşluklarında (6'nın yuvarlağı) alev görünür.
  SmEngel ve rakam hücresi örnekleme kodu tamamen silinsin (köz kaldırmayla birlikte).
  Saat (tüm ortam modları), en büyük font + küçük saniye: küçük saniye rakamları kendi genişlikleriyle konuyor
     (SaatHazirla: Kuc = ... SaatRakamGen(K, r[4]) + SaatRakamGen(K, r[5])) -> toplam genişlik her saniye değişiyor,
     saat ortalanırken başlangıcı kayıyor, Yağmur'da direkler bir kaybolup bir çıkıyor (YmLamba kenar kontrolü).
     Çözüm: küçük saniye de sabit yuvalı (K fontunun en geniş rakamı), toplam genişlik sabit -> saat kaymaz.
     '1' çıkınca yanındaki rakamla arası fazla: '1' yuvasında komşusuna ~3 px yakın konsun (toplam genişlik değişmeden).
  Yağmur ayar adları (Settings.c, Android karttan okur; Layout CRC'ye girmez): "Şiddet" adı -> "Yağmur";
     seçenekler "Çisenti|Yağmur|Sağanak" -> "Hafif|Normal|Sağanak".
     Oto ayarının adı "Oto" -> "Otomatik" (Settings.c SET_ID_YM_OTO; arayüz Türkçe, İngilizce terim yok).
  Yağmur: kayan yıldız KALKSIN (beğenilmedi): YmKayan, U.Ym.Ky* alanları, YM_KY_IZ / YM_KY_ALT tamamen silinsin.
     KyKuyruk Kar'da kullanıldığı için kalır.
  Yağmur: saatin suyun içindeki kısmı mavimsi tonlanıyor (ModeRain sonunda SaatCizRgb sonrası döngü: R*225, G*242,
  B+%11) -> saydam gibi görünüyor. Bu tonlama KALKSIN, rakamlar suda da üstteki gibi düz saat renginde.
- **BEKLİYOR (Fatih "şimdilik dursun", 13 Kasım):** Kısık parlaklıkta Kar/saat saniyede birkaç kez parlayıp sönüyor.
  Sebep: SPEC_Draw sonundaki vuruş pompası (CFG_BEAT_BRIGHT_BOOST %25 * BeatEnv) Ortam modlarında da çalışıyor;
  müziksizken giriş gürültüsü vuruş sayılıyor, düşük parlaklıkta OE birkaç tik olduğu için küçük artış büyük
  sıçrama yapıyor (özellikle sönük renkler, gölge saat). Önerilen çözüm (yalnız SpecDisplay.c, tablo değişmez):
  1) Ortam (Kind 1) modlarında pompa kapalı. 2) Müzikli modlarda parlaklık %20 altındaysa pompa giderek azalsın,
  en düşükte kapansın. Yan not: düşük parlaklıkta alt bitplane'ler 1 tikte sabit kaldığı için sönük tonlar
  orantılı kısılmıyor (HUB75_DMA.c OnTicks), ileride bakılabilir.
- 14 Kasım: Fatih'in ilk saat fontu Arial Bold (arialbold24px.c/.h, HUB75 Aracı çıktısı, 24 px, oransal, 0-9 : C)
  eklendi; geçici font (Font_SaatGecici.h) silindi (PC'de dosya duruyorsa kullanılmıyor). Rakamlar en geniş rakam
  kadar yuvaya ortalanır (dakika değişince saat kaymasın). Saat Yazısı tek font olduğu için hâlâ gizli.
- 14 Kasım (9): Saat camındaki kenar parlatması kalktı. Sol (ve Isı-Nem) yerleşimde ilk yanan piksel x=2'de.
  ':' her fontta rakamların dikey ortasında. Renk ayarına -1 yazılırsa o modun fabrika rengi (LookDef) gelir;
  Android'de renk çubuğu kısaldı, yanında ↺ düğmesi. "Müzikte geç" adı "Müzik algıla"; Android'de Oto geçiş
  satırının sağında yalnız hoparlör simgesi (açık sarı basık, kapalı soluk). Kutu başlıkları (GÖRÜNÜM/RENK/
  HAREKET/SAAT) ve "SES" başlığı kalktı; Parlaklık kutusuz; etiket "Frekans".
  Yağmur: altta perspektifli saydam göl (Hafif 10, Normal 12, Sağanak 15 px); üst sınır iki dokudan + damla
  kabarmasından, asla düz değil. Uzak damla suyun üst (uzak) kısmına, yakın damla alta düşer; düştüğü yerde
  damla hızına göre hızı/boyu değişen elips halka (YmHalka, 28 adet), yakın damlada sıçrama. Saat suyun
  altında kırılarak, renklenerek görünür.
  Yağmur (düzeltme): su kenarı sabit (kıpırdamaz, düz de değil), vuruşla kabarma kalktı; beyaz yok: halka ve
  damlacıklar açık su tonunda, parıltı kalktı; su içinde katman katman dalga (uzakta sık, yakında geniş, iki
  takım, tepe açık / çukur koyu su tonu).
  Kar örtüsü denendi, KALDIRILDI (Fatih: kar yumağı/tümsek görünmüyor, yalnız farklı tonda pikseller; zeminde
  kar olmasın). Kar eski haline döndü, kodu tamamen silindi.
- 14 Kasım (8): "Müzikte geç" (0x0007, genel BOOL, Ayarlar > Genel): ortam modunda 4 sn müzik (SPEC_MusicLevel =
  sessizlik kapısı > 0.5) -> en son spektrum modu; 1 dk sessizlik -> önceki ortam modu; arada elle mod değişirse
  dönüş iptal. Oto geçiş artık yalnız müzikli modlarda çalışır (önceden ortam modundan da çıkarıyordu).
- 14 Kasım (7): Alev Renk'e "Aurora" bloğu (FirePal 8): soluk gökkuşağı turkuaz-yeşil-camgöbeği-mor-pembe-şeftali;
  alev ailesinin hepsinde seçilebilir. Aurora seçiliyken kıvılcımlar (FireEmbers) her biri kendi tonunda
  (Particle.Hue), ömürle söner (SPEC_FireAurora).
- 14 Kasım (6): Model adı SPA12832 (SPA + panel genişlik + yükseklik, tiresiz). STM32 fabrika cihaz adı bu.
  ESP32 açılışta STM32'ye CMD_INFO sorar (2 sn, tekrarlı), Bluetooth/AP/mDNS adı SPA12832-1A2B (tire yalnız
  MAC'ten önce); cevap yoksa Spektrum-1A2B. Kullanıcı adı sadece uygulamada, radyo adı değişmez. Android kendi
  ağını ^(Spektrum|SPA\d+)-XXXX$ ile tanır (bu yüzden bir kez APK güncellemesi gerekti).
  İLERİDE (bu cihaz tutarsa, yeni işlemci): kart kodu boyuttan bağımsız (sabitler W/H'den, bayt bayt
  karşılaştırmayla), APK'yı karttan bağımsız yapma (font örneği karttan bitmap, düzen ipuçları ayar tanımında,
  resmi olmayan modda ad/ikon). Önizleme resimleri temsili, çözünürlüğe göre ayrı set GEREKMEZ (Fatih).
- 14 Kasım (5): Saniye görünürken iki nokta(lar) sabit (SnVar); Şömine ':' saat renginde, aynı kural. Font sırası
  küçükten büyüğe: Arial Bold | Bahn Cond | Bahn Bold. Sahil: kıpırtı ±%22 ve sık bant, denizde hep >= 3 küçük
  sırt, kumdaki suda ucun şeklini izleyen kıvrımlar (gelirken kıyıya, çekilirken denize akar) + köpük izleri.
  Android: sıkı düzen (düğme 30dp, satır 38dp, kutu başlığı çerçeve üstünde), 3 düğme / 2 seçim / 2 çubuk yan
  yana; Saat kutusu sabit düzen: [Font örnekleri][Saniye] / [Sol][Orta][Alt] / [Isı-Nem] / Renk / Parlaklık +
  Saydamlık; Saniye açıkken font örnekleri "12:34:56" (assets/saatyazi/<ad>_sn.png); frekans çubuğu özel
  tutamaç; üst çubukta yazısız bağlantı simgesi (Bluetooth / ev ağı / doğrudan Wi-Fi).
- 14 Kasım (4): Saniye (0x0311, BOOL, Saat): sığarsa büyük fontla 21:47:35 (iki ':' birlikte yanıp söner),
  sığmazsa Arial 13 ile dakikanın sağına alt hizada; hiç yer yoksa (Bahn Bold + Isı-Nem) gösterilmez.
  Vuruş hassasiyeti: tek gizli SENS (0x0005) KALKTI -> mod başına Bas vuruşu (0x0122, kick) ve Tiz vuruşu
  (0x0123, genel vuruş/HitRatio), REACT_MASK modlarında (Klasik ve ortam modlarında yok), grup Hareket.
  Android mod ayarları kutuları: GÖRÜNÜM (mor) / RENK (erik, renk türleri) / HAREKET (mavi-mor) / SAAT (lacivert)
  / PANEL (gri, parlaklık). Kural: Saat grubu > Hareket grubu > renk türü > geri kalan.
- 14 Kasım (3): Fontlar: saat = Arial Bold 24, Bahn Bold 30, Bahn Cond 30 (ayar adı "Font", Android'de açık);
  ısı-nem = Arial 13 (iki satır), sadece sıcaklık = Bahn 16 (tek satır); glif sırası StartChar'a göre ('%'li
  fontlar). Derece = elle 3x3 halka; yer yoksa 'C' düşer. Yerleşim: "Sol|Orta" + Alt (BOOL) + Isı-Nem (BOOL);
  Isı-Nem sensör verisi yoksa uygulanmaz. Saydamlık = renkli cam (arka plan saat tonundan geçer) + sol/üst
  kenarda ince ışık; Şömine'de saydamken alev rakamdan geçer (SmEngel). Mercek: 3 px çizgide anlamsız, YOK.
  Sahil: deniz hep hafif kıpırdar (ShDoku bantları, sütun kayması), sırt beyazlığı geri alındı, kavis kaldı.
  Frekans: LO 60–5000, HI 240–20000 (fabrika 60–16000), Android'de çift tutamaçlı log çubuk (Ayarlar > SES).
  Android: ayarlar ANİMASYON / SAAT (lacivert) / PANEL (gri, tüm modlar) kutularında; BOOL = basılı/kabarık
  düğme; ev ağı satırı hep görünür, köprü cevap vermezse sebebini yazar, dokununca 3 sn bekleyip yeniden sorar;
  uygulama telefonun gezinme çubuğunun üstünde (systemBarsPadding + 16dp).
  BEKLİYOR: sensör okuma (AHT20/SHT3x PB8/PB9 bit-bang I2C; DS18B20 için PA2 USART2 tek tel mi? Fatih karar
  verecek). İLERİDE: otomatik frekans aralığı (hızlı genişle, yavaş daral), font yükleme (flash 16 KB bölüm,
  HUB75 Aracı / telefon fontu).
- 14 Kasım (2): Kar: salınım TAMAMEN kalktı (Fatih: zikzak istemiyor, "oyundaki gibi") -> serbest kayma: Vx hedefe
  yavaşça döner, 1.6-4.8 s'de bir yeni hedef (bazen düz); 3x3 taneler dibinde doğmaz (SnDolu), 4 px'e yaklaşınca
  ayrılır (SnAyir); orta {15,25,35}. Şömine: eski kıvılcım (yükselip söner), 6 karede bir, SM_KIV 16 (yaylı
  kıvılcımı beğenmedi). Saat: yerleşim Orta|Sol|Alt|Isı-Nem (Isı-Nem: saat x=1, ısı-nem sağa yaslı); iki nokta
  her fontun kendi ':' glifi (elle çizim kalktı; Fatih'in fontlarında ':' hep olacak); 'C' derece için, şimdilik
  kullanılmıyor. Saydamlık (0x030E, 0-80 %, mod başına). Sahil: denizdeki sırtlar 16 px kontrol noktalı kavis,
  tepeleri beyazlaşır, küçükler 1.5x belirgin. Android: kısa ayarlar (anahtar, <=3 kısa seçenek) ikişer yan yana.
- 13 Kasım gece (6. tur): Sahil köpüğü çekilmeye geçince tek karede yarıya iniyordu (Fatih: beyazlar bir anda
  kayboluyor) -> çekilme boyunca kademeli söner, delikler sabit desenle yavaşça büyür (tek kare kaybı 106 -> 15 px).
- 13 Kasım gece (5. tur, kartta denenmedi): Kar: yakın taneler zikzak gibiydi (üçgen salınım, sert dönüş) ->
  sinüs tablosu (SnSin 64, ara değerli), genlik 0.4-2 px, 2.5-4.5 s, hafif sürüklenme; orta ~1 px aynı. Orta tane
  {18,32,44}. Kayan yıldız: baş 3x3 yumuşak (ara piksel), kuyruk ~28 px; "Kayan Yıldız" ayarı KALDIRILDI (0x0303 boş),
  hep 6-12 s'de bir. Şömine paleti: A02800 / FF8A18 / FFD060 (kırmızı çoktu, Fatih: gerçek alev gibi).
- 13 Kasım gece (4. tur, kartta denenmedi): Yağmur su birikintisi KALDIRILDI (Fatih: düz renk + beyaz şerit, suya
  benzemedi; tekrar önerme). Sahil: kum üstündeki su Dz2'den başlayıp uca doğru sığ renge açılır (büyük dalgada
  bariz renk sınırı vardı); Gün batımı kaldırıldı, ayar "Işık: Gece|Gündüz" (0x030B SH_ISIK). Şömine: Kıvılcım
  ayarı kaldırıldı (0x0307 boş); 32 kıvılcım, yay çizip düşer, alttaki 7 satırda söner, panelin altına inmez.
  Yerleşim: "Isı altta" kaldırıldı -> Orta|Alt|Isı-Nem (OSD 21 karakter sınırı); nem "%45" yazılır. Saat Yazısı tek font varken gizli
  (H bayrağı; fontlar gelince kaldır). SANİYE: Fatih fontları yapınca eklenecek (büyük fontla, sığmazsa küçük).
- 13 Kasım gece (3. tur, kartta denenmedi): Saat Parlaklığı (0x030D, mod başına %) — renk tam tonda seçilir,
  ApplyOrtam renk x % hesaplar (gece birden parlamasın; genel parlaklığı kısınca damlalar kayboluyordu).
  Kar: 5x5 kristal kaldırıldı (Atari gibi), orta katman {14,24,34}. Yağmur: saydam birikinti 4 px (Sağanak 5),
  yüzey kıpırdayan ışık çizgisi, damla/damlacık yansıması, sıçrama yüzeyden. Sahil: sırtlar parça parça, eğik,
  kavisi ilerledikçe değişir; kıyıya vuran suyun ucu geniş diller (16 px kontrol noktası); denizde köpük
  tepecikleri; büyük dalga alta kadar (Sakin hiç, Normal ~1/10, Dalgalı ~1/5). Fatih'in kızı Sahil'i beğendi.
- 13 Kasım gece (2. tur, kartta denenmedi): Kar "kare düşüyor" -> ara piksel çizim (SnKonum/SnNokta: kesirli
  konum 2x2 komşuya karekök ağırlıkla, CIE yüzünden ışığın karesi korunur). Yağmur: iri damla 3 px yuvarlak baş +
  1 px sönen kuyruk, yere değince damlacıklar yay çizip sıçrar + yerde açılan halka. Sahil baştan: deniz katmanları,
  denizde inen dalga sırtları (büyük sırt kıyıya varınca kırılıp kumu yalar), aynı anda 3 dalga, 2.2-3.4 s, kumun
  ~3/4'üne kadar. Saat Rengi: mod başına COLOR ayarı (0x030C), varsayılan ORTAM override ile her modda farklı.
  Kural (Fatih): görsel değişiklikte ardışık kareleri (hareketi) kontrol et, tek kareye bakma.
- 13 Kasım akşam (Fatih eleştirisi sonrası, kartta denenmedi): Kar: dönme ve 7x7 kalktı ("örümcek" gibiydi);
  yakınlar 3x3 yumuşak + 2 adet 5x5 kısa kollu kristal; SN_TEPE 120; derinlik: uzak 2-3 px/s tek px havada söner
  (SnTn.Son), orta 5-7, yakın 14-20 px/s geniş salınım. Şömine: odunlar silindi; yakıt kenarda tam, ortada 1/3;
  Boy {0.38,0.48,0.60} (Yüksek = eski Alçak); rakam içi sabit amber (saat önde). YENİ: Yağmur (19) ve Sahil (20).
  Yağmur: 8.8 sabit nokta çizgi damlalar, uzak/yakın, alt satırda sıçrama; Şiddet, Rüzgâr (şimşek YOK, Fatih).
  Sahil: kuşbakışı kıyı, gelip giden dalga, düzensiz köpük, ıslak kum kurur; Dalga, Renk (Gece/Gün batımı/Gündüz).
  SET_MODE_MAX 24 -> 36 (27 mod ayarı vardı; aşınca SET_Init sonsuz döngüde kalır! 4 KB'a ~27 moda kadar sığar).
- YAPILDI (13 Kasım, kartta denenmedi): Kar yenilendi + Şömine eklendi (mod 18).
  Kar: perde/rüzgâr silindi; taneler soluk (Kar Işığı %, tepe 70*%); uzak 1 px, orta 3x3, yakın 5x5/7x7 kristal
  (2 fazla döner); düzenli adım (SnTn.Kd/Ac 1/16 kare, salınım adımlı); kayan yıldız (yay, Kapalı/Seyrek/Sık).
  Ortak saat: SaatDurum maske + SaatHazirla (dakika/font/yerleşim değişince), font tablosu SaatYazilari[]
  (Settings SAAT_YAZI seçenekleri aynı sıra; yeni font: .h + tabloya ekle + seçenek + Max), yerleşim
  Orta|Alt|Isı altta|Isı yanda. Sıcaklık/nem: CMD_SENSOR 0x41 [Sic10 LE, Nem] (Saat.c, 10 dk geçerli), yoksa "--".
  Android: Saat Yazısı seçenekleri "12:34" resmi (assets/saatyazi, Tools/Onizleme/saat_yazi_png.py, uret.sh içinde).
  Kar ayarları: Kar Işığı, Yoğunluk, Kayan Yıldız, Saat Yazısı, Yerleşim (+ Hız).
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

## Şömine saat (YAZILDI 13 Kasım, kartta denenmedi)
- Sahne: alev panelin altından yanar, saat rakamları alevin içinde durur (şömine).
- Rakamlar katı (odun gibi): alev içinden geçmez; rakam aralarından ve iç boşluklarından (0,4,6,8,9) sızar.
  Rakam olmayan kenarlarda alev daha yükselir (ortada alçak, kenarda yüksek doğal görünüm).
- Köz: alevin değdiği rakam pikseli ısı biriktirir -> koyu kızıl -> parlak turuncu köz, hafif nefes/çıtırtı.
  Alevin ulaşmadığı yerler soğuk/ılık (koyu, kül grisine yakın); ısı düşünce yavaşça bu tona döner.
- Müzikle ilgisi YOK (Ortam modu). Alev kendi doğal düzensizliğiyle yanar: arada hafif harlanıp sakinleşir.
- Gerçek alev (Fatih, 13 Kasım): taban koyu kızıl/turuncu, orta sarı, uçlar seyrek kopuk diller; diller düzensiz boyda,
  hafif salınır, ara sıra dil kopup söner. Altta 2-3 koyu odun + nefes alan közler; ara sıra yükselen kıvılcım.
  Mevcut alev motoru (tamsayı + tablo) kullanılır, piksel başına sinf yok.
- Ayarlar (Kar'la aynı düzen): Alev boyu, Kıvılcım sıklığı, Saat yazısı, Yerleşim (hazır düzenler).
- Kod: ModeHearth, U.Sm (Heat, Koz, Yk, Kv). Rakam hücreleri alev örneğinden çıkarılır (alev kenarı yalar);
  köz = komşu ısıyı (h-k+15)>>4 ile biriktirir, 4 karede 1 soğur, kazanç 1.4-2x + nefes. Palet: SPEC_SetFirePalette(6,
  7A0C00, FF5000, FFB030) beyazsız. Odunlar en alt 3 satır (iki kütük + nefes alan köz yatağı). Union U +1.7 KB.
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
