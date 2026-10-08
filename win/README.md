# Kotib — Windows

Oʻzbekcha ovozli yozuv. Istalgan ilovada **Ctrl+Alt+D** bosing, gapiring, yana bosing —
matn kursor turgan joyga yoziladi. Toʻliq oflayn, internetsiz.

macOS versiyasining Windows portlanishi. Bir xil model (`rubaiSTT v2 medium`),
bir xil inference parametrlari, bir xil dizayn.

Uchta boʻlim, macOS'dagi kabi:

| Boʻlim | Nima qiladi |
|---|---|
| **Yozish** | Ctrl+Alt+D bilan diktovka, oxirgi yozuvlar tarixi |
| **Fayl** | ovozli/video faylni tashlang — toʻliq matn, sunʼiy intellekt amallari |
| **Tarjima** | 202 til, oflayn (CTranslate2 + NLLB-200) |

Tenglashtirish ishlarining holati: [../docs/windows/WINDOWS-PARITET.md](../docs/windows/WINDOWS-PARITET.md)

---

## Oʻrnatish

**[Kotib-1.2.1-win-setup.exe](https://cdn.mirqobilov.com/dl/win/Kotib-1.2.1-win-setup.exe)** — 845 MB, yagona fayl.

Ichida hammasi bor: ilova, nutq modeli va **ikkala protsessor turi uchun**
binarlar (Intel/AMD va ARM). Oʻrnatuvchi qaysi biri kerakligini oʻzi
aniqlaydi, foydalanuvchi tanlamaydi. Internet faqat yuklab olish uchun
kerak.

Tarjimon (202 til) ATAYLAB ichida emas: uning modeli 3,1 GB va u faqat
kerak boʻlganda, ilovaning «Tarjima» boʻlimidan yuklab olinadi. Aks holda
tarjimondan foydalanmaydiganlar ham 4 GB yuklab olishga majbur boʻlardi.

1. Faylni yuklab oling va ishga tushiring
2. Birinchi ochilishda Sozlamalar oynasi chiqadi — **mikrofoningizni tanlang**

> **Model yoʻqolib qolsa** (oʻchirib yuborilgan, antivirus olib qoʻygan):
> ilovani ochib «Yozish» boʻlimidagi **«Yuklab olish»** tugmasini bosing —
> model bir marta yuklab olinadi va `%LOCALAPPDATA%\Kotib\models\` ga
> tushadi. Qayta oʻrnatish shart emas.

> **Muhim:** mikrofonni albatta tanlang. Windows'da standart qurilma baʼzan
> "Stereo Mix" boʻlib qoladi — u ovozingizni emas, kompyuter ovozini yozadi.
> Ilova bunday qurilmalarni sariq rangda ogohlantiradi.

### SmartScreen ogohlantirishi

Ilova hozircha kod imzosi sertifikati bilan imzolanmagan, shuning uchun ikki joyda
ogohlantirish chiqishi mumkin:

1. **Edge yuklashda:** "…isn't commonly downloaded" — yuklashlar roʻyxatida faylni
   sichqonchaning oʻng tugmasi bilan bosing → **Keep** → **Keep anyway**.
2. **Ochishda:** "Windows protected your PC" — **"Batafsil maʼlumot" (More info) →
   "Baribir ishga tushirish" (Run anyway)**.

Bu faqat oʻrnatuvchini brauzerdan yuklaganda boʻladi. 1.2 dan keyingi yangilanishlarni
Kotib oʻzi yuklaydi va oʻrnatadi — ularda bu ogohlantirishlar chiqmaydi.

## Ishlatish

| Amal | Qanday |
|---|---|
| Yozishni boshlash / toʻxtatish | **Ctrl+Alt+D** |
| Sozlamalar | Tray ikonkasiga **chap tugma** bilan bosing |
| Menyu (diktovka, log, chiqish) | Tray ikonkasiga **oʻng tugma** |

Tugmani Sozlamalardan oʻzgartirish mumkin.

## Talablar

- Windows 10 (1809, build 17763) yoki Windows 11, **64-bit**
- 4 GB RAM (videokarta bilan) / 6 GB (protsessor rejimida)
- ~1 GB disk
- Videokarta tavsiya etiladi — NVIDIA, AMD yoki Intel, farqi yoʻq

Videokarta boʻlmasa protsessorda ishlaydi, taxminan **8 marta sekinroq**.

## Tezlik (RTX 3060, 208 s audio)

| Rejim | Vaqt | Realtime |
|---|---|---|
| Vulkan (videokarta) | 25 s | 0.12× |
| Protsessor (Ryzen 7 5800X) | 196 s | 0.94× |

Amalda: 10 soniyalik diktovka videokartada ~1 soniyada matnga aylanadi.

## Aniqlik

Google FLEURS oʻzbek datasetida oʻlchangan (yangiliklar uslubidagi murakkab matnlar):
**WER 17%** — yaʼni soʻzlarning ~83% toʻgʻri. Kundalik oddiy gaplarda aniqlik yuqoriroq.

---

## Manbadan build qilish

Ikkita yoʻl bor va **asosiysi macOS'dan cross-compile**: butun ishlab chiqish
MacBook'da ketadi, Windows mashina faqat sinash uchun kerak.

### A) macOS'dan (asosiy yoʻl)

```bash
./win/build-mac.sh              # x64 — foydalanuvchilar uchun
./win/build-mac.sh arm64        # ARM64 — UTM'dagi VM'da sinash uchun
./win/build-mac.sh hammasi      # ikkalasi
./win/build-mac.sh --tez        # kutubxonalarni qayta qurmaydi
./win/build-mac.sh --tarjimasiz # CTranslate2'siz, tez iteratsiya uchun
```

Bir martalik talablar skript ichida yozilgan (llvm-mingw, ninja, shaderc,
glslang, vulkan-headers). Natija: `win/build-<arch>/Kotib.exe` va yonida
whisper.cpp DLL'lari.

whisper.cpp, CTranslate2 va SentencePiece `scripts/bogliqliklar.env` dagi
commit'larga pinlangan — skript ularni oʻzi klonlaydi yoki mavjud nusxa shu
commit'dami, tekshiradi. llvm-mingw boshqa joyda boʻlsa —
`TOOLCHAIN=<papka>`, Homebrew boshqa prefiksda boʻlsa — `BREW_PREFIX=<papka>`.

Nega llvm-mingw va nega GCC-mingw emas: `core/audio_capture.cpp` WASAPI'ni
`__uuidof` bilan chaqiradi — buni faqat clang tushunadi.

### B) Windows'da MSVC bilan

Bu yoʻl buzilmagan holda qoladi, lekin asosiy emas.

```powershell
winget install Kitware.CMake
winget install KhronosGroup.VulkanSDK
winget install JRSoftware.InnoSetup
winget install Microsoft.VisualStudio.2022.BuildTools --override `
    "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64"

powershell -ExecutionPolicy Bypass -File win\build.ps1
```

| Bayroq | Vazifasi |
|---|---|
| `-SkipWhisper` | whisper.cpp allaqachon yigʻilgan boʻlsa oʻtkazib yuboradi |
| `-NoInstaller` | faqat `.exe` yasaydi, oʻrnatuvchisiz |
| `-WorkDir <yoʻl>` | build artefaktlari papkasi (standart `D:\rubai`) |

### Testlar

```bash
./win/tests/mac/hammasi.sh    # macOS'da — hamma tekshiruv
./win/tests/mac/sinov.sh      # faqat birlik testlari, bir soniyada
```

`hammasi.sh` beshta narsani tekshiradi: versiya raqami hamma joyda mosligi
(`scripts/versiya-tekshir.sh`), birlik testlari, whisper parametrlarining
ikkala platformada bir xilligi, matn boʻluvchi va formatlashning korpus
ustidagi tengligi, tarjima koʻprigining tengligi.

Toʻliq toʻplam Windows binarisi boʻlib chiqadi (`kotib-testlar.exe`) va uni
Windows mashinada yoki UTM'dagi VM'da ishga tushirish kerak. VM bilan ishlash
SSH va VNC orqali:

```bash
./win/tools/win.sh --joyla                 # yangi build'ni VM'ga koʻchiradi
./win/tools/win.sh 'd: & testlar.cmd'      # sinovlarni ishga tushiradi
python3 win/tools/vnc.py surat /tmp/a.png  # ekran rasmi
python3 win/tools/vnc.py klik 400 300      # sichqoncha
```

VM'ni birinchi marta tayyorlash va uning tuzoqlari — `docs/windows/WINDOWS-PARITET.md`
→ «VM ustida ishlash».

macOS quvuri ICU chegaralari, UTF-16 surrogatlari va WinHTTP qatlamini
QAMRAMAYDI — tafsilotlar `win/tests/mac/sinov.sh` ning boshida.

### Oʻrnatuvchini yigʻish

`iscc.exe` faqat Windows'da ishlaydi, shuning uchun oʻrnatuvchi VM ichida
yigʻiladi. Skript Inno Setup'ni kerak boʻlsa oʻzi oʻrnatadi:

```bash
./win/tools/ornatuvchi-yasa.sh --model .model-cache/ggml-rubaistt.bin
```

Natija: `dist/Kotib-<versiya>-win-setup.exe` — **yagona fayl**: x64 va
ARM64 binarlari hamda nutq modeli bitta oʻrnatuvchida. Avval ikkala
arxitektura ham qurilgan boʻlishi kerak (`./win/build-mac.sh hammasi`).

`--model` bermasangiz modelsiz variant chiqadi
(`…-win-onlayn-setup.exe`) — u faqat ishlab chiqish uchun.

Har safar yonida avto-yangilanish paketi ham chiqadi —
`Kotib-<versiya>-win-yangilash.exe` (modelsiz, ~21 MB; `reliz.sh win` shuni imzolaydi).

1.2 dan oʻrnatish **faqat joriy foydalanuvchiga** (administrator huquqisiz):
ilova `%LOCALAPPDATA%\Programs\Kotib`, nutq modeli `%LOCALAPPDATA%\Kotib\models`.
1.1.0 dagi `C:\Program Files\Kotib` nusxasi birinchi oʻrnatishda koʻchiriladi —
model nusxalanadi, eskisi bir marta UAC soʻrab olib tashlanadi, avtostart yangi
yoʻlga oʻtadi. Tafsilotlar va VM'da sinalgan holatlar — `AGENTS.md` → «Per-user since 1.2».

### Yordamchi vositalar

```powershell
# Yadroni tekshirish
rubai-cli.exe audio.wav --model <yoʻl>    # fayldan transkripsiya
rubai-cli.exe --mics                      # mikrofonlar roʻyxati
rubai-cli.exe --record 5                  # 5 soniya yozib transkripsiya

# Aniqlikni oʻlchash (FLEURS datasetida)
python win\tools\bench_accuracy.py --cli <whisper-cli.exe> --model <model> -n 20
```

---

## Kod tuzilishi

```
win/
  core/                UI'dan mustaqil yadro
    whisper_bridge.c   whisper.cpp ustidan C shim (macOS bilan bir xil parametrlar)
    engine.cpp         model yuklash, isitish, navbat, RAM'ni boʻshatish
    audio_capture.cpp  WASAPI: mikrofon -> 16 kHz mono float32
    samples.cpp        signal normalizatsiyasi va sifat tekshiruvi
    media_decode.cpp   Media Foundation: istalgan audio/video -> 16 kHz mono
    ish.cpp            fayl transkripsiyasi (uzun fayllar boʻlaklanadi)
    hujjat.cpp         transkript ombori (%APPDATA%\Kotib\hujjatlar)
    tarix.cpp          diktovka tarixi
    matn_format.cpp    segmentlardan oʻqishga qulay matn
    llm.cpp            ixtiyoriy LLM qatlami (WinHTTP, SSE)
    amallar.cpp        Studiya amallari va prompt'lari
    tarjima_bridge.cpp CTranslate2 + SentencePiece ustidan C koʻprik
    tarjimon.cpp       tarjima navbati, idle boʻshatish
    matn_boluvchi.cpp  jumlalarga boʻlish (ICU), havolalarni himoyalash
    tillar.cpp         202 til
    json.cpp           qoʻlda yozilgan JSON oʻquvchi/yozuvchi
    statistika.cpp     anonim statistika (ping, amal oʻlchovlari)
    yangilovchi.cpp    avto-yangilanish: jadval, manifest, yuklash, imzo, oʻrnatish
    yangilanish_siyosat.cpp  yangilanishning sof qarorlari (macOS bilan umumiy jadval)
    yuklovchi.cpp      katta faylni Range bilan yuklab olish (davom ettiriladi)
    model_yuklovchi.cpp  nutq modeli (785 MB)
    tarjima_yuklovchi.cpp  tarjima modeli (3,1 GB arxiv)
    config.cpp         sozlamalar fayli
    wav.cpp            WAV oʻquvchi (testlar uchun)
    util.cpp           kodlash, yoʻllar, log
  ui/
    app.cpp            WinMain, tray, hotkey, asosiy oqim
    asosiy_oyna.cpp    uch tabli asosiy oyna (Direct2D)
    uslub.cpp          dizayn tokenlari va chizish qatlami
    vidjet.cpp         chizilgan boshqaruv elementlari
    yozish_tab.cpp / fayl_tab.cpp / tarjima_tab.cpp
    matn_maydon.cpp    RichEdit ustidagi tahrirlanadigan matn maydoni
    soragich.cpp       kichik modal yordamchilar (fayl, matn, bufer)
    overlay.cpp        suzuvchi holat oynasi (fokusni oʻgʻirlamaydi)
    settings_window.cpp
    inserter.cpp       clipboard + Ctrl+V / Unicode yozish
    autostart.cpp      registry Run kaliti
  cmake/               cross-compile toolchain fayllari
  installer/
    rubai.iss          Inno Setup (UTF-8 + BOM boʻlishi SHART)
    Uzbek.isl          sehrgarning oʻzbekcha xabarlari
  tests/               testlar (+ mac/ — macOS quvuri, vm/ — VM skriptlari)
  tools/
    rubai_cli.cpp      yadroni buyruq satridan sinash
    ui_demo.cpp        UI'ni yadrosiz sinash
    win.sh             VM'da buyruq bajarish, build koʻchirish (SSH)
    vnc.py             VM ekranini oʻqish va boshqarish (RFB)
    vm.sh              UTM boshqaruvi, sinov diskini yasash
    ornatuvchi-yasa.sh oʻrnatuvchini VM ichida yigʻish
    zip-yasa.sh        portativ ZIP
```

Fayllar va papkalar:

| Nima | Qayerda |
|---|---|
| Sozlamalar | `%APPDATA%\Kotib\settings.ini` |
| Hujjatlar (Fayl boʻlimi) | `%APPDATA%\Kotib\hujjatlar\` |
| Diktovka tarixi | `%APPDATA%\Kotib\diktovka-tarixi.json` |
| Log | `%LOCALAPPDATA%\Kotib\dictation.log` |
| Tarjima modeli | `%LOCALAPPDATA%\Kotib\tarjima-model-33b\` |
| Ilova | `%LOCALAPPDATA%\Programs\Kotib\` (1.1.0 gacha `C:\Program Files\Kotib`) |
| Nutq modeli | `%LOCALAPPDATA%\Kotib\models\ggml-rubaistt.bin` |
| Yangilanish fayllari | `%LOCALAPPDATA%\Kotib\yangilanish\` |
| API kalit | Windows Credential Manager (`Kotib.llm.<provayder>`) |

1.0 versiyasidan yangilanganda `%APPDATA%\Audio-Matnga` papkasi bir marta
`Kotib` ga koʻchiriladi (`core/util.cpp`, `eskiPapkadanKochir`).

---

## macOS versiyasidan farqlar

**Yoʻq qilingan murakkabliklar:** Accessibility ruxsati, Gatekeeper, notarize —
Windows'da bularning hech biri kerak emas.

**Qoʻshilgan funksiyalar:**

- **Mikrofon tanlash** — Windows'da standart qurilma "Stereo Mix" boʻlib qolishi
  mumkin, u ovoz oʻrniga kompyuter ovozini yozadi
- **"Jim oqim" aniqlash** — Iriun, OBS, VB-Cable va ulanmagan Bluetooth
  qurilmalari `OK` holatida koʻrinib, aslida sukunat beradi
- **Bluetooth HFP ogohlantirishi** — quloqchin mikrofoni 8–16 kHz ga tushadi
- **GPU quvurini oldindan isitish** — Vulkan birinchi ishga tushishda shader'larni
  kompilyatsiya qiladi (~12 s). Bu ilova ochilganda fonda bajariladi, aks holda
  foydalanuvchining birinchi diktovkasi juda sekin boʻlardi
- **Kirill/lotin boʻlmagan yoʻllar** — model ASCII yoʻlda saqlanadi, chunki
  whisper.cpp fayl yoʻlini `char*` sifatida ochadi

## Litsenziya

MIT. Model va whisper.cpp oʻz litsenziyalari ostida.
