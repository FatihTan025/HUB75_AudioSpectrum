#!/usr/bin/env python3
"""Saat fontu ornekleri: SpecDisplay.c SaatYazilari[] sirasindaki her fonttan "12:34" resmi uretir.
Adlar Settings.c SAAT_YAZI secenek listesinden (ayni sira). Cikti: assets/saatyazi/<sade_ad>.png
Fatih'in fontu (FONT_INFO, 0-9; .h ya da .c icinde) tabloya eklenince ornegi kendiliginden cikar."""
import re, sys, pathlib
from PIL import Image
KOK = pathlib.Path(__file__).resolve().parents[2]
CIKIS = KOK / 'Android/app/src/main/assets/saatyazi'
TR = str.maketrans('çğıöşüÇĞİÖŞÜ', 'cgiosuCGIOSU')
sade = lambda a: re.sub(r'[^a-z0-9]+', '_', a.translate(TR).lower()).strip('_')
coz = lambda t: re.sub(rb'\\([0-7]{3})', lambda m: bytes([int(m.group(1), 8)]), t.encode()).decode('utf-8')

sd = (KOK / 'Core/Src/SpecDisplay.c').read_text(encoding='utf-8', errors='replace')
fontlar = re.findall(r'&(\w+)', re.search(r'SaatYazilari\[\]\s*=\s*\{([^}]*)\}', sd).group(1))
st = (KOK / 'Core/Src/Settings.c').read_text(encoding='ascii')
adlar = coz(re.search(r'\{ SET_ID_SAAT_YAZI,.*?"[^"]*",\s*"[^"]*",\s*"([^"]*)"', st, re.S).group(1)).split('|')
basliklar = re.sub(r'/\*.*?\*/', ' ', ''.join(p.read_text(encoding='utf-8', errors='replace')
                    for p in [*(KOK / 'Core/Inc').glob('*.h'), *(KOK / 'Core/Src').glob('*.c')] if 'FONT_INFO' in p.read_text(encoding='utf-8', errors='replace')), flags=re.S)  # yorumlar silinir

def font_oku(ad):
    m = re.search(r'FONT_INFO\s+' + ad + r'\s*=\s*\{\s*(\d+)\s*,[^,]*,[^,]*,\s*(\w+)\s*,\s*(\w+)', basliklar)
    h, desc, bmp = int(m.group(1)), m.group(2), m.group(3)
    d = [tuple(int(v) for v in t) for t in re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}',
         re.search(desc + r'\s*\[[^\]]*\]\s*=\s*\{(.*?)\};', basliklar, re.S).group(1))]
    b = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]{2})', re.search(bmp + r'\s*\[[^\]]*\]\s*=\s*\{(.*?)\};', basliklar, re.S).group(1))]
    return h, d, b

CIKIS.mkdir(parents=True, exist_ok=True)
for f in CIKIS.glob('*.png'): f.unlink()
for i, fad in enumerate(fontlar):
    h, d, b = font_oku(fad)
    gen = lambda r: d[r][1]
    rak = [1, 2, None, 3, 4]
    W = sum(gen(r) + 2 for r in rak if r is not None) + 7
    im = Image.new('RGBA', (W + 4, h + 4), (0, 0, 0, 0)); px = im.load(); x = 2
    for r in rak:
        if r is None:                                  # iki nokta
            for yy in (h // 2 - 5, h // 2 + 4):
                for dx in range(2):
                    for dy in range(2): px[x + 2 + dx, 2 + yy + dy] = (230, 235, 245, 255)
            x += 7; continue
        bw, w, hh, off = d[r]
        for y in range(hh):
            for xx in range(w):
                if b[off + y * bw + (xx >> 3)] & (0x80 >> (xx & 7)): px[x + xx, 2 + y] = (230, 235, 245, 255)
        x += w + 2
    ad = adlar[i] if i < len(adlar) else fad
    im.save(CIKIS / (sade(ad) + '.png'))
    print(f'{fad} -> saatyazi/{sade(ad)}.png')
