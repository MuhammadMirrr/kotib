#!/usr/bin/env python3
"""Kotib q4 sinovi — WER/CER hisoboti. Faqat standart kutubxona.

  python3 score.py natijalar/<nom> [--models models] [--suffix _cpu]

Etalon: f16 chiqishi (<suffix> bilan, boʻlmasa GPU f16). Har variant uchun
f16 ga nisbatan WER/CER; <nom>.ref.txt (inson matni) audio yonida boʻlsa —
har variantning haqiqiy WER/CER i ham.

Normallash (ikkala tomonga bir xil): kichik harf; ʻ ʼ ' ` ‘ ’ ´ -> bitta '
belgisi; harf/raqam/belgi-modifikator (L*, N*, M*) va ' dan boshqa hamma narsa
boʻshliqqa; boʻshliqlar bittaga. Faqat apostrofdan iborat token tashlanadi.
WER = soʻz darajasidagi Levenshtein / etalon soʻzlar soni (korpus boʻyicha
jamlangan: jami xato / jami soʻz). CER = belgi darajasida, normallangan matn
(yagona boʻshliqlar bilan) ustida.
"""
import argparse, json, os, re, sys, unicodedata
from collections import OrderedDict

APOS = "'ʻʼ‘’`´"


def normal(s):
    s = s.lower()
    for ch in APOS:
        s = s.replace(ch, "'")
    out = []
    for c in s:
        if c == "'" or unicodedata.category(c)[0] in "LNM":
            out.append(c)
        else:
            out.append(" ")
    toks = []
    for t in "".join(out).split():
        # Soʻz boshidagi ' — qoʻshtirnoq oʻrnida ishlatilgan; oxiridagisi ham,
        # agar oldida o/g boʻlmasa (togʻ, bogʻ dagi ʻ saqlanadi).
        t = t.lstrip("'")
        while len(t) > 1 and t.endswith("'") and t[-2] not in "og":
            t = t[:-1]
        if t.strip("'"):
            toks.append(t)
    return toks


def trim(a, b):
    """Umumiy bosh/oxirni olib tashlaydi — DP ni keskin arzonlashtiradi."""
    i = 0
    while i < len(a) and i < len(b) and a[i] == b[i]:
        i += 1
    j = 0
    while j < len(a) - i and j < len(b) - i and a[-1 - j] == b[-1 - j]:
        j += 1
    return a[i:len(a) - j], b[i:len(b) - j], i


def lev(a, b):
    a, b, _ = trim(a, b)
    if not a:
        return len(b)
    if not b:
        return len(a)
    prev = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        cur = [i] + [0] * len(b)
        for j, y in enumerate(b, 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (x != y))
        prev = cur
    return prev[-1]


def align(a, b):
    """Soʻzlar boʻyicha hizalash: [(op, a_qism, b_qism)], op in = ~ - +."""
    n, m = len(a), len(b)
    d = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n + 1):
        d[i][0] = i
    for j in range(m + 1):
        d[0][j] = j
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            d[i][j] = min(d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] != b[j - 1]))
    ops = []
    i, j = n, m
    while i > 0 or j > 0:
        if i > 0 and j > 0 and d[i][j] == d[i - 1][j - 1] + (a[i - 1] != b[j - 1]):
            ops.append(("=" if a[i - 1] == b[j - 1] else "~", i - 1, j - 1)); i -= 1; j -= 1
        elif i > 0 and d[i][j] == d[i - 1][j] + 1:
            ops.append(("-", i - 1, None)); i -= 1
        else:
            ops.append(("+", None, j - 1)); j -= 1
    ops.reverse()
    return ops


def farq_parchalari(a, b, kontekst=3):
    """Farq qiluvchi joylar: (etalon parcha, variant parcha) kontekst bilan."""
    ops = align(a, b)
    parchalar = []
    k = 0
    while k < len(ops):
        if ops[k][0] == "=":
            k += 1
            continue
        s = k
        while k < len(ops) and ops[k][0] != "=":
            k += 1
        e = k
        # kontekst: atrofdagi `kontekst` ta amal; farq qismi [qavs]da, boʻsh tomon [∅]
        lo, hi = max(0, s - kontekst), min(len(ops), e + kontekst)

        def tomon(qaysi):
            skip = "+" if qaysi == "a" else "-"
            soz = (lambda o: a[o[1]]) if qaysi == "a" else (lambda o: b[o[2]])
            out = []
            for idx in range(lo, hi):
                o = ops[idx]
                if idx == s:
                    hudud = [ops[t] for t in range(s, e) if ops[t][0] != skip]
                    out.append("[" + " ".join(soz(x) for x in hudud) + "]" if hudud else "[∅]")
                if s <= idx < e or o[0] == skip:
                    continue
                out.append(soz(o))
            return " ".join(out)
        parchalar.append((tomon("a"), tomon("b"), e - s))
    return parchalar


