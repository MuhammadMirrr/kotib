# Audio-Matnga — Windows port rejasi

macOS'dagi `src/dictate.swift` (Swift/AppKit) + `src/whisper_bridge.c` ilovasini
Windows'ga ko'chirish rejasi. Mezon: **eng puxta natija**, vaqt cheklov emas.

Sana: 2026-08-07 · Maqsad platforma: Windows 10 1809+ / Windows 11, x64

---

## HOLAT (2026-08-07)

**Ishlaydigan ilova va o'rnatuvchi tayyor.** `dist\Audio-Matnga-1.0.0-setup.exe` (8 MB).

| Bosqich | Holat |
|---|---|
| 0b — Toolchain | ✅ CMake, VS Build Tools, Vulkan SDK, Inno Setup |
| 0c — Vulkan build + benchmark | ✅ Vulkan tanlandi (§4) |
| 1 — Yadro (`core/`) | ✅ |
| 2 — Audio kirish (WASAPI) | ✅ |
| 3 — Tray + hotkey + matn joylash | ✅ |
| 4 — Overlay | ✅ |
| 5 — Sozlamalar + avtostart | ✅ |
| 5 — **Fayl transkripsiyasi** | ❌ **bajarilmadi** |
| 6 — O'rnatuvchi, ikonka, hujjatlar | ✅ |
| 6 — **Unit testlar, crash handler** | ❌ **bajarilmadi** |
| 6 — **Kod imzosi** | ❌ sertifikat kerak (§10) |
| 6 — **web/index.html Windows bo'limi** | ❌ bajarilmadi |

### Sinovda topilgan va tuzatilgan xatolar

Bular haqiqiy sinov paytida aniqlandi — har biri obunachilarda muammo bo'lardi:

1. **Vulkan shader kompilyatsiyasi 11.7 s** — birinchi diktovka shuncha kutar edi.
   Yechim: model yuklangandan keyin fonda "isitish" (`Engine::warmUp`).
2. **Kirill qurilma nomlari** — `towlower()` kirillni kichraytirmaydi, shuning uchun
   "Стерео микшер" mikrofon deb tavsiya qilinardi. Yechim: `CharLowerBuffW`.
3. **Sozlamalar oynasi orqada qolardi** — Windows fon jarayoniga oynani oldinga
   chiqarishga ruxsat bermaydi. Yechim: `AttachThreadInput` bilan foreground olish.
4. **O'rnatuvchidagi HKCU** — administrator huquqida ishlagani uchun avtostart
   noto'g'ri profilga yozilardi. Yechim: avtostartni ilovaning o'zi yoqadi.
5. **Ikkita RT_MANIFEST resursi** — linker o'z manifestini ham qo'shar edi (CVT1100).
   Yechim: `/MANIFEST:NO`.
6. **`Engine` da data race** — `loaded`/`backend` qulfsiz yozilardi. Yechim: atomik
   flag + alohida qulf.
7. **PowerShell skriptlari BOM'siz UTF-8** — Windows PowerShell 5.1 BOM bo'lmaganda
   faylni tizim kod sahifasida o'qiydi. Rus tilidagi Windows'da (cp1251) `—` belgisi
   `вЂ"` ga aylanib, ichidagi qo'shtirnoq satrni buzardi va build umuman ishlamas edi.
   Yechim: `.ps1` fayllar UTF-8 BOM bilan saqlanadi.

### O'lchangan ko'rsatkichlar

| Nima | Qiymat |
|---|---|
| Aniqlik (FLEURS uz, 20 namuna) | WER **17.0%** |
| Tezlik, Vulkan (RTX 3060) | 0.12× realtime |
| Tezlik, CPU (Ryzen 7 5800X) | 0.94× realtime |
| Ilova + kutubxonalar | 57 MB |
| O'rnatuvchi | 8 MB (model alohida yuklanadi) |
| RAM (model yuklangan) | ~262 MB + 1.2 GB VRAM |

---

## 1. Tanlangan stek va sabab

**C++20 / Win32 native**, whisper.cpp statik linklanadi, bitta self-contained `.exe`.

