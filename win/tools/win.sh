#!/usr/bin/env bash
#
# Windows VM ichida buyruq bajaradi (SSH orqali).
#
#   ./win/tools/win.sh 'd: & dir'
#   ./win/tools/win.sh --nusxa <mahalliy fayl> <mehmondagi yoʻl>
#   ./win/tools/win.sh --olib <mehmondagi yoʻl> <mahalliy fayl>
#   ./win/tools/win.sh --joyla         yangi build'ni D: ga koʻchiradi
#
# SSH server VM ichida `sozla.cmd` bilan oʻrnatiladi, port UTM'ning
# foydalanuvchi tarmogʻi orqali 2222 ga yoʻnaltirilgan. Standart qobiq —
# `cmd.exe` (PowerShell 5.1 da `&&` yoʻq va tirnoqlar boshqacha).
set -euo pipefail

PORT="${KOTIB_VM_PORT:-2222}"
KALIT="${KOTIB_VM_KEY:-$HOME/Developer/.vm/kotib_vm_key}"
MEZBON="kotib@127.0.0.1"
BAYROQ=(-p "$PORT" -i "$KALIT" -o StrictHostKeyChecking=no
        -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR
        -o ConnectTimeout=10)

# Yangi qurilgan binarlarni sinov diskiga koʻchiradi. Disk tasvirini
# qayta yasab, VM'ni oʻchirib-yoqish endi KERAK EMAS — shuning uchun bir
# tsikl 15 daqiqadan 20 soniyaga tushdi.
joyla() {
    local ildiz arch qurilgan
    ildiz="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
    arch="${VM_ARCH:-arm64}"
    qurilgan="$ildiz/win/build-$arch"
    [ -d "$qurilgan" ] || { echo "topilmadi: $qurilgan" >&2; exit 1; }
    # Ishlab turgan ilova oʻz .exe va .dll fayllarini qulflaydi — avval
    # yopamiz, aks holda scp «dest open … Failure» beradi.
    ssh "${BAYROQ[@]}" "$MEZBON" 'taskkill /f /im Kotib.exe' >/dev/null 2>&1 || true
    sleep 2
    scp -P "$PORT" -i "$KALIT" -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
        "$qurilgan"/*.exe "$qurilgan"/*.dll \
        "$ildiz/win/tests/vm/testlar.cmd" "$MEZBON:d:/"
    echo "joylandi: $(basename "$qurilgan") → D:"
}

case "${1:-}" in
    --joyla) joyla ;;
    --nusxa) shift; exec scp -P "$PORT" -i "$KALIT" -o StrictHostKeyChecking=no \
                  -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$1" "$MEZBON:$2" ;;
    --olib)  shift; exec scp -P "$PORT" -i "$KALIT" -o StrictHostKeyChecking=no \
                  -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$MEZBON:$1" "$2" ;;
    "")      exec ssh "${BAYROQ[@]}" "$MEZBON" ;;
    *)       exec ssh "${BAYROQ[@]}" "$MEZBON" "$@" ;;
esac
