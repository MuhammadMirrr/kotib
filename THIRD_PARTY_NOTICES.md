# Uchinchi tomon dasturlari va modellari / Third-party notices

Kotib quyidagi ochiq dasturlar va modellar ustiga qurilgan. Ularning toʻliq
litsenziya matnlari ilova bilan birga tarqatiladi: macOS'da
`Kotib.app/Contents/Resources/Litsenziyalar.txt`, Windows'da Kotib papkasidagi
`Litsenziyalar.txt` (Sozlamalar / tray menyusi → «Litsenziyalar…»). Bu fayl
yigʻish paytida `scripts/litsenziyalar.sh` bilan pinlangan manbalarning oʻz
LICENSE fayllaridan yasaladi.

Kotib is built on the open-source software and models below. Their full license
texts ship with the app (`Litsenziyalar.txt`), generated at build time by
`scripts/litsenziyalar.sh` from the LICENSE files of the pinned sources.

## Modellar / Models

| Model | Litsenziya | Qayerda | Eslatma |
|---|---|---|---|
| [rubaiSTT v2 medium](https://huggingface.co/islomov/rubaistt_v2_medium) — muallif **islomov** | Apache-2.0 | Nutqni tanish (ikkala platforma, oʻrnatuvchi ichida) | Atribusiya shart. ggml formatiga oʻgirilgan va q8_0 ga kvantlangan |
| [OpenAI Whisper medium](https://github.com/openai/whisper) | MIT | rubaiSTT shu model ustida fine-tune qilingan | |
| [Silero VAD v6.2.0](https://github.com/snakers4/silero-vad) — Silero Team ([ggml nusxasi](https://huggingface.co/ggml-org/whisper-vad)) | MIT | Nutq bor-yoʻqligini aniqlash va uzun ovozni boʻlish (ikkala platforma, ilova ichida, 885 KB) | |
| [NLLB-200 3.3B](https://huggingface.co/facebook/nllb-200-3.3B) — Meta AI | **CC-BY-NC-4.0** | Oflayn tarjimon (talab boʻyicha yuklab olinadi) | **Faqat notijorat** foydalanish; atribusiya shart. CTranslate2 formatiga oʻgirilgan, int8 |

## Dasturlar / Software

| Komponent | Litsenziya | Qayerda |
|---|---|---|
| [whisper.cpp / ggml](https://github.com/ggml-org/whisper.cpp) | MIT | Nutqni tanish dvigateli (ikkala platforma) |
| [CTranslate2](https://github.com/OpenNMT/CTranslate2) | MIT | Tarjima dvigateli |
| ↳ [ruy](https://github.com/google/ruy) | Apache-2.0 | CTranslate2 ichida (matritsa hisobi) |
| ↳ [cpuinfo](https://github.com/pytorch/cpuinfo) | BSD-2-Clause | ruy ichida |
| ↳ [cpu_features](https://github.com/google/cpu_features) | Apache-2.0 | CTranslate2 ichida |
| ↳ [spdlog](https://github.com/gabime/spdlog) | MIT | CTranslate2 ichida |
| ↳ [BS::thread_pool](https://github.com/bshoshany/thread-pool) | MIT | CTranslate2 ichida |
| ↳ avx/neon_mathfun (J. Pommier; marian-nmt) | zlib / MIT | CTranslate2 ichida |
| [SentencePiece](https://github.com/google/sentencepiece) | Apache-2.0 | Tarjima tokenizatori |
| ↳ [Abseil](https://github.com/abseil/abseil-cpp) | Apache-2.0 | SentencePiece ichida |
| ↳ protobuf-lite | BSD-3-Clause | SentencePiece ichida |
| ↳ darts-clone | BSD-2-Clause | SentencePiece ichida |
| ↳ esaxx | MIT | SentencePiece ichida |
| [Monocypher](https://monocypher.org) | CC0-1.0 yoki BSD-2-Clause | Windows: yangilanish imzosini tekshirish (Ed25519) |
| [LLVM libc++, libunwind, OpenMP](https://llvm.org) (llvm-mingw) | Apache-2.0 WITH LLVM-exception | Windows: `libc++.dll`, `libunwind.dll`, `libomp.dll` |
| [mingw-w64](https://www.mingw-w64.org) runtime | ZPL-2.1 / ommaviy mulk | Windows: statik CRT qismi |
| [Inno Setup](https://jrsoftware.org/isinfo.php) | Inno Setup License | Windows oʻrnatuvchisi |

Kotib'ning oʻzi — [MIT](LICENSE).
