# Avto-yangilanish: haqiqiy ilovalardagidek

Sana: 2026-10-07
Holat: dizayn yozildi va tasdiqlandi (2026-10-07; majburiy muhlat — 72 soat)
Yoʻl xaritasi: `docs/superpowers/plans/2026-10-07-v1.2.0-yol-xaritasi.md` (S2–S9)

## Muammo

Hozir (1.1.0) yangilanish bunday ishlaydi:

1. Ilova **faqat ishga tushganda** `POST /v1/ping` yuboradi (`src/dictate.swift:717`,
   `win/ui/app.cpp`). «Har 24 soatda» degan izoh bor (`src/yangilanish.swift:66`),
   lekin taymer yoʻq. Kotib menyu-bar/trey ilovasi — kompyuter haftalab
   oʻchirilmaydi, demak koʻp foydalanuvchi yangi versiyani **umuman koʻrmaydi**.
2. `oxirgiTekshiruv` soʻrovdan **oldin** saqlanadi (`yangilanish.swift:75`) —
   tarmoq hali yoʻq paytdagi muvaffaqiyatsiz urinish 20 soat tekshiruvni oʻchiradi.
3. Yangi versiya topilsa banner chiqadi, tugma **saytni brauzerda ochadi**
   (`asosiy_oyna.swift:126`, `win/ui/asosiy_oyna.cpp:525`). Foydalanuvchi
   750–840 MB faylni oʻzi yuklab, oʻzi oʻrnatadi — har safar model bilan birga.
4. Server bergan URL tekshirilmasdan ochiladi (Windows'da `ShellExecuteW` — UNC
   yoʻl yoki lokal `.exe` boʻlsa ishga tushadi).
5. Majburiy yangilanish, bosqichma-bosqich tarqatish, nosoz relizni qaytarib olish
   imkoniyati yoʻq.

## Talab (foydalanuvchidan)

- Ilova **har 24 soatda** oʻzi tekshiradi — ilova ochiq turgan boʻlsa ham.
- Yangi versiya chiqsa ilova uni **oʻzi yuklab oladi, oʻzi oʻrnatadi, oʻzi qayta
  ishga tushadi**. Saytga kirish, alohida yuklab olish, «Next → Install» yoʻq.
- Muhim yangilanishni **majburiy** qilish mumkin boʻlsin: eski versiya
  yangilanmaguncha ishlamaydi.
- Vaqtinchalik emas — yillar davomida ishlaydigan, haqiqiy ilovalardagi kabi tizim.

## Haqiqiy ilovalar buni qanday qiladi

| Ilova turi | Mexanizm | Nega jim ishlaydi |
|---|---|---|
| macOS nativ (iTerm2, VLC, Rectangle, …) | **Sparkle 2** | Ilova foydalanuvchiga tegishli papkada; EdDSA imzo; fonda yuklab, chiqishda yoki darhol oʻrnatadi |
| Chrome / Edge / Firefox | Oʻz yangilovchisi + imtiyozli servis (Keystone, Maintenance Service) | Admin servis UAC'siz yozadi |
| VS Code (user setup), Slack, Discord, Telegram, Zoom | **Per-user oʻrnatish** (`%LOCALAPPDATA%`) + oʻz yangilovchisi (Squirrel/Velopack yoki Inno `/VERYSILENT`) | Foydalanuvchi papkasiga yozish uchun admin kerak emas |

Umumiy qoidalar (hammasida bor):

1. **Kriptografik imzo** — yuklangan fayl va manifest Ed25519 bilan tekshiriladi;
   server yoki CDN buzilsa ham soxta yangilanish oʻrnatilmaydi.
2. **Admin soʻramaslik** — ilova foydalanuvchi yoza oladigan joyda turadi.
3. **Kichik yangilanish paketi** — katta resurslar (bizda 823 MB model) ilovadan
   ajratilgan, har yangilanishda qayta yuklanmaydi.
4. **Xavfsiz payt** — oʻrnatish foydalanuvchi ishini buzmaydi (bizda: yozuv,
   Fayl ishi yoki tarjima ketayotganda emas).
