#!/bin/bash
# Kotib — macOS uchun imzolangan va notarize qilingan .pkg o'rnatuvchi.
#
# Nega .pkg, DMG emas: foydalanuvchi ilovani "Applications'ga sudrash" kerak emas.
# Ikki marta bosadi → Continue → Install → tayyor. Windows'dagi setup.exe bilan bir xil.
#
# Bitta o'rnatgich yasaydi: til modeli paket ichida keladi (~785 MB), internet
# kerak emas. 1.2 dan boshlab model .app ICHIDA EMAS — alohida komponent
# sifatida /Library/Application Support/Kotib/models ga o'rnatiladi:
# avto-yangilanish bundle'ni almashtiradi va model har safar qayta
# yuklanmasligi kerak (spec D2). Ilova ichidagi yuklovchi (model_download.swift)
# zaxira yo'l — model hech qayerda topilmasa ishga tushadi.
#
# Talablar:
#   • "Developer ID Application"  sertifikati (ilovani imzolash uchun)
#   • "Developer ID Installer"    sertifikati (.pkg ni imzolash uchun)
#   • notarytool keychain profili — busiz Gatekeeper ilovani BLOKLAYDI:
#       xcrun notarytool store-credentials rubai-notary \
#           --apple-id "siz@example.com" --team-id "PDNFP37X39"
#
# Ishlatish:
#   ./scripts/make_pkg.sh
#
# O'zgaruvchilar:
#   NOTARY_PROFILE=rubai-notary   SKIP_NOTARIZE=1 (sinov uchun; tarqatishga YARAMAYDI)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist"
WORK="$DIST/work"
# Versiya — `VERSION` faylidan (yagona manba; scripts/versiya.sh).
# shellcheck source=versiya.sh
source "$ROOT/scripts/versiya.sh"
VERSION="$KOTIB_VERSIYA"
# MODEL_SHA256 — paketga faqat tarqatilayotgan aynan shu model kirsin.
# shellcheck source=bogliqliklar.env
source "$ROOT/scripts/bogliqliklar.env"
MODEL_INSTALL_DIR="/Library/Application Support/Kotib/models"
APP_NAME="Kotib.app"
PKG_ID="com.rubaistt.dictation.pkg"
# Model manbai: avval repo ichidagi kesh (.model-cache), keyin setup.sh qo'yadigan yo'l.
# Kesh alohida turadi, chunki ~/rubai-stt ilovaning qidiruv yo'llaridan biri —
# u yerda model turса "toza foydalanuvchi" sinovini o'tkazib bo'lmaydi.
MODEL_SRC="${MODEL_SRC:-$ROOT/.model-cache/ggml-rubaistt.bin}"
[ -f "$MODEL_SRC" ] || MODEL_SRC="$HOME/Library/Application Support/Kotib/models/ggml-rubaistt.bin"
[ -f "$MODEL_SRC" ] || MODEL_SRC="$HOME/Library/Application Support/Audio-Matnga/models/ggml-rubaistt.bin"
[ -f "$MODEL_SRC" ] || MODEL_SRC="$HOME/rubai-stt/models/ggml-rubaistt.bin"
NOTARY_PROFILE="${NOTARY_PROFILE:-rubai-notary}"
ENT="$ROOT/src/entitlements.plist"

# ─────────────────────────────────────────────────────────── sertifikatlar

find_identity() {   # $1 = prefiks, masalan "Developer ID Application"
    security find-identity -v -p codesigning 2>/dev/null | grep "$1" | head -1 |
        sed -E 's/.*"(.*)"/\1/'
}

DEV_APP="${DEV_APP:-$(find_identity 'Developer ID Application')}"
DEV_INST="${DEV_INST:-$(security find-identity -v 2>/dev/null | grep 'Developer ID Installer' | head -1 | sed -E 's/.*"(.*)"/\1/')}"

[ -n "$DEV_APP" ]  || { echo "Xato: 'Developer ID Application' sertifikati topilmadi." >&2; exit 1; }
[ -n "$DEV_INST" ] || { echo "Xato: 'Developer ID Installer' sertifikati topilmadi." >&2; exit 1; }

