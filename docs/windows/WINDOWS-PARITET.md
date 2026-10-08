# Windows paritet rejasi

Maqsad: **Windows'da ham macOS «Kotib» ning toʻliq nusxasi** — diktovka, Studiya
(Fayl tabi), Tarjimon, LLM amallar, tarix, sozlamalar. Ustiga ikkala platformaga
umumiy ikkita yangi narsa: **anonim statistika** va **ilova ichida yangilanish xabari**.

Holat sanasi: **2026-09-25**. Bu fayl ish davomida yangilanadi; bajarilgani
`AGENTS.md` ga koʻchadi va bu yerdan oʻchiriladi.

---

## Qayerda toʻxtadik (yangi sessiya shu yerdan boshlasin)

**Windows tomoni endi VM'da toʻliq ishlatib koʻrildi.** Ilgari eng katta
toʻsiq VM'ga build olib kirish edi; u yechildi (pastda «VM ustida ishlash»).

**Ishlaydi va VM'da koʻz bilan tekshirilgan:**

- Uchala tab: **Yozish** (banner, karta, tarix, kontekst menyusi),
  **Fayl/Studiya** (drag&drop, transkripsiya, natija, amallar),
  **Tarjima** (202 til, natija, nusxa olish).
- Sozlamalar oynasi: qoʻshimcha boʻlim, surish, mikrofon, AI boʻlimi.
- Nutq modelini ilova ichida yuklab olish — 785 MB, bekor qilish va
  `.part` dan davom ettirish bilan.
- Yangi versiya banneri (`stat.mirqobilov.com` javobi bilan).
- Oʻrnatuvchi: oʻrnatish, yorliqlar, avtostart, oʻchirish.
- x64 build ham oʻsha VM'da (Prism emulyatsiyasi) ishlaydi — 244 test,
  transkripsiya, UI.

**VM'da sinab boʻlmaydigan narsalar** — Vulkan / GPU (mehmon tizimda virtual
drayver yoʻq), haqiqiy nutq bilan diktovka va matnni joyiga qoʻyish
(`ui/inserter.cpp`, VM'ning kirish qurilmasi jimlik beradi), LLM amallari
(API kalit kerak).

**Haqiqiy Windows kompyuterda sinaldi (2026-09-25):** `1.1.0` muvaffaqiyatli
ishladi va foydalanuvchilar ishlatmoqda. Keyingi Windows oʻzgarishlarida ham
yuqoridagi uch narsa VM'da emas, haqiqiy kompyuterda tekshiriladi.

---

## VM ustida ishlash

VM: UTM'da **Kotib-Win11-ARM-5** (Windows 11 25H2 ARM64, avtologin `kotib`).

```bash
./win/tools/win.sh 'd: & testlar.cmd'     # buyruq bajarish (SSH)
./win/tools/win.sh --joyla                # yangi build'ni D: ga koʻchirish
python3 win/tools/vnc.py surat /tmp/a.png # ekran rasmi
python3 win/tools/vnc.py tugma super+r    # tugma bosish
python3 win/tools/vnc.py klik 400 300     # sichqoncha
python3 win/tools/vnc.py sudra 100 200 500 300   # sudrab tashlash
./win/tools/ornatuvchi-yasa.sh --model <yoʻl>   # oʻrnatuvchini yigʻish
```

Uchta narsa buni mumkin qildi:

1. **virtio drayverlari** (`virtio-win.iso` → `w11/ARM64`). NetKVM bilan
   VM'da tarmoq paydo boʻldi; shundan keyin OpenSSH server oʻrnatildi va
   port UTM'ning foydalanuvchi tarmogʻi orqali 2222 ga yoʻnaltirildi.
   Endi build'ni koʻchirish 15 daqiqa emas, 20 soniya oladi va
   **Full Disk Access ham, sinov diskini almashtirish ham kerak emas.**

2. **QEMU'ning oʻz VNC serveri** — UTM konfiguratsiyasidagi
   `AdditionalArguments = ["-vnc", "127.0.0.1:1"]` orqali. Nega kerak:
   UTM'ning `input scan code` API'si kengaytirilgan skan kodlarini (Win,
   strelkalar) yubormaydi, `screencapture` esa macOS ekrani qulflangan
   boʻlsa oyna rasmini ololmaydi. VNC ikkalasini ham yechadi.
   Roʻyxat ODDIY satrlardan iborat boʻlishi shart — lugʻat koʻrinishida
   UTM konfiguratsiyani «yaroqsiz» deb rad etadi.

3. **Sichqoncha ikki yoʻldan boshqariladi.** Bosish UTM'ning
   `input mouse click` API'si orqali (u absolyut). Sudrash esa VNC orqali:
   UTM'da tugmani bosib turish yoʻq. VNC koordinatalari QEMU'da NISBIY
   siljishga aylanadi (usb-tablet va usb-mouse ikkalasi ham ulangan),
   shuning uchun avval kursor burchakka qisiladi va mehmonda sichqoncha
   tezlanishi oʻchirilgan boʻlishi shart (`win/tools/vm-sichqoncha.ps1`).

Ovoz qurilmasi UTM konfiguratsiyasida `Sound = [{Hardware: intel-hda}]`
bilan qoʻshildi — busiz mehmonda yozib oluvchi qurilma umuman yoʻq edi va
diktovka yoʻli ishga tushmasdi.

---

## Qabul qilingan qarorlar

**1. UI — sof native Win32 + Direct2D/DirectWrite. WebView2 emas.**
Muhokama qilindi va ataylab rad etildi: WebView2 dizaynni tezroq koʻchirardi, lekin
oyna ochiq turganda ~200 MB xotira va tashqi runtime talab qilardi. Foydalanuvchilarning
katta qismi zaif mashinalarda, shuning uchun bitta mustaqil `.exe`, ~40 MB xotira va
bir zumda ochiladigan oyna tanlandi. Narxi — UI kodi ~3 barobar koʻp.

**Yagona istisno — tahrirlanadigan matn maydonlari.** Ular RichEdit ustida
quriladi (`ui/matn_maydon.cpp`): matn tanlash, Ctrl+Z, IME va ekran
oʻqiruvchilar bilan ishlashni qaytadan yozish bir yillik ish. Ota oynada
`WS_CLIPCHILDREN` boʻlishi SHART — busiz Direct2D bola oynani bosib ketadi.

**2. Build macOS'da, cross-compile bilan.** `llvm-mingw` (clang) `~/Developer/.toolchains/`
da. MSVC yoʻli (`win/build.ps1`) buzilmagan holda qoladi, lekin asosiy yoʻl emas.
Nega GCC-mingw emas: `core/audio_capture.cpp` WASAPI'ni `__uuidof` bilan chaqiradi,
uni faqat clang tushunadi.

**3. Ikkita arxitektura: x64 va ARM64.** x64 — foydalanuvchilar uchun. ARM64 — UTM
ichidagi Windows 11 ARM VM'da **tabiiy tezlikda** sinash uchun (x64 nusxasi u yerda
emulyatsiyada, sekin ishlaydi). ARM64 build yon foyda sifatida Snapdragon X
noutbuklariga ham toʻgʻri keladi.