def oqi(p):
    try:
        with open(p, encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        return None


def time_l(p):
    """/usr/bin/time -l chiqishidan: real s, max RSS bayt, peak footprint bayt."""
    r = {}
    t = oqi(p) or ""
    m = re.search(r"^\s*([\d.]+) real", t, re.M)
    if m: r["real_s"] = float(m.group(1))
    m = re.search(r"^\s*(\d+)\s+maximum resident set size", t, re.M)
    if m: r["rss"] = int(m.group(1))
    m = re.search(r"^\s*(\d+)\s+peak memory footprint", t, re.M)
    if m: r["foot"] = int(m.group(1))
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("natija")
    ap.add_argument("--models", default=None)
    ap.add_argument("--suffix", default="")
    ap.add_argument("--etalon", default=None, help="etalon papka nomi (standart: f16<suffix> yoki f16)")
    a = ap.parse_args()
    N = a.natija.rstrip("/")
    suf = a.suffix

    fayllar = OrderedDict()   # nom -> (guruh, yoʻl)
    with open(os.path.join(N, "fayllar.tsv"), encoding="utf-8") as f:
        for line in f:
            nom, guruh, yol = line.rstrip("\n").split("\t")
            fayllar[nom] = (guruh, yol)

    def bor(v):
        return os.path.isfile(os.path.join(N, v, "meta.json"))

    tartib = ["f16", "f16_takror", "q8_0", "q5_k", "q5_0", "q4_k", "q4_1", "q4_0", "q6_k", "q5_1"]
    if suf:
        variantlar = [v + suf for v in tartib if bor(v + suf)]
        variantlar += sorted(d for d in os.listdir(N) if d.endswith(suf) and bor(d) and d not in variantlar)
    else:
        variantlar = [v for v in tartib if bor(v)]
        variantlar += sorted(d for d in os.listdir(N) if bor(d) and not d.endswith("_cpu") and d not in variantlar)
    etalon = a.etalon or ("f16" + suf if bor("f16" + suf) else "f16")
    if not bor(etalon):
        sys.exit("XATO: etalon (%s) natijasi yoʻq" % etalon)

    def matn(v, nom):
        return oqi(os.path.join(N, v, nom + ".txt"))

    guruhlar = list(OrderedDict((g, 1) for g, _ in fayllar.values()))

    # Vaqt / xotira
    vaqt = {}
    for v in variantlar:
        meta = json.load(open(os.path.join(N, v, "meta.json")))
        tl = time_l(os.path.join(N, v, "stderr.log"))
        per = {}
        for line in (oqi(os.path.join(N, v, "vaqt.tsv")) or "").splitlines()[1:]:
            c = line.split("\t")
            if c[1] != "NA":
                per[c[0]] = (float(c[1]), float(c[3]))
        hajm = None
        if a.models:
            base = v[: -len(suf)] if suf and v.endswith(suf) else v
            base = base.replace("_takror", "")
            mp = os.path.join(a.models, "ggml-rubaistt-%s.bin" % base)
            if os.path.isfile(mp):
                hajm = os.path.getsize(mp)
        vaqt[v] = dict(meta=meta, tl=tl, per=per, hajm=hajm)

    # Sifat: etalonga nisbatan
    sifat = {}   # v -> nom -> (wer_xato, soʻz, cer_xato, belgi, aynan)
    yoq = []
    for v in variantlar:
        if v == etalon:
            continue
        sifat[v] = {}
        for nom in fayllar:
            r, h = matn(etalon, nom), matn(v, nom)
            if r is None or h is None:
                yoq.append((v, nom)); continue
            rw, hw = normal(r), normal(h)
            rc, hc = " ".join(rw), " ".join(hw)
            sifat[v][nom] = (lev(rw, hw), len(rw), lev(list(rc), list(hc)), len(rc), rw == hw)

    # Inson matni (ref.txt)
    refs = {}
    for nom, (g, yol) in fayllar.items():
        p = os.path.join(os.path.dirname(yol), nom + ".ref.txt")
        t = oqi(p)
        if t is not None:
            refs[nom] = normal(t)
    inson = {}
    for v in variantlar:
        inson[v] = {}
        for nom, rw in refs.items():
            h = matn(v, nom)
            if h is None: continue
            hw = normal(h)
            rc, hc = " ".join(rw), " ".join(hw)
            inson[v][nom] = (lev(rw, hw), len(rw), lev(list(rc), list(hc)), len(rc))

    def jam(d, noms):
        we = sum(d[n][0] for n in noms if n in d); wn = sum(d[n][1] for n in noms if n in d)
        ce = sum(d[n][2] for n in noms if n in d); cn = sum(d[n][3] for n in noms if n in d)
        return (100.0 * we / wn if wn else float("nan"), 100.0 * ce / cn if cn else float("nan"), we, wn, ce, cn)

    L = []
    L.append("# Kotib modeli: kvantlash sinovi — `%s`\n" % os.path.basename(N))
    L.append("Etalon: **%s**. Fayllar: %d (%s). Audio jami: %.1f s.\n" % (
        etalon, len(fayllar), ", ".join("%s: %d" % (g, sum(1 for x in fayllar.values() if x[0] == g)) for g in guruhlar),
        vaqt[etalon]["meta"].get("audio_s_total", 0)))
    L.append("Normallash: kichik harf, apostroflar bitta belgiga, tinish belgilari olib tashlangan. "
             "WER/CER korpus boʻyicha jamlangan (jami xato / jami etalon soʻz yoki belgi).\n")

    L.append("## Asosiy jadval\n")
    L.append("| variant | fayl hajmi | yuklash | isitish (1-chaqiruv) | RTF | jami vaqt (real) | max RSS | peak footprint | WER vs %s | CER vs %s | aynan bir xil fayllar |" % (etalon, etalon))
    L.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for v in variantlar:
        t = vaqt[v]
        hajm = "%.1f MB" % (t["hajm"] / 1e6) if t["hajm"] else "?"
        rss = "%.0f MB" % (t["tl"]["rss"] / 1048576) if "rss" in t["tl"] else "?"
        foot = "%.0f MB" % (t["tl"]["foot"] / 1048576) if "foot" in t["tl"] else "?"
        if v == etalon:
            w = c = "— (etalon)"; ay = "—"
        else:
            j = jam(sifat[v], fayllar)
            w = "%.2f%% (%d/%d)" % (j[0], j[2], j[3]); c = "%.2f%% (%d/%d)" % (j[1], j[4], j[5])
            ay = "%d/%d" % (sum(1 for x in sifat[v].values() if x[4]), len(sifat[v]))
        real = "%.1f s" % t["tl"]["real_s"] if "real_s" in t["tl"] else "?"
        L.append("| %s | %s | %.0f ms | %.0f ms | %.3f | %s | %s | %s | %s | %s | %s |" % (
            v, hajm, t["meta"]["load_ms"], t["meta"]["warmup_ms"], t["meta"]["rtf_total"], real, rss, foot, w, c, ay))
    L.append("")
    L.append("RTF = jami transkripsiya vaqti / jami audio (isitish chaqiruvi kirmaydi; dekod kirmaydi). "
             "Jami vaqt (real) / max RSS / peak footprint — `/usr/bin/time -l`, butun jarayon "
             "(yuklash + isitish + barcha fayllarni dekod + transkripsiya).\n")

    if len(guruhlar) > 1:
        L.append("## Guruhlar boʻyicha (WER / CER vs %s)\n" % etalon)
        L.append("| variant | " + " | ".join("%s (%d fayl)" % (g, sum(1 for x in fayllar.values() if x[0] == g)) for g in guruhlar) + " |")
        L.append("|---|" + "---:|" * len(guruhlar))
        for v in sifat:
            row = []
            for g in guruhlar:
                noms = [n for n, x in fayllar.items() if x[0] == g]
                j = jam(sifat[v], noms)
                row.append("%.2f%% / %.2f%%" % (j[0], j[1]))
            L.append("| %s | %s |" % (v, " | ".join(row)))
        L.append("")
        L.append("RTF guruhlar boʻyicha:\n")
        L.append("| variant | " + " | ".join(guruhlar) + " |")
        L.append("|---|" + "---:|" * len(guruhlar))
        for v in variantlar:
            row = []
            for g in guruhlar:
                noms = [n for n, x in fayllar.items() if x[0] == g and n in vaqt[v]["per"]]
                au = sum(vaqt[v]["per"][n][0] for n in noms); tr = sum(vaqt[v]["per"][n][1] for n in noms) / 1000
                row.append("%.3f" % (tr / au) if au else "?")
            L.append("| %s | %s |" % (v, " | ".join(row)))
        L.append("")

    if refs:
        L.append("## Inson matniga nisbatan (haqiqiy WER / CER) — %d fayl\n" % len(refs))
        L.append("| variant | WER | CER |")
        L.append("|---|---:|---:|")
        for v in variantlar:
            j = jam(inson[v], refs)
            L.append("| %s | %.2f%% (%d/%d) | %.2f%% (%d/%d) |" % (v, j[0], j[2], j[3], j[1], j[4], j[5]))
        L.append("")

        # Ahamiyat: har variant − q8_0, juftlangan (bir xil gaplar ustida).
        asos = "q8_0" + suf
        if asos in inson:
            import random
            L.append("## Ahamiyat tekshiruvi: variant − %s (inson matniga nisbatan)\n" % asos)
            L.append("Gap boʻyicha: soʻz xatolari soni %s dagidan koʻp (yomonroq) / kam (yaxshiroq) / teng. "
                     "95%% CI — juftlangan bootstrap, gaplar qaytarib tanlanadi, 1000 marta, urugʻ 12345; "
                     "har namunada korpus WER farqi (jami xato / jami soʻz).\n" % asos)
            L.append("| variant | WER farqi (pp) | 95% CI (pp) | CER farqi (pp) | yomonroq | yaxshiroq | teng |")
            L.append("|---|---:|---:|---:|---:|---:|---:|")
            for v in variantlar:
                if v == asos:
                    continue
                noms = [n for n in refs if n in inson[v] and n in inson[asos]]
                if not noms:
                    continue
                ev = [inson[v][n][0] for n in noms]; eb = [inson[asos][n][0] for n in noms]
                nw = [inson[v][n][1] for n in noms]
                cv = [inson[v][n][2] for n in noms]; cb = [inson[asos][n][2] for n in noms]
                nc = [inson[v][n][3] for n in noms]
                d = 100.0 * (sum(ev) - sum(eb)) / sum(nw)
                dc = 100.0 * (sum(cv) - sum(cb)) / sum(nc)
                yomon = sum(1 for x, y in zip(ev, eb) if x > y)
                yaxshi = sum(1 for x, y in zip(ev, eb) if x < y)
                rng = random.Random(12345)
                k = len(noms); bs = []
                for _ in range(1000):
                    idx = [rng.randrange(k) for _ in range(k)]
                    sw = sum(nw[i] for i in idx)
                    bs.append(100.0 * (sum(ev[i] for i in idx) - sum(eb[i] for i in idx)) / sw if sw else 0.0)
                bs.sort()
                L.append("| %s | %+.2f | [%+.2f, %+.2f] | %+.2f | %d | %d | %d |" % (
                    v, d, bs[24], bs[974], dc, yomon, yaxshi, k - yomon - yaxshi))
            L.append("")
    else:
        L.append("_Inson matni (`<nom>.ref.txt`) topilmadi — haqiqiy WER hisoblanmadi._\n")

    L.append("## Fayllar boʻyicha WER vs %s (%%)\n" % etalon)
    L.append("| fayl | guruh | audio s | soʻz (etalon) | " + " | ".join(sifat.keys()) + " |")
    L.append("|---|---|---:|---:|" + "---:|" * len(sifat))
    for nom, (g, _) in fayllar.items():
        au = vaqt[etalon]["per"].get(nom, (float("nan"), 0))[0]
        nw = next((sifat[v][nom][1] for v in sifat if nom in sifat[v]), 0)
        cells = []
        for v in sifat:
            x = sifat[v].get(nom)
            cells.append("—" if x is None else ("%.1f" % (100.0 * x[0] / x[1]) if x[1] else ("0" if x[0] == 0 else "∞")))
        L.append("| %s | %s | %.1f | %d | %s |" % (nom, g, au, nw, " | ".join(cells)))
    L.append("")
    if yoq:
        L.append("Chiqishi yoʻq juftliklar: %s\n" % ", ".join("%s/%s" % x for x in yoq))

    rep = os.path.join(N, "hisobot%s.md" % suf)
    with open(rep, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")

    # Farq misollari
    fd = os.path.join(N, "farqlar%s" % suf)
    os.makedirs(fd, exist_ok=True)
    for v in sifat:
        out = []
        for nom in fayllar:
            r, h = matn(etalon, nom), matn(v, nom)
            if r is None or h is None: continue
            p = farq_parchalari(normal(r), normal(h))
            if not p: continue
            out.append("### %s  (%d farq joyi)" % (nom, len(p)))
            for pa, pb, _ in p:
                out.append("  %-10s %s" % (etalon + ":", pa))
                out.append("  %-10s %s" % (v + ":", pb))
                out.append("")
        with open(os.path.join(fd, v + ".txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(out) + "\n")

    print("\n".join(L))


if __name__ == "__main__":
    main()
