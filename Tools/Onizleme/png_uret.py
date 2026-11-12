"""onizleme ciktisini (ham RGB kareler) Android icin kare seritlerine (PNG) cevirir.
Her mod: assets/modlar/<ad>.png  -- 128 genis, 32*N yuksek (N kare alt alta).
Ad: mod adinin sade hali (Turkce harfler -> Latin, bosluk -> _). Kotlin tarafi ayni kurali kullanir.
Ayrica tum modlarin LED gorunumlu bir tablosu: onizleme_tablo.png"""
import sys, os, re
import numpy as np
from PIL import Image, ImageDraw

TR = str.maketrans("ÇĞİIÖŞÜçğıöşü", "CGIIOSUcgiosu")
def sade(ad): return re.sub(r"[^a-z0-9]+", "_", ad.translate(TR).lower()).strip("_")

ham, assets, tablo = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(assets, exist_ok=True)
for f in os.listdir(assets):
    if f.endswith(".png"): os.remove(os.path.join(assets, f))
satirlar = [l.rstrip("\n").split("\t") for l in open(os.path.join(ham, "liste.txt"), encoding="utf-8")]

S = 4
m = np.zeros((S, S), np.float32)
for y in range(S):
    for x in range(S):
        d = ((x - 1.5) ** 2 + (y - 1.5) ** 2) ** .5
        m[y, x] = 1.0 if d < 1.6 else 0.25 if d < 2.2 else 0.0
maske = np.tile(m, (32, 128))[..., None]
def led(k): return Image.fromarray((np.repeat(np.repeat(k, S, 0), S, 1) * maske).clip(0, 255).astype(np.uint8))

W, H = 128 * S, 32 * S
sayfa = Image.new("RGB", (W * 2 + 30, (H + 20) * ((len(satirlar) + 1) // 2) + 10), (20, 16, 40))
ciz = ImageDraw.Draw(sayfa)
for i, (no, aile, ad) in enumerate(satirlar):
    k = np.frombuffer(open(os.path.join(ham, f"{int(no):02d}.rgb"), "rb").read(), np.uint8).reshape(-1, 32, 128, 3)
    Image.fromarray(k.reshape(-1, 128, 3)).save(os.path.join(assets, sade(ad) + ".png"), optimize=True)
    x = 10 + (i % 2) * (W + 10); y = 10 + (i // 2) * (H + 20)
    sayfa.paste(led(k[len(k) // 2]), (x, y + 14)); ciz.text((x, y), f"{no}  {sade(ad)}", fill=(255, 210, 63))
    print(f"{no:>2} {ad:<16} -> {sade(ad)}.png  ({len(k)} kare)")
sayfa.save(tablo)
