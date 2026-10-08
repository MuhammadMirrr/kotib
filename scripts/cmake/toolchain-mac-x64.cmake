# macOS: arm64 mashinasida x86_64 (Intel) uchun build.
#
# Nega toolchain fayli kerak, nega oddiy `-DCMAKE_OSX_ARCHITECTURES=x86_64` yetmaydi:
# CTranslate2 arxitekturani `CMAKE_SYSTEM_PROCESSOR` dan oʻqiydi
# (`CMakeLists.txt` ~275-satr) va `CMAKE_OSX_ARCHITECTURES` ni faqat "arm64"
# qiymati uchun tekshiradi. `CMAKE_SYSTEM_PROCESSOR` ni buyruq satridan `-D`
# bilan berib boʻlmaydi: `project()` uni host qiymati bilan qayta yozadi.
# Natijada arm64 mashinasida NEON yadrolari x86_64 uchun kompilyatsiya qilinadi
# va build «<arm_neon.h> is intended only for ARM and AArch64 targets» deb yiqiladi.
#
# `CMAKE_SYSTEM_NAME` ni berish shart — faqat shunda CMake toolchain faylidagi
# `CMAKE_SYSTEM_PROCESSOR` ni saqlab qoladi (va cross-compile rejimiga oʻtadi).
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# CACHE ... FORCE SHART. `CMAKE_SYSTEM_NAME` berilgach CMake'ning Darwin
# moduli `CMAKE_OSX_ARCHITECTURES` ni boʻsh cache qiymati bilan yozadi va
# toolchain faylidagi oddiy `set()` ni yutib yuboradi. Natijada `-arch`
# bayrogʻi umuman qoʻyilmaydi: Ruy AVX512 yoʻlini tanlaydi, lekin kompilyator
# arm64 ga qaratilgan boʻlib qoladi va «unsupported option '-mavx512f' for
# target 'arm64-apple-darwin'» xatosi chiqadi.
set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "Intel (x86_64)" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "macOS 13.0" FORCE)
