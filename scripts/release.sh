#!/bin/bash
# Kotib — tarqatish uchun relizni yig'adi.
#
# 2026-08 dan boshlab macOS tarqatish DMG emas, .pkg orqali:
#   • DMG "Applications'ga sudrash" qadamini talab qilardi;
#   • va eng muhimi — notarize qilinmagani uchun macOS 15+ uni umuman bloklardi.
# Endi Windows'dagi setup.exe kabi sehrgar bor. Butun mantiq make_pkg.sh da.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec bash "$ROOT/scripts/make_pkg.sh" "$@"
