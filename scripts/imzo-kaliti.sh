#!/usr/bin/env bash
#
# Avto-yangilanish imzo kaliti (Ed25519) bilan ishlash.
#
#   ./scripts/imzo-kaliti.sh yarat          kalit yoʻq boʻlsa yaratadi, ochiq kalitni yozadi
#   ./scripts/imzo-kaliti.sh ochiq          ochiq kalitni chiqaradi (base64)
#   ./scripts/imzo-kaliti.sh zaxira <fayl>  MAXFIY kalitni faylga eksport qiladi
#   ./scripts/imzo-kaliti.sh tikla <fayl>   zaxiradan Keychain'ga qaytaradi (yangi Mac)
#   ./scripts/imzo-kaliti.sh tikla-sinov    zaxira tiklanishini sinaydi, hech narsa qoldirmaydi
#
# Kalit qayerda: login Keychain, Sparkle yozuvi, hisob nomi `kotib`
# (`generate_keys --account kotib`). Boshqa Sparkle loyihalari bilan
# aralashmasin deb standart `ed25519` hisobi ishlatilmaydi.
#
# BU KALIT YOʻQOLSA YANGILANISH CHIQARIB BOʻLMAYDI: ilovalar faqat shu kalit
# bilan imzolangan manifest va faylni qabul qiladi (ochiq kalit ilova ichida).
# Shuning uchun maxfiy kalit ikki oflayn joyda, shifrlangan holda saqlanadi
# (`AGENTS.md` → «Imzo kaliti»). Kalit HECH QACHON repoga tushmaydi.
#
# Ochiq kalit maxfiy emas — `scripts/yangilanish-kaliti.pub` da turadi va
# ilovalarga kompilyatsiya vaqtida joylanadi (S5, S8).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=bogliqliklar.sh
source "$ROOT/scripts/bogliqliklar.sh"
sparkle_tayyorla "$ROOT/sparkle"
GK="$ROOT/sparkle/bin/generate_keys"
HISOB="kotib"
PUB="$ROOT/scripts/yangilanish-kaliti.pub"

# ochiq_kalit <hisob> — base64 ochiq kalit, yoʻq boʻlsa boʻsh satr.
# `generate_keys -p` kalit yoʻqligini xato sifatida emas, stdout'ga
# «ERROR! No existing signing key found!» deb yozadi — shuning uchun natija
# formati boʻyicha tekshiriladi: 32 bayt → 44 belgili base64.
ochiq_kalit() {
    local k
    k="$("$GK" --account "$1" -p 2>/dev/null | tr -d '[:space:]' || true)"
    if printf '%s' "$k" | grep -Eq '^[A-Za-z0-9+/]{43}=$'; then printf '%s' "$k"; fi
}

case "${1:-}" in
    yarat)
        bor="$(ochiq_kalit "$HISOB")"
        if [ -n "$bor" ]; then
            echo "Kalit allaqachon bor (hisob: $HISOB) — yangisi yaratilmadi."
        else
            "$GK" --account "$HISOB" >/dev/null
            bor="$(ochiq_kalit "$HISOB")"
            [ -n "$bor" ] || { echo "Xato: kalit yaratilmadi" >&2; exit 1; }
            echo "Yangi kalit yaratildi (hisob: $HISOB)."
        fi
        printf '%s\n' "$bor" > "$PUB"
        echo "Ochiq kalit: $bor"
        echo "Yozildi: scripts/yangilanish-kaliti.pub"
        ;;
    ochiq)
        bor="$(ochiq_kalit "$HISOB")"
        [ -n "$bor" ] || { echo "Kalit yoʻq (hisob: $HISOB). Avval: $0 yarat" >&2; exit 1; }
        echo "$bor"
        ;;
    zaxira)
        fayl="${2:?zaxira fayli yoʻli kerak}"
        [ ! -e "$fayl" ] || { echo "Xato: $fayl allaqachon bor — ustidan yozilmaydi" >&2; exit 1; }
        ( umask 077; "$GK" --account "$HISOB" -x "$fayl" )
        echo "Maxfiy kalit eksport qilindi: $fayl (faqat siz oʻqiy olasiz)"
        echo "Endi uni ikki oflayn joyga shifrlangan holda koʻchiring va bu nusxani oʻchiring."
        ;;
    tikla)
        fayl="${2:?zaxira fayli yoʻli kerak}"
        "$GK" --account "$HISOB" -f "$fayl"
        bor="$(ochiq_kalit "$HISOB")"
        kutilgan="$(tr -d '[:space:]' < "$PUB")"
        if [ "$bor" = "$kutilgan" ]; then
            echo "Tiklandi — ochiq kalit repodagi bilan bir xil."
        else
            echo "DIQQAT: tiklangan kalit repodagi ochiq kalitga mos EMAS" >&2
            exit 1
        fi
        ;;
    tikla-sinov)
        # Zaxira haqiqatan tiklanadimi — asl kalitga tegmasdan: eksport →
        # boshqa hisobga import → ochiq kalitlar solishtiriladi → sinov
        # yozuvi va eksport fayli oʻchiriladi.
        asl="$(ochiq_kalit "$HISOB")"
        [ -n "$asl" ] || { echo "Kalit yoʻq (hisob: $HISOB)" >&2; exit 1; }
        sinov_hisob="kotib-tiklash-sinovi"
        papka="$(mktemp -d)"
        chmod 700 "$papka"
        tozala() {
            rm -f "$papka/zaxira"
            rmdir "$papka" 2>/dev/null || true
            security delete-generic-password -s "https://sparkle-project.org" -a "$sinov_hisob" >/dev/null 2>&1 || true
        }
        trap tozala EXIT
        "$GK" --account "$HISOB" -x "$papka/zaxira" >/dev/null
        "$GK" --account "$sinov_hisob" -f "$papka/zaxira" >/dev/null
        tiklangan="$(ochiq_kalit "$sinov_hisob")"
        if [ -n "$tiklangan" ] && [ "$tiklangan" = "$asl" ]; then
            echo "✓ Zaxira tiklandi: ochiq kalit bir xil ($asl)"
        else
            echo "✗ Zaxiradan tiklangan kalit mos emas (asl: $asl, tiklangan: ${tiklangan:-yoʻq})" >&2
            exit 1
        fi
        ;;
    *)
        sed -n '3,9p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