**4. `rubai_transcribe` ikkala platformada bayt-bayt bir xil qoladi** — `AGENTS.md`
dagi qoida. Windows'ga `rubai_transcribe_segments` qoʻshildi, undagi yagona ataylab
farq: `no_timestamps = false`.

**5. Sozlamalar oynasi native boshqaruv elementlarida qoladi.** macOS'dagi
sheet qayta chizilmadi: Windows'da tizim boshqaruvlari (HOTKEY, COMBOBOX,
EDIT) klaviatura, ekran oʻqiruvchi va IME bilan tayyor ishlaydi, dizayn
foydasi esa kichik. Funksional paritet toʻliq: apostrof, statistika va
sunʼiy intellekt boʻlimlari qoʻshildi.

**6. Havolalarni himoyalash macOS'dagidan torroq.** macOS `NSDataDetector`
ishlatadi; Windows'da unday aniqlagich yoʻq. Shuning uchun faqat SHUBHASIZ
havolalar himoyalanadi (sxemali manzil, `www.`, ichida `/` bor domen, email,
`@handle`, `#hashtag`). `example.uz` kabi yalangʻoch domen ataylab
himoyalanmaydi: uni terim xatosidan ajratib boʻlmaydi va notoʻgʻri himoya
jumlani tarjimasiz qoldirardi.

---

## Sinash imkoniyatlari va cheklovi

