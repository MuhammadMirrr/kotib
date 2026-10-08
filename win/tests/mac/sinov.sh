#!/usr/bin/env bash
#
# Windows tomonining sof mantiq testlarini **macOS'da** ishga tushiradi.
#
#   ./win/tests/mac/sinov.sh
#
# Nega kerak: `kotib-testlar.exe` faqat Windows'da (yoki UTM'dagi VM'da)
# ishlaydi va har oʻzgarishdan keyin VM'ni koʻtarish — bir necha daqiqa.
# Bu skript esa bir soniyada javob beradi va kundalik ishda shu ishlatiladi.
#
# NIMA QAMRALADI: `matn_format`, `vaqt_format`, `matn_boluvchi`, `tillar`,
# avto-yangilanish siyosati va Ed25519 imzo (`yangilanish_siyosat`, `imzo`) —
# ular Win32 API'ga faqat bir necha joyda tegadi va u yerlar
# `win/tests/mac/windows.h` qalqoni bilan yopilgan.
#
# NIMA QAMRALMAYDI (VM'dagi `kotib-testlar.exe` ni almashtirmaydi):
#   • ICU orqali jumlalarga boʻlish — bu yerda zaxira qoida ishlaydi;
#   • UTF-16 surrogat juftliklari — macOS'da `wchar_t` 32-bitli;
#   • WinHTTP, Credential Manager va .exe resursiga bogʻliq qismlar.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CHIQISH="${TMPDIR:-/tmp}/kotib-mac-sinov"

# Monocypher — C kutubxonasi, C sifatida alohida kompilyatsiya qilinadi:
# quyidagi `-include windows.h` qalqoni faqat C++ fayllari uchun.
MONO="$ROOT/win/third_party/monocypher"
clang -std=c99 -O2 -c "$MONO/monocypher.c" -o "$CHIQISH-monocypher.o"
clang -std=c99 -O2 -c "$MONO/monocypher-ed25519.c" -o "$CHIQISH-monocypher-ed25519.o"
# Ovozni boʻlaklash (S12) — sof C, macOS nusxasi bilan bayt-bayt bir xil.
clang -std=c99 -O2 -Wall -Wextra -c "$ROOT/win/core/nutq_bolaklari.c" -o "$CHIQISH-nutq.o"

clang++ -std=c++20 -O1 -Wall -Wextra \
    -I"$ROOT/win/tests/mac" \
    -include "$ROOT/win/tests/mac/windows.h" \
    -o "$CHIQISH" \
    "$ROOT/win/tests/main.cpp" \
    "$ROOT/win/tests/test_matn_format.cpp" \
    "$ROOT/win/tests/test_vaqt_format.cpp" \
    "$ROOT/win/tests/test_tarjima.cpp" \
    "$ROOT/win/tests/test_json.cpp" \
    "$ROOT/win/tests/test_llm.cpp" \
    "$ROOT/win/core/llm_sof.cpp" \
    "$ROOT/win/tests/test_yangilanish.cpp" \
    "$ROOT/win/core/versiya.cpp" \
    "$ROOT/win/core/yangilanish_siyosat.cpp" \
    "$ROOT/win/core/imzo.cpp" \
    "$CHIQISH-monocypher.o" "$CHIQISH-monocypher-ed25519.o" \
    "$ROOT/win/tests/test_nutq.cpp" "$CHIQISH-nutq.o" \
    "$ROOT/win/core/json.cpp" \
    "$ROOT/win/tests/mac/util_shim.cpp" \
    "$ROOT/win/core/matn_format.cpp" \
    "$ROOT/win/core/vaqt_format.cpp" \
    "$ROOT/win/core/matn_boluvchi.cpp" \
    "$ROOT/win/core/tillar.cpp" \
    "$ROOT/win/tests/test_saqlanmagan.cpp" \
    "$ROOT/win/tests/test_sozlama.cpp" \
    "$ROOT/win/core/saqlanmagan.cpp"

"$CHIQISH"
