#!/usr/bin/env bash
#
# Ilova bilan tarqatiladigan `Litsenziyalar.txt` ni yasaydi: Kotib litsenziyasi,
# `THIRD_PARTY_NOTICES.md` (jadval) va har bir statik ulangan / birga
# tarqatiladigan komponentning TOʻLIQ litsenziya matni — pinlangan manbalarning
# oʻz LICENSE fayllaridan. Matnlar qoʻlda koʻchirilmaydi: bogʻliqlik yangilansa,
# fayl ham oʻzi yangilanadi.
#
#   ./scripts/litsenziyalar.sh <chiqish-fayli>
#
# `src/build.sh` (→ Contents/Resources) va `win/build-mac.sh` (→ exe yonida)
# chaqiradi. Manba yetishmasa — xato: litsenziyasiz binar tarqatilmasin.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CHIQISH="${1:?Ishlatish: $0 <chiqish-fayli>}"

# Sarlavha va fayl — tartib bilan. Yoʻl ROOT ga nisbatan.
BOLIMLAR=(
    "Kotib|LICENSE"
    "whisper.cpp / ggml|whisper.cpp/LICENSE"
    "CTranslate2|ctranslate2/LICENSE"
    "ruy|ctranslate2/third_party/ruy/LICENSE"
    "cpuinfo|ctranslate2/third_party/ruy/third_party/cpuinfo/LICENSE"
    "cpu_features|ctranslate2/third_party/cpu_features/LICENSE"
    "spdlog|ctranslate2/third_party/spdlog/LICENSE"
    "SentencePiece|sentencepiece/LICENSE"
    "Abseil|sentencepiece/third_party/absl/LICENSE"
    "protobuf-lite|sentencepiece/third_party/protobuf-lite/LICENSE"
    "darts-clone|sentencepiece/third_party/darts_clone/LICENSE"
    "esaxx|sentencepiece/third_party/esaxx/LICENSE"
    "Monocypher|win/third_party/monocypher/LICENCE.md"
)

for b in "${BOLIMLAR[@]}"; do
    f="$ROOT/${b#*|}"
    [ -f "$f" ] || { echo "Xato: litsenziya fayli yoʻq: ${b#*|} (avval ./setup.sh)" >&2; exit 1; }
done

{
    echo "KOTIB — LITSENZIYALAR / LICENSES"
    echo "================================"
    echo
    cat "$ROOT/THIRD_PARTY_NOTICES.md"
    for b in "${BOLIMLAR[@]}"; do
        echo
        echo
        echo "────────────────────────────────────────────────────────────────"
        echo "${b%%|*}"
        echo "────────────────────────────────────────────────────────────────"
        echo
        cat "$ROOT/${b#*|}"
    done
    # Litsenziyasi alohida faylda emas, manba sarlavhasida turganlar.
    echo
    echo
    echo "────────────────────────────────────────────────────────────────"
    echo "BS::thread_pool (CTranslate2 ichida)"
    echo "────────────────────────────────────────────────────────────────"
    echo
    grep -m1 -i "@copyright" "$ROOT/ctranslate2/third_party/BS_thread_pool.hpp" | sed 's/^[ *]*//'
    echo
    echo "────────────────────────────────────────────────────────────────"
    echo "avx_mathfun / neon_mathfun (CTranslate2 ichida)"
    echo "────────────────────────────────────────────────────────────────"
    echo
    sed -n '1,/\*\//p' "$ROOT/ctranslate2/third_party/avx_mathfun.h"
    echo
    echo "────────────────────────────────────────────────────────────────"
    echo "LLVM libc++ / libunwind / OpenMP, mingw-w64 (faqat Windows)"
    echo "────────────────────────────────────────────────────────────────"
    echo
    echo "Apache License v2.0 with LLVM Exceptions — https://llvm.org/LICENSE.txt"
    echo "mingw-w64 runtime — https://sourceforge.net/p/mingw-w64/mingw-w64/ci/master/tree/COPYING"
    echo
    echo "────────────────────────────────────────────────────────────────"
    echo "Modellar / Models"
    echo "────────────────────────────────────────────────────────────────"
    echo
    echo "rubaiSTT v2 medium — islomov — Apache License 2.0"
    echo "  https://huggingface.co/islomov/rubaistt_v2_medium"
    echo "  https://www.apache.org/licenses/LICENSE-2.0"
    echo "OpenAI Whisper — MIT — https://github.com/openai/whisper/blob/main/LICENSE"
    echo "NLLB-200 — Meta AI — Creative Commons Attribution-NonCommercial 4.0"
    echo "  https://huggingface.co/facebook/nllb-200-3.3B"
    echo "  https://creativecommons.org/licenses/by-nc/4.0/legalcode"
    echo
    echo "────────────────────────────────────────────────────────────────"
    echo "Silero VAD (ggml-silero-v6.2.0.bin, ilova bilan birga)"
    echo "  https://github.com/snakers4/silero-vad · https://huggingface.co/ggml-org/whisper-vad"
    echo "────────────────────────────────────────────────────────────────"
    echo
    # MIT matni whisper.cpp LICENSE dagi bilan bir xil — faqat mualliflik qatori boshqa.
    sed 's/^Copyright.*/Copyright (c) 2020-present Silero Team/' "$ROOT/whisper.cpp/LICENSE"
} > "$CHIQISH"