| Nima | Qayerda sinaladi |
|---|---|
| Sof mantiq (matn, vaqt, tarjima boʻluvchisi, JSON, LLM, versiya) | **macOS'da**, `win/tests/mac/sinov.sh` — bir soniyada |
| macOS va Windows boʻluvchilarining bir xilligi | **macOS'da**, `win/tests/mac/taqqoslash/taqqosla.sh` |
| Tarjima koʻprigining bir xilligi | **macOS'da**, `win/tests/mac/tarjima-taqqosla.sh` — ikkala koʻprik bir xil model bilan |
| ICU chegaralari, UTF-16 surrogatlari, WinHTTP | faqat Windows'da (`kotib-testlar.exe`) |
| Diktovka, Studiya, Tarjima, UI | UTM / Windows 11 ARM64 VM, ARM64 build — tabiiy tezlik |
| x64 binarining ishlashi | oʻsha VM, Prism emulyatsiyasi — funksional, sekin (2× realtime) |
| Tarmoq: yangilanish, statistika, model yuklash | VM'da — NetKVM drayveri bilan ishlaydi |
| Oʻrnatuvchi | VM'da yigʻiladi va oʻsha yerda oʻrnatib sinaladi |
| **Vulkan / GPU tezligi** | **VM'da umuman sinab boʻlmaydi** — Windows mehmoni uchun virtual Vulkan drayveri yoʻq. Faqat haqiqiy Windows kompyuterda tasdiqlanadi. |
| **Haqiqiy nutq bilan diktovka** | VM'ning kirish qurilmasi jimlik beradi. Yozib olish yoʻli sinaladi, matnni joyiga qoʻyish (`inserter.cpp`) — yoʻq |
| **LLM amallari** | API kalit kerak |

Shu sababli **CPU yoʻli birinchi darajali**: GPU topilmasa ilova sekinlashishi mumkin,
lekin **hech qachon ishdan chiqmasligi** kerak.

---

## Bosqichlar

### 1. Poydevor — TUGADI

- [x] `llvm-mingw` toolchain, `ninja`, `glslc` (shaderc), Vulkan sarlavhalari
- [x] `win/cmake/toolchain-win-{x64,arm64}.cmake`
- [x] whisper.cpp x64 va ARM64 uchun cross-build (Vulkan + 15 CPU varianti)
- [x] Backend'lar ish paytida yuklanadigan DLL sifatida (`GGML_BACKEND_DL`)
- [x] `win/tools/make_icon.sh` — `.ico` ni macOS'da yasaydi
- [x] `win/build-mac.sh` — bitta buyruq bilan hammasi

### 2. Windows 11 ARM VM — TUGADI

- [x] Windows 11 25H2 ARM64 oʻrnatildi va ishlaydi (avtologin, `KOTIB-TEST`)
- [x] **Muhim tuzoq:** QEMU'ning USB CD emulyatsiyasidan Windows yuklovchisi
      ishga tushmaydi — FAT32 USB disk tasviri kerak (`wimlib-imagex split`)
- [x] **VM'ni boshqarish:** `./win/tools/vm.sh` — UTM'ning oʻz AppleScript
      API'si ustida (`yoz`, `klik`, `surat`, `ishga`, `toxta`, `yangila`).
      Fokus talab qilmaydi va belgilarni buzmaydi. System Events'ning
      `keystroke` usuli raqamlarni tushirib qoldiradi (bir marta `dism`
      oʻrniga `daism` yozilgan)
- [x] `kotib-testlar.exe` — **244 ta tekshiruv** oʻtdi (ARM64 va x64 da)
- [x] `ui-demo.exe` — oyna dizaynga mos chizildi
- [x] **Tarmoq va SSH** — virtio ARM64 drayverlari + OpenSSH server
      (`win/tests/vm/sozla.cmd`). Sinov diskini almashtirish ham,
      Full Disk Access ham endi kerak emas
- [x] **VNC** — ekranni oʻqish va toʻliq klaviatura (`win/tools/vnc.py`)
- [x] Ovoz qurilmasi (`intel-hda`) — WASAPI yoʻli ishga tushdi
- [x] `rubai-cli.exe` bilan yadro tekshiruvi: WAV, MP3 (Media Foundation),
      `--segmentlar`, tarjima

### 3. Nomni «Kotib» ga oʻtkazish — TUGADI

- [x] `win/CMakeLists.txt`, `res/app.rc` (1.1.0), `res/app.manifest`, `ui/*.cpp`
- [x] `%APPDATA%\Audio-Matnga` → `%APPDATA%\Kotib` koʻchishi
- [x] Avtostart registr yozuvi ham koʻchdi
- [x] `installer/rubai.iss` — yangi AppId, eski ikkala oʻrnatmani oʻchiradi

### 4. Native UI qatlami — TUGADI

