# Tarjima sifatini oʻlchash

Model yoki dekodlash sozlamalari oʻzgarganda **shu harness ishga tushiriladi**
va natija quyidagi jadvalga yoziladi. Busiz regressiya sezilmay qoladi.

Dizayn asoslari: `docs/superpowers/specs/2026-08-31-tarjima-sifati-design.md`.

## Tayyorlash

```bash
cd scripts/eval
python3.12 -m venv .venv
./.venv/bin/pip install ctranslate2 sentencepiece sacrebleu
curl -sL -o flores.tar.gz https://dl.fbaipublicfiles.com/nllb/flores200_dataset.tar.gz
tar xzf flores.tar.gz
```

## Ishga tushirish

```bash
M="$HOME/Library/Application Support/Kotib/tarjima-model-33b"
nice -n 10 ./.venv/bin/python olcha.py \
    --model "$M" --spm "$M/sentencepiece.bpe.model" \
    --manba uzn_Latn --maqsad rus_Cyrl --soni 200
```

Real matnda koʻz bilan tekshirish (etalon yoʻq, chrF++ hisoblanmaydi):

```bash
nice -n 10 ./.venv/bin/python olcha.py \
    --model "$M" --spm "$M/sentencepiece.bpe.model" --matn tibbiy.txt
```

## Ikki korpus, ikkalasi ham shart

`flores200_dataset/devtest` — 1012 qisqa jumla, chrF++ raqami shu yerdan.

`tibbiy.txt` — uzun, koʻp ergashli gaplar va bitta havola. FLORES jumlalari
~50 token va `max_decoding_length` bugʻini **koʻrmaydi** — bu bugʻ aynan uzun
gaplarda chiqadi. Havola esa segmentatsiya regressiyasini ochadi.

## Harness ilova emas — u modelni oʻlchaydi

Bu skript matnni toʻgʻridan-toʻgʻri modelga beradi. Ilovaning Swift
qatlamini — `MatnBoluvchi` segmentatsiyasi, himoyalangan havolalar,
`tozala` normalizatsiyasi — u **chetlab oʻtadi. Shuning uchun bu yerda xom
chiqish koʻrinadi:

    RU : Анализ: Азамов Бахавуддин , PhD , MD  ⁇  t.me/dr_azamoff

Verguldan oldingi boʻsh joy va uzun tire oʻrnidagi `⁇` — ilovada bular
tuzatiladi. Ikki qatlam ikki xil vosita bilan tekshiriladi:

- **model sifati** — shu harness (chrF++ va koʻz bilan)
- **pipeline** — `./src/test.sh` dagi birlik testlari (hozir 1219 tekshiruv)

Pipeline mantiqini bu yerda takrorlash vasvasasiga berilmang: ikki nusxa
darrov bir-biridan uzoqlashadi va harness ilova qilmaydigan narsani
oʻlchay boshlaydi.

## Oʻlchov nimani KOʻRMAYDI

chrF++ — sirt oʻlchovi, harflarni sanaydi. U grammatik tuzatishni kam
baholaydi: `удалён` → `удалена` ikki harflik farq, ballga deyarli taʼsir
qilmaydi, oʻquvchi uchun esa savodli va savodsiz matn farqi. 1.3B dan 3.3B ga
oʻtishda chrF++ atigi +1,2 koʻtarildi, real matnda esa sakkiz nuqsondan
beshtasi tuzaldi.

Shuning uchun raqamni **hech qachon yolgʻiz oʻqimang** — `tibbiy.txt` chiqishi
bilan birga qarang.

Terminologiya xatosi (`issiq bosishi` → `перегрев`) ham chrF++ da deyarli
koʻrinmaydi va offline dekodlash bilan hal qilinmaydi — spec'dagi «Rad etilgan
yondashuvlar» boʻlimiga qarang.

## Maʼlum qiymatlar

M5, 16 GB, `uzn_Latn -> rus_Cyrl`, FLORES-200 devtest, 200 jumla, int8.

| Sana | Model | Sozlama | chrF++ | jumla/s |
|---|---|---|---|---|
| 2026-08-31 | NLLB-200 distilled 1.3B | beam 4, maxlen 256 (eski) | 47,90 | 0,75 |
| 2026-08-31 | NLLB-200 distilled 1.3B | dinamik uzunlik, nrg 4 | 47,87 | — |
| 2026-08-31 | NLLB-200 3.3B | dinamik uzunlik | 49,10 | 0,33 |

`no_repeat_ngram_size` tanlovi (1.3B, dinamik uzunlik):

| nrg | chrF++ | Buzilgan matnda soʻz xilma-xilligi |
|---|---|---|
| 0 | 47,90 | 0,08 — takrorlanish sikli |
| 3 | 47,83 | 0,78 |
| **4** | **47,87** | **0,62** ← tanlandi |
| 6 | 47,94 | 0,60 |
