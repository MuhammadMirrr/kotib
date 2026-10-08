# stt-baho — nutq modelini oʻlchash

Model variantlarini (f16, q8_0, q5_*, q4_*) **ilovaning oʻz kod yoʻli** bilan bir
xil audio ustida solishtiradi va WER/CER, tezlik, xotira hisobotini beradi.
Birinchi ishlatilishi va natijalari: `docs/olchovlar/2026-10-07-model-kvantlash.md`.

```bash
./scripts/stt-baho/fleurs-tayyorla.sh                       # bir marta: FLEURS uz test (345 gap, odam matni)
./scripts/stt-baho/run.sh .stt-baho/audio/fleurs-uz fleurs  # hamma variant (~1 soat)
VARIANTLAR="q8_0 q5_k" TAKROR=0 ./scripts/stt-baho/run.sh <papka> nom
GPU=0 VARIANTLAR="q8_0 q5_k" TAKROR=0 ./scripts/stt-baho/run.sh <papka> nom   # faqat CPU
```

- **Audio papka:** `.ogg .opus .m4a .mp3 .wav .mp4 …`; ichki papka — hisobotdagi guruh.
  Fayl yonidagi `<nom>.ref.txt` (odam yozgan toʻgʻri matn) haqiqiy WER va q8_0 ga
  nisbatan juftlangan bootstrap (95 % CI) beradi; boʻlmasa etalon — f16.
- **Ilova bilan bir xil:** `harness/` `src/whisper_bridge.c` (uz, beam 5,
  no_speech 0,25, flash attention, `temperature_inc` 0,2) va `nutq_bolaklari.c`
  (Silero VAD bilan boʻlaklash, S12 — `VAD=0` bilan oʻchiriladi), `media_decode.swift` (AVFoundation),
  `audio_util.swift` (`kuchaytir`) va `text_format.swift` ni toʻgʻridan-toʻgʻri
  kompilyatsiya qiladi. Manbalar oʻzgarsa `run.sh` uni qayta yigʻadi. CPU nusxasi
  bridge'dan faqat `use_gpu = false` bilan farq qiladi (`build.sh` tekshiradi).
- **Ish papkasi** — `STT_ISH` (standart: repo ildizidagi gitignore'dagi `.stt-baho/`):
  `models/ggml-rubaistt-<variant>.bin`, `natijalar/<nom>/hisobot.md`, yigʻilgan binarlar.
- **Shovqin:** `TAKROR=1` (standart) f16 ni ikki marta yurgizadi. 30 s dan qisqa
  audioda ikki yurish bir xil chiqdi; uzunlarida VAD'siz (`VAD=0`) bir xil model
  ham baʼzan butun 30 soniyalik boʻlakni tashlab yuboradi — sababi S12 da
  aniqlangan va VAD boʻlaklash bilan tuzatilgan; `VAD=0` bilan solishtirishda har
  variantni ikki marta yurgizing.
- Bir vaqtda faqat bitta transkripsiya; build — `nice -n 10`.

## Modellarni qayta yasash

`.stt-baho/models/` da hammasi bor (`SHA256SUMS`, `MANBA.txt`). Yoʻqolsa, f16 —
`scripts/convert_model.sh` usulida (HF `islomov/rubaistt_v2_medium`, snapshot
`af4601176140501f8ffbced420f6e2e23c9bb071`, whisper.cpp `167d225`
`models/convert-h5-to-ggml.py`), variantlar — pinlangan whisper.cpp dan
`whisper-quantize f16 <chiqish> <tur>`. f16 → q8_0 tarqatilayotgan model bilan
**bayt-ma-bayt** bir xil chiqadi (sha256 `1b02df43…c1a3`) — yoʻl toʻgʻriligining isboti.