| Sabab | Izoh |
|---|---|
| Statik linklash | macOS `libwhisper.a` bilan bir xil. DLL chegarasi, marshalling, versiya konflikti yo'q |
| `whisper_bridge.c` qayta ishlatiladi | So'zma-so'z ko'chadi — inference parametrlari ikki platformada **bit-to-bit bir xil** |
| Runtime bog'liqligi yo'q | .NET/Python runtime kerak emas |
| Asl kod Win32 uslubida | Swift kodi Auto Layout ishlatmaydi, hamma joylashuv qo'lda `NSRect` — bu `CreateWindowEx` + qo'lda koordinatalarga mexanik ko'chadi |
| Sinxronlik | Ikki platforma arxitekturasi 1:1 mos qoladi, kelajakdagi o'zgarishlar ikkalasiga oson qo'llanadi |

**Rad etilgan variantlar:** C#/.NET (runtime bog'liqligi, NativeAOT WinForms'ni qo'llab-quvvatlamaydi),
Whisper.net (inference parametrlari ustidan nazorat yo'qoladi), Python (paket og'ir, start sekin).

**Tan olingan zaiflik:** raw Win32 UI kodi — xatoliklar to'planadigan joy. Buni yadroni UI'dan
ajratish, unit testlar va golden-audio regressiya testi bilan qoplaymiz.

---

## 2. 0-bosqich spike natijalari (BAJARILDI ✅)

Tekshirildi: whisper.cpp v1.9.2 prebuilt CUDA binary + loyihaning `ggml-rubaistt.bin` modeli.

| Tekshiruv | Natija |
|---|---|
| Model formati v1.9.2 bilan mos | ✅ `type = 4 (medium)`, `ftype = 7` (q8_0), `qntvr = 2` — muammosiz yuklandi |
| CUDA backend | ✅ RTX 3060, compute 8.6, `ARCHS` ro'yxatida `860` bor |
| `flash_attn = 1` | ✅ ishlaydi |
| beam search 5 | ✅ ishlaydi |
| VRAM | 822 MB model + ~390 MB buferlar ≈ **1.2 GB** (12 GB dan) |
| Tezlik (10 s audio, GPU) | encode 154 ms · **jami 1.02 s** |
| Tezlik (10 s audio, CPU 8 thread) | encode 3448 ms · jami 4.53 s |

Model sinus tovushga `musiqa` deb javob berdi — bu `dictate.swift:101` izohida tasvirlangan
aynan o'sha xatti-harakat. Model macOS'dagidek ishlaydi.

**Qolgan tekshiruv:** haqiqiy o'zbekcha nutq bilan aniqlikni tasdiqlash (foydalanuvchi ovozi kerak).

### Test muhiti

```
D:\rubai-spike\
  ggml-rubaistt.bin              785 MB — loyiha relizidan
  whisper-cuda\Release\          whisper.cpp v1.9.2 CUDA prebuilt
```

Sinov buyrug'i:
```powershell
& "D:\rubai-spike\whisper-cuda\Release\whisper-cli.exe" `
    -m "D:\rubai-spike\ggml-rubaistt.bin" -f <audio.wav> -l uz -bs 5 -nt
