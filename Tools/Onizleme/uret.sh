#!/bin/sh
# Mod onizlemelerini firmware'in kendi cizim koduyla uretir ve Android uygulamasina koyar.
# Linux/WSL/Git Bash: sh Tools/Onizleme/uret.sh   (gcc, python3 + numpy + pillow gerekir)
set -e
python3 "$(dirname "$0")/osd_kontrol.py" > /dev/null || { python3 "$(dirname "$0")/osd_kontrol.py"; exit 1; }   # OSD yazilari sigiyor mu
KOK=$(cd "$(dirname "$0")/../.." && pwd)
GEC=$(mktemp -d)
cp "$KOK"/Core/Inc/*.h "$GEC"/
cp "$KOK"/Tools/Onizleme/host/main.h "$GEC"/main.h          # STM32 HAL yerine sahte katman
cp "$KOK"/Core/Src/Spectrum.c "$KOK"/Core/Src/SpecDisplay.c "$KOK"/Core/Src/Settings.c "$KOK"/Tools/Onizleme/onizleme.c "$GEC"/
gcc -std=gnu11 -O2 -I"$GEC" "$GEC"/onizleme.c "$GEC"/Spectrum.c "$GEC"/SpecDisplay.c "$GEC"/Settings.c -lm -o "$GEC"/onizleme
mkdir -p "$GEC"/ham
"$GEC"/onizleme "$GEC"/ham 48 6                                # 48 kare, ~15 kare/sn
python3 "$KOK"/Tools/Onizleme/png_uret.py "$GEC"/ham "$KOK"/Android/app/src/main/assets/modlar "$KOK"/Tools/Onizleme/onizleme_tablo.png
python3 "$KOK"/Tools/Onizleme/saat_yazi_png.py     # Saat fontu ornekleri
rm -rf "$GEC"
