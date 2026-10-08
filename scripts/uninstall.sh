#!/bin/bash
# Kotib — Mac'dan to'liq o'chirish.
#
# Ilovani, sozlamalarni, ruxsatlarni va o'rnatuvchi qoldiqlarini olib tashlaydi.
# Shundan keyin tizim ilovani hech qachon ko'rmagan holatga qaytadi — "toza
# foydalanuvchi" sinovini o'tkazish uchun ham shu ishlatiladi.
#
# Ishlatish:
#   sudo ./scripts/uninstall.sh                 # hammasini o'chiradi
#   sudo ./scripts/uninstall.sh --keep-model    # til modelini saqlab qoladi (785 MB)

set -uo pipefail

KEEP_MODEL=0
[ "${1:-}" = "--keep-model" ] && KEEP_MODEL=1

if [ "$(id -u)" != "0" ]; then
    echo "Bu skript root huquqini talab qiladi (paket kvitansiyasi va /Applications uchun)." >&2
    echo "Ishlatish:  sudo $0 ${1:-}" >&2
    exit 1
fi

# sudo ostida $HOME root'niki bo'lib qoladi — haqiqiy foydalanuvchini aniqlaymiz
REAL_USER="${SUDO_USER:-$(stat -f%Su /dev/console)}"
REAL_HOME=$(dscl . -read "/Users/$REAL_USER" NFSHomeDirectory 2>/dev/null | awk '{print $2}')
if [ -z "$REAL_HOME" ] || [ ! -d "$REAL_HOME" ]; then
    echo "Xato: '$REAL_USER' foydalanuvchisining uy papkasi aniqlanmadi." >&2
    exit 1
fi
echo "Foydalanuvchi: $REAL_USER  ($REAL_HOME)"
echo

step() { printf "  %-46s" "$1"; }
ok()   { echo "✓ $1"; }

# ─── 1. Ishlab turgan nusxalarni to'xtatish
step "Ishlayotgan nusxalar to'xtatilmoqda"
pkill -f "Kotib.app/Contents/MacOS/Kotib" 2>/dev/null
pkill -f "Audio-Matnga.app/Contents/MacOS/AudioMatnga" 2>/dev/null
pkill -f "RubaiSTT Dictation.app/Contents/MacOS/RubaiSTTDictation" 2>/dev/null
sleep 1
ok ""

# ─── 2. Ilova bundle'lari (yangi va eski nom, tizim va foydalanuvchi papkalari)
step "Ilova papkalari"
removed=0
for p in "/Applications/Kotib.app" \
         "/Applications/Audio-Matnga.app" \
         "/Applications/RubaiSTT Dictation.app" \
         "$REAL_HOME/Applications/Kotib.app" \
         "$REAL_HOME/Applications/Audio-Matnga.app" \
         "$REAL_HOME/Applications/RubaiSTT Dictation.app"; do
    [ -e "$p" ] && { rm -rf "$p"; removed=$((removed+1)); }
done
ok "$removed ta"

# ─── 3. LaunchAgent (eski setup.sh qoldirgan avtoyuklash)
step "LaunchAgent"
PLIST="$REAL_HOME/Library/LaunchAgents/com.rubaistt.dictation.plist"
if [ -f "$PLIST" ]; then
    UID_N=$(id -u "$REAL_USER" 2>/dev/null)
    [ -n "$UID_N" ] && launchctl bootout "gui/$UID_N" "$PLIST" 2>/dev/null
    rm -f "$PLIST"
    ok "o'chirildi"
else
    ok "yo'q edi"
fi

# ─── 4. Paket kvitansiyasi
step "Paket kvitansiyasi"
# 1.2 dan boshlab paket ikki komponentli: ilova va model (.model).
n=0
for id in com.rubaistt.dictation.pkg com.rubaistt.dictation.pkg.model; do
    if pkgutil --pkgs 2>/dev/null | grep -qx "$id"; then
        pkgutil --forget "$id" >/dev/null 2>&1 && n=$((n+1))
    fi
done
ok "$n ta unutildi"

