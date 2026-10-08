# S2 — Imzo kaliti va yangilanish mantigʻi (batafsil reja)

Yoʻl xaritasi: `2026-10-07-v1.2.0-yol-xaritasi.md` → S2.
Spec: `../specs/2026-10-07-avto-yangilanish-design.md` — D4, «Siyosat manifesti»,
«Majburiy yangilanish», «Xavfsizlik», «Sinov → Birlik testlari».

S2 faqat **sof mantiq va kalit**. Tarmoq, taymer, yuklash, UI — S5/S8.

## 1. Bitta umumiy test jadvali

`tests/umumiy/yangilanish_holatlari.def` — ikkala platforma oʻqiydigan yagona
holatlar roʻyxati, makro qatorlari koʻrinishida:

```
VERSIYA("1.10.0", "1.9.0", 1)
SIYOSAT("1.1.0", "1.2.0", "1.0.0", 72, 100, 0, -1, 1000, YUKLA)
URL("https://cdn.mirqobilov.com/dl/v1.2.1/a.exe", FAYL, true)
ED25519("<ochiq kalit hex>", "<xabar hex>", "<imzo hex>", true)
```

- C++ (`win/tests/test_yangilanish.cpp`) uni `#include` qiladi — jadval
  `kotib-testlar.exe` ichiga kompilyatsiya vaqtida kiradi, VM'da fayl kerak emas.
- Swift (`tests/test_yangilanish.swift`) faylni `#filePath` dan topib, ish
  paytida tahlil qiladi.

Nega shunday: «ikkala platformada bir xil holatlar» shartini qoʻlda ikki nusxa
bilan ushlab boʻlmaydi — `parametr-tekshir.sh` ham aynan shu sabab bilan bor.

## 2. Sof mantiq

| Narsa | macOS | Windows |
|---|---|---|
| versiya taqqoslash (`1.10.0`, `1.2.0-rc1`, `sinov9 < sinov10`) | `src/yangilanish_siyosat.swift` | `win/core/yangilanish_siyosat.cpp` |
| siyosat qarori → `hech | yukla | ornat | blokla` | o‘sha | o‘sha |
| manifest tahlili (`{"m","s"}` → imzo → JSON → tekshirilgan tuzilma) | o‘sha | o‘sha |
| URL ruxsat roʻyxati | o‘sha | o‘sha |
| base64 | `Data(base64Encoded:)` | `yangilanish_siyosat.cpp` ichida |
| Ed25519 tekshiruvi | `src/imzo.swift` (CryptoKit) | `win/core/imzo.cpp` (Monocypher 4.0.3, `win/third_party/monocypher/`) |

`yangilanish.swift` Metal import qiladi (barqarorlik I5) — versiya taqqoslash
undan yangi faylga koʻchadi va `test.sh` endi uni kompilyatsiya qilmaydi.
CryptoKit — sof hisob (fayl, tarmoq, Keychain yoʻq), `UNDER_TEST` ga kiradi.

### Siyosat qoidalari (aniq)

Kirish: joriy versiya, manifest (`versiya`, `min_versiya`, `majburiy_muhlat_soat`,
`tarqatish_foiz`), mahalliy holat (`chelak` 0–99, majburiy siyosat birinchi
koʻrilgan vaqt yoki yoʻq, hozirgi vaqt).

1. `min_versiya > versiya` — manifest yaroqsiz → `hech` (aks holda ilova oʻzi
   bajara olmaydigan shart bilan abadiy bloklanardi).
2. `joriy < min_versiya` (majburiy): oʻtgan vaqt = `max(0, hozir − koʻrilgan)`
   (koʻrilmagan boʻlsa 0); oʻtgan ≥ muhlat → `blokla`, aks holda `ornat`.
   Foiz eʼtiborga olinmaydi. Soat orqaga surilsa — blok yoʻq.
3. `versiya > joriy` va `chelak < foiz` → `yukla`, aks holda `hech`.
4. Qolgani (teng, eski — downgrade) → `hech`.

### URL qoidasi

Faqat aniq prefiks: `https://cdn.mirqobilov.com/` (fayl) yoki
`https://stat.mirqobilov.com/` (manifest), kichik harflarda; qolgan qismi faqat
`[A-Za-z0-9._~/-]`, `..` segmenti yoʻq. URL tahlilchisi ishlatilmaydi — ikki
platformada bir xil va qatʼiy boʻlsin (`@`, `\`, `?`, `#`, `:port`, katta harf,
`.evil.com` qoʻshimchasi — hammasi rad).

## 3. Kalit

- Sparkle 2.9.6 vositalari (`generate_keys`, `sign_update`), sha256 bilan
  (`scripts/bogliqliklar.env`), `sparkle/` ga ochiladi (gitignore).
- Kalit login Keychain'da, hisob nomi `kotib` (`--account kotib`) —
  boshqa Sparkle loyihalari bilan toʻqnashmasin.
- Ochiq kalit — `scripts/yangilanish-kaliti.pub` (repoda; maxfiy emas).
- Zaxira sinovi: `-x` bilan eksport → boshqa hisobga (`kotib-tiklash-sinovi`)
  import → ikkala ochiq kalit bir xilmi → sinov yozuvi va eksport fayli oʻchiriladi.
- Oflayn zaxira (ikki joy, shifrlangan) — **foydalanuvchi qiladi**, buyruqlar `AGENTS.md` da.

## 4. Qabul

- `./src/test.sh` va `./win/tests/mac/hammasi.sh` ✓, umumiy jadval ikkala
  tomonda oʻtadi; RFC 8032 (1, 2, 3, 1024 bayt va SHA(abc)) vektorlari.
- Ataylab buzilgan holatlar: imzo/xabar/kalitning 1 biti, notoʻgʻri uzunlik.
- Kalit zaxirasi tiklanib, ochiq kalit mos kelgani tekshirilgan.
