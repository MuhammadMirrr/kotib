#!/bin/bash
# Sinov CLI ni ilovaning oʻz manbalaridan quradi (src/build.sh dagi arm64 yoʻli bilan bir xil):
#   whisper_bridge.c + nutq_bolaklari.c + media_decode.swift + audio_util.swift + text_format.swift
#   + whisper.cpp/build-static (Metal force_load).
# Ikkita binar: kotib-sinov (GPU, ilovadagi kabi) va kotib-sinov-cpu
# (whisper_bridge.c nusxasi, FAQAT `cp.use_gpu = false` bilan farq qiladi).
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$H/../../.." && pwd)"
SRC="$REPO/src"; WC="$REPO/whisper.cpp"; LIB="$WC/build-static"
# Yigʻilgan binarlar repoda emas — ish papkasida (`run.sh` dagi STT_ISH).
ISH="${STT_ISH:-$REPO/.stt-baho}"
OUT="$ISH/harness/bin"; OBJDIR="$ISH/harness/obj"
mkdir -p "$OUT" "$OBJDIR"
[ -f "$LIB/src/libwhisper.a" ] || { echo "whisper.cpp/build-static yoʻq — avval ./setup.sh" >&2; exit 1; }

clang -c "$SRC/whisper_bridge.c" -arch arm64 -O2 -mmacosx-version-min=13.0 \
    -I"$WC/include" -I"$WC/ggml/include" -o "$OBJDIR/whisper_bridge.o"

clang -c "$SRC/nutq_bolaklari.c" -arch arm64 -O2 -mmacosx-version-min=13.0 \
    -o "$OBJDIR/nutq_bolaklari.o"

# CPU nusxasi: bitta qator farq — tekshiriladi.
sed 's/cp.use_gpu = true; /cp.use_gpu = false;/' "$SRC/whisper_bridge.c" > "$OBJDIR/whisper_bridge_cpu.c"
FARQ=$(diff "$SRC/whisper_bridge.c" "$OBJDIR/whisper_bridge_cpu.c" | grep -c '^[<>]' || true)
[ "$FARQ" = "2" ] || { echo "CPU nusxasida kutilmagan farq ($FARQ qator)" >&2; exit 1; }
clang -c "$OBJDIR/whisper_bridge_cpu.c" -arch arm64 -O2 -mmacosx-version-min=13.0 \
    -I"$SRC" -I"$WC/include" -I"$WC/ggml/include" -o "$OBJDIR/whisper_bridge_cpu.o"

for V in gpu cpu; do
    if [ $V = gpu ]; then OBJ="$OBJDIR/whisper_bridge.o"; EXE="$OUT/kotib-sinov";
    else OBJ="$OBJDIR/whisper_bridge_cpu.o"; EXE="$OUT/kotib-sinov-cpu"; fi
    swiftc -O -target arm64-apple-macos13.0 \
        "$H/main.swift" "$H/rubai_log_stub.swift" \
        "$SRC/media_decode.swift" "$SRC/audio_util.swift" "$SRC/text_format.swift" \
        "$OBJ" "$OBJDIR/nutq_bolaklari.o" \
        -import-objc-header "$H/Bridging.h" -Xcc -I"$SRC" \
        "$LIB/src/libwhisper.a" "$LIB/ggml/src/libggml.a" "$LIB/ggml/src/libggml-base.a" \
        -Xlinker -force_load -Xlinker "$LIB/ggml/src/libggml-cpu.a" \
        -Xlinker -force_load -Xlinker "$LIB/ggml/src/ggml-metal/libggml-metal.a" \
        -framework Foundation -framework Accelerate -framework AVFoundation \
        -framework Metal -framework MetalKit -lc++ \
        -module-cache-path "$OBJDIR/modcache" \
        -o "$EXE"
    echo "qurildi: $EXE"
done