# Notarize hisob ma'lumotlari ikki yo'l bilan berilishi mumkin:
#   1) App Store Connect API kaliti (NOTARY_KEY/…) — keychain kerak emas, CI uchun ham qulay
#   2) keychain profili (store-credentials bilan yaratilgan)
# Birinchisi ustun: keychain qulflangan bo'lsa ham ishlaydi.
NOTARY_ARGS=()
HAVE_NOTARY=0
if [ -n "${NOTARY_KEY:-}" ] && [ -n "${NOTARY_KEY_ID:-}" ] && [ -n "${NOTARY_ISSUER:-}" ]; then
    [ -f "$NOTARY_KEY" ] || { echo "Xato: kalit fayli topilmadi: $NOTARY_KEY" >&2; exit 1; }
    NOTARY_ARGS=(--key "$NOTARY_KEY" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_ISSUER")
    HAVE_NOTARY=1
elif xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1; then
    # `security find-generic-password` ishonchsiz: yangi `notarytool
    # store-credentials` maʼlumotni `security` koʻra olmaydigan formatda
    # saqlaydi. Shuning uchun profilni bevosita notarytool bilan sinaymiz —
    # muvaffaqiyatli boʻlsa, profil bor va yaroqli (tarmoq kerak).
    NOTARY_ARGS=(--keychain-profile "$NOTARY_PROFILE")
    HAVE_NOTARY=1
fi

if [ "$HAVE_NOTARY" = "0" ] && [ "${SKIP_NOTARIZE:-0}" != "1" ]; then
    cat >&2 <<MSG
Xato: notarize hisob ma'lumotlari topilmadi.

Notarize qilinmagan .pkg ni macOS 15+ BLOKLAYDI — foydalanuvchi o'rnata olmaydi.
Quyidagilardan birini bering:

  A) App Store Connect API kaliti (tavsiya — keychain qulflangan bo'lsa ham ishlaydi):
       export NOTARY_KEY=~/Downloads/AuthKey_XXXXXXX.p8
       export NOTARY_KEY_ID=XXXXXXX
       export NOTARY_ISSUER=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

  B) keychain profili:
       xcrun notarytool store-credentials $NOTARY_PROFILE \\
           --key ~/Downloads/AuthKey_XXXXXXX.p8 --key-id XXXXXXX --issuer <issuer-id>

