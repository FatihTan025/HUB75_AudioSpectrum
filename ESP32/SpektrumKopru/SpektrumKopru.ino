/* ============================================================================
   SpektrumKopru -- ESP32-WROOM-32D  Wi-Fi + Bluetooth (BLE) <-> UART kopru
   Spektrum Analizor (STM32F446, USART1 PA9/PA10) icin.

   Baglanti yollari (hepsi ayni anda acik):
   - Kendi Wi-Fi agi (modem gerekmez):
       Ag adi : Spektrum-XXXX   (XXXX = MAC son 4 hane)
       Sifre  : AP_SIFRE        Adres: 192.168.4.1:5000
   - Ev agi (bilgiler kayitliysa): ag icinde spektrum-xxxx.local:5000
     mDNS hizmeti _spektrum._tcp, ornek adi Spektrum-XXXX (Bluetooth adiyla ayni; uygulama boyle eslestirir).
     Bilgiler flash'ta (NVS) saklanir, koparsa kendiliginden yeniden baglanir.
   - Bluetooth (BLE, Nordic UART servisi):  ad Spektrum-XXXX
       6E400002 = telefon -> kart (yaz), 6E400003 = kart -> telefon (bildirim)

   Kopru saydamdir: cerceveler STM32'ye aynen aktarilir, STM32'den gelenler
   bagli tum istemcilere (TCP + BLE) gider. Her kaynak kendi cercevesini
   tamamlayinca UART'a yazilir (TCP ve BLE ayni anda gonderse de karismaz).
   Tek istisna CMD_BRIDGE (0x7F) cerceveleri: STM32'ye gitmez, burada islenir,
   cevap sadece soran istemciye gider.
     0x7F | 0 Bilgi                    -> st Surum(2) Mac(6) Uz Ad
     0x7F | 1 Tara                     -> st Adet { Rssi(1) Sifreli(1) Uz(1) Ad }
     0x7F | 2 Baglan Uz Ad Uz Sifre    -> st          (Ad bos = unut)
     0x7F | 3 Durum                    -> st Durum(1) Rssi(1) IP(4) Uz Ad
                                          Durum: 0 kayit yok, 1 baglaniyor, 2 bagli
     0x7F | 4 GuncBasla Boyut(4) [MD5(16)]  -> st
     0x7F | 5 GuncVeri  Konum(4) Veri(..)   -> st Beklenen(4)
              Konum beklenenden farkliysa st = 3, yazilmaz (istemci Beklenen'den surer)
     0x7F | 6 GuncBitir                -> st, sonra yeniden baslar (yeni yazilim)
     0x7F | 7 GuncIptal                -> st
     st: 0 tamam, 2 eksik/bozuk parametre, 3 aralik/sira hatasi, 5 mesgul,
         6 desteklenmiyor, 7 guncelleme hatasi
   Gelistirme: Arduino IDE ag portundan da yuklenebilir (ArduinoOTA, sifre AP_SIFRE).
   Seri monitor (115200): "wifi Ad Sifre", "wifi sil", "wifi durum"

   Baglanti:  ESP32 GPIO27 (TX2) -> STM32 PA10 (USART1 RX)
              ESP32 GPIO25 (RX2) <- STM32 PA9  (USART1 TX)
              GND ortak. ESP32 3.3 V, en az 500 mA.
   Arduino IDE: Kart "ESP32 Dev Module",
                Partition Scheme "Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)"
                (Wi-Fi + BLE birlikte varsayilan 1.2 MB bolume sigmaz).
                Ek kutuphane gerekmez.
   ============================================================================ */
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <freertos/stream_buffer.h>

#define KOPRU_SURUM   0x0200          /* 2.0                                */
#define AP_SIFRE      "spektrum123"   /* En az 8 karakter (WPA2)           */
#define AP_KANAL      6
#define TCP_PORT      5000
#define UART_BAUD     115200
#define UART_RX_PIN   25              /* Bu modulde 16/17 calismiyor (GLOSA'da goruldu) */
#define UART_TX_PIN   27
#define LED_PIN       2               /* DevKit mavi LED: istemci bagli = yanar */
#define TAMPON        512
#define CER_ZAMAN_MS  200             /* Yarim kalan cerceve bu sureden sonra atilir */

#define SOF           0xA5
#define CMD_BRIDGE    0x7F
#define MAX_YUK       256
#define CER_MAX       (MAX_YUK + 7)

#define NUS_SERVIS    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_YAZ       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_OKU       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

enum { K_TCP = 0, K_BLE, K_ADET };