5. **Bosqichli tarqatish va «kill switch»** — avval 10 %, keyin hammaga; nosoz
   reliz manifestdan bir buyruq bilan olib tashlanadi.
6. **Majburiy versiya chegarasi** (`min_versiya`) — kritik xato yoki xavfsizlik uchun.

Oʻrganilgan manbalar: [Sparkle — Customizing](https://sparkle-project.org/documentation/customization/),
[Sparkle — Publishing](https://sparkle-project.org/documentation/publishing/),
[Sparkle — Package Updates](https://sparkle-project.org/documentation/package-updates/),
[SPUUpdaterDelegate](https://sparkle-project.org/documentation/api-reference/Protocols/SPUUpdaterDelegate.html),
[WinSparkle — Publishing updates](https://winsparkle.org/guides/publishing-updates/),
[Velopack C++](https://docs.velopack.io/getting-started/cpp).

## Bizning ikki toʻsiq (tekshirilgan)

**macOS:** `.pkg` ilovani `/Applications` ga **root** egaligida yozadi. Sparkle
hujjati aniq aytadi: root egaligidagi ilovani yangilashda har safar parol
soʻraladi, `.pkg` yangilanishlari esa umuman jim boʻlolmaydi («Installs always
require user authorization which also prevents silent automatic installs»). Bundan
tashqari model (823 MB) bundle ichida — Sparkle butun bundle'ni almashtiradi.

**Windows:** `win/installer/rubai.iss:77` — `PrivilegesRequired=admin`,
`DefaultDirName={autopf}\Kotib` → Program Files. Har yangilanishda UAC oynasi
chiqadi, jim oʻrnatib boʻlmaydi. Model `{app}\models` da.

## Qarorlar

### D1. macOS: Sparkle 2 (oʻz yangilovchimiz emas)

Sparkle — macOS'dagi de-fakto standart: atomar almashtirish, kod imzosi va
EdDSA tekshiruvi, delta yangilanishlar, bosqichli tarqatish, kanallar, kritik
yangilanishlar, qayta ishga tushirish — hammasi sinalgan. Bundle'ni ishlab turgan
ilova ostida xavfsiz almashtirishni oʻzimiz yozish — yillar davomida xato
yigʻadigan ish. Versiya **2.9.x** (oxirgi barqaror), sha256 bilan pinlangan.

- `SUAutomaticallyUpdate=YES`, `SUAllowsAutomaticUpdates=YES`,
  `SUEnableAutomaticChecks=YES` (ruxsat soʻralmaydi), `SUScheduledCheckInterval=86400`.
- `SUPublicEDKey`, `SUVerifyUpdateBeforeExtraction=YES`, `SURequireSignedFeed=YES`.
- Menyu-bar ilova deyarli hech qachon yopilmaydi, shuning uchun «chiqishda
  oʻrnatish» yetmaydi. `updater(_:willInstallUpdateOnQuit:immediateInstallationBlock:)`
  delegati `true` qaytaradi va blokni **ilova boʻsh turganda** chaqiradi —
  Sparkle darhol oʻrnatadi va qayta ishga tushiradi.
- UI: Sparkle'ning standart oynasi oʻzbekcha emas. Avtomatik rejimda u deyarli
  koʻrinmaydi, lekin qoʻlda «Yangilanishni tekshirish» bosilganda koʻrinadi.
  Shuning uchun **oʻzbekcha `SPUUserDriver`** yoziladi (`yangilash_haydovchi.swift`):
  topildi → yuklanmoqda (foiz) → tayyor → oʻrnatilmoqda; xato holatlari.

### D2. macOS: model bundle'dan chiqadi, bundle foydalanuvchiga tegishli boʻladi

- `.pkg` modelni `/Library/Application Support/Kotib/models/ggml-rubaistt.bin`
  ga yozadi (root, faqat oʻqish, barcha foydalanuvchilarga umumiy). Ilova bundle'i
  ~15 MB boʻlib qoladi, yangilanish arxivi ~6–8 MB (siqilgan).
- `postinstall` ilova bundle'ini konsol foydalanuvchisiga beradi:
  `chown -R "$CONSOLE_USER":staff /Applications/Kotib.app`. Bu sudrab
  oʻrnatilgan ilovalardagi holat bilan bir xil. Natijada Sparkle parolsiz yozadi.
  Bir nechta foydalanuvchili Mac'da boshqa foydalanuvchilar uchun Sparkle parol
  soʻraydi — hujjatlanadi, qabul qilinadi.
- Model qidiruv tartibi (`ModelStore.existingPath`), **har biri `fileExists` va
  minimal hajm bilan tekshiriladi**:
  1. `~/Library/Application Support/Kotib/models/` (ilova ichida yuklangan)
  2. `/Library/Application Support/Kotib/models/` (`.pkg` qoʻygan)
  3. bundle ichida (faqat ≤1.1.0 dan oʻtish davri uchun)
  4. `~/rubai-stt/models/` (eski dasturchi yoʻli)

  Bu, shuningdek, «Model yuklanmadi (kod 1)» xatosining ildiz sababini yopadi
  (CFBundle keshi oʻchirilgan fayl yoʻlini qaytaradi — barqarorlik spec'i, A1).

### D3. Windows: per-user oʻrnatish + oʻz yangilovchimiz (WinSparkle/Velopack emas)

**Per-user:** `PrivilegesRequired=lowest`, ilova
`%LOCALAPPDATA%\Programs\Kotib`, model `%LOCALAPPDATA%\Kotib\models`. VS Code
user setup, Slack, Discord, Telegram — hammasi shu yoʻl bilan UAC'siz yangilanadi.

**Nega oʻz yangilovchimiz:**

| Variant | Qarshi dalil |
|---|---|
| WinSparkle | UI wxWidgets'da, oʻzbekcha tarjima yoʻq; MSVC'da yigʻilgan DLL; delta yoʻq; baribir Inno'ni ishga tushiradi |
| Velopack | Inno Setup'ni toʻliq almashtiradi (rozilik sahifasi, eski versiyalarni koʻchirish mantiqi, model manbalari — hammasi qayta yoziladi); Rust kutubxonasi MSVC uchun; llvm-mingw bilan bogʻlash sinalmagan |
| **Oʻzimiz** | Kerakli qismlar allaqachon bor: JSON, HTTPS yuklovchi davom ettirish bilan (`yuklovchi.cpp`), oʻzbekcha UI, Inno oʻrnatuvchi. Qoʻshiladi: Ed25519 tekshiruvi (Monocypher, bitta C fayl, CC0) va jadval |

Yangilanish oʻrnatuvchisi — **alohida, modelsiz Inno skripti**
(`Kotib-<v>-win-yangilash.exe`, taxminan 30–45 MB, oʻlchanadi). Toʻliq
oʻrnatuvchi (`…-win-setup.exe`, model bilan) faqat birinchi oʻrnatish uchun.

### D4. Yagona imzo kaliti va imzolangan manifest

- Bitta Ed25519 kalit juftligi: Sparkle `generate_keys` (macOS Keychain'da
  saqlanadi). Windows fayllari va manifestlar ham shu kalit bilan Sparkle'ning
  `sign_update` vositasida imzolanadi — bitta kalit, bitta vosita.
- **Kalit yoʻqolsa yangilanish chiqarib boʻlmaydi.** `generate_keys -x` bilan
  eksport qilinib, ikki oflayn joyda (shifrlangan) saqlanadi; tartib `AGENTS.md`
  ga yoziladi. Ochiq kalit ilovaga kompilyatsiya vaqtida joylanadi.
- Server buzilsa ham xavf yoʻq: ilova faqat imzosi toʻgʻri manifest va faylni qabul
  qiladi, URL esa faqat `https://cdn.mirqobilov.com/` dan boʻlishi mumkin.

## Arxitektura

```
                    reliz skripti (mac)
                    ├─ build → imzo → notarize
                    ├─ Kotib-1.2.1-mac.zip  + delta'lar   ─┐
                    ├─ Kotib-1.2.1-mac.pkg (yangi oʻrnatish)│→ R2: dl/v1.2.1/… (immutable)
                    ├─ Kotib-1.2.1-win-yangilash.exe ×1    │
                    ├─ Kotib-1.2.1-win-setup.exe           ─┘
                    ├─ appcast-mac.xml (generate_appcast, imzolangan feed)
                    └─ siyosat-{mac,win}.json + imzo        → KV (tez almashtiriladi)

Worker (stat.mirqobilov.com)
  GET /v1/appcast/mac.xml          → KV appcast-mac         (Cache-Control: max-age=300)
  GET /v1/yangilanish/win.json     → KV siyosat-win (+imzo)
  GET /v1/yangilanish/mac.json     → KV siyosat-mac (+imzo) — majburiy siyosat uchun
  POST /v1/ping                    → eski 1.1.0 lar uchun «koʻprik» (pastda)
```

### Siyosat manifesti (ikkala platforma, bir xil sxema)

```json
{
  "platforma": "win",
  "versiya": "1.2.1",
  "min_versiya": "1.2.0",
  "majburiy_muhlat_soat": 72,
  "tarqatish_foiz": 100,
  "izoh": "Mikrofon ochilishidagi xato tuzatildi.",
  "fayllar": {
    "x64":   { "url": "https://cdn.mirqobilov.com/dl/v1.2.1/Kotib-1.2.1-win-yangilash.exe",
               "hajm": 41234567, "sha256": "…", "imzo": "…(Ed25519, base64)" },
    "arm64": { "…": "…" }
  }
}
```

Worker javobi: `{"m": "<manifest baytlari, base64>", "s": "<Ed25519 imzo, base64>"}`.
Ilova avval `s` ni `m` ustidan tekshiradi, keyin `m` ni oʻqiydi. macOS'da `fayllar`
boʻlmaydi — yuklab olishni appcast orqali Sparkle qiladi, siyosat esa faqat
`min_versiya`/muhlat uchun oʻqiladi.

Reliz skripti tekshiradi: `min_versiya ≤ versiya`, URL'lar CDN'da mavjud va
`hajm`/`sha256` mos, imzo tekshiriladi — xato boʻlsa KV'ga yozilmaydi.

### Tekshiruv jadvali (ikkala platforma)

- Ishga tushgandan **60 s keyin** birinchi tekshiruv (tarmoq tayyor boʻlsin).
- Keyin **har 24 soatda** (macOS: Sparkle jadvali; Windows: taymer).
- Uyqudan uygʻonganda (`NSWorkspace.didWakeNotification`,
  `WM_POWERBROADCAST/PBT_APMRESUMEAUTOMATIC`) — oxirgi **muvaffaqiyatli**
  tekshiruvdan 24 soat oʻtgan boʻlsa.
- Muvaffaqiyatsizlikda qayta urinish: 15 daq → 1 soat → 4 soat, keyin jadval.
- Vaqt belgisi **faqat muvaffaqiyatli javobdan keyin** saqlanadi.
- Sozlamalarda: «Yangilanishni hozir tekshirish» tugmasi va oxirgi tekshiruv vaqti.

### Oʻrnatish oqimi

1. Yangi versiya → fonda yuklab olish (davom ettirish bilan; Windows:
   `%LOCALAPPDATA%\Kotib\yangilanish\`).
2. Tekshiruv: hajm → sha256 → Ed25519 (Windows); Sparkle: EdDSA + kod imzosi.
   Xato boʻlsa fayl oʻchiriladi, keyingi jadvalda qayta urinadi, logga yoziladi.
3. **Boʻsh paytni kutish:** yozuv yoʻq, Fayl ishi yoʻq, tarjima yoʻq, oxirgi
   diktovkadan ≥ 2 daqiqa oʻtgan. Tayyor yangilanish 24 soatdan beri kutayotgan
   boʻlsa — keyingi boʻsh daqiqada.
4. O'rnatish:
   - macOS: `immediateInstallationBlock()` → Sparkle almashtiradi → qayta ishga tushadi.
   - Windows: `Kotib-…-yangilash.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
     /KOTIBYANGILASH` ishga tushiriladi, Kotib oʻzi chiqadi. Inno fayllarni
     almashtiradi va `/KOTIBYANGILASH` boʻlsa Kotib'ni qayta ochadi.
5. Qayta ochilgach: «Kotib 1.2.1 ga yangilandi» bildirishnomasi (oxirgi
   ishlagan versiya `UserDefaults`/`settings.ini` da saqlanadi). Izoh matni manifestdan.

### Majburiy yangilanish (`min_versiya`)

`joriy < min_versiya` boʻlsa:

1. Darhol tekshiruv va yuklab olish, boʻsh paytni kutmasdan oʻrnatish (yozuv
   tugashini kutadi, xolos).
2. Muhlat (`majburiy_muhlat_soat`, standart 72) davomida ilova ishlayveradi,
   asosiy oynada doimiy «Muhim yangilanish oʻrnatilmoqda» banneri turadi.
3. Muhlat tugasa va hali oʻrnatilmagan boʻlsa (internet yoʻq, disk toʻla…):
   diktovka **bloklanadi**, oyna «Yangilanish majburiy» kartasini koʻrsatadi —
   holat, «Qayta urinish», va zaxira sifatida «Saytdan yuklab olish».
4. Muhlat siyosat **birinchi marta olingan** paytdan sanaladi va diskda saqlanadi.
   Hech qachon internetga chiqmagan ilova hech qachon bloklanmaydi — oflayn
   mahsulot vaʼdasi buzilmaydi.

Nega muhlat bor: Oʻzbekistonda internet beqaror; bitta muvaffaqiyatsiz yuklash
foydalanuvchini darhol ishsiz qoldirmasligi kerak. Muhlat 0 qilinsa — darhol blok
(faqat xavfsizlik holatida).

### Bosqichli tarqatish va qaytarib olish

- macOS: appcast'da `sparkle:phasedRolloutInterval` (Sparkle 7 guruh).
- Windows: `tarqatish_foiz`; ilova birinchi ishga tushganda tasodifiy 0–99 son
  yaratadi va saqlaydi, `son < foiz` boʻlsa yangilanadi. Serverga hech narsa yuborilmaydi.
- Standart reliz: 10 % → 24 soat kuzatuv (statistika panelidagi xato ulushi) → 100 %.
- **Kill switch:** `scripts/reliz.sh qaytar <versiya>` — KV'dagi appcast va
  siyosatni oldingi holatga qaytaradi. CDN fayllari oʻchirilmaydi (immutable).

### Eski 1.1.0 foydalanuvchilar uchun koʻprik

1.1.0 da avto-yangilovchi yoʻq — 1.2.0 ga **oxirgi marta qoʻlda** oʻtiladi:

- `versiya.json` dagi `url` sayt emas, **toʻgʻridan-toʻgʻri** yangi oʻrnatuvchi
  boʻladi (mac `.pkg`, win `setup.exe`) — banner tugmasi bosilganda fayl darhol
  yuklanadi.
- `izoh`: «Bu oxirgi qoʻlda yangilash — keyingi versiyalar avtomatik oʻrnatiladi».
- Windows 1.2.0 toʻliq oʻrnatuvchisi Program Files'dagi eski nusxani topadi,
  modelni `%LOCALAPPDATA%\Kotib\models` ga koʻchiradi, eski nusxani bir martalik
  UAC bilan olib tashlaydi, avtostartni yangi yoʻlga oʻtkazadi.
- macOS 1.2.0 `.pkg`: modelni `/Library/...` ga qoʻyadi, bundle egaligini
  oʻzgartiradi, eski bundle ichidagi modelni yoʻq qiladi (yangi bundle'da u yoʻq).
- README'dagi `v1.1` Windows havolasi (eskirgan, `v1.1b` emas) shu bosqichda tuzatiladi.

## Xavfsizlik

- Faqat `https://cdn.mirqobilov.com/` (fayllar) va `https://stat.mirqobilov.com/`
  (manifest) — boshqa sxema/xost rad etiladi. Bu 1.1.0 dagi «URL tekshirilmaydi»
  muammosini ham yopadi.
- Versiya pasaytirish (downgrade) rad etiladi.
- Manifest imzosi notoʻgʻri boʻlsa — eʼtiborsiz qoldiriladi, logga yoziladi.
- Yuklangan fayl faqat imzo tekshiruvidan keyin ishga tushiriladi; Windows'da
  vaqtinchalik papka foydalanuvchiniki, nom tasodifiy qoʻshimcha bilan.
- Windows SmartScreen: kod imzolash sertifikati yoʻq (ochiq savol, `WINDOWS-PARITET.md`).
  Avto-yangilanish bunga bogʻliq emas (yuklab olingan fayl brauzerdan kelmaydi,
  Mark-of-the-Web yoʻq), lekin birinchi oʻrnatishda ogohlantirish qoladi.

## Maxfiylik

- Appcast/siyosat soʻrovi hech qanday identifikator yubormaydi. Sparkle'ning
  `SUEnableSystemProfiling` oʻchiq.
- Statistika (`/v1/ping`) yangilanishdan kodda ajratiladi: biri yiqilsa ikkinchisi
  ishlayveradi (barqarorlik spec'i, G2). Statistika doim yoqilgan (Q2).

## Sinov

**Birlik testlari (sof mantiq):** versiya taqqoslash (`1.2.0` vs `1.10.0`,
`1.2.0-rc1`), siyosat qarori (joriy/min/muhlat/foiz jadvali), manifest
tahlili, Ed25519 — RFC 8032 test vektorlari (Windows, Monocypher), URL ruxsat roʻyxati.

**Uchdan-uchga (har platformada):**

1. Mahalliy test server + test kalit: `1.2.0-sinov1` → `1.2.0-sinov2` jim oʻrnatiladi,
   qayta ochiladi, bildirishnoma chiqadi.
2. Yozuv paytida yangilanish tayyor → yozuv tugaguncha kutadi.
3. Fayl buzilgan (1 bayt) → rad etiladi, eski versiya ishlayveradi.
4. Manifest imzosi buzilgan → eʼtiborsiz.
5. Yuklash oʻrtasida tarmoq uziladi / ilova yopiladi → keyingi safar davom etadi.
6. Disk toʻla → xato logga, keyingi jadvalda qayta urinish.
7. `min_versiya` oshirildi → banner; muhlat 0 → blok kartasi; oʻrnatilgach blok yoʻqoladi.
8. Qaytarib olish: KV oldingi holatga → yangi tekshiruvda hech narsa taklif qilinmaydi.
9. Koʻprik: haqiqiy 1.1.0 (mac va win) → banner → toʻgʻridan-toʻgʻri yuklash →
   1.2.0 → keyingi `1.2.1` avtomatik.
10. Windows: per-machine 1.1.0 → per-user 1.2.0 koʻchishi; model koʻchdi,
    avtostart ishlaydi, «Ilovalar» roʻyxatida bitta yozuv.

## Rad etilgan yondashuvlar

- **Sparkle bilan `.pkg` yangilanishlari** — har safar admin paroli, delta yoʻq,
  Sparkle hujjati tavsiya qilmaydi.
- **macOS'da oʻz yangilovchimiz** — Sparkle hal qilgan muammolarni (atomar
  almashtirish, kod imzosi tekshiruvi, qayta ishga tushirish, delta) qayta yozish.
- **Windows'da per-machine + imtiyozli servis** (Chrome usuli) — doim ishlaydigan
  admin servis: hujum yuzasi va murakkablik oshadi, bizga kerak emas.
- **Faqat banner/sayt** (hozirgi holat) — talabga javob bermaydi.
- **Modelni yangilanish paketida qoldirish** — har yangilanish ~800 MB.