- [x] `ui/uslub.cpp` — dizayn tokenlari va Direct2D chizish qatlami
- [x] `ui/vidjet.cpp` — tugma, tanlagich, roʻyxat, ochirgich, konteyner
- [x] `ui/asosiy_oyna.cpp` — toolbar, tab almashtirish, DPI, qurilma yoʻqolishi
- [x] `ui/matn_maydon.cpp` — RichEdit ustidagi tahrirlanadigan matn maydoni
- [x] `ui/soragich.cpp` — matn soʻrash, fayl tanlash/saqlash, bufer
- [x] `tools/ui_demo.cpp` — UI'ni yadrosiz sinash uchun
- [x] Klaviatura navigatsiyasi VM'da tekshirildi (Tab, Enter, Esc —
      `IsDialogMessageW`); ekran oʻqiruvchi bilan sinalmagan

### 5. Uch tab — TUGADI

- [x] **Yozish** — banner, «Bosing va gapiring» kartasi, tarix, toʻlqin, taymer
- [x] **Fayl (Studiya)** — drag&drop, hujjatlar, transkript, LLM amallar
- [x] **Tarjima** — 202 til, ⇄ almashtirish, natija, model banneri
- [x] **Sozlamalar** — tugma, mikrofon, avtostart, apostrof, statistika,
      sunʼiy intellekt, log
- [x] Uchalasi ham VM'da ishlatib koʻrildi (2026-09-10)

### 6. Studiya yadrosi — TUGADI

- [x] `rubai_transcribe_segments` porti
- [x] `core/media_decode.cpp` — Media Foundation
- [x] `core/matn_format.cpp` — testlar bilan
- [x] `core/hujjat.cpp`, `core/ish.cpp`, `core/tarix.cpp`
- [x] `core/llm.cpp` — WinHTTP ustida LLM klient, SSE oqimi, model roʻyxati
- [x] `core/amallar.cpp` — amallar va prompt'lar (macOS bilan belgima-belgi)
- [x] API kalit — Windows Credential Manager

### 7. Tarjimon — TUGADI

- [x] CTranslate2 + SentencePiece Windows uchun cross-build (x64 va ARM64)
- [x] `tarjima_bridge.cpp` porti — macOS bilan bir xil parametrlar
- [x] `matn_boluvchi.cpp` — ICU dinamik yuklanadi, zaxira qoida bilan
- [x] 202 til jadvali (nomlar macOS `Locale` dan koʻchirilgan)
- [x] Model yuklovchi (3,1 GB arxiv, disk joyi tekshiruvi, Range, `tar.exe`)

### 8. Statistika va yangilanish — TUGADI

- [x] Cloudflare Worker + D1, admin panel, macOS va Windows mijozlari
- [x] Sozlamalarda oʻchirish tugmasi (ikkala platformada)
- [x] Windows UI'da yangilanish banneri
- [x] README va saytda maxfiylik yozuvi

### 9. Oʻrnatuvchi va reliz

- [x] `installer/rubai.iss` — «Kotib» nomi, mingw va MSVC ikkalasiga mos
- [x] `installer/Uzbek.isl` — sehrgarning 296 ta xabari oʻzbekchada
- [x] `tools/ornatuvchi-yasa.sh` — oʻrnatuvchi VM ichida yigʻiladi
- [x] Oʻrnatish, yorliq, avtostart va oʻchirish VM'da sinaldi
- [x] Model yuklab olish sinaldi (bekor qilish va davom ettirish bilan)
- [x] Yagona universal fayl: x64 + ARM64 + nutq modeli bitta `.exe` da
- [x] R2 ga yuklandi — `dl/v1.1/Kotib-1.1.0-win-setup.exe` (CDN'dan tekshirilgan)
- [ ] macOS `.pkg` — notarizatsiya maʼlumotlari kerak (`KEYINGI-ISH.md` §1)
- [ ] Sayt deploy — Mac fayli chiqqandan KEYIN
- [ ] Yangi public repo: faqat README, skrinshotlar, yuklab olish havolalari

---

## Ochiq savollar

- **Kod imzosi.** Windows ilovasi imzolanmagan — SmartScreen ogohlantiradi. Sertifikat
  yiliga ~$200–400. Hozircha README'da tushuntiriladi.
- **Tarjima modeli 3,36 GB.** Zaif Windows mashinalarida RAM yetadimi? macOS'da
  batch=1 da 2,65 GB. 8 GB mashinada whisper bilan birga ogʻirlik qiladi.
- ~~**ARM64 da GPU yoʻq.**~~ Hal qilindi (2026-09-10): ARM64 build ham
  Vulkan bilan yigʻiladi. `GGML_CPU_ALL_VARIANTS` esa ARM+Windows'da
  qoʻllab-quvvatlanmaydi (ggml «Unsupported ARM target OS: Windows» deydi),
  shuning uchun u faqat x64 da yoqiladi va ARM64 bitta umumiy CPU DLL oladi.
