#!/bin/bash
# Kotib — bir buyruqli o'rnatuvchi (macOS, Apple Silicon).
#   ./setup.sh
#   FAQAT_KUTUBXONALAR=1 UNIVERSAL=0 ./setup.sh   # CI: faqat arm64 kutubxonalar
#
# whisper.cpp, CTranslate2 va SentencePiece aniq commit'larga pinlangan
# (`scripts/bogliqliklar.env`) — toza mashinada ham sinalgan kod yigʻiladi.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=scripts/bogliqliklar.sh
source "$ROOT/scripts/bogliqliklar.sh"
WC="$ROOT/whisper.cpp"
MODELDIR="$HOME/rubai-stt/models"
MODEL="$MODELDIR/ggml-rubaistt.bin"       # q8_0 (yengil ~820MB) — ilova shu nomdan o'qiydi
MODEL_F16="$MODELDIR/ggml-rubaistt-f16.bin"
# Tayyor q8_0 ggml model (CDN). Bo'sh bo'lsa — HF'dan konversiya + quant qilinadi.
MODEL_URL="${MODEL_URL-$MODEL_CDN_URL}"

echo "==> Kotib o'rnatilmoqda"

# 1) Talablar
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools kerak. O'rnatish: xcode-select --install" >&2; exit 1
fi
if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew kerak: https://brew.sh" >&2; exit 1
fi
command -v cmake  >/dev/null 2>&1 || { echo "==> cmake o'rnatilmoqda";  brew install cmake; }
# ffmpeg KERAK EMAS: audio/video dekodlash AVFoundation orqali
# (`src/media_decode.swift`). Ilgari bu yerda keraksiz oʻrnatilardi.

# 2) whisper.cpp (Metal, statik)
bogliqlik_tayyorla "$WC" "$WHISPER_URL" "$WHISPER_COMMIT"
if [ ! -f "$WC/build-static/src/libwhisper.a" ]; then
    echo "==> whisper.cpp build (Metal)..."
    cmake -S "$WC" -B "$WC/build-static" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DGGML_METAL=ON -DGGML_METAL_EMBED_LIBRARY=ON -DBUILD_SHARED_LIBS=OFF \
        -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF -DGGML_BLAS=OFF
    nice -n 10 cmake --build "$WC/build-static" --config Release -j4 --target whisper
fi

# 2b) x86_64 (Intel) statik — universal ilova uchun (CPU, Metal'siz). Faqat Apple Silicon'da cross-compile.
if [ "$(uname -m)" = "arm64" ] && [ "${UNIVERSAL:-1}" = "1" ] && [ ! -f "$WC/build-x64/src/libwhisper.a" ]; then
    echo "==> whisper.cpp x86_64 (Intel, CPU) build..."
    cmake -S "$WC" -B "$WC/build-x64" \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DGGML_NATIVE=OFF -DGGML_METAL=OFF -DBUILD_SHARED_LIBS=OFF \
        -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF -DGGML_BLAS=OFF
    nice -n 10 cmake --build "$WC/build-x64" --config Release -j4 --target whisper
fi

# 2c) CTranslate2 — tarjima dvigateli (statik, faqat CPU)
#
# Nega llama.cpp emas: llama-server encoder-decoder (T5 / M2M100) modellarini
# qoʻllab-quvvatlamaydi — sinalgan, maʼnosiz belgilar chiqaradi.
#
# Bayroqlar haqida:
#   OPENMP_RUNTIME=NONE          — aks holda ilova libomp.dylib ga bogʻlanadi va
#                                  Homebrew oʻrnatilmagan mashinada ishga tushmaydi
#   CMAKE_POLICY_VERSION_MINIMUM — ichidagi eski `clog` CMake 4.x da configure
#                                  boʻlmaydi ("Compatibility with CMake < 3.5 has
#                                  been removed")
#   nice + -j4                   — cheklovsiz `-j` bu mashinani yarim soat qotirdi:
#                                  CTranslate2 manbalarini bir necha protsessor
#                                  komandalari uchun qayta-qayta kompilyatsiya
#                                  qiladi, har jarayon 1–2 GB oladi
CT2="$ROOT/ctranslate2"
bogliqlik_tayyorla "$CT2" "$CT2_URL" "$CT2_COMMIT" --submodullar
if [ ! -f "$CT2/build-static/libctranslate2.a" ]; then
    echo "==> CTranslate2 build (15-25 daqiqa, mashina qiziydi)..."
    cmake -B "$CT2/build-static" -S "$CT2" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_CLI=OFF \
        -DWITH_MKL=OFF -DWITH_DNNL=OFF \
        -DWITH_ACCELERATE=ON -DWITH_RUY=ON \
        -DOPENMP_RUNTIME=NONE \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_OSX_ARCHITECTURES=arm64
    nice -n 10 cmake --build "$CT2/build-static" --config Release -j4
