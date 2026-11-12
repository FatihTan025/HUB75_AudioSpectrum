#!/usr/bin/env python3
"""Uygulama simgesi: siyah zemin, LED gibi bolmeli spektrum cubuklari + beyaz tepe noktalari.
Tek tasarimdan hem adaptive vektor (Android 8+) hem eski telefonlar icin PNG uretir."""
import pathlib, sys
from PIL import Image, ImageDraw
KOK = pathlib.Path(__file__).resolve().parents[2] / 'Android/app/src/main/res'

# 108 birimlik tuval (adaptive), guvenli alan ~ 21..87
YUK = [16, 28, 40, 32, 22, 30, 18]           # cubuk yukseklikleri
TEPE = [22, 34, 44, 38, 28, 35, 24]          # tepe noktasinin yerden yuksekligi
G, ARA, X0, TAB = 6.0, 2.0, 27.0, 76.0       # cubuk eni, ara, ilk x, taban
SEG, BOS = 4.0, 1.2                          # LED bolmesi ve bosluk
RENK = [(0, 0xE5, 0xFF), (0x7C, 0x4D, 0xFF), (0xFF, 0x40, 0x81)]   # alt -> ust (Y 76 -> 32)
UST = TAB - 44

def renk(y):                                 # dikey gecis
    t = min(1, max(0, (TAB - y) / (TAB - UST)))
    i = 0 if t < .5 else 1; u = t * 2 - i
    a, b = RENK[i], RENK[i + 1]
    return tuple(round(a[k] + (b[k] - a[k]) * u) for k in range(3))

def bolmeler():                              # (x, y_ust, en, boy) LED bolmeleri
    for i, h in enumerate(YUK):
        x = X0 + i * (G + ARA); y = TAB
        while y - SEG >= TAB - h - .01:
            yield x, y - SEG + BOS, G, SEG - BOS; y -= SEG

def vektor():
    hx = lambda c: '#FF%02X%02X%02X' % c
    yol = ''.join('M%.1f,%.1fh%.1fv%.1fh-%.1fz' % (x, y, w, h, w) for x, y, w, h in bolmeler())
    tepe = ''.join('M%.1f,%.1fh%.1fv2.2h-%.1fz' % (X0 + i * (G + ARA), TAB - t, G, G) for i, t in enumerate(TEPE))
    return f'''<?xml version="1.0" encoding="utf-8"?>
<!-- Tools/Ikon/ikon_uret.py ile uretildi -->
<vector xmlns:android="http://schemas.android.com/apk/res/android" xmlns:aapt="http://schemas.android.com/aapt"
    android:width="108dp" android:height="108dp" android:viewportWidth="108" android:viewportHeight="108">
    <path android:pathData="{yol}">
        <aapt:attr name="android:fillColor">
            <gradient android:type="linear" android:startX="0" android:startY="{TAB}" android:endX="0" android:endY="{UST}">
                <item android:offset="0" android:color="{hx(RENK[0])}"/>
                <item android:offset="0.5" android:color="{hx(RENK[1])}"/>
                <item android:offset="1" android:color="{hx(RENK[2])}"/>
            </gradient>
        </aapt:attr>
    </path>
    <path android:fillColor="#FFFFFFFF" android:pathData="{tepe}"/>
</vector>
'''

def png(px, yuvarlak):                       # eski Android: kendi zeminiyle tam simge
    O = 4; S = px * O; k = S / 108 * 1.25; d = 54 - 54 / 1.25   # 108'in ortadaki %80'i
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
    if yuvarlak: dr.ellipse([0, 0, S - 1, S - 1], fill=(0, 0, 0, 255))
    else: dr.rounded_rectangle([0, 0, S - 1, S - 1], S * .2, fill=(0, 0, 0, 255))
    P = lambda v: (v - d) * k
    for x, y, w, h in bolmeler():
        dr.rectangle([P(x), P(y), P(x + w), P(y + h)], fill=renk(y + h / 2))
    for i, t in enumerate(TEPE):
        x = X0 + i * (G + ARA); dr.rectangle([P(x), P(TAB - t), P(x + G), P(TAB - t + 2.2)], fill=(255, 255, 255))
    return im.resize((px, px), Image.LANCZOS)

if __name__ == '__main__':
    (KOK / 'drawable').mkdir(parents=True, exist_ok=True)
    (KOK / 'drawable/ikon_on.xml').write_text(vektor())
    (KOK / 'mipmap-anydpi-v26').mkdir(exist_ok=True)
    for ad in ('ic_launcher', 'ic_launcher_round'):
        (KOK / f'mipmap-anydpi-v26/{ad}.xml').write_text('''<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@android:color/black"/>
    <foreground android:drawable="@drawable/ikon_on"/>
    <monochrome android:drawable="@drawable/ikon_on"/>
</adaptive-icon>
''')
    for q, px in dict(mdpi=48, hdpi=72, xhdpi=96, xxhdpi=144, xxxhdpi=192).items():
        (KOK / f'mipmap-{q}').mkdir(exist_ok=True)
        png(px, False).save(KOK / f'mipmap-{q}/ic_launcher.png')
        png(px, True).save(KOK / f'mipmap-{q}/ic_launcher_round.png')
    if len(sys.argv) > 1: png(512, False).save(sys.argv[1])