/* Her kaynagin kendi cerceve ayristiricisi */
typedef struct
{
  uint8_t  Cer[CER_MAX];
  uint16_t N;
  uint32_t Son;                               /* Son bayt zamani (ms) */
} Kaynak_t;

static WiFiServer  Sunucu(TCP_PORT);
static WiFiClient  Istemci;
static Preferences Ayar;
static uint8_t     Tampon[TAMPON];
static Kaynak_t    Kay[K_ADET];
static String      EvAd, EvSifre;
static bool        MdnsAcik = false;
static String      SeriSatir;
static char        CihazAd[20];
static char        MdnsAd[20];                /* kucuk harf: spektrum-xxxx      */

/* BLE */
static BLEServer         *BleSunucu;
static BLECharacteristic *BleOku;
static StreamBufferHandle_t BleGelen;         /* BLE gorevinden loop'a       */
static volatile bool     BleBagli = false;
static volatile uint16_t BleKonId = 0;
static volatile bool     BleYenidenYayin = false;

/* Guncelleme */
static bool     GuncAcik = false;
static uint32_t GuncBoyut = 0, GuncKonum = 0;
static bool     YenidenBasla = false;
static uint32_t YenidenZaman = 0;

/* ---------------------------------------------------------------------------
   Protokol yardimcilari (STM32 ile ayni CRC16-CCITT)
   --------------------------------------------------------------------------- */
