# Hissa qoʻshish / Contributing

Kotib'ga qiziqqaningiz uchun rahmat. Bu hujjat — yigʻish, sinash va kod
yozish qoidalari. Muhandislik tafsilotlari (arxitektura, «nega shunday»
qarorlari, tuzoqlar) — [`AGENTS.md`](AGENTS.md); kod oʻzgartirishdan oldin
tegishli boʻlimini oʻqing.

*English summary at the end.*

## Yigʻish — macOS

Talablar: macOS 13+, Xcode Command Line Tools, [Homebrew](https://brew.sh).

```bash
./setup.sh          # bir marta: bogʻliqliklar (pinlangan), model, ilova
./src/test.sh       # sof mantiq testlari, bir necha soniya
./src/build.sh      # faqat ilovani qayta yigʻish → ~/Applications/Kotib.app
```

- `setup.sh` whisper.cpp, CTranslate2 va SentencePiece'ni aniq commit'larda
  (`scripts/bogliqliklar.env`) quradi — Apple Silicon'da x86_64 nusxalari bilan,
  ilova universal chiqadi. CTranslate2 ~20 daqiqa oladi.
- **Kalitlarsiz yigʻish mumkin.** `build.sh` Developer ID sertifikatini topmasa
  ad-hoc imzoga oʻtadi — ilova ishlaydi, faqat Accessibility ruxsati har
  yigʻishdan keyin qayta soʻraladi (sababi `AGENTS.md` → «Dev builds are signed
  with Developer ID»). Sinov paketi: `SKIP_NOTARIZE=1 ./scripts/make_pkg.sh`
  (tarqatishga yaramaydi).
- `APP_OUT=<yoʻl> ./src/build.sh` — boshqa joyga yigʻish (ishlab turgan Kotib'ga
  tegmaslik uchun).

## Yigʻish — Windows

Windows ilovasi **macOS'dan kross-kompilyatsiya** qilinadi
([llvm-mingw](https://github.com/mstorsjo/llvm-mingw), versiyasi pinlangan):

```bash
./win/build-mac.sh            # x64 (yoki: arm64, hammasi)
./win/tests/mac/hammasi.sh    # macOS'da ishlaydigan hamma Windows tekshiruvlari
```

Tafsilotlar, MSVC yoʻli (`win/build.ps1`) va VM bilan ishlash — [`win/README.md`](win/README.md).

## Tekshiruvlar

PR'dan oldin hammasi yashil boʻlsin:

```bash
./src/test.sh                   # macOS sof mantiq + versiya tekshiruvi
./win/tests/mac/hammasi.sh      # Windows testlari, whisper parametr pariteti, korpus diff
./scripts/formatla.sh --tekshir # Swift (swift format) va C/C++ (clang-format)
git ls-files '*.sh' | xargs shellcheck
```

Formatlash: `./scripts/formatla.sh` (sozlamalar — `.swift-format`, `.clang-format`).
`clang-format` llvm-mingw toolchain'idan olinadi — boshqa versiya boshqacha
formatlaydi.

## Kod qoidalari

- **Til.** Izohlar, log va foydalanuvchi matni — **oʻzbekcha**. Identifikatorlar
  ham oʻzbekcha (`yozuvchi`, `kirituvchi`, `saqlanmagan`); tizim tushunchalari
  inglizcha qoladi (`AppDelegate`, `Overlay`, `Engine`).
- **Bitta tushuncha — bitta nom.** diktovka (dictation), yozuv/yozish
  (recording), ovoz (audio), matn (text), tarjima (translation), hujjat
  (Studiya document), yoʻl (path), sozlama (setting). Yangi nom kiritishdan oldin
  mavjudini qidiring.
- **Apostrof.** Foydalanuvchi matni va oʻzbekcha soʻzlar — `ʻ` (U+02BB, oʻ/gʻ) va
  `ʼ` (U+02BC, tutuq belgisi). ASCII `'` faqat kod yoki chet nom bilan oʻzbekcha
  qoʻshimcha orasida: `Keychain'da`, `macOS'ning`. Hech qachon regex bilan
  ommaviy almashtirmang (`AGENTS.md` → «Apostrophe convention»).
- **Ikki platforma — bitta xulq.** whisper parametrlari
  (`src/whisper_bridge.c` ↔ `win/core/whisper_bridge.c`), tarjima koʻprigi,
  matn formatlash va yangilanish siyosati ikkala tomonda bir xil boʻlishi
  shart — testlar buni ushlaydi. Bir tomonni oʻzgartirsangiz, ikkinchisini ham.
- **Sof mantiq — testlanadigan faylda.** AppKit/Win32/whisper/fayl tizimiga
  bogʻliq boʻlmagan kod `src/test.sh` yoki `win/tests/mac/sinov.sh` qamraydigan
  faylga yoziladi va test qoʻshiladi.
- **Har fayl boshida** 2–4 qatorli «bu fayl nima qiladi» izohi; fayllar ~500
  qatordan oshmasin.

## Sirlar

Repoga **hech qachon**: imzo kalitlari, notarizatsiya profili, API kalitlari,
`statistika/.dev.vars*`. Ilova hech qanday kalit bilan tarqatilmaydi — LLM kaliti
foydalanuvchining oʻzi, Keychain/Credential Manager'da. Zaiflik topsangiz —
[`SECURITY.md`](SECURITY.md).

## Commit va PR

- Commit xabari — oʻzbekcha: qisqa sarlavha, keyin «nima va nega».
- Bitta PR — bitta mavzu. Formatlash oʻzgarishlarini mantiq oʻzgarishidan
  ajrating.
- Xatti-harakat oʻzgarsa — `AGENTS.md` va kerak boʻlsa `CHANGELOG.md` ni
  yangilang.

---

## English summary

- **macOS:** `./setup.sh` once (pinned deps, universal build), then
  `./src/build.sh`; tests `./src/test.sh`. No Developer ID? The build falls back
  to ad-hoc signing; test packages with `SKIP_NOTARIZE=1 ./scripts/make_pkg.sh`.
- **Windows:** cross-compiled from macOS with pinned llvm-mingw —
  `./win/build-mac.sh`, checks `./win/tests/mac/hammasi.sh`. See `win/README.md`.
- **Before a PR:** both test runners, `./scripts/formatla.sh --tekshir`, shellcheck.
- **Style:** Uzbek comments/identifiers (system concepts stay English), one name
  per concept, proper `ʻ`/`ʼ` in user-facing text, both platforms must behave the
  same, pure logic goes into a tested file. Read `AGENTS.md` before non-trivial work.
- **Never commit secrets.** Report vulnerabilities privately — see `SECURITY.md`.
