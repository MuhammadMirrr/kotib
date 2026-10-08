#!/usr/bin/env bash
#
# UTM'dagi Windows 11 ARM VM'ni boshqarish.
#
#   ./win/tools/vm.sh yoz '<buyruq>'   VM ichiga buyruq yozadi va Enter bosadi
#   ./win/tools/vm.sh klik <x> <y>     VM ichida sichqoncha bosadi (mehmon piksellari)
#   ./win/tools/vm.sh surat [fayl]     VM oynasining rasmini oladi
#   ./win/tools/vm.sh ishga            VM'ni yoqadi
#   ./win/tools/vm.sh toxta            VM'ni oʻchiradi va toʻxtashini kutadi
#   ./win/tools/vm.sh disk-yasa        sinov disk tasvirini yasaydi (almashtirmasdan)
#   ./win/tools/vm.sh disk             sinov diskini yangi build bilan almashtiradi
#   ./win/tools/vm.sh yangila          disk + ishga tushirish (eng koʻp ishlatiladigan)
#
# NEGA UTM'NING OʻZ API'SI: `System Events` ning `keystroke` usuli raqamlarni,
# `=`, `-`, `/`, `*` va nuqtani tushirib qoldiradi va fokusni oʻgʻirlaydi —
# bir marta `dism` oʻrniga `daism` yozilib, Windows oʻrnatilishi yarim tunda
# toʻxtab qolgan. UTM'ning `input keystroke` esa fokus talab qilmaydi va
# belgilarni buzmaydi.
#
# BU SKRIPT ENDI KUNDALIK ISH UCHUN EMAS. VM'da tarmoq va SSH bor, shuning
# uchun yangi build `./win/tools/win.sh --joyla` bilan 20 soniyada koʻchadi.
# Bu yerda faqat VM'ni yoqish/oʻchirish va sinov diskini NOLDAN yasash
# (`disk-yasa`) qoladi — masalan yangi VM tayyorlashda.
#
# `disk` buyrugʻi (diskni konteyner ichida almashtirish) Terminal'ga
# **Full Disk Access** talab qiladi va deyarli kerak emas.
#
# UTM'ning `update configuration` API'sini disk almashtirish uchun ISHLATMANG:
# u disk fayl yoʻlini umuman koʻrsatmaydi, va `drives` ga id'siz tegish bir
# marta toʻliq oʻrnatilgan Windows'ni yoʻq qilgan.
set -euo pipefail

VM_NOM="${VM_NOM:-Kotib-Win11-ARM-6}"
# Identifikatorni NOM boʻyicha topamiz: VM qayta import qilinsa u oʻzgaradi,
# nom esa qoladi.
VM_ID="$(osascript -e "tell application \"UTM\" to get id of virtual machine named \"$VM_NOM\"" 2>/dev/null || true)"
[ -n "$VM_ID" ] || { echo "UTM'da «$VM_NOM» nomli VM topilmadi" >&2; exit 1; }
KONTEYNER="$HOME/Library/Containers/com.utmapp.UTM/Data/Documents/${VM_NOM}.utm/Data"
SINOV_DISK="$KONTEYNER/kotib-test.img"
YANGI_DISK="$HOME/Developer/.vm/kotib-test2.img"
# virtio ARM64 drayverlari va qemu-ga (virtio-win.iso dan bir marta ajratilgan).
VIRTIO="$HOME/Developer/.vm/virtio-arm64"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARCH="${VM_ARCH:-arm64}"          # VM ichidagi Windows ARM64

utm() { osascript -e "tell application \"UTM\" to $*"; }

yoz() {
    local matn="${1//\\/\\\\}"
    matn="${matn//\"/\\\"}"
    utm "input keystroke (virtual machine id \"$VM_ID\") text \"$matn\"" >/dev/null
    utm "input scan code (virtual machine id \"$VM_ID\") codes {28, 156}" >/dev/null
}

