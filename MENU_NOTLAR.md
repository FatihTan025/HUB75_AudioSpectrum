# Panel menusu (12 Kasim 2026) -- sade OSD

Eski plan (simgeli ana menu, doner dugme cizimi, Windows temasi, ~12-15 KB) BIRAKILDI:
telefon asil kontrol, panel menusu sade olacak.

## Davranis
| Durum | Encoder | PB3 / PB4 |
|---|---|---|
| Menu kapali | cevir = mod, basili cevir = parlaklik, kisa bas = menu ac, uzun bas = ekran ac/kapa | kisa = mod +/-, PB3 uzun (800 ms) = menu ac |
| Menu acik | cevir = deger, kisa bas = sonraki ayar (sondan sonra kapanir), uzun bas = kapat | kisa = deger +/-, PB3 uzun = sonraki ayar |
- 8 s islem yoksa kapanir. Degisiklik aninda uygulanir, kaydedilir, uygulamaya bildirilir (EVT_PARAM).
- Liste: aktif modun ayarlari, cihaz tablosundan (gizli/gelismis degil, o modda var, ShowId kosulu tutuyor).
  Android ve PC araci ile ayni kural. Genel ayarlar (oto sure vb.) menude yok: uygulamadan.
- Butonlarda kisa basma artik BIRAKINCA algilanir (uzun basmadan ayirmak icin).

## OSD (ust bant)
- Ust 9 satir karartilir, 5x7 Turkce buyuk harf (SpecDisplay.c Font57). Ayara gecince 1 s ad, sonra "AD: DEGER"
  (sigmazsa yalniz deger). Renk/ton ayarlarinda altinda gokkusagi seridi + imlec (ust 13 satir).
- Cikista kisa "TAMAM".

## Kararlar
- Modun TUM kullanici ayarlari menude (ayri "panelde goster" bayragi yok). Ayar simgeleri simdilik yok.
- Adlar OSD'ye sigsin diye kisaltildi: Genislik, Sutun Renk, Tepe Renk, Alev Renk;
  Renk secenekleri Tek / Hi-Fi / Gokkusagi / Akan. Kural: "AD: DEGER" en cok 21 karakter.
  Denetim: Tools/Onizleme/osd_kontrol.py (uret.sh basinda da calisir). Adlar CRC'ye girmez, kayit sifirlanmaz.

- Renk ayarlarinda beyaz: seridin basinda beyaz bolum; encoder mordan sonra beyaza gecer (Android'de de ayni).

## Dosyalar
Menu.c/.h (mantik), SpecDisplay.c (SPEC_Osd, OsdCiz, Font57), Buttons.c (kisa/uzun), main.c (baglanti).
Kartta henuz denenmedi.
