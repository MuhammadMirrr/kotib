#!/bin/bash
# Kotib modeli: f16 / q8_0 / q5 / q4 variantlarini bir xil audio ustida solishtiradi.
#
#   ./run.sh <audio_papka> [natija_nomi]
#
# <audio_papka> ichidagi (va bir daraja ichki papkalardagi) .ogg .oga .opus .m4a
# .mp3 .wav .mp4 .mov .flac .aac fayllar olinadi. Ichki papka nomi = guruh
# (hisobotda alohida jami). Fayl yonida <nom>.ref.txt boʻlsa — inson yozgan
# toʻgʻri matn sifatida haqiqiy WER/CER hisoblanadi.
#
# Muhit oʻzgaruvchilari:
#   VARIANTLAR="f16 q8_0 q5_k q5_0 q4_k q4_1 q4_0"   (birinchisi — etalon, f16 boʻlishi shart)
#   TAKROR=1      f16 ni ikkinchi marta ham ishga tushiradi (f16_takror) — shovqin
#                 darajasini oʻlchash uchun: u 0 boʻlmasa, farqlar tasodifiy ham boʻlishi mumkin
#   GPU=0         Metal oʻrniga faqat CPU (natija papkasiga _cpu qoʻshiladi)
#
# Natija: natijalar/<natija_nomi>/<variant>/<nom>.txt + hisobot.md
set -euo pipefail
W="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$W/../.." && pwd)"
# Silero VAD — ilovadagi kabi boʻlaklash (S12). VAD=0 bilan — boʻlaksiz (1.1 yoʻli).
# shellcheck source=../bogliqliklar.sh
source "$ROOT/scripts/bogliqliklar.sh"
VAD_ARG=()
if [ "${VAD:-1}" != 0 ]; then
    vad_tayyorla "$ROOT/vad/$VAD_NOM"
    VAD_ARG=(--vad "$ROOT/vad/$VAD_NOM")
fi
# Katta maʼlumot (modellar, natijalar, yigʻilgan binarlar) repodan tashqarida:
# standart — repo ildizidagi gitignore'dagi `.stt-baho/`.
ISH="${STT_ISH:-$ROOT/.stt-baho}"
MODELLAR="$ISH/models"
AUDIO="${1:?foydalanish: ./run.sh <audio_papka> [natija_nomi]}"
AUDIO="$(cd "$AUDIO" && pwd)"
NOM="${2:-$(basename "$AUDIO")}"
VARIANTLAR="${VARIANTLAR:-f16 q8_0 q5_k q5_0 q4_k q4_1 q4_0}"
TAKROR="${TAKROR:-1}"
GPU="${GPU:-1}"

BIN="$ISH/harness/bin/kotib-sinov"; SUF=""
if [ "$GPU" = "0" ]; then BIN="$ISH/harness/bin/kotib-sinov-cpu"; SUF="_cpu"; fi
# Manba (src/ yoki sinov dasturi) oʻzgargan boʻlsa — qayta yigʻiladi: eski
# binar bilan oʻlchash ilovaning hozirgi xulqini aks ettirmaydi.
qayta=0
[ -x "$BIN" ] || qayta=1
for f in "$ROOT/src/whisper_bridge.c" "$ROOT/src/media_decode.swift" "$ROOT/src/audio_util.swift" \
         "$ROOT/src/text_format.swift" "$W/harness/main.swift"; do
    [ "$f" -nt "$BIN" ] && qayta=1
done
[ "$qayta" = 0 ] || STT_ISH="$ISH" nice -n 10 "$W/harness/build.sh"

NAT="$ISH/natijalar/$NOM"
mkdir -p "$NAT"

# Fayllar roʻyxati: nom \t guruh \t yoʻl
FAYLLAR=()
: > "$NAT/fayllar.tsv"
while IFS= read -r -d '' f; do
    rel="${f#"$AUDIO"/}"
    guruh="$(dirname "$rel")"; [ "$guruh" = "." ] && guruh="-"
    b="$(basename "$f")"; b="${b%.*}"
    if cut -f1 "$NAT/fayllar.tsv" | grep -qxF "$b"; then
        echo "XATO: bir xil nomli ikki fayl: $b" >&2; exit 1
    fi
    printf '%s\t%s\t%s\n' "$b" "$guruh" "$f" >> "$NAT/fayllar.tsv"
    FAYLLAR+=("$f")
done < <(find -L "$AUDIO" -maxdepth 2 -type f \( -iname '*.ogg' -o -iname '*.oga' -o -iname '*.opus' \
            -o -iname '*.m4a' -o -iname '*.mp3' -o -iname '*.wav' -o -iname '*.mp4' -o -iname '*.mov' \
            -o -iname '*.flac' -o -iname '*.aac' \) -print0 | sort -z)
[ ${#FAYLLAR[@]} -gt 0 ] || { echo "XATO: $AUDIO da audio topilmadi" >&2; exit 1; }
echo "==> ${#FAYLLAR[@]} ta fayl, natija: $NAT"

ishga() {   # $1 = variant nomi (papka), $2 = model fayli
    local v="$1" m="$2" out="$NAT/$1$SUF"
    [ -f "$m" ] || { echo "XATO: model yoʻq: $m" >&2; exit 1; }
    rm -rf "$out"; mkdir -p "$out"
    echo "==> $v$SUF ($(basename "$m"))"
    # Bir vaqtda faqat BITTA transkripsiya — ketma-ket.
    /usr/bin/time -l "$BIN" --model "$m" ${VAD_ARG[@]+"${VAD_ARG[@]}"} --out "$out" "${FAYLLAR[@]}" \
        > "$out/stdout.log" 2> "$out/stderr.log" || { echo "XATO: $v (qarang $out/stderr.log)" >&2; exit 1; }
    tail -1 "$out/stdout.log"
}

for v in $VARIANTLAR; do
    ishga "$v" "$MODELLAR/ggml-rubaistt-$v.bin"
    if [ "$v" = "f16" ] && [ "$TAKROR" = "1" ]; then
        ishga "f16_takror" "$MODELLAR/ggml-rubaistt-f16.bin"
    fi
done

# Etalon: f16 (shu ishga tushirishda boʻlsa), aks holda roʻyxatdagi birinchi variant.
ETALON="f16$SUF"
# shellcheck disable=SC2086  # $VARIANTLAR — boʻshliq bilan ajratilgan roʻyxat, ataylab boʻlinadi
[ -f "$NAT/$ETALON/meta.json" ] || ETALON="$(set -- $VARIANTLAR; echo "$1")$SUF"
python3 "$W/score.py" "$NAT" --models "$MODELLAR" --suffix "$SUF" --etalon "$ETALON"
echo "==> Hisobot: $NAT/hisobot$SUF.md"