# Maxsus tugmalar. PS/2 set 1 skan kodlari: bosish = kod, qoʻyib yuborish
# = kod|0x80. Kengaytirilgan tugmalar oldiga 224 (0xE0) qoʻyiladi.
# `input keystroke` faqat oddiy matn yuboradi — Win, Ctrl, UAC tasdigʻi
# uchun aynan shu funksiya kerak.
tugma() {
    local kodlar
    case "$1" in
        enter)  kodlar="28, 156" ;;
        esc)    kodlar="1, 129" ;;
        tab)    kodlar="15, 143" ;;
        chap)   kodlar="224, 75, 224, 203" ;;
        ong)    kodlar="224, 77, 224, 205" ;;
        win)    kodlar="224, 91, 224, 219" ;;
        win-r)  kodlar="224, 91, 19, 147, 224, 219" ;;
        # Ctrl+Shift+Enter — «administrator sifatida ishga tushirish».
        admin-enter) kodlar="29, 42, 28, 156, 170, 157" ;;
        *) echo "nomaʼlum tugma: $1" >&2; return 1 ;;
    esac
    utm "input scan code (virtual machine id \"$VM_ID\") codes {$kodlar}" >/dev/null
}

# Matnni Enter'siz yozadi (dialog maydonlari uchun).
yozEnt() {
    local matn="${1//\\/\\\\}"
    matn="${matn//\"/\\\"}"
    utm "input keystroke (virtual machine id \"$VM_ID\") text \"$matn\"" >/dev/null
}

klik() {
    utm "input mouse click (virtual machine id \"$VM_ID\") at {$1, $2}" >/dev/null
}

# Oyna raqamini beradigan kichik yordamchi. Birinchi chaqiruvda yigʻiladi.
oynaIdVositasi() {
    local bin="${TMPDIR:-/tmp}/kotib-vm-oyna-id"
    local manba="$ROOT/win/tools/vm-oyna-id.swift"
    if [ ! -x "$bin" ] || [ "$manba" -nt "$bin" ]; then
        swiftc -O -o "$bin" "$manba" >/dev/null 2>&1 || return 1
    fi
    echo "$bin"
}

surat() {
    local chiqish="${1:-/tmp/kotib-vm.png}"
    local vosita id
    vosita="$(oynaIdVositasi)" || { echo "vm-oyna-id yigʻilmadi" >&2; return 1; }
    id="$("$vosita" "$VM_NOM" 2>/dev/null || true)"
    if [ -z "$id" ]; then
        echo "VM oynasi topilmadi — UTM ochiqmi?" >&2
        return 1
    fi
    screencapture -x -o -l "$id" "$chiqish"
    echo "$chiqish"
}

toxta() {
    if ! pgrep -f 'QEMULauncher' >/dev/null; then
        echo "VM allaqachon toʻxtagan"
        return 0
    fi
    echo "Windows oʻchirilmoqda…"
    yoz 'shutdown /s /t 0' || true
    local kutish=0
    while pgrep -f 'QEMULauncher' >/dev/null; do
        sleep 3
        kutish=$((kutish + 3))
        if [ "$kutish" -gt 90 ]; then
            echo "Yumshoq oʻchirish ishlamadi — UTM orqali toʻxtatamiz" >&2
            utm "stop virtual machine id \"$VM_ID\"" >/dev/null || true
        fi
        if [ "$kutish" -gt 150 ]; then
            echo "VM toʻxtamadi" >&2
            return 1
        fi
    done
    echo "VM toʻxtadi"
}

ishga() {
    utm "start virtual machine id \"$VM_ID\"" >/dev/null
    echo "VM ishga tushdi"
}