static uint16_t Crc16(const uint8_t *d, uint16_t n)
{
  uint16_t c = 0xFFFF;
  while(n--)
  {
    c ^= (uint16_t)(*d++) << 8;
    for(uint8_t i = 0; i < 8; i++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

static uint32_t Al32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint8_t *Koy32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; return p + 4; }

/* BLE'ye parca parca bildirim (MTU - 3) */
static void BleYaz(const uint8_t *d, uint16_t n)
{
  if(!BleBagli) return;
  uint16_t Mtu = BleSunucu->getPeerMTU(BleKonId);
  uint16_t Parca = (Mtu > 23) ? (uint16_t)(Mtu - 3) : 20;
  while(n)
  {
    uint16_t k = (n > Parca) ? Parca : n;
    BleOku->setValue((uint8_t *)d, k);
    BleOku->notify();
    d += k; n -= k;
  }
}

static void Gonder(uint8_t Hedef, const uint8_t *d, uint16_t n)
{
  if(Hedef == K_TCP) { if(Istemci && Istemci.connected()) Istemci.write(d, n); }
  else BleYaz(d, n);
}

static void Yanit(uint8_t Hedef, uint8_t Seq, const uint8_t *Yuk, uint16_t n)
{
  uint8_t f[CER_MAX];
  f[0] = SOF; f[1] = n & 0xFF; f[2] = n >> 8; f[3] = Seq; f[4] = CMD_BRIDGE | 0x80;
  memcpy(&f[5], Yuk, n);
  uint16_t c = Crc16(&f[1], n + 4);
  f[5 + n] = c & 0xFF; f[6 + n] = c >> 8;
  Gonder(Hedef, f, n + 7);
}

/* ---------------------------------------------------------------------------
   Ev agi
   --------------------------------------------------------------------------- */
static void EvBaglan()
{
  WiFi.disconnect(false, false);
  MdnsAcik = false;
  if(EvAd.length()) WiFi.begin(EvAd.c_str(), EvSifre.c_str());
}

static void EvKaydet(const String &Ad, const String &Sifre)
{
  EvAd = Ad; EvSifre = Sifre;
  Ayar.putString("ad", EvAd);
  Ayar.putString("sifre", EvSifre);
  EvBaglan();
}

static uint8_t EvDurum()
{
  if(!EvAd.length()) return 0;
  return (WiFi.status() == WL_CONNECTED) ? 2 : 1;
}

/* ---------------------------------------------------------------------------
   CMD_BRIDGE: kopru kendi isler
   --------------------------------------------------------------------------- */
static void Kopru(uint8_t Hedef, uint8_t Seq, const uint8_t *p, uint16_t n)
{
  uint8_t o[MAX_YUK];
  uint16_t k = 0;

  if(n < 1) { o[0] = 2; Yanit(Hedef, Seq, o, 1); return; }
  o[k++] = 0;
  switch(p[0])
  {
  case 0:                                         /* Bilgi */
  {
    uint8_t Mac[6];
    WiFi.macAddress(Mac);
    uint8_t u = strlen(CihazAd);
    o[k++] = KOPRU_SURUM & 0xFF; o[k++] = KOPRU_SURUM >> 8;
    memcpy(&o[k], Mac, 6); k += 6;
    o[k++] = u;
    memcpy(&o[k], CihazAd, u); k += u;
    break;
  }
  case 1:                                         /* Tara (2-3 s surer) */
  {
    if(GuncAcik) { o[0] = 5; break; }
    int16_t Say = WiFi.scanNetworks(false, false);
    uint8_t *Adet = &o[k++];
    *Adet = 0;
    for(int16_t i = 0; i < Say; i++)
    {
      String s = WiFi.SSID(i);
      uint8_t u = (s.length() > 32) ? 32 : s.length();
      if(!u || k + 3 + u > MAX_YUK) continue;
      o[k++] = (uint8_t)(int8_t)WiFi.RSSI(i);
      o[k++] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? 1 : 0;
      o[k++] = u;
      memcpy(&o[k], s.c_str(), u); k += u;
      (*Adet)++;
    }
    WiFi.scanDelete();
    break;
  }
  case 2:                                         /* Baglan / unut */
  {
    uint16_t i = 1;
    if(i >= n) { o[0] = 2; break; }
    uint8_t ua = p[i++];
    String Ad;
    for(uint8_t j = 0; j < ua && i < n; j++) Ad += (char)p[i++];
    String Sifre;
    if(i < n) { uint8_t us = p[i++]; for(uint8_t j = 0; j < us && i < n; j++) Sifre += (char)p[i++]; }
    EvKaydet(Ad, Sifre);
    Serial.printf("Ev agi: '%s'\n", Ad.c_str());
    break;
  }
  case 3:                                         /* Durum */
  {
    IPAddress ip = WiFi.localIP();
    uint8_t d = EvDurum();
    uint8_t u = (EvAd.length() > 32) ? 32 : EvAd.length();
    o[k++] = d;
    o[k++] = (uint8_t)(int8_t)((d == 2) ? WiFi.RSSI() : 0);
    for(uint8_t j = 0; j < 4; j++) o[k++] = (d == 2) ? ip[j] : 0;
    o[k++] = u;
    memcpy(&o[k], EvAd.c_str(), u); k += u;
    break;
  }
  case 4:                                         /* Guncelleme basla */
  {
    if(n < 5) { o[0] = 2; break; }
    if(GuncAcik) Update.abort();
    GuncBoyut = Al32(&p[1]);
    GuncKonum = 0;
    GuncAcik  = Update.begin(GuncBoyut, U_FLASH);
    if(GuncAcik && n >= 21)
    {
      char Hex[33];
      for(uint8_t j = 0; j < 16; j++) sprintf(&Hex[j * 2], "%02x", p[5 + j]);
      Update.setMD5(Hex);
    }
    if(!GuncAcik) o[0] = 7;
    Serial.printf("Guncelleme: %u bayt %s\n", (unsigned)GuncBoyut, GuncAcik ? "basladi" : "acilamadi");
    break;
  }
  case 5:                                         /* Guncelleme veri */
  {
    if(!GuncAcik) { o[0] = 3; Koy32(&o[k], 0); k += 4; break; }
    if(n < 5)     { o[0] = 2; break; }
    uint32_t Konum = Al32(&p[1]);
    uint16_t m = n - 5;
    if(Konum != GuncKonum || GuncKonum + m > GuncBoyut) o[0] = 3;
    else if(Update.write((uint8_t *)&p[5], m) != m) { o[0] = 7; Update.abort(); GuncAcik = false; }
    else GuncKonum += m;
    Koy32(&o[k], GuncKonum); k += 4;
    break;
  }
  case 6:                                         /* Guncelleme bitir */
    if(!GuncAcik || GuncKonum != GuncBoyut) { o[0] = 3; break; }
    GuncAcik = false;
    if(!Update.end(true)) { o[0] = 7; Serial.printf("Guncelleme hatasi: %s\n", Update.errorString()); break; }
    Serial.println("Guncelleme tamam, yeniden baslatiliyor");
    YenidenBasla = true;
    YenidenZaman = millis();
    break;
  case 7:                                         /* Guncelleme iptal */
    if(GuncAcik) { Update.abort(); GuncAcik = false; }
    break;
  default:
    o[0] = 6;                                     /* desteklenmiyor */
    break;
  }
  Yanit(Hedef, Seq, o, k);
}

/* Bir kaynaktan gelen bayt: cerceve tamamlaninca UART'a ya da kopruye */
static void KaynakBayt(uint8_t Kim, uint8_t b)
{
  Kaynak_t *K = &Kay[Kim];

  if(K->N && (millis() - K->Son) > CER_ZAMAN_MS) K->N = 0;   /* Yarim kalan cerceve */
  K->Son = millis();
  if(K->N == 0 && b != SOF) return;                          /* Cerceve disi bayt   */
  K->Cer[K->N++] = b;
  if(K->N < 5) return;

  uint16_t n = K->Cer[1] | (K->Cer[2] << 8);
  if(n > MAX_YUK) { K->N = 0; return; }
  if(K->N < n + 7) return;

  if(K->Cer[4] == CMD_BRIDGE)
  {
    uint16_t c = K->Cer[5 + n] | (K->Cer[6 + n] << 8);
    if(Crc16(&K->Cer[1], n + 4) == c) Kopru(Kim, K->Cer[3], &K->Cer[5], n);
  }
  else Serial2.write(K->Cer, K->N);       /* CRC'yi STM32 denetler */
  K->N = 0;
}

/* ---------------------------------------------------------------------------
   BLE (Nordic UART servisi)
   --------------------------------------------------------------------------- */
class SunucuGeri : public BLEServerCallbacks
{
  void onConnect(BLEServer *s, esp_ble_gatts_cb_param_t *p) override
  {
    BleKonId = p->connect.conn_id;
    BleBagli = true;
  }
  void onDisconnect(BLEServer *s) override
  {
    BleBagli = false;
    BleYenidenYayin = true;                       /* loop'ta yeniden yayina basla */
  }
};

class YazGeri : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *c) override
  {
    size_t n = c->getLength();
    if(n) xStreamBufferSend(BleGelen, c->getData(), n, 0);
  }
};

