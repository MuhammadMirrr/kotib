"""
rubaiSTT modelining aniqligini o'lchaydi (WER — Word Error Rate).

Google FLEURS uz_uz datasetidagi haqiqiy o'zbek nutqi va uning to'g'ri matnidan
foydalanadi. Backendlarni (Vulkan / CUDA / CPU) tezlik va aniqlik bo'yicha solishtiradi.

Ishlatish:
    python bench_accuracy.py --cli <whisper-cli.exe> --model <ggml-rubaistt.bin> [-n 20]
"""

import argparse
import os
import re
import subprocess
import sys
import time
import unicodedata

# O'zbek lotin alifbosidagi apostrof variantlari — WER'ni adolatli hisoblash uchun
# hammasini bitta belgiga keltiramiz.
APOSTROPHES = "\u02bb\u02bc\u2018\u2019\u00b4\u0060'"


def normalize(text):
    """Taqqoslash uchun matnni normallashtiradi."""
    text = unicodedata.normalize("NFC", text).lower()
    for ch in APOSTROPHES:
        text = text.replace(ch, "'")
    text = re.sub(r"[^\w\s']", " ", text, flags=re.UNICODE)
    return re.sub(r"\s+", " ", text).strip()


def wer(ref, hyp):
    """Levenshtein masofasi so'zlar darajasida -> (xatolar, jami so'z)."""
    r, h = ref.split(), hyp.split()
    if not r:
        return (len(h), 0)
    prev = list(range(len(h) + 1))
    for i, rw in enumerate(r, 1):
        cur = [i]
        for j, hw in enumerate(h, 1):
            cur.append(min(prev[j] + 1,          # o'chirish
                           cur[j - 1] + 1,       # qo'shish
                           prev[j - 1] + (rw != hw)))  # almashtirish
        prev = cur
    return (prev[-1], len(r))


def load_refs(tsv_path):
    """FLEURS tsv -> {fayl nomi: normallashtirilgan matn}."""
    refs = {}
    with open(tsv_path, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 4:
                refs[parts[1]] = parts[3]   # 4-ustun: normallashtirilgan matn
    return refs


def run_batch(cli, model, wavs, extra_args, label):
    """whisper-cli ni bir marta ishga tushirib bir nechta faylni qayta ishlaydi."""
    for w in wavs:
        txt = w + ".txt"
        if os.path.exists(txt):
            os.unlink(txt)

    cmd = [cli, "-m", model, "-l", "uz", "-bs", "5", "-nt", "-np", "-otxt"] + extra_args + wavs
    t0 = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    elapsed = time.time() - t0

    if p.returncode != 0:
        print(f"  {label}: XATO\n{(p.stderr or '')[-800:]}")
        return None

    out = {}
    for w in wavs:
        txt = w + ".txt"
        if os.path.exists(txt):
            with open(txt, encoding="utf-8", errors="replace") as f:
                out[os.path.basename(w)] = f.read().strip()
            os.unlink(txt)
    return out, elapsed


def wav_duration(path):
    """WAV davomiyligi (soniya). RIFF chunk'larini qo'lda o'qiydi.

    Python'ning `wave` moduli faqat PCM (format 1) ni qo'llab-quvvatlaydi,
    FLEURS fayllari esa IEEE float (format 3) — shuning uchun o'zimiz o'qiymiz.
    """
    import struct
    with open(path, "rb") as f:
        if f.read(4) != b"RIFF":
            return 0.0
        f.read(4)
        if f.read(4) != b"WAVE":
            return 0.0
        byte_rate = 0
        while True:
            hdr = f.read(8)
            if len(hdr) < 8:
                return 0.0
            cid, size = struct.unpack("<4sI", hdr)
            body = f.read(size)
            if cid == b"fmt " and len(body) >= 16:
                byte_rate = struct.unpack("<I", body[8:12])[0]
            elif cid == b"data":
                return size / byte_rate if byte_rate else 0.0
            if size % 2:
                f.read(1)   # chunk'lar juft baytga tekislanadi


def audio_seconds(wavs):
    total = 0.0
    for w in wavs:
        try:
            total += wav_duration(w)
        except Exception:
            pass
    return total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--tsv", default=r"D:\rubai-spike\fleurs\dev.tsv")
    ap.add_argument("--audio-dir", default=r"D:\rubai-spike\fleurs\dev\dev")
    ap.add_argument("-n", type=int, default=20, help="nechta namuna")
    ap.add_argument("--cpu", action="store_true", help="GPU'ni o'chirib CPU'da ishlatish")
    ap.add_argument("--label", default="")
    args = ap.parse_args()

    refs = load_refs(args.tsv)
    if not refs:
        print(f"XATO: {args.tsv} dan matnlar o'qilmadi")
        sys.exit(1)

    wavs = []
    for name in sorted(refs):
        p = os.path.join(args.audio_dir, name)
        if os.path.isfile(p):
            wavs.append(p)
        if len(wavs) >= args.n:
            break

    if not wavs:
        print(f"XATO: {args.audio_dir} da wav topilmadi")
        sys.exit(1)

    secs = audio_seconds(wavs)
    label = args.label or ("CPU" if args.cpu else "GPU")
    print(f"\n=== {label} ===")
    print(f"{len(wavs)} namuna, jami {secs:.1f}s audio")

    extra = ["-ng"] if args.cpu else []
    res = run_batch(args.cli, args.model, wavs, extra, label)
    if res is None:
        sys.exit(1)
    hyps, elapsed = res

    tot_err = tot_words = 0
    rows = []
    for w in wavs:
        name = os.path.basename(w)
        ref = normalize(refs[name])
        hyp = normalize(hyps.get(name, ""))
        e, n = wer(ref, hyp)
        tot_err += e
        tot_words += n
        rows.append((e / n if n else 1.0, name, ref, hyp))

    overall = tot_err / tot_words if tot_words else 1.0
    rtf = elapsed / secs if secs else 0

    print(f"\nWER (xatolik):   {overall * 100:.1f}%   -> aniqlik ~{(1 - overall) * 100:.1f}%")
    print(f"Vaqt:            {elapsed:.1f}s  ({rtf:.2f}x realtime, kichik = tez)")
    print(f"So'zlar:         {tot_words}, xatolar: {tot_err}")

    rows.sort()
    print("\n--- Eng yaxshi 2 ---")
    for r, name, ref, hyp in rows[:2]:
        print(f"  WER {r*100:5.1f}%  kutilgan: {ref[:90]}")
        print(f"                natija:   {hyp[:90]}")
    print("\n--- Eng yomon 2 ---")
    for r, name, ref, hyp in rows[-2:]:
        print(f"  WER {r*100:5.1f}%  kutilgan: {ref[:90]}")
        print(f"                natija:   {hyp[:90]}")

    return overall, rtf


if __name__ == "__main__":
    main()
