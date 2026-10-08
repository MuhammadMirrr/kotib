# Monocypher 4.0.3

Windows yangilovchisi yuklangan fayl va manifest imzosini (Ed25519) shu bilan
tekshiradi — `win/core/imzo.cpp`. macOS tomonida bu ishni CryptoKit qiladi.

| | |
|---|---|
| Manba | https://monocypher.org/download/monocypher-4.0.3.tar.gz |
| sha512 | `40904ada5c7ee4f7741733e38b69a30a4b0561cbffba5ffe7c2dce16136d540251ec0d9056ff606510d3b5b708fb8a40db7e0870d4a0b2dc17ba2bfb880f8965` (rasmiy `.sha512` bilan mos) |
| Olingan sana | 2026-10-07 |
| Litsenziya | CC0-1.0 yoki BSD-2-Clause (tanlov bizda) — `LICENCE.md` |

Fayllar arxivdan **oʻzgartirishsiz** koʻchirilgan:
`src/monocypher.{c,h}`, `src/optional/monocypher-ed25519.{c,h}`, `LICENCE.md`.
GitHub'dagi `4.0.3` tegi bilan solishtirilgan — farq faqat birinchi qatordagi
versiya yozuvida (`4.0.3` va `__git__`).

Nega `monocypher-ed25519`: Monocypher'ning asosiy EdDSA'si BLAKE2b bilan
ishlaydi. Sparkle esa RFC 8032 Ed25519 (SHA-512) bilan imzolaydi — mos
keladigani faqat shu ixtiyoriy modul.

Nega Monocypher: ikki C fayl, tashqi bogʻliqliksiz, llvm-mingw va MSVC'da
bir xil yigʻiladi va Win32'ga tegmaydi — shuning uchun imzo testlari
`win/tests/mac/sinov.sh` orqali **macOS'da ham** ishlaydi. Tizim
kriptografiyasiga bogʻlansak, bu testlar faqat VM'da ishlardi.

Yangilash: yangi relizni xuddi shu tartibda oling, sha512 ni tekshiring, bu
jadvalni yangilang va `./win/tests/mac/hammasi.sh` ni ishga tushiring
(RFC 8032 vektorlari `tests/umumiy/yangilanish_holatlari.def` da).
