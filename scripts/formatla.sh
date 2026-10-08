#!/usr/bin/env bash
#
# Kodni repo uslubida formatlaydi — faqat koʻrinish, xulqqa tegmaydi.
#   Swift  — `.swift-format` (lint qoidalari ATAYLAB oʻchiq: nomlar va tuzilish
#            oʻzgarmasin), Xcode/Swift 6 dagi `swift format`.
#   C/C++  — `.clang-format` (include tartibi va izohlar tegilmaydi), llvm-mingw
#            toolchain'idagi `clang-format`: VERSIYASI PINLANGAN, chunki boshqa
#            versiya boshqacha formatlaydi va `--tekshir` bekorga yiqiladi.
#            `win/third_party/` (Monocypher) — begona kod, tegilmaydi.
#
#   ./scripts/formatla.sh            # joyida formatlaydi
#   ./scripts/formatla.sh --tekshir  # faqat tekshiradi (CI): farq boʻlsa — 1
#
# Fayllar roʻyxati — `git ls-files`: YANGI fayl git'ga qoʻshilmaguncha
# (`git add -N <fayl>` yetadi) formatlanmaydi ham, tekshirilmaydi ham.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

source "$ROOT/scripts/bogliqliklar.env"
CF="${CLANG_FORMAT:-${TOOLCHAIN:-$HOME/Developer/.toolchains/llvm-mingw-${LLVM_MINGW_VERSIYA}-ucrt-macos-universal}/bin/clang-format}"
[ -x "$CF" ] || { echo "clang-format topilmadi: $CF (CLANG_FORMAT= bilan bering)" >&2; exit 1; }

SWIFT=()
while IFS= read -r f; do SWIFT+=("$f"); done < <(git ls-files '*.swift')
CPP=()
while IFS= read -r f; do CPP+=("$f"); done < <(git ls-files 'src/*.c' 'src/*.cpp' 'src/*.h' \
    'win/*.c' 'win/*.cpp' 'win/*.h' | grep -v '^win/third_party/')

if [ "${1:-}" = "--tekshir" ]; then
    farq=0
    for f in "${SWIFT[@]}"; do
        if ! swift format format --configuration .swift-format "$f" | cmp -s "$f" -; then
            echo "formatlanmagan: $f"; farq=1
        fi
    done
    for f in "${CPP[@]}"; do
        if ! "$CF" --style=file "$f" | cmp -s "$f" -; then
            echo "formatlanmagan: $f"; farq=1
        fi
    done
    [ "$farq" = 0 ] && echo "✓ ${#SWIFT[@]} Swift va ${#CPP[@]} C/C++ fayli formatlangan"
    exit "$farq"
fi

swift format format --configuration .swift-format -i "${SWIFT[@]}"
"$CF" --style=file -i "${CPP[@]}"
echo "✓ ${#SWIFT[@]} Swift va ${#CPP[@]} C/C++ fayli formatlandi"
