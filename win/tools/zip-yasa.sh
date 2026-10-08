#!/usr/bin/env bash
#
# Portativ Windows tarqatmasi (ZIP) — oʻrnatuvchisiz.
#
#   ./win/tools/zip-yasa.sh [x64|arm64]
#
# Natija: `dist/Kotib-<versiya>-win-<arch>-portable.zip`
#
# Nega bu mumkin boʻldi: nutq modeli endi ilova ichida yuklab olinadi
# (`core/model_yuklovchi.cpp`). Ilgari model faqat oʻrnatuvchi bilan
# kelardi va modelsiz nusxa ishlamasdi.
#
# Nega ZIP kerak: Inno Setup oʻrnatuvchisi faqat Windows'da yigʻiladi
# (`iscc.exe`), macOS'da esa Wine talab qiladi. ZIP esa shu yerda yigʻiladi
# va SmartScreen ogohlantirishisiz ishlaydi — oʻrnatuvchi kerak emas,
# papkani ochib `Kotib.exe` ni bosish kifoya.
#
# Cheklov: avtostart va Start menyusidagi yorliq YOʻQ (ularni oʻrnatuvchi
# qiladi). Avtostartni ilovaning oʻzi Sozlamalardan yoqadi.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARCH="${1:-x64}"
BUILD="$ROOT/win/build-$ARCH"
DIST="$ROOT/dist"

[ -f "$BUILD/Kotib.exe" ] || {
    echo "Kotib.exe topilmadi: $BUILD" >&2
    echo "Avval: ./win/build-mac.sh $ARCH" >&2
    exit 1
}

# Versiya — `VERSION` faylidan (yagona manba). app.rc ham uni CMake orqali oladi.
# shellcheck source=../../scripts/versiya.sh
source "$ROOT/scripts/versiya.sh"
VERSIYA="$KOTIB_VERSIYA"

NOM="Kotib-$VERSIYA-win-$ARCH-portable"
ISH="${TMPDIR:-/tmp}/$NOM"
rm -rf "$ISH"
mkdir -p "$ISH" "$DIST"

cp "$BUILD/Kotib.exe" "$ISH/"
cp "$BUILD"/*.dll "$ISH/" 2>/dev/null || true
cp "$ROOT/LICENSE" "$ISH/LICENSE.txt"

cat > "$ISH/OQING.txt" <<'TXT'
Kotib — oʻzbekcha ovozli yozuv (portativ nusxa)
================================================

ISHGA TUSHIRISH
  Kotib.exe ni ikki marta bosing. Ilova vazifalar panelining oʻng chetida
  (soat yonida) 🎙 ikonkasi boʻlib turadi.

BIRINCHI QADAM — MODEL
  Birinchi ochilishda «Nutq modeli topilmadi» degan sariq chiziq chiqadi.
  «Yuklab olish» ni bosing — model bir marta yuklab olinadi (785 MB) va
  kompyuterda saqlanadi. Shundan keyin internet kerak emas.

ISHLATISH
  Istalgan ilovada matn yozadigan joyga kursorni qoʻying, Ctrl+Alt+D
  bosing, gapiring, yana Ctrl+Alt+D bosing. Matn oʻsha joyga yoziladi.

  Tugmani Sozlamalardan (⚙) oʻzgartirish mumkin.

MIKROFON
  Sozlamalarda mikrofonni ALBATTA tanlang. Windows'da standart qurilma
  baʼzan «Stereo Mix» boʻlib qoladi — u ovozingizni emas, kompyuter
  ovozini yozadi. Ilova bunday qurilmalarni ogohlantiradi.

SMARTSCREEN
  Ilova kod imzosi sertifikati bilan imzolanmagan, shuning uchun Windows
  «Windows protected your PC» deb ogohlantirishi mumkin:
  «More info» → «Run anyway».

BU NUSXADA YOʻQ
  Start menyusidagi yorliq va avtomatik ishga tushish oʻrnatuvchi bilan
  keladi. Avtostartni ilovaning oʻzi Sozlamalardan yoqadi.

TALABLAR
  Windows 10 (1809) yoki Windows 11, 64-bit. 4 GB RAM (videokarta bilan)
  yoki 6 GB (protsessor rejimida). ~1 GB disk.

Litsenziya: LICENSE.txt. Model va whisper.cpp oʻz litsenziyalari ostida.
TXT

# `ditto` HFS metadata qoʻshmaydi va Windows'da `__MACOSX` chiqmaydi.
rm -f "$DIST/$NOM.zip"
(cd "$(dirname "$ISH")" && ditto -c -k --sequesterRsrc --keepParent "$NOM" "$DIST/$NOM.zip")

echo "Tayyor: dist/$NOM.zip"
unzip -l "$DIST/$NOM.zip" | tail -20
