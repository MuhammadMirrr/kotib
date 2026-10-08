# Tarjima sifati — implementatsiya rejasi

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tarjima pipeline'idagi deterministik nuqsonlarni butunlay yopish va modelni NLLB-200 3.3B int8 ga ko'chirish.

**Architecture:** To'g'ridan-to'g'ri tarjima, bitta o'tishda, beam 4, partiya 1. Hech qanday qayta saralash, N-variant yoki teskari tarjima yo'q — uchalasi ham o'lchab ko'rilib rad etilgan (spec, «Rad etilgan yondashuvlar»). Sifat ikki manbadan keladi: kattaroq model (grammatika, tushib qolish) va deterministik pipeline (segmentatsiya, himoyalangan bo'laklar, dekodlash uzunligi, normalizatsiya).

**Tech Stack:** Swift 5 (Foundation-only sof mantiq), C++ (CTranslate2 ko'prigi), Python 3.12 (o'lchov harness'i), Cloudflare R2 (tarqatish).

**Spec:** `docs/superpowers/specs/2026-08-31-tarjima-sifati-design.md`

## Global Constraints

- Sof mantiq fayllari **faqat Foundation** import qiladi — `test.sh` ning `UNDER_TEST` ro'yxati shunga tayanadi. AppKit, AVFoundation, whisper, Keychain yoki fayl tizimi import qilgan kod u yerga tushmaydi.
- Testlar `tests/` dagi minimal freymvork bilan yoziladi: `testQosh(nom) { ... }`, ichida `tengmi(nom, olingan, kutilgan)` yoki `tekshir(nom, shart)`. XCTest yo'q.
- Har bir yangi test fayli o'z registratsiya funksiyasini `tests/test_ruyxat.swift` ga qo'shadi.
- Sinov buyrug'i doimo: `./src/test.sh`. Hozirgi holat — **1196 tekshiruv**, hammasi o'tadi.
- Apostrof qoidasi: kod va matnlarda `oʻ`/`gʻ` (U+02BB), ASCII `'` emas.
- CDN kalitlari `Cache-Control: immutable` — **hech qachon ustiga yozilmaydi**, yangi versiya yangi prefiks oladi.
- Model papkasi: `~/Library/Application Support/Kotib/…`, yo'l faqat `src/yollar.swift` dan olinadi.
- Beam 4 va partiya 1 o'zgarmaydi (spec: RAM o'lchovi partiyani, AGENTS.md beam'ni qat'iylashtirgan).
- Og'ir build/o'lchovlarda hech qachon yalang'och `-j` ishlatilmaydi: `-j4` va `nice`.

---

### Task 1: ICU jumla segmentatsiyasi

Hozirgi `jumlalargaBol` har qanday `.` dan keyin kesadi, shu sabab `t.me/dr_azamoff` → `["t.", "me/dr_azamoff"]` bo'lib, `t.` alohida «jumla» sifatida modelga ketadi va `п.` bo'lib qaytadi. ICU chegaralari bu sinfdagi barcha holatni (`3.14`, `v1.0`, qisqartma, URL) hal qiladi va 202 tilga baravar ishlaydi.

Foydalaniladigan API tekshirilgan: `String.enumerateSubstrings(in:options:.bySentences)` yuqoridagi kirishlarning hammasida to'g'ri natija berdi, jumladan xitoy `。` va arab `؟` chegaralarida.

**Files:**
- Modify: `src/matn_boluvchi.swift` — `jumlalargaBol` (hozir 105-119-qatorlar)
- Test: `tests/test_matn_boluvchi.swift`

**Interfaces:**
- Consumes: yo'q (birinchi vazifa)
- Produces: `MatnBoluvchi.bol(_ matn: String) -> [[Bolak]]` xulq-atvori o'zgaradi; imzo o'zgarmaydi. Keyingi vazifalar shu imzoga tayanadi.

- [ ] **Step 1: Write the failing tests**

`tests/test_matn_boluvchi.swift` ichidagi `matnBoluvchiTestlari()` funksiyasining oxiriga qo'shing:

```swift
    testQosh("URL nuqtasi jumlani kesmaydi") {
        let b = MatnBoluvchi.bol("t.me/dr_azamoff")
        tengmi("bitta boʻlak", b[0].count, 1)
        tengmi("butun qoldi", b[0][0], .jumla(matn: "t.me/dr_azamoff", qoshimcha: ""))
    }

    testQosh("oʻnlik son jumlani kesmaydi") {
        let b = MatnBoluvchi.bol("Pi soni 3.14 ga teng. Ikkinchi gap.")
        tengmi("ikkita jumla", b[0].count, 2)
        tengmi("birinchi butun", b[0][0], .jumla(matn: "Pi soni 3.14 ga teng.", qoshimcha: ""))
    }

    testQosh("versiya raqami jumlani kesmaydi") {
        let b = MatnBoluvchi.bol("Versiya v1.0 chiqdi.")
        tengmi("bitta jumla", b[0].count, 1)
    }

    testQosh("xitoy nuqtasi chegara boʻladi") {
        let b = MatnBoluvchi.bol("这是第一句。这是第二句。")
        tengmi("ikkita jumla", b[0].count, 2)
    }

    testQosh("arab savol belgisi chegara boʻladi") {
        let b = MatnBoluvchi.bol("هذه جملة؟ وهذه أخرى.")
        tengmi("ikkita jumla", b[0].count, 2)
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `./src/test.sh`
Expected: FAIL — «URL nuqtasi jumlani kesmaydi» va «oʻnlik son…» yiqiladi, chunki hozirgi kod `t.` dan keyin kesadi. Xitoy/arab testlari ham yiqiladi (hozirgi kod `。` va `؟` ni bilmaydi).

- [ ] **Step 3: Replace `jumlalargaBol` with ICU boundaries**

`src/matn_boluvchi.swift` da mavjud `jumlalargaBol` ni butunlay almashtiring:

```swift
    /// Jumla chegaralari — ICU qoidalari boʻyicha (`enumerateSubstrings`).
    ///
    /// Nega qoʻlda `.` sanamaymiz: nuqta jumla oxiri BOʻLMAGAN holatlar koʻp —
    /// `t.me/dr_azamoff`, `3.14`, `v1.0`, qisqartmalar. Ilgari shu sabab
    /// havolalar buzilardi: `t.` alohida jumla boʻlib modelga ketib, `п.`
    /// boʻlib qaytardi. ICU bularning hammasini biladi va bu 202 tilga
    /// baravar ishlaydi — xitoy `。`, arab `؟` ham shu yerdan chiqadi.
    private static func jumlalargaBol(_ qator: String) -> [String] {
        var natija: [String] = []
        qator.enumerateSubstrings(in: qator.startIndex..., options: .bySentences) { sub, _, _, _ in
            guard let t = sub?.trimmingCharacters(in: .whitespacesAndNewlines), !t.isEmpty else { return }
            natija.append(t)
        }
        return natija
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `./src/test.sh`
Expected: PASS — yangi beshta test ham, mavjud «bir qatordagi bir necha jumla ajraladi» testi ham o'tadi (ICU `Salom. Qalaysiz? Yaxshi!` ni xuddi shunday uchga bo'ladi).

- [ ] **Step 5: Commit**

```bash
git add src/matn_boluvchi.swift tests/test_matn_boluvchi.swift
git commit -m "Jumla chegaralari ICU qoidalariga koʻchirildi"
```

---

### Task 2: Himoyalangan boʻlaklar — URL, email, handle

ICU endi URL'ni kesmaydi, lekin u hamon **jumla** sifatida modelga boradi va model uni tarjima qilishga urinadi. `t.me/dr_azamoff` butunligicha ham `п.ме/…` bo'lib chiqishi mumkin. Yechim: bunday bo'laklarni `Bolak.xom` qilish — ular modelga umuman berilmaydi va `yig` ularni o'z joyiga qaytaradi.

Mavjud ma'lumot modeli buni allaqachon qo'llaydi: qator `[Bolak]`, `yig` ularni `" "` bilan birlashtiradi. Ya'ni `[jumla("Manba:"), xom("t.me/dr_azamoff")]` to'g'ri yig'iladi. Yangi mexanizm kerak emas.

Aniqlash usuli tekshirilgan: `NSDataDetector(.link)` `t.me/dr_azamoff`, `https://…`, `user@example.com`, `www.example.uz` ni topadi va `3.14`, `v1.0` ga **tegmaydi**. `@handle` linkka kirmaydi, unga alohida regex.

**Files:**
- Modify: `src/matn_boluvchi.swift` — `qatorniBol` ichida, `bolakYasa` chaqirilishidan oldin
- Test: `tests/test_matn_boluvchi.swift`

**Interfaces:**
- Consumes: Task 1 dagi `jumlalargaBol`
- Produces: `MatnBoluvchi.himoyalanganlar(_ jumla: String) -> [Range<String.Index>]` — jumla ichidagi tarjima qilinmaydigan oraliqlar, chapdan o'ngga, kesishmagan holda.

- [ ] **Step 1: Write the failing tests**

```swift
    testQosh("yolgʻiz havola tarjimaga berilmaydi") {
        let b = MatnBoluvchi.bol("t.me/dr_azamoff")
        tengmi("bitta boʻlak", b[0].count, 1)
        tengmi("xom qoldi", b[0][0], .xom("t.me/dr_azamoff"))
        tengmi("yigʻilganda oʻzgarmaydi",
               MatnBoluvchi.yig(b, tarjimalar: []), "t.me/dr_azamoff")
    }

    testQosh("jumla ichidagi havola ajratiladi") {
        let b = MatnBoluvchi.bol("Manba: t.me/dr_azamoff")
        tengmi("ikkita boʻlak", b[0].count, 2)
        tengmi("matn qismi", b[0][0], .jumla(matn: "Manba:", qoshimcha: ""))
        tengmi("havola xom", b[0][1], .xom("t.me/dr_azamoff"))
        tengmi("yigʻildi",
               MatnBoluvchi.yig(b, tarjimalar: ["Источник:"]),
               "Источник: t.me/dr_azamoff")
    }

    testQosh("email himoyalanadi") {
        let b = MatnBoluvchi.bol("Xat: user@example.com")
        tengmi("email xom", b[0][1], .xom("user@example.com"))
    }

    testQosh("handle himoyalanadi") {
        let b = MatnBoluvchi.bol("Obuna: @dr_azamoff")
        tengmi("handle xom", b[0][1], .xom("@dr_azamoff"))
    }

    testQosh("oddiy son himoyalanmaydi") {
        let b = MatnBoluvchi.bol("Pi soni 3.14 ga teng.")
        tengmi("bitta jumla, ajratilmadi", b[0].count, 1)
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `./src/test.sh`
Expected: FAIL — «yolgʻiz havola tarjimaga berilmaydi» yiqiladi, chunki hozir URL `.jumla` bo'lib qoladi.

- [ ] **Step 3: Add protected-span detection**

`src/matn_boluvchi.swift` ga qo'shing:

```swift
    /// Havola va email aniqlagichi. Bir marta quriladi — `NSDataDetector`
    /// yaratish qimmat.
    private static let havolaDetektor: NSDataDetector? =
        try? NSDataDetector(types: NSTextCheckingResult.CheckingType.link.rawValue)

    /// `@handle` va `#hashtag` — bular havola emas, alohida naqsh kerak.
    private static let handleNaqsh: NSRegularExpression? =
        try? NSRegularExpression(pattern: "[@#][\\p{L}\\p{N}_]{2,}")

    /// Jumla ichidagi tarjima qilinmaydigan oraliqlar — chapdan oʻngga,
    /// kesishmagan holda.
    ///
    /// Nega `NSDataDetector`: u `t.me/dr_azamoff` va `www.example.uz` ni
    /// topadi, lekin `3.14` va `v1.0` ga tegmaydi. Qoʻlda yozilgan TLD
    /// roʻyxati bunday aniqlikni bermaydi va yangi domenlar chiqqanda eskiradi.
    static func himoyalanganlar(_ jumla: String) -> [Range<String.Index>] {
        let toliq = NSRange(jumla.startIndex..., in: jumla)
        var oraliqlar: [Range<String.Index>] = []
        for d in [havolaDetektor, handleNaqsh] as [NSRegularExpression?] {
            guard let d else { continue }
            for m in d.matches(in: jumla, range: toliq) {
                if let r = Range(m.range, in: jumla) { oraliqlar.append(r) }
            }
        }
        oraliqlar.sort { $0.lowerBound < $1.lowerBound }
        // Kesishganlarini tashlaymiz — birinchisi ustun.
        var toza: [Range<String.Index>] = []
        for r in oraliqlar where toza.last.map({ $0.upperBound <= r.lowerBound }) ?? true {
            toza.append(r)
        }
        return toza
    }
```

- [ ] **Step 4: Split each sentence around protected spans**

`qatorniBol` ichidagi `for jumla in jumlalargaBol(qator)` sikliga kirishdan oldin, har bir jumlani bo'laklarga ajrating. `qatorniBol` ni shunday almashtiring:

```swift
    private static func qatorniBol(_ qator: String) -> [Bolak] {
        guard !qator.trimmingCharacters(in: .whitespaces).isEmpty else { return [] }
        var natija: [Bolak] = []
        for jumla in jumlalargaBol(qator) {
            for qism in himoyaBoyichaAjrat(jumla) {
                qoshib(&natija, qism)
            }
        }
        return natija
    }

    /// Jumlani himoyalangan oraliqlar boʻyicha kesadi: matn qismlari
    /// `bolakYasa` dan oʻtadi, himoyalanganlari toʻgʻridan-toʻgʻri `.xom`.
    private static func himoyaBoyichaAjrat(_ jumla: String) -> [Bolak] {
        let oraliqlar = himoyalanganlar(jumla)
        guard !oraliqlar.isEmpty else { return [bolakYasa(jumla)] }
        var natija: [Bolak] = []
        var joriy = jumla.startIndex
        for r in oraliqlar {
            let oldi = String(jumla[joriy..<r.lowerBound])
            if !oldi.trimmingCharacters(in: .whitespaces).isEmpty {
                natija.append(bolakYasa(oldi))
            }
            natija.append(.xom(String(jumla[r])))
            joriy = r.upperBound
        }
        let qoldiq = String(jumla[joriy...])
        if !qoldiq.trimmingCharacters(in: .whitespaces).isEmpty {
            natija.append(bolakYasa(qoldiq))
        }
        return natija
    }

    /// Harfsiz boʻlak oʻzidan oldingi jumlaga qoʻshiladi — tarjimadan keyin oʻz
    /// joyiga qaytadi. Oldida jumla boʻlmasa oʻzi xom boʻlib qoladi.
    ///
    /// Havola bundan MUSTASNO: u xom boʻlib alohida turishi kerak, aks holda
    /// jumla oxiriga surilib, matn tartibi buziladi.
    private static func qoshib(_ natija: inout [Bolak], _ bolak: Bolak) {
        if case .xom(let s) = bolak, himoyalanganlar(s).isEmpty,
           let oxirgi = natija.last, case .jumla(let m, let q) = oxirgi {
            let qq = q + " " + s.trimmingCharacters(in: .whitespaces)
            natija[natija.count - 1] = .jumla(matn: m, qoshimcha: qq)
        } else {
            natija.append(bolak)
        }
    }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `./src/test.sh`
Expected: PASS — yangi beshta test va mavjud emoji testlari («emoji jumladan ajratiladi…», «faqat emoji'dan iborat qator…») ham o'tadi, chunki emoji `himoyalanganlar` da topilmaydi va eski yo'ldan boradi.

- [ ] **Step 6: Commit**

```bash
git add src/matn_boluvchi.swift tests/test_matn_boluvchi.swift
git commit -m "Havola, email va handle tarjimaga berilmaydigan boʻlak boʻldi"
```

---

### Task 3: Chiqish normalizatsiyasi

Model chiqishida ` ,` (vergul oldidan boʻsh joy) kabi nuqsonlar bor — foydalanuvchi matnida bu xato bo'lib ko'rinadi. Bundan tashqari `bolakYasa` uzun tire `—` ni `-` ga almashtiradi (model `—` ni bilmaydi), lekin chiqishda uni tiklamaydi.

Qoidalar maqsad **tilga** emas, **yozuvga** bog'lanadi — shunda 202 til uchun bitta jadval yetadi.

**Files:**
- Modify: `src/matn_boluvchi.swift` — `tozala` (hozir 80-85-qatorlar)
- Test: `tests/test_matn_boluvchi.swift`

**Interfaces:**
- Consumes: Task 2 dagi `yig`
- Produces: `MatnBoluvchi.tozala(_ s: String) -> String` imzosi o'zgarmaydi, xulq-atvori kengayadi.

- [ ] **Step 1: Write the failing tests**

```swift
    testQosh("tinish belgisi oldidagi boʻsh joy olinadi") {
        tengmi("vergul", MatnBoluvchi.tozala("У женщин , принимающих"), "У женщин, принимающих")
        tengmi("nuqta", MatnBoluvchi.tozala("Konец ."), "Konец.")
        tengmi("ikki nuqta", MatnBoluvchi.tozala("Natija : yaxshi"), "Natija: yaxshi")
        tengmi("qavs", MatnBoluvchi.tozala("matn ( izoh )"), "matn (izoh)")
    }

    testQosh("uzun tire tiklanadi") {
        tengmi("tire", MatnBoluvchi.tozala("Альцгеймер - это болезнь"),
               "Альцгеймер — это болезнь")
    }

    testQosh("defis oʻrtasidagi soʻz tegilmaydi") {
        tengmi("qoʻshma soʻz", MatnBoluvchi.tozala("ijtimoiy-iqtisodiy"), "ijtimoiy-iqtisodiy")
    }

    testQosh("unk hamon olinadi") {
        tengmi("unk", MatnBoluvchi.tozala("Salom <unk> dunyo"), "Salom dunyo")
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `./src/test.sh`
Expected: FAIL — «tinish belgisi oldidagi boʻsh joy olinadi» va «uzun tire tiklanadi» yiqiladi.

- [ ] **Step 3: Extend `tozala`**

`src/matn_boluvchi.swift` dagi `tozala` ni almashtiring:

```swift
    /// Model chiqishini tozalaydi.
    ///
    /// Uch ish: bilmagan belgi oʻrniga qoʻygan `<unk>` ni olib tashlash;
    /// tinish belgisi oldidagi ortiqcha boʻsh joyni yigʻish (model buni
    /// muntazam chiqaradi); `bolakYasa` da `-` ga aylantirilgan uzun tireni
    /// tiklash.
    ///
    /// Tire faqat ikki tomonida boʻsh joy boʻlgandagina tiklanadi — aks holda
    /// `ijtimoiy-iqtisodiy` kabi qoʻshma soʻz buzilardi.
    static func tozala(_ s: String) -> String {
        var t = s.replacingOccurrences(of: "<unk>", with: "")
        for belgi in [",", ".", ":", ";", "!", "?", ")", "»", "”"] {
            t = t.replacingOccurrences(of: " " + belgi, with: belgi)
        }
        for belgi in ["(", "«", "“"] {
            t = t.replacingOccurrences(of: belgi + " ", with: belgi)
        }
        t = t.replacingOccurrences(of: " - ", with: " — ")
        while t.contains("  ") { t = t.replacingOccurrences(of: "  ", with: " ") }
        return t.trimmingCharacters(in: .whitespaces)
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `./src/test.sh`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/matn_boluvchi.swift tests/test_matn_boluvchi.swift
git commit -m "Tarjima chiqishida tinish belgilari normallashtirildi"
```

---

### Task 4: Dinamik `max_decoding_length`

`tarjima_bridge.cpp` da `kMaxUzunlik = 256` doimiy. Uzun jumla shu chegarada **jimgina kesiladi** — na xato, na ogohlantirish. FLORES bu bug'ni ko'rmaydi (jumlalari ~50 token), shuning uchun u shu paytgacha sezilmagan; sizning tibbiy matningizdagi 40+ so'zli gaplar esa unga yaqinlashadi.

Bu vazifa C++ tomonida va `test.sh` uni qamramaydi — tekshiruv build va qo'lda sinov orqali.

**Files:**
- Modify: `src/tarjima_bridge.cpp` — `kMaxUzunlik` (39-qator) va `rubai_tarjima` ichidagi `opt.max_decoding_length` (98-qator atrofida)

**Interfaces:**
- Consumes: yo'q
- Produces: `rubai_tarjima` C imzosi o'zgarmaydi.

- [ ] **Step 1: Replace the constant with a formula**

`src/tarjima_bridge.cpp` da `kMaxUzunlik` e'lonini olib tashlang va uning izohini quyidagiga almashtiring:

```cpp
// Dekodlash uzunligi manba uzunligiga bogʻlanadi. Ilgari u doimiy 256 edi va
// uzun jumla JIMGINA kesilardi — foydalanuvchi na xato, na ogohlantirish
// koʻrardi. Koeffitsient 2: tarjima manbadan uzunroq boʻlishi normal (rus tili
// oʻzbekchadan uzunroq), +30 esa qisqa jumlalar uchun zaxira.
constexpr size_t kUzunlikKoeff = 2;
constexpr size_t kUzunlikZaxira = 30;
```

- [ ] **Step 2: Use it in `rubai_tarjima`**

`opt.max_decoding_length = kMaxUzunlik;` qatorini almashtiring:

```cpp
        opt.max_decoding_length = tokenlar.size() * kUzunlikKoeff + kUzunlikZaxira;
```

- [ ] **Step 3: Build**

Run: `nice -n 10 ./src/build.sh`
Expected: build xatosiz o'tadi.

- [ ] **Step 4: Manual check with a long sentence**

Ilovani oching, «Tarjima» tabiga quyidagi jumlani qo'ying va uz→ru tarjima qiling:

```
Alsgeymer — miya hujayralari asta-sekin zararlanib, avvalo xotira, keyinchalik fikrlash, nutq va kundalik ishlarni mustaqil bajarish qobiliyati pasayib boradigan, dunyo boʻyicha oʻn millionlab kishilarni qamrab olgan va hozircha davosi topilmagan ogʻir nevrodegenerativ kasallikdir.
```

Expected: natija to'liq gap bo'lib chiqadi, o'rtada kesilib qolmaydi.

- [ ] **Step 5: Add repetition guard (bajarilganda qoʻshildi)**

Chegarani ochish yangi nuqsonni ochdi: tinish belgisiz uzun matnda model
takrorlanish sikliga tushadi («мышление В дальнейшем мышление В дальнейшем…»).
Ilgari buni 256 lik chegara tasodifan kesib turgan ekan.

O'lchov (FLORES-200, 200 jumla, uzn_Latn → rus_Cyrl):

| `no_repeat_ngram_size` | chrF++ | Farq | Buzilgan matnda soʻz xilma-xilligi |
|---|---|---|---|
| 0 | 47,90 | bazaviy | 0,08 |
| 3 | 47,83 | −0,07 | 0,78 |
| **4** | **47,87** | **−0,03** | **0,62** |
| 6 | 47,94 | +0,04 | 0,60 |

`4` tanlandi: sifatga taʼsiri shovqin darajasida, himoya jiddiy.

```cpp
        opt.no_repeat_ngram_size = kTakrorNgram;  // kTakrorNgram = 4
```

- [ ] **Step 6: Commit**

```bash
git add src/tarjima_bridge.cpp
git commit -m "Dekodlash uzunligi manba uzunligiga bogʻlandi"
```

---

### Task 5: Doimiy oʻlchov harness'i

«Doimiy yechim» degani — o'zgarish sifatni tushirganda buni **bilib turish**. Hozir harness yo'q; spec uchun o'tkazilgan o'lchovlar vaqtinchalik papkada qoldi.

Ikki korpus shart. FLORES-200 jumlalari qisqa va `max_decoding_length` bug'ini ko'rmaydi — real uzun matn korpusi busiz o'lchov ko'r bo'ladi.

**Files:**
- Create: `scripts/eval/olcha.py`
- Create: `scripts/eval/tibbiy.txt`
- Create: `scripts/eval/README.md`
- Create: `scripts/eval/.gitignore`

**Interfaces:**
- Consumes: yo'q
- Produces: `python olcha.py --model DIR --spm FILE --manba uzn_Latn --maqsad rus_Cyrl --soni 200` → bitta JSON qator: `{"model", "juft", "rejim", "jumla", "chrf++", "soniya", "jumla/s"}`.

- [ ] **Step 1: Create the eval script**

`scripts/eval/olcha.py`:

```python
#!/usr/bin/env python3
"""Kotib — tarjima sifatini oʻlchash harness'i.

Tokenizatsiya `src/tarjima_bridge.cpp` bilan AYNAN bir xil boʻlishi shart:
    [manba_til] + sentencepiece boʻlaklari + "</s>"
maqsad tomonda prefiks — [maqsad_til]. Tartib buzilsa sifat JIMGINA tushadi.

Ishlatish uchun muhit:
    python3.12 -m venv .venv
    ./.venv/bin/pip install ctranslate2 sentencepiece sacrebleu
"""
import argparse, json, os, time
import ctranslate2, sentencepiece as spm
from sacrebleu.metrics import CHRF


def kodla(sp, matn, til):
    return [til] + sp.EncodeAsPieces(matn) + ["</s>"]


def dekodla(sp, tokenlar):
    return sp.DecodePieces([t for t in tokenlar[1:] if t != "</s>"])


def tarjima(tr, sp, jumlalar, manba, maqsad):
    """Partiya 1 — ilova ham shunday ishlaydi (RAM sababli, spec'ga qarang)."""
    natija = []
    for j in jumlalar:
        tok = kodla(sp, j, manba)
        r = tr.translate_batch([tok], beam_size=4,
                               target_prefix=[[maqsad]],
                               max_decoding_length=len(tok) * 2 + 30)[0]
        natija.append(dekodla(sp, r.hypotheses[0]))
    return natija


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--model", required=True)
    p.add_argument("--spm", required=True)
    p.add_argument("--manba", default="uzn_Latn")
    p.add_argument("--maqsad", default="rus_Cyrl")
    p.add_argument("--soni", type=int, default=200)
    p.add_argument("--threads", type=int, default=4)
    p.add_argument("--flores", default="flores200_dataset/devtest")
    p.add_argument("--chiqish", default=None)
    a = p.parse_args()

    src = open(f"{a.flores}/{a.manba}.devtest").read().splitlines()[:a.soni]
    ref = open(f"{a.flores}/{a.maqsad}.devtest").read().splitlines()[:a.soni]

    tr = ctranslate2.Translator(a.model, device="cpu", compute_type="int8",
                                inter_threads=1, intra_threads=a.threads)
    sp = spm.SentencePieceProcessor()
    sp.Load(a.spm)

    t0 = time.time()
    gipoteza = tarjima(tr, sp, src, a.manba, a.maqsad)
    ketgan = time.time() - t0

    ball = CHRF(word_order=2).corpus_score(gipoteza, [ref]).score
    print(json.dumps({
        "model": os.path.basename(a.model.rstrip("/")),
        "juft": f"{a.manba}->{a.maqsad}",
        "jumla": len(src),
        "chrf++": round(ball, 2),
        "soniya": round(ketgan, 1),
        "jumla/s": round(len(src) / ketgan, 2),
    }, ensure_ascii=False))

    if a.chiqish:
        with open(a.chiqish, "w") as f:
            f.write("\n".join(gipoteza) + "\n")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Create the real-text corpus**

`scripts/eval/tibbiy.txt` — FLORES ko'rmaydigan uzun, ko'p ergashli gaplar:

```
Alsgeymer — miya hujayralari asta-sekin zararlanib, avvalo xotira, keyinchalik fikrlash, nutq va kundalik ishlarni mustaqil bajarish qobiliyati pasayib boradigan kasallik.
Olimlar ikkita yirik tibbiy maʼlumotlar bazasidagi natijalarni oʻrganishgan.
Alsgeymerga xos miya oʻzgarishlari ehtimoli 35% pastroq boʻlgan.
Preparatlarning dozasi, terapiya qachon boshlangani va qancha vaqt davom etgani ham toʻliq maʼlum emas.
Shunga qaramay, natijalar umidli: menopauzal gormon terapiyasi nafaqat issiq bosishi, uyqu buzilishi va boshqa menopauza belgilarini yengillashtirishi, balki kelajakda miya salomatligini saqlashda ham muhim oʻrin tutishi mumkin.
U asosan bachadoni olib tashlangan ayollarga buyuriladi.
Bachadon saqlangan ayollarga esa odatda estrogen va progesterondan iborat kombinatsiyalangan terapiya tanlanadi.
Bu tadqiqot ayollar miya salomatligini menopauza davridan boshlab asrash mumkinligi haqida yangi va umidli yoʻnalish boʻlishi mumkin.
Tahlil: Azamov Bakhovuddin, PhD, MD — t.me/dr_azamoff
```

- [ ] **Step 3: Write the README**

`scripts/eval/README.md`:

```markdown
# Tarjima sifatini oʻlchash

Model yoki dekodlash sozlamalari oʻzgarganda SHU harness ishga tushiriladi va
natija `natijalar.md` ga yoziladi. Aks holda regressiya sezilmay qoladi.

## Tayyorlash

    cd scripts/eval
    python3.12 -m venv .venv
    ./.venv/bin/pip install ctranslate2 sentencepiece sacrebleu
    curl -sL -o flores.tar.gz https://dl.fbaipublicfiles.com/nllb/flores200_dataset.tar.gz
    tar xzf flores.tar.gz

## Ishga tushirish

    M=~/Library/Application\ Support/Kotib/tarjima-model-33b
    nice -n 10 ./.venv/bin/python olcha.py \
        --model "$M" --spm "$M/sentencepiece.bpe.model" \
        --manba uzn_Latn --maqsad rus_Cyrl --soni 200

## Ikki korpus, ikkalasi ham shart

`flores200_dataset/devtest` — 1012 qisqa jumla, chrF++ raqami shu yerdan.

`tibbiy.txt` — uzun, koʻp ergashli gaplar. FLORES jumlalari ~50 token va
`max_decoding_length` bugʻini KOʻRMAYDI; bu bugʻ aynan uzun gaplarda chiqadi.
Bu fayl uchun etalon tarjima yoʻq — u chrF uchun emas, koʻz bilan tekshirish
uchun (`--chiqish` bilan faylga yozib solishtiriladi).

## Maʼlum qiymatlar (2026-08-31, M5, uzn_Latn -> rus_Cyrl, 200 jumla)

| Model | chrF++ | jumla/s |
|---|---|---|
| NLLB-200 distilled 1.3B int8 | 47,9 | 0,75 |
| NLLB-200 3.3B int8 | 49,1 | 0,33 |
```

- [ ] **Step 4: Ignore the heavy artefacts**

`scripts/eval/.gitignore`:

```
.venv/
flores200_dataset/
flores.tar.gz
*.out.txt
```

- [ ] **Step 5: Verify the harness runs**

Run:

```bash
cd scripts/eval && python3.12 -m venv .venv && \
  ./.venv/bin/pip -q install ctranslate2 sentencepiece sacrebleu && \
  curl -sL -o flores.tar.gz https://dl.fbaipublicfiles.com/nllb/flores200_dataset.tar.gz && \
  tar xzf flores.tar.gz && \
  M="$HOME/Library/Application Support/Kotib/tarjima-model" && \
  nice -n 10 ./.venv/bin/python olcha.py --model "$M" --spm "$M/sentencepiece.bpe.model" --soni 20
```

Expected: bitta JSON qator, `chrf++` maydoni 40-55 oralig'ida.

- [ ] **Step 6: Commit**

```bash
git add scripts/eval
git commit -m "Tarjima sifati uchun doimiy oʻlchov harness'i"
```

---

### Task 6: Modelni 3.3B ga koʻchirish — kod

Papka nomi o'zgaradi, chunki hozir `TarjimaModel.tayyor` eski 1.3B papkasini ham «tayyor» deydi va yangilanish hech qachon boshlanmaydi. Eski papka topilsa o'chiriladi — foydalanuvchi diskida 1,38 GB bo'shaydi.

Yangi tekshiruv: disk joyi. Vaqtincha ~7 GB kerak (3,2 GB arxiv + 3,4 GB ochilgan nusxa). Hozir bunday tekshiruv yo'q va to'la diskda yuklash 3 GB dan keyin quladi.

**Files:**
- Modify: `src/yollar.swift` — `tarjimaModeli` va yangi `eskiTarjimaModeliniOchir`
- Modify: `src/tarjima_model.swift` — `taxminiyBayt`, yangi `yetarliJoyBormi`
- Modify: `src/tarjima_yuklovchi.swift:23` — URL; `:208` — izoh matni
- Test: `tests/test_yollar.swift`, `tests/test_tarjima_model.swift`

**Interfaces:**
- Consumes: yo'q
- Produces:
  - `Yollar.tarjimaModeli: URL` — endi `tarjima-model-33b` ga ishora qiladi
  - `Yollar.eskiTarjimaModeliniOchir(baza: URL, fm: FileManager) -> Bool` — eski papka o'chirilgan bo'lsa `true`
  - `TarjimaModel.yetarliJoyBormi(_ papka: URL, kerak: Int64, fm: FileManager) -> Bool`
  - `TarjimaModel.taxminiyBayt: Int64` — Task 7 dagi arxivning aniq hajmi

- [ ] **Step 1: Write the failing tests**

`tests/test_yollar.swift` dagi `yollarTestlari()` oxiriga:

```swift
    testQosh("eski tarjima modeli papkasi oʻchiriladi") {
        let fm = FileManager.default
        let baza = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let eski = baza.appendingPathComponent("tarjima-model", isDirectory: true)
        try? fm.createDirectory(at: eski, withIntermediateDirectories: true)
        fm.createFile(atPath: eski.appendingPathComponent("model.bin").path, contents: Data([1]))

        let ochirildi = Yollar.eskiTarjimaModeliniOchir(baza: baza, fm: fm)
        tekshir("oʻchirildi deb qaytdi", ochirildi)
        tekshir("papka yoʻq", !fm.fileExists(atPath: eski.path))
        try? fm.removeItem(at: baza)
    }

    testQosh("eski papka boʻlmasa hech nima qilinmaydi") {
        let fm = FileManager.default
        let baza = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try? fm.createDirectory(at: baza, withIntermediateDirectories: true)
        tekshir("false qaytdi", !Yollar.eskiTarjimaModeliniOchir(baza: baza, fm: fm))
        try? fm.removeItem(at: baza)
    }
```

`tests/test_tarjima_model.swift` dagi `tarjimaModelTestlari()` oxiriga:

```swift
    testQosh("disk joyi yetarli boʻlsa true") {
        let fm = FileManager.default
        tekshir("1 KB uchun joy bor",
                TarjimaModel.yetarliJoyBormi(fm.temporaryDirectory, kerak: 1024, fm: fm))
    }

    testQosh("imkonsiz katta hajm uchun false") {
        let fm = FileManager.default
        tekshir("1 PB uchun joy yoʻq",
                !TarjimaModel.yetarliJoyBormi(fm.temporaryDirectory,
                                              kerak: 1_000_000_000_000_000, fm: fm))
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `./src/test.sh`
Expected: FAIL — kompilyatsiya xatosi: `eskiTarjimaModeliniOchir` va `yetarliJoyBormi` mavjud emas.

- [ ] **Step 3: Add the path change and old-folder cleanup**

`src/yollar.swift` da `tarjimaModeli` ni almashtiring va yangi funksiya qo'shing:

```swift
    /// ~/Library/Application Support/Kotib/tarjima-model-33b
    ///
    /// CTranslate2 papkasi (model.bin + lugʻat + sentencepiece). `.app` ichida
    /// KELMAYDI — 3,4 GB; foydalanuvchi kerak boʻlganda yuklab oladi.
    ///
    /// Nega nomda `-33b`: 1.3B dan 3.3B ga oʻtishda papka nomi oʻzgarishi
    /// SHART. Aks holda `TarjimaModel.tayyormi` eski 1.3B papkasini toʻliq deb
    /// oʻqiydi va yangilanish hech qachon boshlanmaydi.
    static var tarjimaModeli: URL {
        qollab.appendingPathComponent("tarjima-model-33b", isDirectory: true)
    }

    /// Eski 1.3B papkasini oʻchiradi — foydalanuvchi diskida 1,38 GB boʻshaydi.
    /// Oʻchirgan boʻlsa `true`. Xato bersa `false`, ilova baribir ishlaydi.
    ///
    /// `baza` parametri sinov uchun: testlar vaqtinchalik papka beradi.
    @discardableResult
    static func eskiTarjimaModeliniOchir(baza: URL, fm: FileManager = .default) -> Bool {
        let eski = baza.appendingPathComponent("tarjima-model", isDirectory: true)
        guard fm.fileExists(atPath: eski.path) else { return false }
        do {
            try fm.removeItem(at: eski)
            return true
        } catch {
            return false
        }
    }
```

- [ ] **Step 4: Add the disk-space check**

`src/tarjima_model.swift` ga qo'shing va `taxminiyBayt` ni yangilang:

```swift
    /// Toʻliq arxivning taxminiy hajmi — yuklab olish koʻrsatkichi uchun.
    /// Aniq qiymat Task 7 da arxiv yasalgach qoʻyiladi.
    static let taxminiyBayt: Int64 = 3_360_000_000

    /// Yuklash uchun kerakli boʻsh joy: arxiv + ochilgan nusxa.
    /// Ochilgandan keyin arxiv oʻchiriladi, lekin oʻrtada ikkalasi ham turadi.
    static var kerakliJoy: Int64 { taxminiyBayt * 2 }

    /// Papka turgan diskda `kerak` bayt boʻsh joy bormi.
    ///
    /// Nega kerak: busiz toʻla diskda yuklash 3 GB dan keyin quladi va
    /// foydalanuvchi sababini bilmasdi.
    static func yetarliJoyBormi(_ papka: URL, kerak: Int64,
                                fm: FileManager = .default) -> Bool {
        guard let qiymatlar = try? papka.resourceValues(
                forKeys: [.volumeAvailableCapacityForImportantUsageKey]),
              let bosh = qiymatlar.volumeAvailableCapacityForImportantUsage
        else { return true }  // Aniqlab boʻlmasa toʻsmaymiz.
        return bosh >= kerak
    }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `./src/test.sh`
Expected: PASS

- [ ] **Step 6: Update the downloader URL, text and disk guard**

`src/tarjima_yuklovchi.swift:23`:

```swift
    static let url = URL(string: "https://cdn.mirqobilov.com/dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz")!
```

`src/tarjima_yuklovchi.swift:208` dagi izoh matni:

```swift
        let izoh = NSTextField(wrappingLabelWithString:
            "Bu faqat bir marta bajariladi. Model ~3,4 GB. Ulanish uzilsa, oʻsha joydan davom etadi.")
```

`yuklashniBoshla` funksiyasining eng boshiga disk tekshiruvini qo'ying:

```swift
        guard TarjimaModel.yetarliJoyBormi(Yollar.qollab,
                                           kerak: TarjimaModel.kerakliJoy) else {
            xatoKorsat("Diskda joy yetarli emas. Kamida 7 GB boʻsh joy kerak.")
            return
        }
```

- [ ] **Step 7: Build and verify**

Run: `nice -n 10 ./src/build.sh && ./src/test.sh`
Expected: build o'tadi, testlar o'tadi.

- [ ] **Step 8: Commit**

```bash
git add src/yollar.swift src/tarjima_model.swift src/tarjima_yuklovchi.swift \
        tests/test_yollar.swift tests/test_tarjima_model.swift
git commit -m "Tarjima modeli 3.3B ga koʻchirildi, disk joyi tekshiruvi qoʻshildi"
```

---

### Task 7: Arxiv yasash va CDN'ga joylashtirish

`v1` prefiksi `Cache-Control: immutable` — ustiga yozilmaydi. Yangi kalit: `dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz`.

300 MiB dan katta obyekt uchun `wrangler r2 object put` ishlamaydi va wrangler OAuth token'ida S3 API uchun `r2` scope yo'q. Ishlaydigan yo'l — AGENTS.md da yozilgan bir martalik Worker: R2 binding bilan `createMultipartUpload` / `uploadPart` / `complete` ni ochadi, 50 MiB bo'laklar HTTPS orqali yuboriladi, keyin Worker o'chiriladi.

**Files:**
- Modify: `AGENTS.md` — «Tarjimon» bo'limi

**Interfaces:**
- Consumes: Task 6 dagi `TarjimaModel.taxminiyBayt`
- Produces: CDN'da `dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz`

- [ ] **Step 1: Assemble the model folder**

```bash
S=$(mktemp -d)/nllb-200-3.3B-int8 && mkdir -p "$S"
HF=$(find ~/.cache/huggingface/hub/models--OpenNMT--nllb-200-3.3B-ct2-int8/snapshots -name model.bin | head -1 | xargs dirname)
cp "$HF/model.bin" "$HF/config.json" "$HF/shared_vocabulary.json" "$S/"
cp "$HOME/Library/Application Support/Kotib/tarjima-model/sentencepiece.bpe.model" "$S/"
ls -la "$S"
```

Expected: to'rtta fayl. `sentencepiece.bpe.model` 1.3B papkasidan olinadi — uning sha256'si `facebook/nllb-200-3.3B` dagisi bilan aynan mos (`14bb8dfb35c0ffdea7bc01e56cea38b9e3d5efcdcb9c251d6b40538e1aab555a`), tekshirilgan.

- [ ] **Step 2: Create the archive**

```bash
cd "$(dirname "$S")" && nice -n 10 tar czf nllb-200-3.3B-int8.tar.gz nllb-200-3.3B-int8
stat -f "%z" nllb-200-3.3B-int8.tar.gz
```

Expected: hajm ~3,2-3,4 GB. **Shu aniq raqamni yozib oling** — u `TarjimaModel.taxminiyBayt` ga tushadi.

- [ ] **Step 3: Verify the archive extracts into a complete folder**

```bash
V=$(mktemp -d) && tar xzf nllb-200-3.3B-int8.tar.gz -C "$V" --strip-components 1 && ls -la "$V"
```

Expected: to'rtta fayl, hammasi nolga teng bo'lmagan hajmda. `--strip-components 1` — `tarjima_yuklovchi.swift` aynan shunday ochadi.

- [ ] **Step 4: Upload to R2 via the one-off Worker**

AGENTS.md dagi multipart Worker usulini bajaring: R2 binding'li Worker deploy qiling, 50 MiB bo'laklarni yuboring, `complete` chaqiring, keyin Worker'ni **o'chiring**. Kalit: `dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz`.

- [ ] **Step 5: Verify the CDN copy byte-for-byte**

```bash
curl -sL -o /tmp/cdn-check.tar.gz https://cdn.mirqobilov.com/dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz
shasum -a 256 /tmp/cdn-check.tar.gz nllb-200-3.3B-int8.tar.gz
```

Expected: ikkala sha256 aynan bir xil.

- [ ] **Step 6: Put the exact size into the code**

`src/tarjima_model.swift` dagi `taxminiyBayt` ni 2-qadamda olingan aniq raqamga almashtiring.

- [ ] **Step 7: End-to-end check on a clean install**

```bash
mv "$HOME/Library/Application Support/Kotib/tarjima-model-33b" /tmp/zaxira-33b 2>/dev/null || true
nice -n 10 ./src/build.sh
```

Ilovani oching, «Tarjima» tabiga o'ting, tarjimani boshlang. Kutilgan: yuklash oynasi «~3,4 GB» deydi, progress to'g'ri ko'rsatadi, tugagach tarjima ishlaydi. Eski `tarjima-model` papkasi mavjud bo'lsa — o'chirilgan bo'lishi kerak.

- [ ] **Step 8: Update AGENTS.md**

`AGENTS.md` ning «Tarjimon» bo'limida yangilang: model nomi va hajmi, CDN URL (v2), papka nomi (`tarjima-model-33b`), chrF++ qiymatlari (1.3B 47,9 → 3.3B 49,1), RAM raqamlari (partiya 1 da 2,65 GB) va `max_decoding_length` endi dinamik ekani. `scripts/eval/` harness'iga havola qo'shing.

- [ ] **Step 9: Commit**

```bash
git add src/tarjima_model.swift AGENTS.md
git commit -m "3.3B arxivi CDN'ga joylashtirildi, AGENTS.md yangilandi"
```

---

## Self-Review

**Spec qamrovi:**

| Spec bandi | Vazifa |
|---|---|
| Model almashtirish (arxiv, CDN, kod, migratsiya) | Task 6, 7 |
| Disk joyi tekshiruvi | Task 6 |
| Segmentatsiya — ICU | Task 1 |
| Himoyalangan boʻlaklar | Task 2 |
| Dekodlash — dinamik uzunlik | Task 4 |
| Normalizatsiya | Task 3 |
| Doimiy oʻlchov harness'i | Task 5 |
| Testlar | Task 1, 2, 3, 6 ichida |

Spec'ning «Ochiq masalalar» bo'limi (GPU yo'li, 4 GB mashinalar, terminologiya lug'ati) ataylab reja tashqarisida — ular keyingi qarorlar.

**Tur mosligi:** `himoyalanganlar` Task 2 da e'lon qilinadi va faqat o'sha yerda ishlatiladi. `tozala` imzosi Task 3 da o'zgarmaydi. `yetarliJoyBormi` va `kerakliJoy` Task 6 da e'lon qilinib, o'sha vazifada ishlatiladi. `taxminiyBayt` Task 6 da qo'yilib, Task 7, 6-qadamda aniq qiymatga almashtiriladi.

**Tartib bogʻliqligi:** Task 2 Task 1 ga tayanadi (ICU chegaralari). Task 7 Task 6 ga tayanadi (URL va papka nomi). Qolganlari mustaqil.