```

---

## 3. macOS → Windows API xaritasi

| Vazifa | macOS (`dictate.swift`) | Windows |
|---|---|---|
| Inference | whisper.cpp + **Metal** | whisper.cpp + **CUDA** (yoki Vulkan — §4) |
| C shim | `whisper_bridge.c` | **o'zgarishsiz** |
| Mikrofon | `AVAudioEngine` + `AVAudioConverter` | **WASAPI** `IAudioClient` (`AUTOCONVERTPCM` bilan to'g'ridan-to'g'ri 16 kHz mono f32) |
| Qurilma o'zgarishi | Har `start()`da yangi `AVAudioEngine` | `IMMNotificationClient` + `AUDCLNT_E_DEVICE_INVALIDATED` ishlovi, har `start()`da yangi client |
| Menyu-bar | `NSStatusItem` | `Shell_NotifyIcon` (tray) + `WM_TASKBARCREATED` qayta tiklash |
| Global hotkey | Carbon `RegisterEventHotKey` | `RegisterHotKey` + `WM_HOTKEY` (`MOD_NOREPEAT`) |
| Matn kiritish | `CGEvent` ⌘V + Accessibility | `SendInput` Ctrl+V (**ruxsat kerak emas**) |
| Overlay | `NSPanel` `.nonactivatingPanel` + `NSVisualEffectView` | `WS_EX_LAYERED\|TOOLWINDOW\|NOACTIVATE\|TOPMOST` + DWM acrylic backdrop |
| Fayl → audio | `AVAssetReader` | **Media Foundation** `IMFSourceReader` (mp3/mp4/m4a/wav/wma native) |
| Sozlamalar | `UserDefaults` | JSON: `%APPDATA%\RubaiSTT\settings.json` |
| Avto-ishga tushish | `SMAppService` / LaunchAgent | Registry `HKCU\...\CurrentVersion\Run` |
| Log | `~/rubai-stt/dictation.log` | `%LOCALAPPDATA%\RubaiSTT\dictation.log` |
| Model | `~/rubai-stt/models/` yoki bundle | `%LOCALAPPDATA%\RubaiSTT\models\` yoki `.exe` yonida (portable) |
| Paket | `.app` + DMG + notarize | `.exe` + Inno Setup + Authenticode |
| Ruxsatlar | Mikrofon **+ Accessibility** (TCC) | Faqat mikrofon (Windows Privacy) |

### Yo'qoladigan murakkabliklar ✅
Accessibility ruxsati, `AXIsProcessTrusted`, Gatekeeper, notarize, "Open Anyway",
universal binary (lipo), entitlements — **hech biri kerak emas**.

### Yangi murakkabliklar ⚠️
- **UIPI** — administrator huquqi bilan ishlayotgan oynaga `SendInput` jimgina ishlamaydi
- **SmartScreen** — imzosiz `.exe` uchun ogohlantirish (kod imzosi sertifikati kerak)
- **Explorer restart** — tray ikonasi yo'qoladi, `WM_TASKBARCREATED` bilan tiklanadi
- **Mikrofon zoosi** — quyida

---

## 4. Hal qilinishi kerak: GPU backend

Prebuilt CUDA paketining hajmi muammo:

| Fayl | Hajm |
|---|---|
| `ggml-cuda.dll` | 512 MB (8 ta GPU arxitekturasi uchun kompilyatsiya qilingan) |
| `cublasLt64_12.dll` | 452 MB |
| `cublas64_12.dll` | 95 MB |
| **Jami** | **~1.1 GB** |

Variantlar:

**A) O'z CUDA buildimiz** — `CMAKE_CUDA_ARCHITECTURES` ni faqat kerakli arxitekturalar bilan
cheklash (masalan `75;86;89;120` — Turing/Ampere/Ada/Blackwell), PTX'siz (`-real`).
`ggml-cuda.dll` sezilarli qisqaradi, lekin `cublasLt` (452 MB) NVIDIA redistributable — qisqarmaydi.

**B) Vulkan backend** — `ggml-vulkan.dll` bir necha MB, **NVIDIA + AMD + Intel** GPU'larda ishlaydi.
Tezlik CUDA'dan biroz past, lekin CPU'dan ancha yuqori. Universal tarqatish uchun ideal.

**C) Ikkalasi + CPU fallback** — o'rnatuvchi GPU'ni aniqlab kerakli backendni yuklab oladi.

**QAROR (tarqatish talabidan kelib chiqib): B — Vulkan asosiy backend.**

Ilova obunachilarga tarqatilgani uchun u **NVIDIA'siz kompyuterlarda ham** ishlashi shart.
CUDA faqat NVIDIA'da ishlaydi va +1.1 GB qo'shadi — ommaviy tarqatish uchun yaramaydi.

Yakuniy konfiguratsiya:
- **Vulkan** — asosiy GPU backend (NVIDIA + AMD + Intel, drayverlar Win10/11 da odatda mavjud)
- **CPU** — fallback, GPU/Vulkan bo'lmasa
- **CUDA** — ixtiyoriy alohida build ("NVIDIA tezkor versiya"), keyingi bosqichda

Vulkan vs CUDA benchmarki baribir o'tkaziladi — natija README'da e'lon qilinadi va
CUDA versiyasi kerakligini hal qiladi.

### Eski CPU'lar bilan moslik ⚠️

whisper.cpp v1.9.2 relizi `ggml-cpu-sandybridge.dll` … `ggml-cpu-icelake.dll` — **9 ta**
CPU variantini tashiydi va runtime'da mos kelganini tanlaydi. Bitta statik `.exe` qilsak,
bitta CPU bazasini tanlashga majbur bo'lamiz (masalan AVX2 — 2013 yilgacha bo'lgan
protsessorlarda ilova umuman ishga tushmaydi).

Obunachilarning eski kompyuterlari bo'lishi mumkinligi uchun **backend DLL dispatch**
tanlanadi: `GGML_BACKEND_DL=ON` + `GGML_CPU_ALL_VARIANTS=ON`.

Ya'ni §1 dagi "bitta statik `.exe`" ideali qisman o'zgaradi: `RubaiSTT.exe` + bir nechta
kichik `ggml-*.dll` (har biri ~0.8 MB). O'rnatuvchi bularni yashiradi, foydalanuvchi
farqni sezmaydi. Bu **moslik uchun ongli almashtirish** — ilova Sandy Bridge'dan
(2011) boshlab har qanday x64 kompyuterda ishga tushadi.

---

## 5. Mikrofon: shu mashinadagi holat

Aniqlangan kirish qurilmalari:

| Qurilma | Holat | Diktovka uchun |
|---|---|---|
| Микрофон (Iriun Webcam) | OK | ✅ Hozirgi yagona haqiqiy mikrofon (telefon) |
| Стерео микшер (Realtek) | OK | ❌ **Tizim ovozini** yozadi, mikrofon emas |
| MIDI (Iriun Webcam) | OK | ❌ Nutq emas |
| soundcore R50i Hands-Free | Ulanmagan | ⚠️ Bluetooth HFP — sifat past |
| soundcore R50i (Наушники) | Ulanmagan | ❌ Faqat chiqish (A2DP) |

Bundan uchta **majburiy** talab kelib chiqadi (macOS versiyasida yo'q):

1. **Mikrofon tanlash ro'yxati** Sozlamalar oynasida. Sabab: default qurilma Stereo Mix
   bo'lib qolsa, ilova ovoz o'rniga kompyuter ovozini yozadi va tushunarsiz natija beradi.
   macOS shunchaki default input'ni oladi — Windows'da bu xavfli.

2. **Bluetooth HFP ogohlantirishi.** Bluetooth quloqchin mikrofoni ishlatilganda Windows
   HFP profiliga o'tadi (8–16 kHz, kuchli siqilgan) → whisper aniqligi sezilarli tushadi.
   Ilova buni aniqlab foydalanuvchini ogohlantirishi kerak.

3. **"Jim oqim" aniqlash.** Spike paytida amalda kuzatildi: Iriun Webcam qurilmasi
   Windows'da `OK` holatida ko'rinadi va WASAPI oqim beradi, lekin telefon ilovasi
   ishlamagani uchun oqim **raqamli sukunat** (`max_volume: -90.3 dB`). Whisper bunga
   `musiqa` deb javob beradi — foydalanuvchi uchun tushunarsiz.

   `dictate.swift:30` dagi `prepareSamples` faqat `peak > 0.0001` bo'lsa kuchaytiradi,
   aks holda xom signalni qaytaradi — ya'ni bu holat aniqlanmaydi, shunchaki noto'g'ri
   natija chiqadi. Windows versiyasi peak'ni tekshirib **"Mikrofon jim — qurilmani
   tekshiring"** deb aniq xabar berishi kerak. Bu virtual mikrofonlar (Iriun, OBS,
   VB-Cable, ulanmagan Bluetooth) keng tarqalgan Windows'da ayniqsa muhim.

---

## 6. Loyiha strukturasi

Windows kodi shu repoga `win/` papkasi sifatida qo'shiladi (model, README, web sahifa umumiy).

```
win/
  CMakeLists.txt
  core/                        # UI'dan mustaqil — unit test qilinadi
    whisper_bridge.c/.h        # macOS'dan so'zma-so'z ko'chirilgan
    engine.cpp/.h              # Whisper singleton, idle-unload (180 s), ish navbati
    audio_capture.cpp/.h       # WASAPI capture → 16 kHz mono f32
    audio_file.cpp/.h          # Media Foundation: fayl → 16 kHz mono f32 + progress
    samples.cpp/.h             # prepareSamples() normalizatsiyasi (macOS'dan)
    device_enum.cpp/.h         # mikrofon ro'yxati, HFP aniqlash
    config.cpp/.h              # settings.json o'qish/yozish
    log.cpp/.h                 # RubaiLog ekvivalenti
  ui/
    app.cpp                    # WinMain, message loop, AppDelegate ekvivalenti
    tray.cpp/.h                # Shell_NotifyIcon + kontekst menyu
    hotkey.cpp/.h              # RegisterHotKey, konflikt ishlovi
    inserter.cpp/.h            # clipboard + SendInput Ctrl+V, Unicode fallback
    overlay.cpp/.h             # NOACTIVATE HUD oyna, DWM acrylic
    settings_window.cpp/.h     # hotkey yozib olish + mikrofon tanlash
    welcome_window.cpp/.h      # onboarding (mikrofon ruxsati, avtostart)
    autostart.cpp/.h           # registry Run key
    dpi.cpp/.h                 # Per-Monitor V2
  res/
    app.rc, AppIcon.ico
  installer/
    rubai.iss                  # Inno Setup
  tests/
    test_samples.cpp, test_config.cpp, test_resample.cpp
    golden/                    # regressiya: audio → kutilgan matn
