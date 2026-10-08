#!/bin/bash
# Sof mantiq funksiyalari uchun testlar. AppKit/whisper'ga bog'liq kod
# bu yerda sinalmaydi — u qo'lda tekshiriladi (spec, 16-band).
#
# Oxirida versiya muvofiqligi ham tekshiriladi (`scripts/versiya-tekshir.sh`):
# u ham sekundlar oladi va unutilgan versiya raqami birinchi shu yerda ushlansin.
set -euo pipefail
SRC="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SRC/.." && pwd)"
OUT="$ROOT/tests/.build"
mkdir -p "$OUT"

# Sinaladigan manbalar — FAQAT sof (Foundation'dan boshqa importsiz) fayllar.
UNDER_TEST=()
[ -f "$SRC/text_format.swift" ] && UNDER_TEST+=("$SRC/text_format.swift")
[ -f "$SRC/audio_util.swift" ]  && UNDER_TEST+=("$SRC/audio_util.swift")
[ -f "$SRC/llm_client.swift" ]  && UNDER_TEST+=("$SRC/llm_client.swift")
[ -f "$SRC/diktovka_tarixi.swift" ] && UNDER_TEST+=("$SRC/diktovka_tarixi.swift")
[ -f "$SRC/yollar.swift" ]      && UNDER_TEST+=("$SRC/yollar.swift")
[ -f "$SRC/vaqt_format.swift" ] && UNDER_TEST+=("$SRC/vaqt_format.swift")
[ -f "$SRC/matn_boluvchi.swift" ] && UNDER_TEST+=("$SRC/matn_boluvchi.swift")
[ -f "$SRC/tillar.swift" ]       && UNDER_TEST+=("$SRC/tillar.swift")
[ -f "$SRC/tarjima_model.swift" ] && UNDER_TEST+=("$SRC/tarjima_model.swift")
[ -f "$SRC/model_tanlov.swift" ] && UNDER_TEST+=("$SRC/model_tanlov.swift")
[ -f "$SRC/log_siyosati.swift" ] && UNDER_TEST+=("$SRC/log_siyosati.swift")
[ -f "$SRC/saqlanmagan.swift" ]  && UNDER_TEST+=("$SRC/saqlanmagan.swift")
# yangilanish.swift EMAS: u Metal import qiladi (GPU nomi uchun). Uning sof
# mantigʻi — versiya taqqoslash, siyosat, manifest — alohida faylda.
# imzo.swift CryptoKit'ni import qiladi: bu ham sof hisob (fayl/tarmoq yoʻq).
[ -f "$SRC/yangilanish_siyosat.swift" ] && UNDER_TEST+=("$SRC/yangilanish_siyosat.swift")
[ -f "$SRC/imzo.swift" ]        && UNDER_TEST+=("$SRC/imzo.swift")

swiftc -O "$ROOT"/tests/main.swift "$ROOT"/tests/test_*.swift "${UNDER_TEST[@]}" -o "$OUT/testlar"
"$OUT/testlar"

"$ROOT/scripts/versiya-tekshir.sh"
