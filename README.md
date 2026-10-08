# 🎙️ Kotib — Oʻzbekcha ovozli yozuv

Istalgan ilovada (Telegram, brauzer, hujjat — qayerda kursor boʻlsa) tugmani bosib oʻzbekcha gapiring — matn avtomatik oʻsha joyga lotin alifbosida yoziladi.

**Ikkala platforma uchun ham tayyor:**

| Platforma | Tugma | Yuklab olish | Texnik hujjat |
|---|---|---|---|
| **macOS** 13+ | ⌃⌥D | [.pkg (789 MB)](https://cdn.mirqobilov.com/dl/mac/Kotib-1.2.0-mac.pkg) | shu hujjat |
| **Windows** 10/11 | Ctrl+Alt+D | [.exe (845 MB)](https://cdn.mirqobilov.com/dl/win/Kotib-1.2.0-win-setup.exe) | [win/README.md](win/README.md) |

Ikkala versiya bir xil modeldan (`rubaiSTT v2 medium`) va bir xil inference parametrlaridan
foydalanadi — natijalar bir xil boʻladi. Tizimning oʻrnatilgan diktovkasi kabi, lekin
**oʻzbek tili uchun maxsus**, **butunlay oflayn** (internetsiz), va **bepul**.

> **Nom haqida:** mahsulot nomi 1.1.0 dan boshlab — **Kotib** (`Kotib.app`).
> Ilgari u *Audio-Matnga*, undan ham oldin *RubaiSTT Dictation* deb atalgan.
> Bundle identifikatori esa
> ataylab `com.rubaistt.dictation` boʻlib **qoldi**: macOS'da Accessibility va
> Mikrofon ruxsatlari shu ID ga bogʻlangan, oʻzgartirilsa mavjud
> foydalanuvchilarda ruxsatlar qaytadan soʻralar edi. Foydalanuvchi maʼlumotlari
> (`~/Library/Application Support/Audio-Matnga` → `Kotib`) birinchi ishga
> tushishda avtomatik koʻchiriladi — `src/yollar.swift` ga qarang.

> System-wide Uzbek speech-to-text dictation for **macOS and Windows**. Press the hotkey
> anywhere, speak Uzbek, and the transcribed text is typed into the focused field.
> Powered by the [rubaiSTT](https://huggingface.co/islomov/rubaistt_v2_medium) model running
> locally via [whisper.cpp](https://github.com/ggml-org/whisper.cpp) — Metal on macOS,
> Vulkan on Windows. Fully offline.

---

## 🗂 Repozitoriy tuzilishi

```
src/          macOS ilovasi (Swift + C shim)
win/          Windows ilovasi (C++20 + Win32)   -> win/README.md
scripts/      macOS release skriptlari
web/          yuklab olish sahifasi
assets/       ikonkalar
```

Oʻzgarishlar: [CHANGELOG.md](CHANGELOG.md) · arxitektura va reliz tartibi: [AGENTS.md](AGENTS.md) · hissa qoʻshish: [CONTRIBUTING.md](CONTRIBUTING.md)

Windows portining texnik tafsilotlari, oʻlchangan koʻrsatkichlar va sinovda topilgan
xatolar: [docs/windows/WINDOWS-PORT-PLAN.md](docs/windows/WINDOWS-PORT-PLAN.md)

---

## ✨ Xususiyatlar / Features

- 🌐 **Tizim boʻylab** — istalgan ilovada ishlaydi (global hotkey **⌃⌥D**)
- 🇺🇿 **Oʻzbek tiliga maxsus** — `rubaiSTT v2 medium` modeli, lotin alifbosi
- ⚡ **Metal tezlashtirish** — Apple Silicon GPU'da tez · Intel'da CPU bilan ishlaydi (universal)
- 🔌 **Toʻliq oflayn** — hech qanday server/internet kerak emas, ovoz qurilmangizdan chiqmaydi
- 🪶 **Yengil** — menyu-bar ilovasi; model 3 daqiqa ishlatilmasa RAM'dan boʻshaydi
- ⌨️ **Sozlanadigan tugma** — diktovka tugmasini Sozlamalardan oʻzgartirish mumkin (standart ⌃⌥D)
- 📄 **Fayl boʻlimi** — ovozli yoki video faylni tashlang, toʻliq matn chiqadi (uzun fayllar boʻlaklab qayta ishlanadi)
- 🌍 **Oflayn tarjimon** — 202 til, istalgan yoʻnalishda (oʻzbek, rus, ingliz, turk, qozoq…). Internetsiz ishlaydi va videokarta talab qilmaydi. Model (3,1 GB) bir marta yuklab olinadi.
- 🤖 **Ixtiyoriy sunʼiy intellekt** — oʻz API kalitingizni qoʻysangiz matnni tozalash va xulosa qilish qoʻshiladi. Model roʻyxati provayderning oʻzidan olinadi, shuning uchun u hech qachon eskirmaydi. Ilova busiz ham toʻliq ishlaydi va hech qanday kalit bilan tarqatilmaydi.

## 📋 Talablar / Requirements

- macOS 13+ · **Universal** — Apple Silicon (M1–M5, Metal bilan tez) yoki Intel (CPU, sekinroq)
- [Homebrew](https://brew.sh)
- Xcode Command Line Tools (`xcode-select --install`)
- ~1 GB disk (model ~820 MB, q8_0)

## 🚀 Oʻrnatish / Install

### A) Tayyor ilova (oson)

**[⬇️ macOS uchun — Kotib-1.2.0-mac.pkg (789 MB)](https://cdn.mirqobilov.com/dl/mac/Kotib-1.2.0-mac.pkg)**
· **[⬇️ Windows uchun — Kotib-1.2.0-win-setup.exe (845 MB)](https://cdn.mirqobilov.com/dl/win/Kotib-1.2.0-win-setup.exe)**

Dastur va oʻzbek tili modeli bitta faylda keladi. Internet faqat yuklab olish
uchun kerak — oʻrnatgandan keyin butunlay internetsiz ishlaydi. Windows fayli
ikkala protsessor turini ham qamraydi (Intel/AMD va ARM) — qaysi biri
borligini bilish shart emas, oʻrnatuvchi oʻzi tanlaydi.

1. Faylni oching va oʻrnatuvchi koʻrsatmalariga amal qiling
2. Ilovani ishga tushiring — **Xush kelibsiz** oynasi ikkita ruxsatni
   (mikrofon + Accessibility) berishda yoʻl-yoʻriq koʻrsatadi

macOS oʻrnatuvchisi Apple tomonidan **notarize qilingan** (`Developer ID
Installer: Muhammad Mirkabilov`) — Gatekeeper ogohlantirishi chiqmaydi,
qoʻshimcha harakat kerak emas.

### B) Manbadan build (developer)

```bash
git clone https://github.com/MuhammadMirrr/kotib.git
cd kotib
./setup.sh
```

`setup.sh` avtomatik: kerakli vositalarni oʻrnatadi → whisper.cpp, CTranslate2 va SentencePiece'ni pinlangan commit'lardan (`scripts/bogliqliklar.env`) build qiladi → modelni yuklab sha256 ni tekshiradi → ilovani build qilib oʻrnatadi → login'da avto-ishga tushishni sozlaydi.

### Oxirgi qadam — Accessibility ruxsati

Matn avtomatik joylashishi uchun (⌘V yuborish) bir marta ruxsat bering:

1. **System Settings → Privacy & Security → Accessibility**
2. **"Kotib"** ni qoʻshing (`+`) va **yoqing** ✅

Birinchi yozishda **mikrofon** ruxsati ham soʻraladi — ruxsat bering.

## 🎯 Ishlatish / Usage

1. Istalgan joyda kursorni yozish maydoniga qoʻying
2. **⌃⌥D** bosing → 🔴 gapiring → **⌃⌥D** yana bosing
3. Matn oʻsha joyga yoziladi

Menyu-bardagi 🎙️ ikonadan ham boshqarish mumkin.

## 📦 Tarqatish / Release (developer)

Imzolangan + notarize qilingan `.pkg` oʻrnatuvchi yasash:

```bash
# Bir martalik: notarize uchun keychain profil
xcrun notarytool store-credentials rubai-notary \
    --apple-id "siz@example.com" --team-id "TEAMID" \
    --password "xxxx-xxxx-xxxx-xxxx"   # app-specific parol

./scripts/release.sh        # build → Developer ID imzo → notarize → .pkg
```

Natija: `dist/Kotib-<versiya>-mac.pkg` — model ichida, bitta toʻliq fayl.
Butun mantiq `scripts/make_pkg.sh` da; skript sertifikatlarni avtomatik topadi.
Notarize qilinmagan `.pkg` ni macOS 15+ **bloklaydi**, shuning uchun bu qadam majburiy.

## ⚠️ Eslatma / Notes

- Tarqatiladigan `.pkg` **Developer ID bilan imzolangan va notarize qilingan** (hardened runtime) — foydalanuvchida hech qanday Gatekeeper qadami kerak emas. Manbadan build (`setup.sh`) esa **ad-hoc imzolangan** (lokal, Gatekeeper bloklamaydi).
- App Store'ga **chiqmaydi** — tizim boʻylab matn yozish (synthetic ⌘V) sandbox'da taqiqlangan; shuning uchun Developer ID orqali tarqatiladi.
- **Universal binary** — Apple Silicon (Metal GPU) va Intel (CPU). Intel'da sezilarli sekinroq, lekin ishlaydi.

## 🔒 Maxfiylik

**Ovoz va matn hech qayerga yuborilmaydi.** Nutqni matnga oʻgirish butunlay
sizning kompyuteringizda ketadi — model ham, hisob-kitob ham lokal. Internet
faqat uchta ish uchun kerak boʻladi va uchalasi ham ixtiyoriy:

| Nima | Qachon | Nima yuboriladi |
|---|---|---|
| **Model yuklab olish** | birinchi oʻrnatishda | hech nima (oddiy fayl yuklash) |
| **Yangi versiya tekshiruvi** | kuniga bir marta | ilova versiyasi |
| **Anonim statistika** | kuniga bir marta, **oʻchirsa boʻladi** | platforma (mac/win), ilova versiyasi, tizim versiyasi va tasodifiy raqam |

Anonim statistika Sozlamalardan oʻchiriladi. Oʻchirilgan boʻlsa ham yangi
versiya haqidagi xabar ishlab turadi — u alohida soʻrov.

**Hech qachon yuborilmaydi:** ovoz, transkript, mikrofon nomi, fayl nomi,
elektron pochta, ism, qurilma identifikatori. Tasodifiy raqam qurilmaga
bogʻlanmagan: ilovani oʻchirib qayta oʻrnatsangiz yangisi paydo boʻladi.
IP manzil soʻrov jurnalida saqlanmaydi — faqat mamlakat kodi (Cloudflare
beradi) yoziladi.

**Sunʼiy intellekt amallari (ixtiyoriy).** Studiyadagi «Matnni yaxshilash»
sizning **oʻz** API kalitingiz bilan ishlaydi va matnni siz tanlagan
provayderga yuboradi. Ilova hech qanday kalit bilan kelmaydi, kalit faqat
tizim kalit omborida saqlanadi (macOS Keychain / Windows Credential Manager)
va hech qachon logʻga yozilmaydi. Kalit kiritilmasa bu qism butunlay
oʻchiq boʻladi va ilovaning qolgan hamma qismi toʻliq ishlaydi.

**Tarjima ham oflayn** — model kompyuterga yuklab olinadi va matn hech
qayerga chiqmaydi.

## 🛠 Texnik tafsilotlar

- **Model:** [`islomov/rubaistt_v2_medium`](https://huggingface.co/islomov/rubaistt_v2_medium) → ggml **q8_0** (8-bit) ga siqilgan — ~820 MB, ~700 MB kamroq RAM, aniqlik deyarli oʻzgarmaydi
- **Inference:** whisper.cpp + Metal, beam search (maksimal aniqlik)
- **Til:** `uz`, lotin alifbosi
- **UI:** Swift / AppKit, menyu-bar (LSUIElement), global hotkey Carbon orqali
- **Matn kiritish:** clipboard + ⌘V (CGEvent) — Accessibility ruxsati kerak

## 🤝 Hissa qoʻshish / Contributing

- [CONTRIBUTING.md](CONTRIBUTING.md) — yigʻish (kalitlarsiz ham), testlar, kod qoidalari
- [CHANGELOG.md](CHANGELOG.md) — versiyalar tarixi
- [SECURITY.md](SECURITY.md) — zaiflik haqida maxfiy xabar berish, imzo kaliti siyosati
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) — xulq-atvor qoidalari
- [AGENTS.md](AGENTS.md) — arxitektura va muhandislik qarorlari

## 📄 Litsenziya

MIT ([LICENSE](LICENSE)). Uchinchi tomon dasturlari va modellari oʻz
litsenziyalari ostida — [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); toʻliq
matnlar ilova ichida («Litsenziyalar…»). Eʼtibor: tarjima modeli **NLLB-200 —
CC-BY-NC-4.0**, faqat notijorat foydalanish uchun.

## 🙏 Minnatdorchilik

- [rubaiSTT](https://huggingface.co/islomov/rubaistt_v2_medium) — Sardor Islomov (oʻzbek STT modeli)
- [whisper.cpp](https://github.com/ggml-org/whisper.cpp) — Georgi Gerganov
- [OpenAI Whisper](https://github.com/openai/whisper)
- [CTranslate2](https://github.com/OpenNMT/CTranslate2) — OpenNMT (tarjima dvigateli)
- [NLLB-200](https://huggingface.co/facebook/nllb-200-3.3B) — Meta AI (tarjima modeli)
- [SentencePiece](https://github.com/google/sentencepiece) — Google

---

## 🇬🇧 English

**Kotib** is a free, fully offline Uzbek speech-to-text app for **macOS** (13+,
universal) and **Windows** (10/11, x64 and ARM64). Press the hotkey (⌃⌥D /
Ctrl+Alt+D) in any app, speak Uzbek, press it again — the text is typed into the
focused field in the Latin script, with proper `oʻ`/`gʻ`/`ʼ`.

- **Dictation** anywhere, **Audio/File** transcription of long recordings, and an
  **offline translator** for 202 languages (NLLB-200, downloaded on demand).
- **Private by design:** audio and text never leave the device. Anonymous usage
  statistics contain no audio, no text and no raw IP — see [PRIVACY.md](PRIVACY.md).
- **Same output on both platforms:** one model ([rubaiSTT v2 medium](https://huggingface.co/islomov/rubaistt_v2_medium),
  q8_0) and identical inference parameters, enforced by tests.
- **Build from source:** see [CONTRIBUTING.md](CONTRIBUTING.md) — works without any
  signing keys. Architecture and design decisions: [AGENTS.md](AGENTS.md).
- **License:** MIT; third-party components in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
  (note: the NLLB-200 translation model is CC-BY-NC-4.0, non-commercial).