# Sinov diskini noldan yasaydi: yangi binarlar, DLL'lar, nutq modeli va
# sinov audiolari. Windows uni FAT32 disk sifatida koʻradi.
diskYasa() {
    local model="$ROOT/.model-cache/ggml-rubaistt.bin"
    # Tarjima modeli macOS ilovasi yuklab olgan joyda turadi. Bor boʻlsa
    # diskka qoʻshamiz va VM'da tarjimani ham sinash mumkin boʻladi —
    # VM'da tarmoq yoʻq, uni u yerda yuklab boʻlmaydi.
    local tarjima="$HOME/Library/Application Support/Kotib/tarjima-model-33b"
    local qurilgan="$ROOT/win/build-$ARCH"
    local dllar="$ROOT/whisper.cpp/build-win-$ARCH-dl/bin"

    for y in "$qurilgan/Kotib.exe" "$qurilgan/kotib-testlar.exe" "$dllar"; do
        [ -e "$y" ] || { echo "topilmadi: $y (avval ./win/build-mac.sh $ARCH)" >&2; exit 1; }
    done

    # Hajm: binarlar + nutq modeli (0,8 GB) + tarjima modeli (3,4 GB) + zaxira.
    # FAT32 bitta faylni 4 GB gacha koʻtaradi — `model.bin` 3,4 GB, sigʻadi.
    local hajm=2500
    [ -d "$tarjima" ] && hajm=5500

    echo "Sinov diski yasalmoqda (${hajm} MB)…"
    rm -f "$YANGI_DISK"
    mkdir -p "$(dirname "$YANGI_DISK")"
    mkfile -n "${hajm}m" "$YANGI_DISK"

    local dev
    dev=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount "$YANGI_DISK" \
          | head -1 | awk '{print $1}')
    diskutil partitionDisk "$dev" MBR "MS-DOS FAT32" KOTIBTEST 100% >/dev/null

    local v=/Volumes/KOTIBTEST
    mkdir -p "$v/models"
    cp "$qurilgan"/*.exe "$v/"
    # Build papkasidagi DLL'lar: whisper.cpp niki VA llvm-mingw ish paytidagi
    # kutubxonalari (`libc++.dll` va boshqalar) — ular boʻlmasa ilova
    # umuman ochilmaydi.
    cp "$qurilgan"/*.dll "$v/" 2>/dev/null || true
    cp "$dllar"/*.dll "$v/" 2>/dev/null || true
    [ -f "$model" ] && cp "$model" "$v/models/"
    if [ -d "$tarjima" ]; then
        mkdir -p "$v/tarjima-model-33b"
        cp "$tarjima"/* "$v/tarjima-model-33b/"
    fi
    cp "$ROOT/whisper.cpp/samples/jfk.wav" "$ROOT/whisper.cpp/samples/jfk.mp3" "$v/" 2>/dev/null || true
    # virtio drayverlari: ularsiz VM'da tarmoq ham, mehmon agenti ham yoʻq —
    # har bir sinov uchun diskni almashtirib qayta yuklashga toʻgʻri keladi.
    [ -d "$VIRTIO" ] && cp -R "$VIRTIO" "$v/virtio"
    sozlaSkripti "$v"

    # `.cmd` fayllari CRLF bilan saqlanadi: cmd.exe faqat LF boʻlgan
    # skriptni satr oʻrtasidan kesib yuboradi (`jfk.wav` → `k.wav`).
    cp "$ROOT/win/tests/vm/testlar.cmd" "$v/"

    sync
    hdiutil detach "$dev" >/dev/null
    echo "Tayyor: $YANGI_DISK"
}

# VM'ni bir marta sozlaydigan skript diskka koʻchiriladi.
# ADMINISTRATOR huquqi bilan ishga tushirilishi SHART (pnputil, msiexec, sc).
sozlaSkripti() {
    cp "$ROOT/win/tests/vm/sozla.cmd" "$1/"
    # Ochiq kalit — sozla.cmd uni C:\ProgramData\ssh ga koʻchiradi.
    cp "$HOME/Developer/.vm/kotib_vm_key.pub" "$1/" 2>/dev/null || true
}

diskAlmashtir() {
    if ! /bin/ls "$KONTEYNER" >/dev/null 2>&1; then
        cat >&2 <<EOF
UTM konteyneriga kirib boʻlmadi:
  $KONTEYNER

Terminal'ga **Full Disk Access** bering (bir marta):
  System Settings → Privacy & Security → Full Disk Access → Terminal ✅
Keyin Terminal'ni qayta ishga tushiring va shu buyruqni takrorlang.
EOF
        exit 1
    fi
    if pgrep -f 'QEMULauncher' >/dev/null; then
        echo "VM ishlab turibdi — avval toʻxtatamiz" >&2
        toxta
    fi
    diskYasa
    rm -f "$SINOV_DISK"
    cp "$YANGI_DISK" "$SINOV_DISK"
    echo "Sinov diski almashtirildi"
}

buyruq="${1:-}"
shift || true
case "$buyruq" in
    yoz)   yoz "$@" ;;
    yoz-ent) yozEnt "$@" ;;
    tugma) tugma "$@" ;;
    klik)  klik "$@" ;;
    surat) surat "$@" ;;
    ishga) ishga ;;
    toxta) toxta ;;
    disk-yasa) diskYasa ;;
    disk)  diskAlmashtir ;;
    yangila) diskAlmashtir; ishga ;;
    *)
        sed -n '3,27p' "${BASH_SOURCE[0]}" | sed 's|^# \{0,1\}||'
        exit 2
        ;;
esac