fi

# 2c') CTranslate2 x86_64 — universal ilova uchun (faqat Apple Silicon'da).
#
# Ilgari bu qadam yoʻq edi: toza checkout'dan faqat arm64 ilova chiqardi va
# `build.sh` buni jimgina qabul qilardi — Intel Mac'larda ishlamaydigan paket
# (barqarorlik I3). `toolchain-mac-x64.cmake` SHART: oddiy
# `CMAKE_OSX_ARCHITECTURES=x86_64` bilan CTranslate2 NEON yadrolarini x86_64
# uchun kompilyatsiya qilib yiqiladi (sababi toolchain faylining izohida).
if [ "$(uname -m)" = "arm64" ] && [ "${UNIVERSAL:-1}" = "1" ] && [ ! -f "$CT2/build-x64/libctranslate2.a" ]; then
    echo "==> CTranslate2 x86_64 (Intel) build (yana 15-25 daqiqa)..."
    cmake -B "$CT2/build-x64" -S "$CT2" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/scripts/cmake/toolchain-mac-x64.cmake" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_CLI=OFF \
        -DWITH_MKL=OFF -DWITH_DNNL=OFF \
        -DWITH_ACCELERATE=ON -DWITH_RUY=ON \
        -DOPENMP_RUNTIME=NONE \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    nice -n 10 cmake --build "$CT2/build-x64" --config Release -j4
fi

# 2d) SentencePiece — tarjima tokenizatori (statik)
#
# v0.2.0 ATAYLAB: `master` shoxi tashqi abseil-cpp ga bogʻlanib qolgan va uni
# FetchContent bilan tortadi — 94 ta qoʻshimcha statik kutubxona. v0.2.0 da
# absl `third_party/absl` ichida keladi, jami 2 ta kutubxona.
SP="$ROOT/sentencepiece"
bogliqlik_tayyorla "$SP" "$SP_URL" "$SP_COMMIT"
if [ ! -f "$SP/build-static/src/libsentencepiece.a" ]; then
    echo "==> SentencePiece build..."
    cmake -B "$SP/build-static" -S "$SP" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSPM_ENABLE_SHARED=OFF -DSPM_ENABLE_TCMALLOC=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_OSX_ARCHITECTURES=arm64
    nice -n 10 cmake --build "$SP/build-static" --config Release -j4
fi

# 2d') SentencePiece x86_64 — universal ilova uchun (faqat Apple Silicon'da).
# Bu yerda toolchain fayli shart emas: SentencePiece arxitekturani oʻzi tanlamaydi.
if [ "$(uname -m)" = "arm64" ] && [ "${UNIVERSAL:-1}" = "1" ] && [ ! -f "$SP/build-x64/src/libsentencepiece.a" ]; then
    echo "==> SentencePiece x86_64 (Intel) build..."
    cmake -B "$SP/build-x64" -S "$SP" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSPM_ENABLE_SHARED=OFF -DSPM_ENABLE_TCMALLOC=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
        -DCMAKE_OSX_ARCHITECTURES=x86_64
    nice -n 10 cmake --build "$SP/build-x64" --config Release -j4
fi

# CI va tekshiruv uchun: faqat kutubxonalar — model yuklanmaydi, ilova
# oʻrnatilmaydi. `UNIVERSAL=0` x86_64 nusxalarini oʻtkazib yuboradi (CI'da
# CTranslate2 ning ikkinchi ~20 daqiqasi kerak emas).
if [ "${FAQAT_KUTUBXONALAR:-0}" = "1" ]; then
    echo "==> Kutubxonalar tayyor (FAQAT_KUTUBXONALAR=1 — model va ilova oʻtkazib yuborildi)"
    exit 0
