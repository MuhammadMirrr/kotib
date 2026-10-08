#!/usr/bin/env bash
#
# whisper inference parametrlari ikkala platformada bir xilmi.
#
#   ./win/tests/mac/parametr-tekshir.sh
#
# `src/whisper_bridge.c` va `win/core/whisper_bridge.c` — bir xil model
# ustidagi bir xil chaqiruv. Parametrlardan bittasi ogʻib ketsa (masalan
# `beam_size` yoki `no_speech_thold`), ikkala platforma bir xil ovozdan
# BOSHQA matn beradi va bu «ikkita boshqa ilova» degani.
#
# Fayllarning oʻzi bayt-bayt bir xil EMAS: Windows tomoni UTF-16 yoʻl bilan
# yuklaydi (`rubai_load_w`, VAD ham). Shuning uchun:
#   • `p.<maydon>` (whisper) va `vp.<maydon>` (VAD, S12) satrlari solishtiriladi;
#   • «Ovozni boʻlaklash» boʻlimi (VAD parametrlari, boʻlaklash, ikkala
#     transkripsiya funksiyasi, segment xotirasi) — SOʻZMA-SOʻZ: u yerdagi har
#     qanday farq ikki platformada boshqa matn degani;
#   • `nutq_bolaklari.{c,h}` — bayt-bayt.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ISH="${TMPDIR:-/tmp}/kotib-parametr"
mkdir -p "$ISH"

# Berilgan funksiyadan parametr satrlarini ajratadi.
ajrat() {
    local fayl="$1" funksiya="$2"
    # `grep` hech narsa topmasa 1 qaytaradi va `pipefail` butun skriptni
    # toʻxtatardi — shuning uchun `|| true`.
    # BSD sed (macOS) BRE'da `\|` alternatsiyasini tushunmaydi — `-E` kerak.
    sed -nE "/^[^ ].*[ *]${funksiya}\\(/,/^\\}/p" "$fayl" \
        | { grep -E '^[[:space:]]+v?p\.' || true; } \
        | sed -E 's|//.*$||' \
        | sed -E 's/[[:space:]]+/ /g; s/^ //; s/ $//'
}

xato=0
for f in rubai_transcribe rubai_transcribe_segments vad_parametrlari; do
    ajrat "$ROOT/src/whisper_bridge.c"      "$f" > "$ISH/mac-$f.txt"
    ajrat "$ROOT/win/core/whisper_bridge.c" "$f" > "$ISH/win-$f.txt"

    if [ ! -s "$ISH/mac-$f.txt" ] || [ ! -s "$ISH/win-$f.txt" ]; then
        echo "✗ $f: parametrlar topilmadi (funksiya nomi oʻzgardimi?)" >&2
        xato=1
        continue
    fi
    if diff -u "$ISH/mac-$f.txt" "$ISH/win-$f.txt"; then
        echo "✓ $f: parametrlar bir xil ($(wc -l < "$ISH/mac-$f.txt" | tr -d ' ') ta)"
    else
        echo "✗ $f: parametrlar farq qiladi" >&2
        xato=1
    fi
done

# «Ovozni boʻlaklash» boʻlimi — sarlavhadan `rubai_free_str` gacha, soʻzma-soʻz.
bolim() { sed -n '/^\/\/ ---- Ovozni boʻlaklash/,/^void rubai_free_str/p' "$1"; }
bolim "$ROOT/src/whisper_bridge.c" > "$ISH/mac-bolim.txt"
bolim "$ROOT/win/core/whisper_bridge.c" > "$ISH/win-bolim.txt"
if [ ! -s "$ISH/mac-bolim.txt" ]; then
    echo "✗ «Ovozni boʻlaklash» boʻlimi topilmadi" >&2
    xato=1
elif diff -u "$ISH/mac-bolim.txt" "$ISH/win-bolim.txt"; then
    echo "✓ ovozni boʻlaklash: ikkala bridge'da soʻzma-soʻz bir xil ($(wc -l < "$ISH/mac-bolim.txt" | tr -d ' ') qator)"
else
    echo "✗ ovozni boʻlaklash: bridge'lar farq qiladi" >&2
    xato=1
fi
for f in nutq_bolaklari.c nutq_bolaklari.h; do
    if cmp -s "$ROOT/src/$f" "$ROOT/win/core/$f"; then
        echo "✓ $f: bayt-bayt bir xil"
    else
        echo "✗ $f: src/ va win/core/ nusxalari farq qiladi" >&2
        xato=1
    fi
done

exit "$xato"
