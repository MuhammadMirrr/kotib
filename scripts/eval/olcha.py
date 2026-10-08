#!/usr/bin/env python3
"""Kotib — tarjima sifatini oʻlchash harness'i.

Model yoki dekodlash sozlamalari oʻzgarganda SHU skript ishga tushiriladi.
Busiz regressiya sezilmay qoladi: chrF++ grammatik tuzatishlarni kam
baholaydi, shuning uchun raqamni real matn bilan birga oʻqish kerak.

Tokenizatsiya `src/tarjima_bridge.cpp` bilan AYNAN bir xil boʻlishi shart:
    [manba_til] + sentencepiece boʻlaklari + "</s>"
maqsad tomonda prefiks — [maqsad_til]. Tartib buzilsa model baribir natija
beradi, lekin sifat JIMGINA tushadi.

Dekodlash sozlamalari ham koʻprik bilan bir xil: beam 4, partiya 1,
max_decoding_length = 2*manba + 30, no_repeat_ngram_size = 4.
"""
import argparse, json, os, time
import ctranslate2, sentencepiece as spm
from sacrebleu.metrics import CHRF

# `src/tarjima_bridge.cpp` dagi qiymatlar bilan bir xil turishi shart.
BEAM = 4
UZUNLIK_KOEFF = 2
UZUNLIK_ZAXIRA = 30
TAKROR_NGRAM = 4


def kodla(sp, matn, til):
    return [til] + sp.EncodeAsPieces(matn) + ["</s>"]


def dekodla(sp, tokenlar):
    return sp.DecodePieces([t for t in tokenlar[1:] if t != "</s>"])


def tarjima(tr, sp, jumlalar, manba, maqsad, nrg):
    """Partiya 1 — ilova ham shunday ishlaydi.

    Partiyani kattalashtirish vasvasasiga berilmang: 3.3B da partiya 16
    RAM'ni 2,65 GB dan 6,00 GB ga koʻtaradi va 8 GB mashinani swap'ga
    tushiradi (spec'dagi oʻlchovga qarang).
    """
    natija = []
    for j in jumlalar:
        tok = kodla(sp, j, manba)
        r = tr.translate_batch(
            [tok],
            beam_size=BEAM,
            target_prefix=[[maqsad]],
            no_repeat_ngram_size=nrg,
            max_decoding_length=len(tok) * UZUNLIK_KOEFF + UZUNLIK_ZAXIRA,
        )[0]
        natija.append(dekodla(sp, r.hypotheses[0]))
    return natija


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--model", required=True, help="CTranslate2 papkasi")
    p.add_argument("--spm", required=True, help="sentencepiece.bpe.model yoʻli")
    p.add_argument("--manba", default="uzn_Latn")
    p.add_argument("--maqsad", default="rus_Cyrl")
    p.add_argument("--soni", type=int, default=200)
    p.add_argument("--nrg", type=int, default=TAKROR_NGRAM)
    p.add_argument("--threads", type=int, default=4)
    p.add_argument("--flores", default="flores200_dataset/devtest")
    p.add_argument("--matn", default=None,
                   help="FLORES oʻrniga oddiy matn fayli — etalonsiz, koʻz bilan tekshirish uchun")
    p.add_argument("--chiqish", default=None)
    a = p.parse_args()

    tr = ctranslate2.Translator(a.model, device="cpu", compute_type="int8",
                                inter_threads=1, intra_threads=a.threads)
    sp = spm.SentencePieceProcessor()
    sp.Load(a.spm)

    if a.matn:
        src = [l.strip() for l in open(a.matn) if l.strip()]
        ref = None
    else:
        src = open(f"{a.flores}/{a.manba}.devtest").read().splitlines()[:a.soni]
        ref = open(f"{a.flores}/{a.maqsad}.devtest").read().splitlines()[:a.soni]

    t0 = time.time()
    gipoteza = tarjima(tr, sp, src, a.manba, a.maqsad, a.nrg)
    ketgan = time.time() - t0

    natija = {
        "model": os.path.basename(a.model.rstrip("/")),
        "juft": f"{a.manba}->{a.maqsad}",
        "nrg": a.nrg,
        "jumla": len(src),
        "soniya": round(ketgan, 1),
        "jumla/s": round(len(src) / ketgan, 2),
    }
    if ref is not None:
        natija["chrf++"] = round(CHRF(word_order=2).corpus_score(gipoteza, [ref]).score, 2)
    print(json.dumps(natija, ensure_ascii=False))

    if a.matn:
        for m, t in zip(src, gipoteza):
            print(f"\nUZ : {m}\nRU : {t}")

    if a.chiqish:
        with open(a.chiqish, "w") as f:
            f.write("\n".join(gipoteza) + "\n")


if __name__ == "__main__":
    main()
