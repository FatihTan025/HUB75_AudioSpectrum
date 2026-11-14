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
    bas = re.search(r'FONT_INFO\s+' + ad + r'\s*=\s*\{\s*\d+\s*,\s*\'(.)\'', basliklar).group(1)
    return h, d, b, ord(bas)

CIKIS.mkdir(parents=True, exist_ok=True)
for f in CIKIS.glob('*.png'): f.unlink()
KUCUK = font_oku('arialbd13FontInfo')                      # Sigmayan saniye (SpecDisplay.c ile ayni kural)
ARA, PANEL_W = 2, 128

def glif(F, c):
    h, d, b, bas = F
    return d[ord(c) - bas]

def ciz(px, F, c, x, y):
    h, d, b, bas = F
    bw, w, hh, off = glif(F, c)
    for yy in range(hh):
        for xx in range(w):
            if b[off + yy * bw + (xx >> 3)] & (0x80 >> (xx & 7)): px[x + xx, y + yy] = (230, 235, 245, 255)
    return w

for i, fad in enumerate(fontlar):
    F = font_oku(fad)
    h = F[0]
    yv = max(glif(F, str(r))[1] for r in range(10))
    wc = glif(F, ':')[1]
    ad = adlar[i] if i < len(adlar) else fad
    for saniye in (False, True):
        rak = '1234' + ('56' if saniye else '')
        buyuk = saniye and (6 * yv + 2 * wc + 7 * ARA) <= PANEL_W - 1
        im = Image.new('RGBA', (PANEL_W, h + 4), (0, 0, 0, 0)); px = im.load(); x = 2
        n = 6 if buyuk else 4
        for k in range(n):
            ch = rak[k]
            ciz(px, F, ch, x + (yv - glif(F, ch)[1]) // 2, 2)
            x += yv + ARA
            if k == 1 or (k == 3 and buyuk):
                ciz(px, F, ':', x, 2); x += wc + ARA
        if saniye and not buyuk:                           # Kucuk saniye, alt hizada
            for ch in '56':
                x += ciz(px, KUCUK, ch, x, 2 + h - KUCUK[0]) + 1
        im = im.crop((0, 0, x + 2, h + 4))
        im.save(CIKIS / (sade(ad) + ('_sn' if saniye else '') + '.png'))
    print(f'{fad} -> saatyazi/{sade(ad)}.png (+ _sn)')