static void BleKur()
{
  BleGelen = xStreamBufferCreate(1024, 1);
  BLEDevice::init(CihazAd);
  BLEDevice::setMTU(247);
  BleSunucu = BLEDevice::createServer();
  BleSunucu->setCallbacks(new SunucuGeri());

  BLEService *Srv = BleSunucu->createService(NUS_SERVIS);
  BleOku = Srv->createCharacteristic(NUS_OKU, BLECharacteristic::PROPERTY_NOTIFY);
  BleOku->addDescriptor(new BLE2902());
  BLECharacteristic *Yaz = Srv->createCharacteristic(NUS_YAZ,
                             BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  Yaz->setCallbacks(new YazGeri());
  Srv->start();

  /* Yayin: servis UUID (128 bit) + ad tek pakete sigmaz -> ad tarama yanitinda */
  BLEAdvertising *Yayin = BLEDevice::getAdvertising();
  BLEAdvertisementData Ana, Yanit_;
  Ana.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  Ana.setCompleteServices(BLEUUID(NUS_SERVIS));
  Yanit_.setName(CihazAd);
  Yayin->setAdvertisementData(Ana);
  Yayin->setScanResponseData(Yanit_);
  BLEDevice::startAdvertising();
}

/* ---------------------------------------------------------------------------
   Gelistirme: seri monitor komutlari
   --------------------------------------------------------------------------- */
static void SeriKomut(String s)
{
  s.trim();
  if(s == "wifi sil")        { EvKaydet("", ""); Serial.println("Ev agi silindi"); }
  else if(s == "wifi durum")
  {
    const char *D[] = { "kayit yok", "baglaniyor", "bagli" };
    Serial.printf("Ev agi: '%s' %s  IP: %s  BLE: %s\n", EvAd.c_str(), D[EvDurum()],
                  WiFi.localIP().toString().c_str(), BleBagli ? "bagli" : "yok");
  }
  else if(s.startsWith("wifi "))
  {
    String r = s.substring(5);
    int k = r.lastIndexOf(' ');                   /* Son kelime sifre, oncesi ag adi */
    if(k > 0) { EvKaydet(r.substring(0, k), r.substring(k + 1)); Serial.println("Kaydedildi, baglaniliyor..."); }
    else Serial.println("Kullanim: wifi AgAdi Sifre");
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial2.setRxBufferSize(2048);                 /* Durum akisi + GET_ALL icin */
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  Ayar.begin("kopru", false);
  EvAd    = Ayar.getString("ad", "");
  EvSifre = Ayar.getString("sifre", "");

  const uint64_t Mac = ESP.getEfuseMac();        /* Cipten okunur: AP baslamadan da gecerli */
  snprintf(CihazAd, sizeof(CihazAd), "Spektrum-%02X%02X",
           (unsigned)((Mac >> 32) & 0xFF), (unsigned)((Mac >> 40) & 0xFF));
  for(uint8_t i = 0; i < sizeof(MdnsAd); i++) MdnsAd[i] = (char)tolower((unsigned char)CihazAd[i]);

  WiFi.persistent(false);                        /* Eski (GLOSA) kayitlari kullanilmasin */
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(CihazAd, AP_SIFRE, AP_KANAL, 0, 2);    /* en fazla 2 cihaz */
  /* Wi-Fi + BLE birlikteyken modem uykusu zorunlu (kapatilirsa ESP32 cokuyor).
     Kendi agimiz (AP) bundan etkilenmez; sadece ev agi tarafinda gecikme biraz artar. */
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  EvBaglan();

  Sunucu.begin();
  Sunucu.setNoDelay(true);

  BleKur();

  ArduinoOTA.setHostname(MdnsAd);
  ArduinoOTA.setPassword(AP_SIFRE);
  ArduinoOTA.setMdnsEnabled(false);              /* mDNS'i kendimiz aciyoruz */
  ArduinoOTA.begin();

  Serial.printf("\n%s  sifre: %s  adres: 192.168.4.1:%d  surum %d.%d\n", CihazAd, AP_SIFRE, TCP_PORT,
                KOPRU_SURUM >> 8, KOPRU_SURUM & 0xFF);
  Serial.printf("Ev agi: '%s'  (degistirmek icin: wifi AgAdi Sifre)\n", EvAd.c_str());
}

void loop()
{
  /* Ev agina baglaninca mDNS: spektrum-xxxx.local (+ Arduino IDE ag portu) */
  if(!MdnsAcik && WiFi.status() == WL_CONNECTED)
  {
    MDNS.end();
    if(MDNS.begin(MdnsAd))
    {
      MDNS.setInstanceName(CihazAd);
      MDNS.addService("spektrum", "tcp", TCP_PORT);
      MDNS.enableArduino(3232, true);
      MdnsAcik = true;
    }
    Serial.printf("Ev agina baglandi: %s  (%s.local)\n", WiFi.localIP().toString().c_str(), MdnsAd);
  }
  if(MdnsAcik && WiFi.status() != WL_CONNECTED) MdnsAcik = false;
  ArduinoOTA.handle();

  /* Guncelleme bitti: cevap gitsin, sonra yeniden basla */
  if(YenidenBasla && millis() - YenidenZaman > 500) ESP.restart();

  /* Seri monitor */
  while(Serial.available())
  {
    char ch = (char)Serial.read();
    if(ch == '\n' || ch == '\r') { if(SeriSatir.length()) SeriKomut(SeriSatir); SeriSatir = ""; }
    else if(SeriSatir.length() < 120) SeriSatir += ch;
  }

  /* Yeni TCP istemci: eskisini birak */
  if(Sunucu.hasClient())
  {
    WiFiClient Yeni = Sunucu.available();
    if(Istemci && Istemci.connected()) Istemci.stop();
    Istemci = Yeni;
    Istemci.setNoDelay(true);
    Kay[K_TCP].N = 0;
    Serial.println("Istemci baglandi");
  }

  const bool Bagli = Istemci && Istemci.connected();
  digitalWrite(LED_PIN, (Bagli || BleBagli) ? HIGH : LOW);

  /* BLE koptu: yeniden gorunur ol */
  if(BleYenidenYayin)
  {
    BleYenidenYayin = false;
    Kay[K_BLE].N = 0;
    delay(50);
    BLEDevice::startAdvertising();
  }

  /* TCP -> UART */
  if(Bagli)
  {
    int n = Istemci.available();
    if(n > 0)
    {
      if(n > TAMPON) n = TAMPON;
      n = Istemci.read(Tampon, n);
      for(int i = 0; i < n; i++) KaynakBayt(K_TCP, Tampon[i]);
    }
  }

  /* BLE -> UART */
  size_t m;
  while((m = xStreamBufferReceive(BleGelen, Tampon, TAMPON, 0)) > 0)
    for(size_t i = 0; i < m; i++) KaynakBayt(K_BLE, Tampon[i]);

  /* UART -> tum istemciler (kimse yoksa bosalt, tampon tasmasin) */
  int n = Serial2.available();
  if(n > 0)
  {
    if(n > TAMPON) n = TAMPON;
    n = Serial2.readBytes(Tampon, n);
    if(n > 0)
    {
      if(Bagli) Istemci.write(Tampon, n);
      BleYaz(Tampon, n);
    }
  }

  if(!Bagli && Istemci) { Istemci.stop(); Serial.println("Istemci ayrildi"); }
  delay(1);
}