fi

# 3) Model — avval CDN'dan tayyor q8_0 (tez), bo'lmasa HuggingFace'dan + quant
#
# Yuklash `.part` ga ketadi va sha256 tekshirilgandan keyingina asl nomga
# koʻchadi: ilova modelni shu nom bilan qidiradi, chala yoki buzilgan fayl
# esa «Model yuklanmadi» bilan yiqiladi.
if [ ! -f "$MODEL" ]; then
    mkdir -p "$MODELDIR"
    if [[ "$MODEL_URL" == http* ]] && curl -fL --progress-bar -o "$MODEL.part" "$MODEL_URL"; then
        hash="$(shasum -a 256 "$MODEL.part" | cut -d' ' -f1)"
        if [ "$hash" != "$MODEL_SHA256" ]; then
            rm -f "$MODEL.part"
            echo "Xato: yuklangan model sha256 mos emas — fayl buzilgan yoki boshqa." >&2
            echo "  kutilgan: $MODEL_SHA256" >&2
            echo "  olingan:  $hash" >&2
            exit 1
        fi
        mv "$MODEL.part" "$MODEL"
        echo "==> Model (q8_0, yengil) CDN'dan yuklandi, sha256 mos ✓"
    else
        echo "==> CDN'dan olinmadi — HuggingFace'dan yuklab konversiya qilinmoqda"
        echo "    (ochiq model, token shart emas; biroz vaqt oladi)..."
        rm -f "$MODEL" "$MODEL.part"
        # f16 ga o'giradi -> $MODEL_F16
        bash "$ROOT/scripts/convert_model.sh"
        # quantize vositasini build qilib, f16 -> q8_0 ga siqamiz (kamroq RAM/disk)
        echo "==> Model q8_0 ga quantize qilinmoqda (kamroq RAM)..."
        if [ ! -x "$WC/build-quant/bin/whisper-quantize" ]; then
            cmake -S "$WC" -B "$WC/build-quant" \
                -DWHISPER_BUILD_EXAMPLES=ON -DWHISPER_BUILD_TESTS=OFF \
                -DGGML_METAL=OFF -DGGML_BLAS=OFF >/dev/null
            nice -n 10 cmake --build "$WC/build-quant" --target whisper-quantize -j4
        fi
        "$WC/build-quant/bin/whisper-quantize" "$MODEL_F16" "$MODEL" q8_0
        rm -f "$MODEL_F16"   # f16 zaxira kerak emas — diskni bo'shatamiz
    fi
fi

# 4) Ilovani build qilish
echo "==> Ilova build qilinmoqda..."
bash "$ROOT/src/build.sh"

# 5) Avto-ishga tushish
# setup.sh endi oʻz LaunchAgent'ini OʻRNATMAYDI. Ilova login'da ishga tushishni
# .app ichidagi LaunchAgent orqali (SMAppService) oʻzi boshqaradi — birinchi
# ishga tushishda roʻyxatdan oʻtadi. Ikkita agent qolsa login'da ikki nusxa
# yonardi, shuning uchun eskisi bor boʻlsa olib tashlanadi.
ESKI_PLIST="$HOME/Library/LaunchAgents/com.rubaistt.dictation.plist"
if [ -f "$ESKI_PLIST" ]; then
    launchctl bootout "gui/$(id -u)" "$ESKI_PLIST" 2>/dev/null || true
    rm -f "$ESKI_PLIST"
    echo "==> Eski LaunchAgent olib tashlandi (ilova endi buni oʻzi boshqaradi)"
fi

echo ""
echo "✅ O'rnatildi! Menyu-bardagi 🎙️ ikonkani ko'rasiz."
echo ""
echo "OXIRGI QADAM — Accessibility ruxsati (⌘V yuborish uchun):"
echo "  System Settings > Privacy & Security > Accessibility"
echo "  → 'Kotib' ni qo'shing va yoqing"
echo ""
echo "Ishlatish: istalgan joyda  ⌃⌥D  → gapiring → ⌃⌥D"
