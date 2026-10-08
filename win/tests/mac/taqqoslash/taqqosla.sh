#!/usr/bin/env bash
#
# macOS va Windows matn boʻluvchilarini YONMA-YON solishtiradi.
#
#   ./win/tests/mac/taqqoslash/taqqosla.sh
#
# Ikkita taqqoslash bor:
#   1. matn boʻluvchi  — `matn_boluvchi.swift` va `matn_boluvchi.cpp`
#   2. matn formatlash — `text_format.swift` va `matn_format.cpp`
#
# Nega kerak: ular bitta ilovaning ikki tanasi va BIR XIL natija berishi
# shart (`AGENTS.md`). Birlik testlari qoʻlda yozilgan holatlarni qamraydi;
# bu esa haqiqiy korpus ustida ikkalasini yugurtirib, farqni koʻrsatadi.
#
# MUHIM CHEKLOV: macOS'da C++ tomoni ICU'ni topa olmaydi (Windows ICU'ning
# eksport nomlari boshqacha) va **zaxira qoida** bilan ishlaydi. Yaʼni bu
# taqqoslash eng YOMON holatni oʻlchaydi: Windows'da ICU bor va natija
# faqat yaxshiroq boʻladi. Farq chiqsa — zaxira qoidani tuzatish kerak.
#
# Korpusga yangi holat qoʻshish: `korpus.txt` (boʻluvchi) yoki
# `segmentlar.txt` (formatlash) ga qator qoʻshing.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ISH="${TMPDIR:-/tmp}/kotib-taqqoslash"
BU="$ROOT/win/tests/mac/taqqoslash"
mkdir -p "$ISH"

swiftc -O -o "$ISH/swift-tomon" "$BU/main.swift" \
    "$ROOT/src/matn_boluvchi.swift" "$ROOT/src/tillar.swift" "$ROOT/src/text_format.swift"

clang++ -std=c++20 -O1 \
    -I"$ROOT/win/core" -I"$ROOT/win/tests/mac" \
    -include "$ROOT/win/tests/mac/windows.h" \
    -o "$ISH/cpp-tomon" "$BU/cpp-tomon.cpp" \
    "$ROOT/win/core/matn_boluvchi.cpp" "$ROOT/win/tests/mac/util_shim.cpp"

"$ISH/swift-tomon" "$BU/korpus.txt" > "$ISH/swift.out"
"$ISH/cpp-tomon"   "$BU/korpus.txt" > "$ISH/cpp.out"

xato=0
if diff -u "$ISH/swift.out" "$ISH/cpp.out"; then
    echo "✓ matn boʻluvchi: macOS va Windows bir xil"
else
    echo "✗ matn boʻluvchi: farq bor" >&2
    xato=1
fi

# ---- Matn formatlash ------------------------------------------------------
#
# Bu ilovaning ASOSIY chiqishi: foydalanuvchi koʻradigan matn. AGENTS.md
# ikkala platformada bir xil boʻlishini talab qiladi va bu yerda tekshiriladi.
mkdir -p "$ISH/fmt"
cp "$BU/format-main.swift" "$ISH/fmt/main.swift"
swiftc -O -o "$ISH/fmt-swift" "$ISH/fmt/main.swift" "$ROOT/src/text_format.swift"

clang++ -std=c++20 -O1 \
    -I"$ROOT/win/core" -I"$ROOT/win/tests/mac" \
    -include "$ROOT/win/tests/mac/windows.h" \
    -o "$ISH/fmt-cpp" "$BU/format-cpp.cpp" \
    "$ROOT/win/core/matn_format.cpp" "$ROOT/win/tests/mac/util_shim.cpp"

"$ISH/fmt-swift" "$BU/segmentlar.txt" > "$ISH/fmt-swift.out"
"$ISH/fmt-cpp"   "$BU/segmentlar.txt" > "$ISH/fmt-cpp.out"

if diff -u "$ISH/fmt-swift.out" "$ISH/fmt-cpp.out"; then
    echo "✓ matn formatlash: macOS va Windows bir xil"
else
    echo "✗ matn formatlash: farq bor" >&2
    xato=1
fi

exit "$xato"
