#!/usr/bin/env bash
#
# macOS tomonining tarjima natijasini chiqaradi — Windows bilan solishtirish
# uchun ETALON.
#
#   ./win/tests/mac/tarjima-etalon.sh "Bugun havo juda yaxshi." [manba] [maqsad]
#
# Windows tomonida xuddi shu matn:
#
#   rubai-cli.exe --tarjima "Bugun havo juda yaxshi." --dan uzn_Latn --ga rus_Cyrl
#
# Ikkalasi AYNAN bir xil chiqishi kerak: koʻprik fayllari (`tarjima_bridge.cpp`)
# bir xil, beam va uzunlik chegaralari bir xil, model bir xil. Farq chiqsa —
# parametr ogʻib ketgan yoki tokenizatsiya tartibi buzilgan.
#
# Talab: macOS tomonida `setup.sh` ishlatilgan boʻlishi (CTranslate2 va
# SentencePiece statik kutubxonalari) va tarjima modeli yuklab olingan
# boʻlishi (`~/Library/Application Support/Kotib/tarjima-model-33b`).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MODEL="$HOME/Library/Application Support/Kotib/tarjima-model-33b"
ISH="${TMPDIR:-/tmp}/kotib-tarjima-etalon"
mkdir -p "$ISH"

MATN="${1:-Bugun havo juda yaxshi. Ertaga yomgʻir yogʻishi mumkin.}"
DAN="${2:-uzn_Latn}"
GA="${3:-rus_Cyrl}"

[ -d "$MODEL" ] || { echo "Tarjima modeli topilmadi: $MODEL" >&2; exit 1; }
[ -f "$ROOT/ctranslate2/build-static/libctranslate2.a" ] || {
    echo "CTranslate2 statik kutubxonasi yoʻq — ./setup.sh ishlating" >&2; exit 1; }

cat > "$ISH/main.cpp" <<'CPP'
// macOS koʻprigi ustidan eng kichik CLI. Windows'dagi
// `rubai-cli --tarjima` bilan bir xil chiqish beradi.
#include "tarjima_bridge.h"

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    if (argc < 5) return 2;
    if (rubai_tarjima_yukla(argv[1]) != 0) {
        fprintf(stderr, "model yuklanmadi\n");
        return 1;
    }
    char* p = rubai_tarjima(argv[2], argv[3], argv[4]);
    if (!p) { fprintf(stderr, "tarjima chiqmadi\n"); return 1; }
    printf("%s\n", p);
    rubai_tarjima_str_bosat(p);
    rubai_tarjima_bosat();
    return 0;
}
CPP

# ruy koʻplab kichik kutubxonaga boʻlinadi — hammasini beramiz (Windows
# CMake'ida ham shunday: `win/CMakeLists.txt` dagi `RUY_LIBS`).
QOSHIMCHA=()
for k in "$ROOT/ctranslate2/build-static/third_party/ruy/ruy/"*.a \
         "$ROOT/ctranslate2/build-static/third_party/ruy/third_party/cpuinfo/"*.a \
         "$ROOT/ctranslate2/build-static/third_party/ruy/third_party/cpuinfo/deps/clog/"*.a \
         "$ROOT/ctranslate2/build-static/third_party/cpu_features/"*.a \
         "$ROOT/sentencepiece/build-static/third_party/absl/flags/"*.a; do
    [ -f "$k" ] && QOSHIMCHA+=("$k")
done

if [ ! -x "$ISH/etalon" ] || [ "$ROOT/src/tarjima_bridge.cpp" -nt "$ISH/etalon" ]; then
    clang++ -std=c++17 -O2 \
        -I"$ROOT/src" -I"$ROOT/ctranslate2/include" -I"$ROOT/sentencepiece/src" \
        -o "$ISH/etalon" "$ISH/main.cpp" "$ROOT/src/tarjima_bridge.cpp" \
        "$ROOT/ctranslate2/build-static/libctranslate2.a" \
        "$ROOT/sentencepiece/build-static/src/libsentencepiece.a" \
        "${QOSHIMCHA[@]}" \
        -framework Accelerate -lpthread
fi

# Matn jumlalarga boʻlinmaydi — koʻprik bitta jumlani oladi. Windows
# tomonida ham `--tarjima` shu yoʻldan yuradi (matn boʻluvchi alohida
# `taqqosla.sh` da tekshiriladi).
"$ISH/etalon" "$MODEL" "$MATN" "$DAN" "$GA"
