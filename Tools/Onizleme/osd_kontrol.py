#!/usr/bin/env python3
"""OSD sigma kontrolu: her ayar icin "AD: DEGER" (yalniz panel menusundekiler) 21 karakteri (128 px / 6 px) asiyor mu?"""
import re, sys, pathlib
KOK = pathlib.Path(__file__).resolve().parents[2]
s = (KOK / 'Core/Src/Settings.c').read_text(encoding='ascii')
coz = lambda t: re.sub(rb'\\([0-7]{3})', lambda m: bytes([int(m.group(1), 8)]), t.encode()).decode('utf-8')
SINIR, hata = 21, 0
# { ..., "Ad", "Grup", "Secenek" "devami", Fonksiyon
for m in re.finditer(r'"([^"]*)",\s*"([^"]*)",\s*((?:"[^"]*"\s*)+),\s*(?:/\*.*?\*/\s*)?(?:Apply\w+|0)', s):
    ad = coz(m.group(1)); sec = coz(''.join(re.findall(r'"([^"]*)"', m.group(3))))
    if '|' in sec: ops = [o.split(':')[0] for o in sec.split('|')]
    elif sec == '': ops = ['']          # renk/ton: yalniz ad + renk cubugu
    else: ops = ['9999']                # sayi + birim (birim OSD'de yazilmaz)
    bas = s.rfind('{ SET_ID', 0, m.start())
    bayrak = s[bas:m.start()].split(',')[2]
    # Panel menusu yalniz mod ayarlarini gosterir (gizli/gelismis/salt okunur haric)
    if 'PER_MODE' not in bayrak or re.search(r'\b[HAR]\b|HIDDEN|ADVANCED|READONLY', bayrak): continue
    uzun = max(ops, key=len)
    L = len(ad) + (2 + len(uzun) if uzun else 0)
    isaret = '!!' if L > SINIR else '  '
    hata += L > SINIR
    print(f'{isaret} {L:2}  {ad.upper()}' + (f': {uzun.upper()}' if uzun else ''))
print('TAMAM' if not hata else f'{hata} ayar sigmiyor'); sys.exit(1 if hata else 0)