(Faqat sinov uchun notarize'siz yasash:  SKIP_NOTARIZE=1 $0)
MSG
    exit 1
fi

echo "Sertifikat (ilova):     $DEV_APP"
echo "Sertifikat (o'rnatuvchi): $DEV_INST"
echo "Notarize:               $([ "$HAVE_NOTARY" = 1 ] && echo "ha (${NOTARY_ARGS[0]})" || echo "YO'Q — sinov rejimi")"
echo

# ─────────────────────────────────────────────────────────── yordamchilar

# Ilovani hardened runtime bilan imzolaydi. Ichkaridan tashqariga tartibda.
sign_app() {
    local app="$1"
    xattr -cr "$app" 2>/dev/null || true
    # Sparkle (S5) — ichdan tashqariga, hardened runtime bilan, bizning
    # entitlement'larimizsiz (notarizatsiya ichki kodni ham tekshiradi).
    local fw="$app/Contents/Frameworks/Sparkle.framework"
    if [ -d "$fw" ]; then
        codesign --force --options runtime --timestamp --sign "$DEV_APP" "$fw/Versions/B/Autoupdate"
        codesign --force --options runtime --timestamp --sign "$DEV_APP" "$fw/Versions/B/Updater.app"
        codesign --force --options runtime --timestamp --sign "$DEV_APP" "$fw"
    fi
    codesign --force --options runtime --timestamp \
        --entitlements "$ENT" --sign "$DEV_APP" \
        "$app/Contents/MacOS/Kotib"
    codesign --force --options runtime --timestamp \
        --entitlements "$ENT" --sign "$DEV_APP" "$app"
    codesign --verify --strict --verbose=2 "$app"
}

# O'rnatuvchi skriptlarini tayyorlaydi (eski nusxani tozalash + ilovani ochish).
write_scripts() {
    local dir="$1"
    mkdir -p "$dir"

    cat > "$dir/preinstall" <<'PRE'
#!/bin/bash
# Ishlab turgan nusxalarni to'xtatamiz — aks holda fayl band bo'lib o'rnatish buziladi
pkill -f "Kotib.app/Contents/MacOS/Kotib" 2>/dev/null || true
pkill -f "Audio-Matnga.app/Contents/MacOS/AudioMatnga" 2>/dev/null || true
pkill -f "RubaiSTT Dictation.app/Contents/MacOS/RubaiSTTDictation" 2>/dev/null || true

# ESKI VERSIYALARNI TO'LIQ OLIB TASHLAYMIZ.
# Ilova 1.1.0 da "Kotib" deb qayta nomlandi, ya'ni yangi bundle boshqa fayl nomida
# keladi va eskisi o'z-o'zidan almashmaydi. Qoldirilsa foydalanuvchida ikkita
# ilova, menyu satrida ikkita ikonka bo'lib qoladi va qaysi biri ishlayotgani
# tushunarsiz bo'ladi. Faqat ANIQ shu yo'llar o'chiriladi — global qidiruv yo'q.
# /Applications/Kotib.app ham — oldindan, BUTUNLAY. 1.1.0 bundle'i ichida
# 800 MB model bor edi; yangi bundle'da u yo'q. Eski fayl qolib ketsa, bundle'da
# imzoga kirmagan ortiqcha fayl bo'lib qoladi (kod imzosi buziladi) va diskda
# keraksiz nusxa turadi. Model endi alohida komponent.
rm -rf "/Applications/Kotib.app"
for dir in /Applications /Users/*/Applications; do
    [ -d "$dir" ] || continue
    rm -rf "$dir/Audio-Matnga.app"
    rm -rf "$dir/RubaiSTT Dictation.app"
    # ~/Applications dagi Kotib ham ketadi: paket /Applications ga o'rnatadi,
    # ikkalasi qolsa yana o'sha ikkita ikonka muammosi qaytadi.
    [ "$dir" = "/Applications" ] || rm -rf "$dir/Kotib.app"
done

# setup.sh qoldirgan LaunchAgent'ni olib tashlaymiz — ilova endi login'da ishga
# tushishni o'z bundle'idagi LaunchAgent (SMAppService) orqali boshqaradi.
# Ikkalasi qolsa login'da ikki nusxa yonadi.
for userhome in /Users/*; do
    plist="$userhome/Library/LaunchAgents/com.rubaistt.dictation.plist"
    [ -f "$plist" ] || continue
    username=$(basename "$userhome")
    uid=$(id -u "$username" 2>/dev/null)
    [ -n "$uid" ] && launchctl bootout "gui/$uid" "$plist" 2>/dev/null
    rm -f "$plist"
done
exit 0
PRE

    cat > "$dir/postinstall" <<'POST'
#!/bin/bash
# O'rnatish tugagach ilovani ishga tushiramiz — menyu satrida 🎙 ikonka darrov paydo
# bo'lsin.
# Installer root sifatida ishlaydi, shuning uchun konsoldagi foydalanuvchi nomidan ochamiz.
CONSOLE_USER=$(stat -f "%Su" /dev/console 2>/dev/null)
if [ -n "$CONSOLE_USER" ] && [ "$CONSOLE_USER" != "root" ]; then
    # Bundle konsol foydalanuvchisiniki boʻlsin — avto-yangilanish (Sparkle)
    # uni parolsiz almashtira olsin, xuddi sudrab oʻrnatilgan ilovadek
    # (spec D2). Model esa root'da qoladi (/Library — hamma uchun, faqat oʻqish).
    chown -R "$CONSOLE_USER":staff "/Applications/Kotib.app" 2>/dev/null || true
    USER_UID=$(id -u "$CONSOLE_USER" 2>/dev/null)
    if [ -n "$USER_UID" ]; then
        launchctl asuser "$USER_UID" sudo -u "$CONSOLE_USER" \
            open -a "/Applications/Kotib.app" 2>/dev/null || true
    fi
fi
exit 0
POST

    chmod +x "$dir/preinstall" "$dir/postinstall"
}

# productbuild uchun distribution XML — sehrgar sahifalari va macOS 13 talabi.
write_distribution() {
    local file="$1"
    cat > "$file" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Kotib</title>
    <organization>com.rubaistt</organization>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <volume-check>
        <allowed-os-versions><os-version min="13.0"/></allowed-os-versions>
    </volume-check>
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.txt" mime-type="text/plain"/>
    <choices-outline><line choice="default"/></choices-outline>
    <choice id="default"><pkg-ref id="$PKG_ID"/><pkg-ref id="$PKG_ID.model"/></choice>
    <pkg-ref id="$PKG_ID" version="$VERSION">component.pkg</pkg-ref>
    <pkg-ref id="$PKG_ID.model" version="$VERSION">model.pkg</pkg-ref>
</installer-gui-script>
XML
}

write_resources() {
    local dir="$1"
    mkdir -p "$dir"
    local note="<p>Til modeli oʻrnatuvchi ichida — <b>internet kerak emas</b>.</p>"
    cat > "$dir/welcome.html" <<HTML
<!DOCTYPE html><html><head><meta charset="utf-8">
<style>body{font:13px -apple-system,sans-serif;margin:16px;color:#111}
h2{font-size:15px;margin:0 0 10px}li{margin:4px 0}</style></head><body>
<h2>Kotib — o'zbekcha diktovka</h2>
<p>Istalgan ilovada <b>⌃⌥D</b> bosing → o'zbekcha gapiring → matn avtomatik yoziladi.</p>
$note
<p>O'rnatilgandan keyin ikkita ruxsat so'raladi:</p>
<ul>
  <li><b>Mikrofon</b> — ovozingizni eshitish uchun</li>
  <li><b>Accessibility</b> — matnni faol maydonga yozish uchun</li>
</ul>
<p>Ilova butunlay oflayn ishlaydi — ovozingiz qurilmangizdan chiqmaydi.
Ilovani yaxshilash uchun anonim ishlash statistikasi yuboriladi (ovoz va matn
EMAS). Keyingi sahifadagi shartlarga qarang.</p>
</body></html>
HTML
    # Litsenziya sahifasi — foydalanuvchi «Rozi boʻlaman» bosishi shart. Bu —
    # anonim statistika uchun ochiq rozilik olinadigan joy. Kod litsenziyasi
    # ham shu yerda qoladi.
    cat > "$dir/license.txt" <<TXT
KOTIB — FOYDALANISH SHARTLARI VA MAXFIYLIK SIYOSATI (XULOSA)

Kotib diktovka, audio fayldan matn va oflayn tarjima ilovasi. Ovoz tanish va
tarjima butunlay qurilmangizda bajariladi.

OVOZ VA MATN HECH QACHON YUBORILMAYDI. Nimani diktovka qilganingiz,
transkripsiya yoki tarjima natijasi faqat qurilmangizda qoladi.

Ilovani yaxshilash uchun ANONIM statistika yuboriladi: ilova va tizim
versiyasi, qurilma turi (protsessor, videokarta, xotira), taxminiy joylashuv
(mamlakat/shahar) va har transkripsiya/tarjima uchun tezlik hamda davomiylik.
Bu statistika doim yoqiq.

YIGʻILMAYDI: ovoz, matn, fayl nomlari, xom IP, seriya raqami, foydalanuvchi
nomi, oʻrnatilgan dasturlar yoki jarayonlar roʻyxati.

Davom etib, siz shu shartlar va maxfiylik siyosatiga rozilik bildirasiz.
Toʻliq matn: https://uzb.mirqobilov.com/maxfiylik

----------------------------------------------------------------------
DASTUR LITSENZIYASI

TXT
    cat "$ROOT/LICENSE" >> "$dir/license.txt"
}

# ─────────────────────────────────────────────────────────── o'rnatgichni yasash

build_pkg() {
    local variant="pkg"   # WORK ichidagi papka nomi
    local stage="$WORK/$variant/root"
    local scripts="$WORK/$variant/scripts"
    local res="$WORK/$variant/resources"
    local app="$stage/$APP_NAME"
    local out="$DIST/Kotib-$VERSION-mac.pkg"

    echo "══════════════════════════════════════ Kotib $VERSION"
    rm -rf "${WORK:?}/${variant:?}"
    mkdir -p "$stage"

    echo "[1/6] Ilova build qilinmoqda..."
    mkdir -p "$WORK/$variant"
    if ! APP_OUT="$app" bash "$ROOT/src/build.sh" > "$WORK/$variant/build.log" 2>&1; then
        echo "Build muvaffaqiyatsiz:" >&2
        cat "$WORK/$variant/build.log" >&2
        exit 1
    fi

    # Paket `hostArchitectures="arm64,x86_64"` deb eʼlon qiladi — binar ham
    # ikkalasini oʻz ichiga olishi SHART (I3). Ilgari bu tekshirilmasdi va
    # `build.sh` x86_64 kutubxonalari yoʻqligida arm64 ga tushib qolsa, Intel
    # foydalanuvchilariga ishga tushmaydigan ilova notarizatsiyadan oʻtib ketardi.
    local arxlar
    arxlar="$(lipo -archs "$app/Contents/MacOS/Kotib" 2>/dev/null || true)"
    if [[ " $arxlar " != *" arm64 "* ]] || [[ " $arxlar " != *" x86_64 "* ]]; then
        echo "Xato: ilova universal emas (lipo: '${arxlar:-?}') — ./setup.sh x86_64 kutubxonalarini quradi" >&2
        exit 1
    fi

    # Ilova ichidagi versiya paket versiyasi bilan bir xil boʻlishi SHART:
    # ikkalasi ham VERSION dan keladi, lekin build.sh ni kimdir oʻzgartirib
    # qoʻysa, notoʻgʻri versiyali paket notarizatsiyagacha yetib bormasin.
    local ichki
    ichki="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app/Contents/Info.plist")"
    if [ "$ichki" != "$VERSION" ]; then
        echo "Xato: ilova versiyasi ($ichki) VERSION ($VERSION) bilan mos emas" >&2
        exit 1
    fi

    # Model — ALOHIDA komponent (S4). Paketga faqat tarqatilayotgan aynan shu
    # fayl kiradi: sha256 tekshiriladi, aks holda chala yoki boshqa model
    # notarizatsiyagacha yetib borardi.
    echo "[2/6] Model tekshirilmoqda..."
    [ -f "$MODEL_SRC" ] || { echo "Xato: model topilmadi: $MODEL_SRC" >&2; exit 1; }
    local model_root="$WORK/$variant/model-root"
    mkdir -p "$model_root"
    cp -c "$MODEL_SRC" "$model_root/ggml-rubaistt.bin" 2>/dev/null \
        || cp "$MODEL_SRC" "$model_root/ggml-rubaistt.bin"
    chmod 644 "$model_root/ggml-rubaistt.bin"
    local hash
    hash="$(shasum -a 256 "$model_root/ggml-rubaistt.bin" | cut -d' ' -f1)"
    [ "$hash" = "$MODEL_SHA256" ] || { echo "Xato: model sha256 mos emas: $hash" >&2; exit 1; }
    [ ! -e "$app/Contents/Resources/ggml-rubaistt.bin" ] \
        || { echo "Xato: model .app ichida — build.sh eskirgan" >&2; exit 1; }

    echo "[3/6] Ilova imzolanmoqda..."
    sign_app "$app"

    echo "[4/6] .pkg yig'ilmoqda..."
    write_scripts "$scripts"
    write_resources "$res"
    write_distribution "$WORK/$variant/distribution.xml"

    # Bundle relocation'ni O'CHIRAMIZ.
    # Standart holatda macOS ayni bundle ID li ilovani tizimdan qidiradi va paketni
    # /Applications ga emas, o'sha topilgan joyga yozadi. Eski foydalanuvchilarda ilova
    # ~/Applications da turgani uchun ular "o'rnatdim, lekin /Applications da yo'q"
    # holatiga tushardi. false qilinsa — doim /Applications ga tushadi.
    pkgbuild --analyze --root "$stage" "$WORK/$variant/component.plist" > /dev/null
    # `Set` faqat mavjud kalitni oʻzgartiradi; `pkgbuild --analyze` baʼzan
    # `BundleIsRelocatable` kalitini umuman qoʻshmaydi (macOS/Xcode versiyasiga
    # qarab) va `Set` "Does Not Exist" deb yiqiladi. Shuning uchun avval Set,
    # boʻlmasa Add qilamiz.
    /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" \
        "$WORK/$variant/component.plist" > /dev/null 2>&1 || \
    /usr/libexec/PlistBuddy -c "Add :0:BundleIsRelocatable bool false" \
        "$WORK/$variant/component.plist" > /dev/null

    pkgbuild --root "$stage" \
             --install-location /Applications \
             --identifier "$PKG_ID" \
             --version "$VERSION" \
             --scripts "$scripts" \
             --component-plist "$WORK/$variant/component.plist" \
             "$WORK/$variant/component.pkg" > /dev/null

    # Model komponenti — scripts'siz: ilovani toʻxtatish/ochish app
    # komponentining preinstall/postinstall'ida.
    pkgbuild --root "$model_root" \
             --install-location "$MODEL_INSTALL_DIR" \
             --identifier "$PKG_ID.model" \
             --version "$VERSION" \
             "$WORK/$variant/model.pkg" > /dev/null

    mkdir -p "$DIST"
    rm -f "$out"
    productbuild --distribution "$WORK/$variant/distribution.xml" \
                 --package-path "$WORK/$variant" \
                 --resources "$res" \
                 --sign "$DEV_INST" --timestamp \
                 "$out" > /dev/null

    if [ "$HAVE_NOTARY" = "1" ]; then
        echo "[5/6] Notarize (Apple'ga yuborilmoqda — bir necha daqiqa)..."
        xcrun notarytool submit "$out" "${NOTARY_ARGS[@]}" --wait
        xcrun stapler staple "$out"
    else
        echo "[5/6] Notarize O'TKAZIB YUBORILDI — bu fayl tarqatishga yaramaydi!"
    fi

    # Sparkle yangilanishi uchun .zip (S6). Sparkle .pkg dan yangilanmaydi
    # (`generate_appcast` .pkg ni qoʻllamaydi) — ilovaning oʻzi kerak. U
    # notarizatsiyalangan va STAPLED boʻlishi shart: aks holda yangilangan
    # ilovani Gatekeeper birinchi ochishda tarmoqdan tekshiradi. .pkg ni
    # notarizatsiya qilish ichidagi ilovaga ham chipta beradi — odatda ilovani
    # shunchaki staple qilish yetadi; boʻlmasa .zip alohida yuboriladi.
    local zip="$DIST/Kotib-$VERSION-mac.zip"
    if [ "$HAVE_NOTARY" = "1" ]; then
        if ! xcrun stapler staple "$app" >/dev/null 2>&1; then
            echo "[5b/6] Ilova alohida notarize qilinmoqda (Sparkle .zip uchun)..."
            rm -f "$zip"
            ditto -c -k --keepParent "$app" "$zip"
            xcrun notarytool submit "$zip" "${NOTARY_ARGS[@]}" --wait
            xcrun stapler staple "$app"
        fi
    fi
    rm -f "$zip"
    ditto -c -k --keepParent "$app" "$zip"

    # Tekshiruvlar "muvaffaqiyatsiz" qaytarishi mumkin (notarize'siz holatda) —
    # bu yerda ular natijani KO'RSATISH uchun, skriptni to'xtatish uchun emas.
    echo "[6/6] Tekshiruv..."
    echo -n "    Gatekeeper: "
    { spctl -a -vv -t install "$out" 2>&1 || true; } | tail -2 | tr '\n' ' ' ; echo
    echo -n "    Staple:     "
    { xcrun stapler validate "$out" 2>&1 || true; } | tail -1

    echo -n "    Ilova:      "
    { xcrun stapler validate "$app" 2>&1 || true; } | tail -1
    echo "✅ $out  ($(du -h "$out" | cut -f1))"
    echo "✅ $zip  ($(du -h "$zip" | cut -f1)) — Sparkle: ./scripts/reliz.sh mac $VERSION"
    echo
}

build_pkg

rm -rf "$WORK"
echo "Tayyor. Fayllar: $DIST"
