#!/usr/bin/env bash
#
# Google FLEURS oʻzbek TEST toʻplamini oʻlchovga tayyorlaydi:
# har noyob gapdan bitta yozuv (345 ta, ~70 daqiqa) va yonida odam yozgan
# toʻgʻri matn — `<nom>.ref.txt`. `run.sh` shu matn bilan haqiqiy WER beradi.
#
#   ./scripts/stt-baho/fleurs-tayyorla.sh
#   ./scripts/stt-baho/run.sh "$STT_ISH/audio/fleurs-uz" fleurs
#
# Manba: huggingface.co/datasets/google/fleurs (ochiq, gated emas), data/uz_uz.
# Yuklash ~520 MB, bir marta; keyingi ishga tushirishda qayta yuklanmaydi.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISH="${STT_ISH:-$ROOT/.stt-baho}"
MANBA="$ISH/fleurs-uz-manba"
CHIQISH="$ISH/audio/fleurs-uz"
HF="https://huggingface.co/datasets/google/fleurs/resolve/main/data/uz_uz"

mkdir -p "$MANBA" "$CHIQISH"
[ -s "$MANBA/test.tsv" ] || curl -fsSL -o "$MANBA/test.tsv" "$HF/test.tsv"
if [ ! -d "$MANBA/test" ]; then
    [ -s "$MANBA/test.tar.gz" ] || curl -fL --retry 3 -o "$MANBA/test.tar.gz" "$HF/audio/test.tar.gz"
    tar -xzf "$MANBA/test.tar.gz" -C "$MANBA"
fi

# TSV ustunlari: id, fayl, xom matn, normallangan matn, belgilar, namunalar, jins.
# Bir gap bir necha soʻzlovchi tomonidan oʻqilgan — har gapdan BIRINCHI yozuv
# olinadi (gaplar takrorlanmasin). Matn — xom (bosh harf va tinish bilan);
# score.py ikkala tomonni bir xil normallaydi.
python3 - "$MANBA" "$CHIQISH" <<'PY'
import csv, os, sys
manba, chiqish = sys.argv[1], sys.argv[2]
rows = csv.reader(open(os.path.join(manba, "test.tsv"), encoding="utf-8"),
                  delimiter="\t", quoting=csv.QUOTE_NONE)
korilgan, n, soniya = set(), 0, 0.0
for r in rows:
    sid, fayl, xom, namuna = r[0], r[1], r[2], int(r[5])
    src = os.path.join(manba, "test", fayl)
    if sid in korilgan or not os.path.exists(src):
        continue
    korilgan.add(sid)
    nom = f"fleurs_{sid}"
    dst = os.path.join(chiqish, nom + ".wav")
    if not os.path.lexists(dst):
        os.symlink(os.path.abspath(src), dst)
    with open(os.path.join(chiqish, nom + ".ref.txt"), "w", encoding="utf-8") as f:
        f.write(xom.strip() + "\n")
    soniya += namuna / 16000
    n += 1
print(f"{n} ta audio, jami {soniya / 60:.1f} daqiqa → {chiqish}")
PY
