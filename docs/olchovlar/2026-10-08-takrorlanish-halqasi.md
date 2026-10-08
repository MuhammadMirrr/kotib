# Takrorlanish halqasi: `temperature_inc` va matn darajasidagi qisqartirish (2026-10-08)

**Qaror: `temperature_inc = 0.2` (ikkala bridge) + `takrorniQisqartir`
(`matnniTayyorla` ichida, ikkala platforma).**

## Qanday topildi

S23 da Windows VM'da mikrofon bilan qoʻlda sinov (D4, E5, S10). VM'ning
«Line In» qurilmasi Mac'ning standart mikrofonidan oladi, ovoz esa Mac
karnayidan (`afplay`) beriladi — FLEURS uz test gaplari. Uchinchi diktovkada
Notepad'ga «toʻrt yuz ellik ming» 15 marta yozildi (396 belgi, 9 s ovozdan).
Xuddi shu WAV toʻgʻridan-toʻgʻri (`rubai-cli`) — toza, halqasiz.

## Usul

- **Akustik toʻplam:** FLEURS uz test dan 20 ta gap (6–14 s), karnay → Mac
  mikrofoni → VM `rubai-cli --record … --saqla` (shu ish uchun qoʻshildi).
  Ovoz juda past va buzuq (peak 0,015–0,037; WER ~80 %) — bu odatdagi
  diktovka emas, uzoqdagi yoki yomon mikrofonning eng yomon holati.
- **Regressiya toʻplamlari** — S12 dagilar (`.stt-baho/s12/audio`): `uzun`
  (10 × ~2 daq), `uzun-shovqin` (xuddi shu + shovqin), `qisqa50`, `nutqsiz` (10).
  Va akustik 20 tasining toza asli.
- Mac: `scripts/stt-baho` harness'i, q8_0, Metal, Silero VAD — bridge'ning
  ikki nusxasi (`temperature_inc` 0 va 0,2). Windows: VM, `rubai-cli`, CPU.
- «Halqa» — 1–6 soʻzli ibora ketma-ket ≥ 4 marta.

## Natija

| toʻplam | temperature_inc 0 | temperature_inc 0,2 |
|---|---|---|
| akustik 20, Mac Metal | WER 78,0 %, halqa 1/20 | WER 67,1 %, halqa 0/20 |
| akustik 20, Windows CPU | WER 83,6 %, halqa 4/20 | WER 68,8 %, halqa 2/20 |
| toza 20 | WER 10,5 % | **bayt-ma-bayt bir xil** |
| uzun 10 | WER 7,5 % | **bayt-ma-bayt bir xil** |
| uzun-shovqin 10 | WER 7,5 % | **bayt-ma-bayt bir xil** |
| qisqa50 | WER 6,6 % | **bayt-ma-bayt bir xil** |
| nutqsiz 10 | 0/10 matn | 0/10 matn |

Qayta urinish faqat buzuq ovozda ishga tushadi: qolgan hamma toʻplamda
chiqish oʻzgarmadi. Akustik toʻplamda 0,2 bilan ikki yurish bir xil matn berdi.

Windows'da qolgan 2 ta halqa («toʻla» ×8, «oʻzbekiston respublikasi» ×N) —
model hamma haroratda ham halqa chiqargan. Ular uchun matn darajasidagi
`takrorniQisqartir`: ≥ 4 marta ketma-ket ibora → bitta. Takror boʻlmasa satr
aynan qaytadi (oddiy matnga tegmaydi). Ilovada uchdan-uchga: ikkala WAV
«saqlanmagan» papkasi orqali qayta oʻgirildi — tarixda halqa yoʻq.

## Cheklov

whisper.cpp 0-dekoderning tasodifiy generatorini faqat holat yaratilganda
urugʻlaydi (`whisper.cpp:3470`; 1+ dekoderlar har chaqiruvda — `:6915`).
Demak qayta urinish ishga tushgan (buzuq) boʻlakda natija shu sessiyadagi
oldingi chaqiruvlarga bogʻliq boʻlishi mumkin. Odatdagi ovozda qayta urinish
umuman ishga tushmagani uchun S12 dagi «bir xil fayl — bir xil matn» saqlanadi.
