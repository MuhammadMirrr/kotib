#!/usr/bin/env bash
#
# Windows oʻrnatuvchisini (Inno Setup) VM ichida yigʻadi.
#
#   ./win/tools/ornatuvchi-yasa.sh [--model <yoʻl>]
#
# Natija: `dist/Kotib-<versiya>-win-setup.exe` — YAGONA fayl:
#   • x64 va ARM64 binarlari ikkalasi ham ichida, oʻrnatish paytida mosi
#     tanlanadi (`IsArm64`);
#   • nutq modeli ham ichida (`--model` bilan) — macOS'dagi `.pkg` kabi;
#   • tarjima modeli ICHKARIDA EMAS, u ilova ichida yuklab olinadi.
#
# `--model` berilmasa modelsiz variant chiqadi (`…-win-onlayn-setup.exe`) —
# u faqat ishlab chiqish uchun, tarqatiladigani har doim modelli.
#
# Har safar yonida avto-yangilanish paketi ham chiqadi —
# `Kotib-<versiya>-win-yangilash.exe` (modelsiz, `/DYangilash`, S7).
#
# NEGA VM: `iscc.exe` faqat Windows'da ishlaydi va macOS'da Wine talab
# qiladi. Windows 11 ARM VM'ida esa u emulyatsiya orqali muammosiz ishlaydi,
# ustiga-ustak oʻrnatuvchini shu yerning oʻzida sinab ham koʻrish mumkin.
#
# VM tayyorligi: `win/tools/win.sh` orqali SSH ishlashi kerak
# (`docs/windows/WINDOWS-PARITET.md` → «VM ustida ishlash»). Inno Setup bir marta
# oʻrnatiladi — skript uni topa olmasa oʻzi yuklab oladi.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Versiya — `VERSION` faylidan; iscc ga /DAppVersion bilan beriladi.
# shellcheck source=../../scripts/versiya.sh
source "$ROOT/scripts/versiya.sh"
WIN="$ROOT/win/tools/win.sh"
MEHMON='C:\kotib-orn'
ISCC='C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
INNO_URL='https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe'
PORT="${KOTIB_VM_PORT:-2222}"
KALIT="${KOTIB_VM_KEY:-$HOME/Developer/.vm/kotib_vm_key}"

MODEL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --model) MODEL="$2"; shift ;;
        *) echo "nomaʼlum parametr: $1" >&2; exit 2 ;;
    esac
    shift
done

qadam() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }

kochir() {   # kochir <mahalliy...> <mehmon papkasi>
    scp -P "$PORT" -i "$KALIT" -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$@"
}

# --- Inno Setup bormi
if ! "$WIN" "if exist \"$ISCC\" (echo BOR) else (echo YOQ)" | grep -q BOR; then
    qadam "Inno Setup oʻrnatilmoqda (bir martalik)"
    "$WIN" "cd /d C:\\Users\\kotib & curl -L -o is.exe $INNO_URL"
    "$WIN" 'cd /d C:\Users\kotib & is.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-'
    "$WIN" "if exist \"$ISCC\" (echo BOR) else (echo YOQ)" | grep -q BOR \
        || { echo "Inno Setup oʻrnatilmadi" >&2; exit 1; }
fi

qadam "Manbalar VM'ga koʻchirilmoqda"
"$WIN" "rmdir /s /q $MEHMON 2>nul & mkdir $MEHMON\\win\\installer & mkdir $MEHMON\\dist" >/dev/null 2>&1 || true
kochir "$ROOT/win/installer/rubai.iss" "$ROOT/win/installer/Uzbek.isl" "$ROOT/win/installer/shartlar.txt" kotib@127.0.0.1:C:/kotib-orn/win/installer/
kochir "$ROOT/LICENSE" kotib@127.0.0.1:C:/kotib-orn/LICENSE

# Har arxitektura uchun FAQAT tarqatiladigan fayllar. Sinov binarlarini
# (`kotib-testlar.exe`, `rubai-cli.exe`, `ui-demo.exe`) yuborishning
# keragi yoʻq va ular oʻrnatuvchiga ham tushmasligi kerak.
for arch in x64 arm64; do
    qurilgan="$ROOT/win/build-$arch"
    [ -f "$qurilgan/Kotib.exe" ] || {
        echo "topilmadi: $qurilgan/Kotib.exe (avval ./win/build-mac.sh hammasi)" >&2
        exit 1
    }
    "$WIN" "mkdir $MEHMON\\$arch" >/dev/null 2>&1 || true
    kochir "$qurilgan/Kotib.exe" "$qurilgan"/*.dll "$qurilgan/Litsenziyalar.txt" \
        "$qurilgan/ggml-silero-v6.2.0.bin" \
        "kotib@127.0.0.1:C:/kotib-orn/$arch/"
done

if [ -n "$MODEL" ]; then
    qadam "Model koʻchirilmoqda ($(du -h "$MODEL" | cut -f1))"
    kochir "$MODEL" 'kotib@127.0.0.1:C:/kotib-orn/ggml-rubaistt.bin'
fi

qadam "Oʻrnatuvchi yigʻilmoqda (Kotib $KOTIB_VERSIYA)"
bayroq="/DAppVersion=$KOTIB_VERSIYA /DSrcX64=$MEHMON\\x64 /DSrcArm64=$MEHMON\\arm64"
[ -n "$MODEL" ] && bayroq="$bayroq /DBundleModel=$MEHMON\\ggml-rubaistt.bin"
"$WIN" "\"$ISCC\" $bayroq $MEHMON\\win\\installer\\rubai.iss"

# Avto-yangilanish paketi — modelsiz (`Kotib-<v>-win-yangilash.exe`, S7).
# `reliz.sh win` aynan shu faylni imzolaydi.
qadam "Yangilanish paketi yigʻilmoqda (modelsiz)"
"$WIN" "\"$ISCC\" /DAppVersion=$KOTIB_VERSIYA /DSrcX64=$MEHMON\\x64 /DSrcArm64=$MEHMON\\arm64 /DYangilash $MEHMON\\win\\installer\\rubai.iss"

qadam "Natijani olib kelamiz"
mkdir -p "$ROOT/dist"
for f in $("$WIN" "cd /d $MEHMON\\dist & dir /b *.exe" | tr -d '\r'); do
    "$WIN" --olib "C:/kotib-orn/dist/$f" "$ROOT/dist/$f"
    chmod 644 "$ROOT/dist/$f"
    echo "  dist/$f  ($(du -h "$ROOT/dist/$f" | cut -f1))"
done