# ─── 5. Sozlamalar, kesh, holat fayllari
step "Sozlama va kesh fayllari"
n=0
# --keep-model: Application Support/Kotib dagi hamma narsa ketadi, faqat
# models/ qoladi. Ilgari papka butunligicha oʻchirilardi — bayroq modelni
# saqlamas edi (barqarorlik spec'i, I4).
KOTIB_DATA="$REAL_HOME/Library/Application Support/Kotib"
if [ "$KEEP_MODEL" = "1" ] && [ -d "$KOTIB_DATA" ]; then
    for q in "$KOTIB_DATA"/* "$KOTIB_DATA"/.[!.]*; do
        [ -e "$q" ] || continue
        [ "$(basename "$q")" = "models" ] && continue
        rm -rf "$q"; n=$((n+1))
    done
    KOTIB_DATA=""
fi
for p in "$REAL_HOME/Library/Preferences/com.rubaistt.dictation.plist" \
         ${KOTIB_DATA:+"$KOTIB_DATA"} \
         "$REAL_HOME/Library/Application Support/Audio-Matnga" \
         "$REAL_HOME/Library/Logs/Kotib.log" \
         "$REAL_HOME/Library/Logs/Kotib.1.log" \
         "$REAL_HOME/Library/Logs/Kotib.2.log" \
         "$REAL_HOME/Library/Caches/com.rubaistt.dictation" \
         "$REAL_HOME/Library/HTTPStorages/com.rubaistt.dictation" \
         "$REAL_HOME/Library/Saved Application State/com.rubaistt.dictation.savedState"; do
    [ -e "$p" ] && { rm -rf "$p"; n=$((n+1)); }
done
# UserDefaults keshda ham turadi — cfprefsd'ni yangilatamiz
sudo -u "$REAL_USER" defaults delete com.rubaistt.dictation >/dev/null 2>&1
ok "$n ta"

# ─── 5b. Keychain — foydalanuvchining LLM API kalitlari (`llm_providers.swift`,
# servis `com.rubaistt.dictation.llm`, har provayder bitta yozuv). Ilgari ular
# ilova oʻchirilgandan keyin ham qolardi (I4). `security` bir chaqiruvda
# bittasini oʻchiradi — topilmay qolguncha takrorlanadi (chegara bilan).
step "Keychain (AI kalitlari)"
KEYCHAIN="$REAL_HOME/Library/Keychains/login.keychain-db"
n=0
if [ -f "$KEYCHAIN" ]; then
    for _ in $(seq 1 50); do
        sudo -u "$REAL_USER" security delete-generic-password \
            -s com.rubaistt.dictation.llm "$KEYCHAIN" >/dev/null 2>&1 || break
        n=$((n+1))
    done
fi
ok "$n ta"

# ─── 6. Ruxsatlar (System Settings ro'yxatidagi yozuvlar shu bilan tozalanadi)
step "Accessibility va Mikrofon ruxsatlari"
sudo -u "$REAL_USER" tccutil reset Accessibility com.rubaistt.dictation >/dev/null 2>&1
sudo -u "$REAL_USER" tccutil reset Microphone    com.rubaistt.dictation >/dev/null 2>&1
ok "qaytarildi"

# ─── 7a. Umumiy model (1.2+ .pkg qoʻyadi, hamma foydalanuvchi uchun)
step "Umumiy model (/Library/Application Support/Kotib)"
TIZIM="/Library/Application Support/Kotib"
if [ "$KEEP_MODEL" = "1" ]; then
    ok "saqlandi (--keep-model)"
elif [ -d "$TIZIM" ]; then
    sz=$(du -sh "$TIZIM" 2>/dev/null | cut -f1)
    rm -rf "$TIZIM"
    ok "o'chirildi ($sz)"
else
    ok "yo'q edi"
fi

# ─── 7. Eski model va log (~/rubai-stt) — FAQAT maʼlum fayllar.
#
# Ilgari `rm -rf ~/rubai-stt` edi — root sifatida, butun papka. Bu papka
# dasturchi mashinasida manba kodi yoki boshqa narsa ham boʻlishi mumkin
# (I4). Endi faqat ilova va `setup.sh`/`convert_model.sh` qoʻyadigan nomlar
# oʻchiriladi, papkalar esa boʻsh qolsagina.
step "Eski model va log (~/rubai-stt)"
ESKI="$REAL_HOME/rubai-stt"
n=0
if [ -f "$ESKI/dictation.log" ]; then
    rm -f "$ESKI/dictation.log"; n=$((n+1))
fi
if [ "$KEEP_MODEL" != "1" ]; then
    for f in "$ESKI/models/ggml-rubaistt.bin" "$ESKI/models/ggml-rubaistt.bin.part" \
             "$ESKI/models/ggml-rubaistt-f16.bin" "$ESKI/models/ggml-model.bin"; do
        [ -f "$f" ] && { rm -f "$f"; n=$((n+1)); }
    done
fi
rmdir "$ESKI/models" 2>/dev/null
rmdir "$ESKI" 2>/dev/null
if [ -d "$ESKI" ]; then
    ok "$n ta fayl (papkada boshqa narsa bor — qoldirildi)"
else
    ok "$n ta fayl"
fi

echo
echo "✅ Kotib Mac'dan to'liq olib tashlandi."
echo
echo "Eslatma: System Settings > Privacy & Security > Accessibility ro'yxatida"
echo "eski yozuvlar ko'rinib qolsa, Settings'ni yopib qayta oching — ro'yxat yangilanadi."
