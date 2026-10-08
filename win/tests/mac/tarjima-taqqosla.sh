#!/usr/bin/env bash
#
# macOS va Windows tarjima koʻpriklarini YONMA-YON solishtiradi.
#
#   ./win/tests/mac/tarjima-taqqosla.sh
#
# `src/tarjima_bridge.cpp` va `win/core/tarjima_bridge.cpp` — bitta fayl,
# yagona farqi Windows'dagi yoʻl tayyorlash (`yolniTayyorla`). Tokenlar
# tartibi, beam, uzunlik chegarasi va takror taqigʻi ogʻib ketsa, ikkala
# platforma bir xil matnga BOSHQA tarjima beradi va buni sezish qiyin —
# natija baribir «tarjimaga oʻxshaydi».
#
# Bu yerda ikkalasi ham macOS'da yigʻiladi va bir xil model bilan bir xil
# jumlalarni tarjima qiladi. Chiqish bayt-bayt bir xil boʻlishi shart.
#
# Talab: `./setup.sh` (CTranslate2 va SentencePiece statik kutubxonalari) va
# yuklab olingan tarjima modeli.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MODEL="$HOME/Library/Application Support/Kotib/tarjima-model-33b"
ISH="${TMPDIR:-/tmp}/kotib-tarjima-taqqoslash"
mkdir -p "$ISH"

[ -d "$MODEL" ] || { echo "Tarjima modeli topilmadi: $MODEL" >&2; exit 1; }

JUMLALAR=(
    "Bugun havo juda yaxshi."
    "Ertaga soat oʻnda yigʻilish bor, hujjatlarni tayyorlab qoʻying."
    "Mustaqillik - bu erkinlik."
    "Prof. Azamov aytdiki, ish tugadi."
    "1 500 000 soʻm toʻlandi."
)

cat > "$ISH/main.cpp" <<'CPP'
#include "tarjima_bridge.h"
#include <cstdio>
int main(int argc, char** argv) {
    if (argc < 4) return 2;
    if (rubai_tarjima_yukla(argv[1]) != 0) { fprintf(stderr, "model yuklanmadi\n"); return 1; }
    for (int i = 4; i < argc; i++) {
        char* p = rubai_tarjima(argv[i], argv[2], argv[3]);
        printf("%s\n", p ? p : "(NULL)");
        if (p) rubai_tarjima_str_bosat(p);
    }
    rubai_tarjima_bosat();
    return 0;
}
CPP

QOSHIMCHA=()
for k in "$ROOT/ctranslate2/build-static/third_party/ruy/ruy/"*.a \
         "$ROOT/ctranslate2/build-static/third_party/ruy/third_party/cpuinfo/"*.a \
         "$ROOT/ctranslate2/build-static/third_party/ruy/third_party/cpuinfo/deps/clog/"*.a \
         "$ROOT/ctranslate2/build-static/third_party/cpu_features/"*.a \
         "$ROOT/sentencepiece/build-static/third_party/absl/flags/"*.a; do
    [ -f "$k" ] && QOSHIMCHA+=("$k")
done

qur() {
    local nom="$1" koprik="$2" qoshimcha_bayroq="${3:-}"
    # shellcheck disable=SC2086
    clang++ -std=c++17 -O2 $qoshimcha_bayroq \
        -I"$(dirname "$koprik")" -I"$ROOT/win/tests/mac" \
        -I"$ROOT/ctranslate2/include" -I"$ROOT/sentencepiece/src" \
        -o "$ISH/$nom" "$ISH/main.cpp" "$koprik" \
        "$ROOT/ctranslate2/build-static/libctranslate2.a" \
        "$ROOT/sentencepiece/build-static/src/libsentencepiece.a" \
        "${QOSHIMCHA[@]}" \
        -framework Accelerate -lpthread 2>&1 | grep -E '^.*error:' || true
}

qur mac-koprik "$ROOT/src/tarjima_bridge.cpp"
qur win-koprik "$ROOT/win/core/tarjima_bridge.cpp" "-include $ROOT/win/tests/mac/windows.h"

"$ISH/mac-koprik" "$MODEL" uzn_Latn rus_Cyrl "${JUMLALAR[@]}" > "$ISH/mac.out"
"$ISH/win-koprik" "$MODEL" uzn_Latn rus_Cyrl "${JUMLALAR[@]}" > "$ISH/win.out"

if diff -u "$ISH/mac.out" "$ISH/win.out"; then
    echo "✓ tarjima koʻprigi: macOS va Windows bir xil"
    echo "--- natija ---"
    cat "$ISH/mac.out"
else
    echo "✗ tarjima koʻprigi: farq bor" >&2
    exit 1
fi
