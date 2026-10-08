# Nutq modelini kvantlash: q8_0, q5 yoki q4? (2026-10-07)

**Qaror: q8_0 qoladi. q4 rad etildi. q5_k ham yopildi — egasi 2026-10-08 da
«q8 qolaversin» dedi (x86 tezlik oʻlchovi endi kerak emas).**

Savol (foydalanuvchidan): model hajmini kamaytirish uchun q4 ga oʻtsak, sifat
yomonlashadimi? Oʻlchov vositasi: `scripts/stt-baho/`. Xom natijalar:
`.stt-baho/olchov-2026-10-07/` (gitignore'da, asosiy nusxada).

## Usul

- Model: `islomov/rubaistt_v2_medium` (Whisper medium, oʻzbek tiliga fine-tune;
  HF kartasiga koʻra ~475 soat). HF dagi fp32 → f16 (whisper.cpp konvertori) →
  `whisper-quantize`. f16 → q8_0 tarqatilayotgan fayl bilan **bayt-ma-bayt** bir xil.
- Transkripsiya ilovaning oʻz kodi bilan (`whisper_bridge.c`, uz, beam 5,
  no_speech 0,25), Apple M5, Metal; CPU sinovi alohida.
- **Asosiy toʻplam:** Google FLEURS uz_uz **test** qismi, har noyob gapdan bitta
  yozuv — 345 gap, 70 daqiqa, **odam yozgan matn** bilan.
- Foydalanuvchining 50 ta yozuvi yoʻq edi (ilova diktovka ovozini saqlamaydi);
  Studiya tarixidagi 6 ta fayldan 5 tasi diskdan oʻchirilgan, biri arabcha.
  Oʻrniga oʻzbek tilida dublyaj qilingan serialdan 19 ta bir daqiqalik parcha
  olindi (odam matnisiz — faqat f16 ga nisbatan farq).
- **Qaror mezonlari natijani koʻrishdan OLDIN yozildi:** kichik variant qabul
  qilinadi, agar WER farqi q8_0 ga nisbatan ≤ +0,5 pp, 95 % CI yuqori chegarasi
  ≤ +1,0 pp va halokatli chiqish yoʻq; bir nechtasi oʻtsa va q4/q5 CI'lari
  kesishsa — xavfsizroq q5.

## Natija — FLEURS (odam matniga nisbatan)

| variant | fayl | WER | Δ WER vs q8_0 [95 % CI] | yomon/yaxshi/teng gap | raqamsiz 278 gapda WER | max RSS |
|---|---:|---:|---|---|---:|---:|
| f16 | 1534 MB | 8,11 % | −0,03 [−0,14; +0,08] | 3 / 6 / 336 | 3,47 % | 2224 MB |
| **q8_0** (hozirgi) | 823 MB | 8,15 % | — | — | 3,55 % | 1508 MB |
| q5_k | 539 MB | 8,15 % | +0,00 [−0,24; +0,22] | 22 / 21 / 302 | 3,43 % | 1200 MB |
| q5_0 | 539 MB | 7,90 % | −0,25 [−0,54; +0,00] | 17 / 28 / 300 | 3,39 % | 1262 MB |
| q4_k | 445 MB | 8,50 % | **+0,36 [+0,03; +0,69]** | 37 / 19 / 289 | 3,88 % | 1168 MB |
| q4_1 | 492 MB | 8,41 % | +0,26 [−0,08; +0,60] | 39 / 25 / 281 | 3,92 % | 1211 MB |
| q4_0 | 445 MB | 8,63 % | **+0,48 [+0,14; +0,87]** | 46 / 27 / 272 | 4,02 % | 1165 MB |

f16 ikki marta yurgizildi — 345/345 gap aynan bir xil (shovqin 0). Mutlaq WER
raqamlar sababli oshgan: model raqamni soʻz bilan yozadi, FLEURS — raqam bilan;
raqamsiz gaplardagi ustun shuni chiqarib tashlaydi.

q4 da maʼno oʻzgaradigan xatolar uchradi (q8_0 toʻgʻri, q4 notoʻgʻri):
«ozuqalarini» → «vulqonlarini», «osonroq» → «ozorroq», «avval» → «afzal»,
«onlayn tarzda» → «onlayn darslar».

## Tezlik

- **Metal (M5):** hamma variant yurish-yurish shovqini ichida. FLEURS yurishida
  RTF 0,090–0,118 (q4_k bir marta 0,162 — chetga chiqqan), teskari tartibdagi
  qayta yurishda 0,065–0,076; hech bir variant izchil tezroq emas.
- **CPU (M5 ning ARM yadrolari, 3 ta bir daqiqalik parcha, ikki tartibda):**

  | | q8_0 | q5_k | q5_0 | q4_k |
  |---|---:|---:|---:|---:|
  | RTF, toʻgʻri tartib | **0,195** | 0,252 | 0,451 | 0,311 |
  | RTF, teskari tartib | 0,301 | 0,350 | 0,502 | **0,298** |

  CPU'da q8_0 eng tez yoki teng; q5_k 15–30 % sekin, q5_0 ~2 baravar sekin.
  **x86 (AVX2) va Vulkan oʻlchanmagan** — Windows oʻrnatmalarining ~76 % i aynan shu.

## Xulosa

1. **q4 — rad.** Mezon chegarasiga sigʻadi, lekin pasayish statistik jihatdan
   aniqlanadi (q4_k, q4_0), maʼno buziladigan xatolar bor, tezlikda yutuq yoʻq,
   CPU'da sekinroq. Mezonga koʻra CI q5 bilan kesishgani uchun q5 afzal.
2. **q5_k — sifatda yoʻqotish yoʻq** (Δ +0,00), 284 MB kichik, ~300 MB kam RAM.
   Lekin CPU'da sekinroq va ikki platforma **bir xil modelda** qolishi shart
   (bir xil natija — `AGENTS.md`). Kuchsiz Windows kompyuterlarida diktovka
   kechikishi oshishi xavfi oʻlchanmagan.
3. **Shuning uchun q8_0 qoladi.** q5_k ga oʻtish uchun kerak boʻlgan yagona
   oʻlchov: haqiqiy x86 Windows kompyuterda (CPU va Vulkan) q8_0 va q5_k ning
   RTF'i. q5_k teng yoki tezroq boʻlsa — oʻtiladi. S4 model qidiruvini bir nechta
   maʼlum model (hajm + sha256) bilan quradi, shunda almashtirish kod emas,
   maʼlumot oʻzgarishi boʻladi.

## Yoʻl-yoʻlakay topilgan xato (kvantlashga bogʻliq emas)

30 soniyadan uzun audioda **bir xil model va bir xil kirish** baʼzan butun
30 soniyalik boʻlakni tashlab yuboradi: f16 `jazolovchi7_10m` ni bir yurishda
107, boshqasida 48 soʻz bilan berdi (q8_0 beshala yurishda 107). 30 s dan qisqa
FLEURS'da bu koʻrinmadi. Sabab tekshirilmagan (`no_speech_thold 0,25`, temperatura
fallback yoki Metal nodeterminizmi). Uzun diktovka va Studiya fayllariga taʼsir
qiladi → S12 (`whisper` parametrlari) ga kiritildi.
