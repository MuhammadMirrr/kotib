#!/usr/bin/env bash
#
# Windows build'ini macOS'da bitta buyruq bilan yigʻadi (cross-compile).
#
#   ./win/build-mac.sh              — x64 (foydalanuvchilar uchun)
#   ./win/build-mac.sh arm64        — ARM64 (UTM'dagi VM'da sinash uchun)
#   ./win/build-mac.sh hammasi      — ikkalasi
#
# Bayroqlar:
#   --tez           whisper.cpp va tarjimon kutubxonalarini qayta qurmaydi
#   --tarjimasiz    CTranslate2/SentencePiece qurilmaydi (tezroq iteratsiya)
#
# Natija: `win/build-<arch>/Kotib.exe` + yonida whisper DLL'lari.
#
# Nega bunday skript kerak: quvurda toʻrtta mustaqil build bor (whisper.cpp,
# CTranslate2, SentencePiece, ilovaning oʻzi) va ularning har birida bir
# nechta yuk koʻtaruvchi bayroq. Ularni qoʻlda terish — xato manbai.
#
# **Har bir `cmake --build` ataylab `nice -n 10 … -j4` bilan cheklangan**:
# cheklovsiz `-j` bu mashinani (16 GB) yarim soat qotirgan — CTranslate2 oʻz
# manbalarini har bir CPU komandalar toʻplami uchun qaytadan kompilyatsiya
# qiladi va har jarayon 1–2 GB oladi.
#
# Muhit oʻzgaruvchilari (ixtiyoriy):
#   TOOLCHAIN     llvm-mingw papkasi (standart: ~/Developer/.toolchains/
#                 llvm-mingw-<LLVM_MINGW_VERSIYA>-ucrt-macos-universal)
#   BREW_PREFIX   Homebrew prefiksi — Vulkan sarlavhalari va shader
#                 kompilyatorlari shu yerdan (standart: `brew --prefix`)
#
# whisper.cpp, CTranslate2 va SentencePiece `scripts/bogliqliklar.env` dagi
# commit'larga pinlangan: papka yoʻq boʻlsa — klonlanadi, bor boʻlsa —
# commit tekshiriladi (macOS bilan bir xil kod yigʻilsin).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
# shellcheck source=../scripts/bogliqliklar.sh
source "$ROOT/scripts/bogliqliklar.sh"

TC="${TOOLCHAIN:-$HOME/Developer/.toolchains/llvm-mingw-${LLVM_MINGW_VERSIYA}-ucrt-macos-universal}"
# CMake toolchain fayllari (`win/cmake/llvm-mingw.cmake`) shu oʻzgaruvchidan
# oʻqiydi — skript va CMake bir xil toolchain'ni koʻrsin.
export TOOLCHAIN="$TC"
BREW_PREFIX="${BREW_PREFIX:-$(brew --prefix 2>/dev/null || echo /opt/homebrew)}"
JOBS=4

ARCHLAR=()
TEZ=0
TARJIMA=1

for a in "$@"; do
    case "$a" in
        x64|arm64) ARCHLAR+=("$a") ;;
        hammasi)   ARCHLAR=(x64 arm64) ;;
        --tez)     TEZ=1 ;;
        --tarjimasiz) TARJIMA=0 ;;
        *) echo "Nomaʼlum argument: $a" >&2; exit 2 ;;
    esac