```

---

## 7. Bosqichlar

### 0-bosqich — Spike ✅ BAJARILDI
Model + CUDA Windows'da ishlashi tasdiqlandi. Qolgani: o'zbekcha nutq bilan aniqlik testi.

### 0b-bosqich — Toolchain va backend qarori
- `winget`: CMake, Visual Studio Build Tools (C++ workload), CUDA Toolkit → **D: diskka**
  (C:da 29 GB bo'sh, D:da 745 GB)
- whisper.cpp'ni CUDA va Vulkan bilan build qilib benchmark → §4 qarori
- `.exe` hajmi va statik linklash tekshiruvi

### 1-bosqich — Yadro (`core/`)
`whisper_bridge.c` ni ko'chirish, `engine` (yuklash / idle-unload / thread navbati),
`samples`, `config`, `log`. Konsol test dasturi: `.wav` → matn.
**Chiqish mezoni:** `test.wav` GPU'da to'g'ri transkripsiya qilinadi, unit testlar o'tadi.

### 2-bosqich — Audio kirish
WASAPI capture (`AUTOCONVERTPCM` bilan to'g'ridan-to'g'ri 16 kHz mono f32),
qurilma ro'yxati, qurilma o'zgarishi/uzilishi ishlovi.
**Chiqish mezoni:** mikrofondan yozib, transkripsiya qilinadi; qurilma almashtirilganda qotmaydi.

### 3-bosqich — Tray + hotkey + matn kiritish
`Shell_NotifyIcon`, `RegisterHotKey(Ctrl+Alt+D)`, clipboard + `SendInput`.
**Shu nuqtada ilova asosiy vazifasini bajaradi** — MVP.

### 4-bosqich — Overlay
NOACTIVATE HUD, 🔴/✍️/✅ holatlar, DWM acrylic (Win11) + fallback (Win10), Per-Monitor DPI.

### 5-bosqich — Sozlamalar / avtostart / fayl transkripsiyasi
Hotkey yozib olish oynasi, mikrofon tanlash, HFP ogohlantirishi, registry avtostart,
Media Foundation orqali audio/video → matn + progress + `.txt` saqlash, Welcome oynasi.

### 6-bosqich — Sifat va tarqatish
Unit testlar, golden-audio regressiya, crash handler, strukturalangan log,
Inno Setup o'rnatuvchi, kod imzosi, `web/index.html`ga Windows bo'limi, README yangilash.

---

## 8. Xatarlar

| Xatar | Ehtimol | Yechim |
|---|---|---|
| Vulkan CUDA'dan sezilarli sekin | O'rta | CUDA'da qolamiz, o'rnatuvchi GPU'ga qarab yuklab oladi |
| UIPI — elevated oynaga matn tushmaydi | Yuqori | Aniqlash + ogohlantirish; ixtiyoriy "admin sifatida ishga tushirish" |
| SmartScreen ogohlantirishi | Yuqori | Kod imzosi sertifikati (pullik) yoki README'da tushuntirish |
| Bluetooth HFP sifati | Yuqori | Aniqlash + ogohlantirish (§5) |
| Win32 UI xatoliklari | O'rta | Yadroni UI'dan ajratish, testlar, bosqichma-bosqich sinov |
| O'rnatuvchi hajmi | O'rta | Model va GPU runtime birinchi ishga tushishda yuklab olinadi |
| C: diskda joy tanqisligi (29 GB) | O'rta | Toolchain va build artefaktlari D: diskda |

---

## 9. Tarqatish talablari (obunachilar uchun)

Ilova texnik bo'lmagan foydalanuvchilarga tarqatiladi. Bu quyidagilarni **majburiy** qiladi:

| Talab | Yechim |
|---|---|
| Har qanday x64 Windows'da ishlasin | Backend DLL dispatch (§4), Win10 1809+ |
| NVIDIA'siz kompyuterlarda ishlasin | Vulkan asosiy backend + CPU fallback |
| Eski CPU'larda ishga tushsin | `GGML_CPU_ALL_VARIANTS` (Sandy Bridge 2011+) |
| Terminal kerak bo'lmasin | Grafik o'rnatuvchi, model avtomatik yuklab olinadi |
| O'zbek tilida | O'rnatuvchi UI, xato xabarlari, yo'riqnoma — hammasi o'zbekcha |
| Ishonchli ko'rinsin | Kod imzosi (SmartScreen ogohlantirishisiz) |
| Diagnostika oson bo'lsin | "Muammoni xabar qilish" tugmasi → log fayl bilan |

### Model yetkazish
O'rnatuvchi kichik (~5 MB), model (785 MB) **birinchi ishga tushishda** progress bilan
yuklab olinadi. Sabab: 800 MB `.exe` ni tarqatish qiyin, yangilanish esa og'ir.
Yuklab olish uzilsa — davom ettirish (HTTP Range) va SHA-256 tekshiruvi.

### Minimal talablar (README uchun)
- Windows 10 1809+ / Windows 11, x64
- 4 GB RAM (GPU bilan) / 6 GB (CPU rejimida)
- ~1 GB disk
- GPU tavsiya etiladi; CPU'da ~4× sekinroq

---

## 10. Keng tarqatishdan OLDIN sinash kerak

Ilova hozirgacha **bitta kompyuterda** sinalgan: Windows 11 Pro, RTX 3060,
Ryzen 7 5800X, rus tilidagi interfeys. Minglab foydalanuvchiga tarqatishdan
oldin quyidagilar tekshirilishi shart — har biri alohida xavf.

### Sinalmagan va xavfli

| Nima | Nega muhim | Qanday tekshirish |
|---|---|---|
| **AMD videokarta** | Vulkan yo'li AMD'da umuman sinalmagan | Radeon li kompyuterda o'rnatib, log'da `using Vulkan0 backend` borligini ko'rish |
| **Intel integrated (UHD/Iris)** | O'zbekistondagi arzon noutbuklarda eng ko'p uchraydi | Log'da Vulkan yoki CPU'ga tushganini, tezlikni o'lchash |
| **Videokartasiz / eski CPU** | CPU fallback real temirda sinalmagan | AVX2'siz protsessorda (2013 gacha) ishga tushishini ko'rish |
| **Windows 10** | Faqat Windows 11 da sinalgan | 1809+ da o'rnatish va ishlatish |
| **4 GB RAM li noutbuk** | Model ~1.2 GB video xotira oladi; Intel iGPU uni tizim xotirasidan oladi | Yuklanish va transkripsiya ishlashini ko'rish |
| **Antivirus** | Imzosiz o'rnatuvchi + 785 MB yuklab olish — klassik false positive | Kaspersky, Avast, ESET, Windows Defender bilan sinash |

### Foydalanuvchi to'siqlari

- **SmartScreen** — imzosiz `.exe` uchun har bir foydalanuvchi
  "Windows protected your PC" ogohlantirishini ko'radi va qo'shimcha ikki
  marta bosishi kerak. Texnik bo'lmagan odamlarning katta qismi shu yerda
  to'xtaydi. **Bu eng katta tarqatish to'sig'i.**
  Yechim: kod imzosi sertifikati (~$200–400/yil).
- **Log yuborish yo'li yo'q** — foydalanuvchi muammoni qanday xabar qilishini
  bilmaydi. Tray menyusiga "Muammoni xabar qilish" qo'shilishi kerak.
- **Crash handler yo'q** — ilova qulasa hech qanday ma'lumot qolmaydi.

### Tavsiya

10–20 kishilik sinov guruhi (turli videokarta, Windows 10 va 11, eski
noutbuklar), bir hafta. Keyin keng tarqatish.

---

## 11. Ochiq savollar

1. ~~**GPU backend**~~ → hal qilindi: Vulkan + CPU fallback (§4)
2. ~~**Model yetkazish**~~ → hal qilindi: birinchi ishga tushishda yuklash (§9)
3. **Kod imzosi:** sertifikat sotib olinadimi? (~$200–400/yil). Bo'lmasa obunachilar
   SmartScreen ogohlantirishini ko'radi va README'da tushuntirish kerak bo'ladi.
4. **Repo:** `win/` papka shu repoda (tavsiya) yoki alohida repo?
5. **Yangilanish:** avtomatik yangilanish tekshiruvi kerakmi?
