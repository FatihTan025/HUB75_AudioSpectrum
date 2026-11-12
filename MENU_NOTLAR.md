# Menu Plani (8 Kasim 2026) -- yeni oturum once bunu ve NOTLAR.md'yi okusun

Kurallar: Turkce, kisa. Fatih "basla/uygula" demeden kod yazma. Yazmadan once dosyayi
cihazdan al, yazdiktan sonra geri okuyup karsilastir. Kaldirilan ozelligin tum kodu silinir.
.ioc kodla senkron DEGIL (gpio.c/tim.c elle) -> CubeMX'te "Generate Code" YASAK.
Yeni donanim eklenince .ioc'ye de islenecek (sonraya birakildi; encoder icin henuz yapilmadi).

## Yapildi
- Encoder.c/.h: PB6/PB7 TIM4 donanim encoder, buton PB5 (pull-up). ENC_Poll -> Steps, Held, Click, Long.
- main.c: cevir = mod, basili cevir = parlaklik (+-%5, SPEC_ShowLevel cubugu), uzun bas = ekran ac/kapa.
  e.Click bos: menuye baglanacak.
- Protocol: PROTO_NotifyParam (EVT_PARAM: Mod Id Deger), PROTO_NotifyPower (EVT_POWER).
- SpektrumAraci.py: EVT_PARAM gelince ilgili satiri gunceller.
- gpio.c: PB5 cikis listesinden cikarildi.
- Fatih'te su an encoder YOK; PB5'i GND'ye degdirerek dener. PB3/PB4 butonlari bagli.

## Giris duzeni (giris katmani: Ileri / Geri / Sec / Cik)
| Durum | PB5 (encoder butonu) | PB3/PB4 | Encoder cevirme |
| Menu kapali | kisa: menu ac, uzun: ekran ac/kapa | mod | mod, basili: parlaklik |
| Menude gezinme | kisa: sec, uzun: cik | ileri/geri | ileri/geri |
| Deger duzenleme | kisa: onayla | artir/azalt | artir/azalt |
- Menuye giris ayrica: PB3+PB4 birlikte (simdiki butonlar buna izin vermiyor ama kalsin), ileride IR OK.
- Butonlara uzun basma algisi Buttons.c'ye eklenecek (su an sadece kisa).

## Yapilacaklar (sirayla, her adim sonunda kartta deneme)
1. 5x7 Turkce font: BUYUK harf A-Z, 0-9, % : - . < > ve Ç Ğ İ Ö Ş Ü (~250 B, HUB75_Font.h yapisi).
   Ayar adlari cizilirken Turkce buyuk harfe cevrilir (ı->I, i->İ). 21 karakter/satir.
2. Cizim parcalari (kodla, resim yok; menu_onizleme.png / renk_onizleme.png taslaklari Fatih'te):
   - Doner dugme ~28 px: metalik gri, etrafinda degere gore yanan 11 nokta, gosterge cizgisi.
   - Yatay kaydirici (vurgu dolgu + topuz + centik), hap anahtar (bool), "◀ AD ▶" (enum),
     ton halkasi + ton seridi (COLOR), renk bloklari secili buyuk + cerceve (SWATCH).
   - Koyu zemin, gri/beyaz yazi, tek VURGU rengi = yeni ayar "Menu rengi" (Ekran grubu, blok;
     varsayilan camgobegi).
3. Menu:
   - Ana menu: 5 simge 20x20 (Mod, Renk, Hareket, Ses, Ekran, Wi-Fi...), basliklar ayar
     tablosundaki Grup adlarindan; tanimsiz grup genel simge.
   - Alt menu: o grubun ayarlari, Available() ne derse (Python araci gibi kartin tablosundan).
   - Her ayar turune gore ekran (INT dugme+kaydirici, ENUM oklar, BOOL anahtar, COLOR halka, SWATCH bloklar).
   - Onizleme tablosu (Id -> cizim fonksiyonu): CLS_COLOR (mini cubuk onizlemeleri),
     CLS_WIDTH (1-4 px cubuklar), CLS_SHAPE (yukari/sarkan/simetrik). Digerleri otomatik.
   - Hareketler: deger hedefe kayarak (pos += (hedef-pos)*k), menu acilirken arka animasyon ~0.3 s
     sonuklesir, ekran gecisleri yatay kayma, dugmeye basinca kisa cokme. 10 s islem yoksa cik + kaydet.
   - Menu acikken animasyon arkada sonuk devam eder; deger degisince canli uygulanir.
   - Her degisiklikte SET_Set + SET_MarkLocalChange + PROTO_NotifyParam.
4. Flash tahmini ~12-15 KB (simdi ~62/256 KB), RAM birkac yuz bayt.

## Sonraki isler (menu sonrasi)
- .ioc senkronu (Fatih CubeMX'te, ben kontrol listesi), ESP32 NTP saat + "saat ayarla" komutu,
  BLE (NUS), Android uygulamasi (renk ekrani elle tasarlanacak, digerleri tablodan).

## Gorunum (Fatih panelde onizlemeyi gordu, 8 Kasim)
- Onizleme: MenuDemo.c (GECICI, ~74 KB), SpecConfig.h CFG_MENU_DEMO 1 -> PB3/PB4 9 resim arasinda gezer,
  0 = normal. Karar sonrasi MenuDemo.c + CFG_MENU_DEMO + main.c/SpecDisplay.c'deki demo bloklari TAMAMEN silinecek.
- Fatih: su anki tasarim Windows gibi gorunmuyor, biraz daha guzellestirilebilir; secenek olarak kalsin.
- Ikinci tema fikri "Pencere" (Windows 95/98 tarzi): baslik cubugu, 3B kenarlar (ust-sol acik, alt-sag koyu),
  liste + secim seridi, kabarik Tamam/Iptal dugmeleri. AMA zemin GRI DEGIL, KOYU olacak (panel guc harcamasin).
  Menu kodu ayni, yalnizca cizim parcalari temaya gore. Ayar: "Menu temasi" (Klasik / Pencere), ~2-3 KB.
  Once ayni yolla (butonlarla) onizlemesi gosterilecek.