done
[ ${#ARCHLAR[@]} -eq 0 ] && ARCHLAR=(x64)

# ---------------------------------------------------------------- tekshiruv

if [ ! -d "$TC" ]; then
    cat >&2 <<EOF
llvm-mingw topilmadi: $TC

Oʻrnatish (bir marta):
  mkdir -p ~/Developer/.toolchains && cd ~/Developer/.toolchains
  curl -fL -O https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_VERSIYA}/llvm-mingw-${LLVM_MINGW_VERSIYA}-ucrt-macos-universal.tar.xz
  tar xf llvm-mingw-${LLVM_MINGW_VERSIYA}-ucrt-macos-universal.tar.xz
Boshqa joyda boʻlsa: TOOLCHAIN=<papka> ./win/build-mac.sh

Qolgan yordamchilar:
  brew install ninja shaderc glslang vulkan-headers wimlib
EOF
    exit 1
fi

for vosita in ninja cmake; do
    command -v "$vosita" >/dev/null || { echo "$vosita topilmadi (brew install $vosita)" >&2; exit 1; }
done
for fayl in include/vulkan/vulkan.h bin/glslc bin/glslangValidator; do
    [ -e "$BREW_PREFIX/$fayl" ] || {
        echo "$BREW_PREFIX/$fayl topilmadi (brew install shaderc glslang vulkan-headers)" >&2
        exit 1
    }
done

qadam() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

# ------------------------------------------------------------- whisper.cpp
#
# GGML_BACKEND_DL — backend'lar ish paytida yuklanadigan DLL boʻlsin.
# Statik ulansa ilova `vulkan-1.dll` ga qatʼiy bogʻlanadi va Vulkan drayveri
# yoʻq mashinada UMUMAN ochilmaydi.
# GGML_CPU_ALL_VARIANTS — 15 xil CPU DLL (sse42 … zen4): eski mashinada ham
# ishlaydi, yangisida tez. **Faqat x64 da**: ggml ARM uchun variantlarni
# Linux, Android va Apple tizimlarida biladi, Windows'da esa
# «Unsupported ARM target OS: Windows» bilan konfiguratsiyani toʻxtatadi.
#
# Misollar va testlar OʻCHIRILGAN: bizga faqat kutubxonalar va DLL'lar kerak.
# Ular build vaqtini ikki barobar uzaytiradi va ustiga
# `examples/deprecation-warning` mingw'da umuman kompilyatsiya boʻlmaydi
# (`EXIT_FAILURE` eʼlon qilinmagan).
whisper_qur() {
    local arch="$1"
    local build="whisper.cpp/build-win-${arch}-dl"
    bogliqlik_tayyorla "$ROOT/whisper.cpp" "$WHISPER_URL" "$WHISPER_COMMIT"
    if [ "$TEZ" = 1 ] && [ -f "$build/bin/libwhisper.dll" ]; then
        echo "whisper.cpp ($arch) — tayyor, oʻtkazib yuborildi"
        return
    fi
    qadam "whisper.cpp — $arch"

    local vulkan_lib="$ROOT/win/cmake/vulkan-lib/libvulkan-1-${arch}.a"
    local variantlar=ON
    [ "$arch" = arm64 ] && variantlar=OFF
    cmake -S whisper.cpp -B "$build" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/win/cmake/toolchain-win-${arch}.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DGGML_VULKAN=ON -DGGML_BACKEND_DL=ON \
        -DGGML_CPU_ALL_VARIANTS="$variantlar" \
        -DBUILD_SHARED_LIBS=ON -DGGML_NATIVE=OFF \
        -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF \
        -DWHISPER_BUILD_SERVER=OFF \
        -DVulkan_INCLUDE_DIR="$BREW_PREFIX/include" \
        -DVulkan_LIBRARY="$vulkan_lib" \
        -DVulkan_GLSLC_EXECUTABLE="$BREW_PREFIX/bin/glslc" \
        -DVulkan_GLSLANG_VALIDATOR_EXECUTABLE="$BREW_PREFIX/bin/glslangValidator"
    nice -n 10 cmake --build "$build" -j"$JOBS"
}

# --------------------------------------------------------------- tarjimon
#
# SentencePiece v0.2.0 ATAYLAB: `master` tashqi abseil-cpp ni tortadi.
# CTranslate2 ARM64 da alohida toolchain ishlatadi — sababi
# `toolchain-win-arm64-tarjima.cmake` ichida yozilgan.
tarjima_qur() {
    local arch="$1"
    [ "$TARJIMA" = 1 ] || return 0

    local sp="sentencepiece/build-win-${arch}"
    local ct2="ctranslate2/build-win-${arch}"
    bogliqlik_tayyorla "$ROOT/sentencepiece" "$SP_URL" "$SP_COMMIT"
    bogliqlik_tayyorla "$ROOT/ctranslate2" "$CT2_URL" "$CT2_COMMIT" --submodullar
    if [ "$TEZ" = 1 ] && [ -f "$ct2/libctranslate2.a" ] && [ -f "$sp/src/libsentencepiece.a" ]; then
        echo "tarjimon ($arch) — tayyor, oʻtkazib yuborildi"
        return
    fi

    qadam "SentencePiece — $arch"
    cmake -S sentencepiece -B "$sp" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/win/cmake/toolchain-win-${arch}.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSPM_ENABLE_SHARED=OFF -DSPM_ENABLE_TCMALLOC=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    nice -n 10 cmake --build "$sp" -j"$JOBS"

    # cpuinfo Windows'ning ARM64 versiyasini bilmaydi va har chaqiruvda
    # stderr'ga «operating system is not supported in cpuinfo» yozadi —
    # bitta tarjimada besh marta. Log darajasini oʻchiramiz.
    qadam "CTranslate2 — $arch (15–25 daqiqa, mashina qiziydi)"
    local ct2_tc="$ROOT/win/cmake/toolchain-win-${arch}.cmake"
    [ "$arch" = arm64 ] && ct2_tc="$ROOT/win/cmake/toolchain-win-arm64-tarjima.cmake"
    cmake -S ctranslate2 -B "$ct2" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ct2_tc" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_CLI=OFF \
        -DWITH_MKL=OFF -DWITH_DNNL=OFF -DWITH_ACCELERATE=OFF -DWITH_RUY=ON \
        -DOPENMP_RUNTIME=NONE -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCPUINFO_LOG_LEVEL=none
    nice -n 10 cmake --build "$ct2" -j"$JOBS"
}

# ------------------------------------------------------------------ ilova

ilova_qur() {
    local arch="$1"
    qadam "Kotib — $arch"
    # `app.rc` ikonkani talab qiladi, u esa gitignore'da (`assets/icon_1024.png`
    # dan yasaladi) — toza checkout'da busiz llvm-rc «AppIcon.ico not found»
    # bilan toʻxtardi. `build.ps1` ham shunday qiladi (`make_icon.ps1`).
    [ -f "win/res/AppIcon.ico" ] || "$ROOT/win/tools/make_icon.sh"
    cmake -S win -B "win/build-${arch}" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/win/cmake/toolchain-win-${arch}.cmake" \
        -DWHISPER_ROOT="$ROOT/whisper.cpp" \
        -DWHISPER_BUILD="$ROOT/whisper.cpp/build-win-${arch}-dl"
    nice -n 10 cmake --build "win/build-${arch}" -j"$JOBS"

    # whisper.cpp DLL'lari .exe yonida boʻlishi shart.
    cp -f "whisper.cpp/build-win-${arch}-dl/bin/"*.dll "win/build-${arch}/" 2>/dev/null || true

    # llvm-mingw ish paytidagi kutubxonalari.
    #
    # Ilovaning oʻzi `-static` bilan yigʻiladi va ularga muhtoj emas, LEKIN
    # whisper.cpp DLL'lari (`ggml*.dll`, `libwhisper.dll`) `BUILD_SHARED_LIBS`
    # bilan qurilgani uchun `libc++.dll`, `libunwind.dll` va `libomp.dll` ga
    # bogʻlanadi. Ular yonida boʻlmasa ilova UMUMAN ochilmaydi:
    #
    #     rubai-cli.exe - System Error
    #     The code execution cannot proceed because libc++.dll was not found.
    #
    # Bu xato faqat toza Windows'da koʻrinadi — VM'da birinchi ishga
    # tushirishdayoq chiqdi.
    local uch="$TC/${arch/x64/x86_64}-w64-mingw32"
    [ "$arch" = arm64 ] && uch="$TC/aarch64-w64-mingw32"
    for d in libc++.dll libunwind.dll libomp.dll; do
        [ -f "$uch/bin/$d" ] && cp -f "$uch/bin/$d" "win/build-${arch}/"
    done

    # Silero VAD modeli (S12) — exe yonida; oʻrnatuvchi `{app}` ga qoʻyadi.
    vad_tayyorla "$ROOT/vad/$VAD_NOM"
    cp -f "$ROOT/vad/$VAD_NOM" "win/build-${arch}/"

    # Uchinchi tomon litsenziyalari — oʻrnatuvchi `{app}` ga qoʻyadi, tray
    # menyusidagi «Litsenziyalar…» ochadi.
    "$ROOT/scripts/litsenziyalar.sh" "win/build-${arch}/Litsenziyalar.txt"
}

for arch in "${ARCHLAR[@]}"; do
    whisper_qur "$arch"
    tarjima_qur "$arch"
    ilova_qur "$arch"
done

qadam "Tayyor"
for arch in "${ARCHLAR[@]}"; do
    echo "  win/build-${arch}/Kotib.exe"
    echo "  win/build-${arch}/kotib-testlar.exe   (Windows'da yoki VM'da ishga tushiring)"
done
