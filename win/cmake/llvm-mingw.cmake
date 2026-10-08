# llvm-mingw toolchain'i qayerdaligini aniqlaydi — ikkala toolchain fayli
# (`toolchain-win-x64.cmake`, `toolchain-win-arm64.cmake`) shuni include qiladi.
#
# Tartib:
#   1. `TOOLCHAIN` muhit oʻzgaruvchisi — `win/build-mac.sh` uni doim eksport
#      qiladi, boshqa joyga oʻrnatilgan toolchain shu bilan beriladi;
#   2. aks holda standart joy, versiyasi `scripts/bogliqliklar.env` dagi
#      `LLVM_MINGW_VERSIYA` dan (yagona manba — raqam bu yerda takrorlanmaydi).
#
# Natija: `KOTIB_LLVM_MINGW_BIN` — kompilyatorlar turgan `bin/` papka.
if(DEFINED ENV{TOOLCHAIN} AND NOT "$ENV{TOOLCHAIN}" STREQUAL "")
    set(_kotib_tc "$ENV{TOOLCHAIN}")
else()
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../scripts/bogliqliklar.env" _kotib_qator
         REGEX "^LLVM_MINGW_VERSIYA=")
    string(REGEX REPLACE "^LLVM_MINGW_VERSIYA=" "" _kotib_v "${_kotib_qator}")
    if(_kotib_v STREQUAL "")
        message(FATAL_ERROR "scripts/bogliqliklar.env da LLVM_MINGW_VERSIYA topilmadi")
    endif()
    set(_kotib_tc "$ENV{HOME}/Developer/.toolchains/llvm-mingw-${_kotib_v}-ucrt-macos-universal")
endif()

if(NOT EXISTS "${_kotib_tc}/bin")
    message(FATAL_ERROR "llvm-mingw topilmadi: ${_kotib_tc}\n"
                        "O'rnatish yo'riqnomasini ./win/build-mac.sh chiqaradi.")
endif()
set(KOTIB_LLVM_MINGW_BIN "${_kotib_tc}/bin")
