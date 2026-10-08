#!/bin/bash
# Kotib — native macOS ilovasini build qiladi.
# whisper.cpp statik kutubxonalari $ROOT/whisper.cpp/build-static da bo'lishi kerak
# (setup.sh buni avtomatik tayyorlaydi).
set -euo pipefail
SRC="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SRC/.." && pwd)"
# Versiya — `VERSION` faylidan (yagona manba), Info.plist ga quyida yoziladi.
# shellcheck source=../scripts/versiya.sh
source "$ROOT/scripts/versiya.sh"
# shellcheck source=../scripts/bogliqliklar.sh
source "$ROOT/scripts/bogliqliklar.sh"
WC="$ROOT/whisper.cpp"
# Sparkle — avto-yangilanish (S5). Pinlangan reliz, sha256 bilan; yoʻq boʻlsa
# yuklab olinadi (setup.sh ni 1.2 dan oldin ishlatganlar uchun ham).
SPARKLE="$ROOT/sparkle"
sparkle_tayyorla "$SPARKLE"
# Silero VAD (S12) — bundle ichida (885 KB). Pinlangan, sha256 bilan.
VAD="$ROOT/vad/$VAD_NOM"
vad_tayyorla "$VAD"
# Tarjimon kutubxonalari har arxitektura uchun ALOHIDA quriladi: statik `.a`
# fayllar bitta arxitekturani saqlaydi, shuning uchun x86_64 linki arm64
# kutubxonalarini qabul qilmaydi ("building for macOS-x86_64 but attempting
# to link with file built for macOS-arm64").
CT2_ARM="$ROOT/ctranslate2/build-static"
CT2_X64="$ROOT/ctranslate2/build-x64"
SP_ARM="$ROOT/sentencepiece/build-static"
SP_X64="$ROOT/sentencepiece/build-x64"
LIB_ARM="$WC/build-static"   # arm64 — Metal GPU
LIB_X64="$WC/build-x64"      # x86_64 — faqat CPU (Intel)
APP="${APP_OUT:-$HOME/Applications/Kotib.app}"

if [ ! -f "$LIB_ARM/src/libwhisper.a" ]; then
    echo "Xato: whisper.cpp statik kutubxonalari topilmadi. Avval setup.sh ni ishga tushiring." >&2
    exit 1
fi
if [ ! -f "$CT2_ARM/libctranslate2.a" ] || [ ! -f "$SP_ARM/src/libsentencepiece.a" ]; then
    echo "Xato: CTranslate2/SentencePiece kutubxonalari topilmadi. Avval setup.sh ni ishga tushiring." >&2
    exit 1
fi
# Qaysi arxitekturalar build qilinadi: doim arm64; UCHALA x86_64 kutubxonasi
# (whisper, CTranslate2, SentencePiece) mavjud boʻlsa — universal.
# Bittasi yetishmasa arm64 da qolamiz (yarim universal binar link bosqichida
# emas, foydalanuvchining Intel mashinasida yiqilardi) — lekin endi JIMGINA
# EMAS (I3): ishlab chiqishda ogohlantirish, relizda `make_pkg.sh` toʻxtaydi.
ARCHS=("arm64")
X64_YOQ=()
[ -f "$LIB_X64/src/libwhisper.a" ]        || X64_YOQ+=("whisper.cpp/build-x64")
[ -f "$CT2_X64/libctranslate2.a" ]        || X64_YOQ+=("ctranslate2/build-x64")
[ -f "$SP_X64/src/libsentencepiece.a" ]   || X64_YOQ+=("sentencepiece/build-x64")
if [ ${#X64_YOQ[@]} -eq 0 ]; then
    ARCHS+=("x86_64")
else
    echo "⚠ Faqat arm64: x86_64 kutubxonalari yoʻq (${X64_YOQ[*]})." >&2
    echo "  Bu ilova Intel Mac'larda ishga tushmaydi. Universal build uchun: ./setup.sh" >&2
fi

# Barcha Swift manbalari (ilova.swift + whisper.swift + settings.swift …)
SWIFT_SRCS=("$SRC"/*.swift)

# Metal ikkala arxitekturaga: arm64 GPU backend'i uchun (pastda force_load bilan)
# va x86_64 da GPU MODELINI oʻqish uchun (yangilanish.swift → MTLCreateSystemDefaultDevice).
# Metal — barcha Mac'larda (Intel ham) bor tizim freymvorki, shuning uchun bu
# xavfsiz. arm64'da METAL bilan takrorlanadi — linker dublikatni oʻzi tashlaydi.
COMMON_FW="-framework Foundation -framework Accelerate -framework AVFoundation \
    -framework AppKit -framework QuartzCore -framework CoreGraphics \
    -framework Carbon -framework ApplicationServices -framework Metal \
    -F $SPARKLE -framework Sparkle -Xlinker -rpath -Xlinker @executable_path/../Frameworks"

echo "[1/4] Kompilyatsiya + linking (${ARCHS[*]})..."
EXES=()
for ARCH in "${ARCHS[@]}"; do
    if [ "$ARCH" = "arm64" ]; then
        LIB="$LIB_ARM"; TRIPLE="arm64-apple-macos13.0"
        CT2="$CT2_ARM"; SP="$SP_ARM"
        # arm64 — Metal backend'ni force_load + Metal freymvorklari
        METAL="-Xlinker -force_load -Xlinker $LIB/ggml/src/ggml-metal/libggml-metal.a \
            -framework Metal -framework MetalKit"
    else
        LIB="$LIB_X64"; TRIPLE="x86_64-apple-macos13.0"; METAL=""
        CT2="$CT2_X64"; SP="$SP_X64"
    fi
    # CTranslate2 Ruy bilan birga oʻnlab statik kutubxona beradi — bittasi emas.
    # zsh/bash massiv sintaksisi SHART: oddiy $(...) bitta soʻz boʻlib ketadi va
    # clang uni bitta fayl nomi deb qabul qiladi.
    CT2_LIBS=()
    while IFS= read -r f; do CT2_LIBS+=("$f"); done < <(find "$CT2/" -name '*.a')  # «/» — symlink boʻlsa ham ichiga kiradi (worktree)
    echo "  -> $ARCH"
    # -mmacosx-version-min: aks holda obyekt fayli joriy macOS versiyasi bilan
    # belgilanadi va linker "built for newer macOS" deb ogohlantiradi
    clang -c "$SRC/whisper_bridge.c" -arch "$ARCH" -O2 -mmacosx-version-min=13.0 \
        -I"$WC/include" -I"$WC/ggml/include" -o "$SRC/whisper_bridge.$ARCH.o"
    clang -c "$SRC/nutq_bolaklari.c" -arch "$ARCH" -O2 -mmacosx-version-min=13.0 \
        -o "$SRC/nutq_bolaklari.$ARCH.o"
    # Tarjima koʻprigi — clang++ bilan (CTranslate2 sarlavhalari C++17).
    # -w: sarlavhalar C++20 da eskirgan literal-operator sintaksisini ishlatadi
    # va oʻnlab ogohlantirish beradi; ular bizga tegishli emas.
    clang++ -c "$SRC/tarjima_bridge.cpp" -arch "$ARCH" -O2 -std=c++17 -w \
        -mmacosx-version-min=13.0 \
        -I"$ROOT/ctranslate2/include" \
        -I"$ROOT/sentencepiece/src" -I"$ROOT/sentencepiece/third_party" \
        -o "$SRC/tarjima_bridge.$ARCH.o"
    # shellcheck disable=SC2086  # $METAL va $COMMON_FW — bayroqlar roʻyxati, ataylab boʻlinadi
    swiftc -O -target "$TRIPLE" "${SWIFT_SRCS[@]}" \
        "$SRC/whisper_bridge.$ARCH.o" "$SRC/nutq_bolaklari.$ARCH.o" "$SRC/tarjima_bridge.$ARCH.o" \
        -import-objc-header "$SRC/Bridging.h" \
        "$LIB/src/libwhisper.a" \
        "$LIB/ggml/src/libggml.a" \
        "$LIB/ggml/src/libggml-base.a" \
        -Xlinker -force_load -Xlinker "$LIB/ggml/src/libggml-cpu.a" \
        "${CT2_LIBS[@]}" "$SP/src/libsentencepiece.a" \
        $METAL $COMMON_FW -lc++ \
        -o "$SRC/Kotib.$ARCH"
    EXES+=("$SRC/Kotib.$ARCH")
done

# Universal binarga birlashtirish (yoki bitta arxitektura)
if [ ${#EXES[@]} -gt 1 ]; then
    lipo -create "${EXES[@]}" -output "$SRC/Kotib"
    echo "  universal: $(lipo -archs "$SRC/Kotib")"
else
    cp "${EXES[0]}" "$SRC/Kotib"
fi
rm -f "$SRC"/whisper_bridge.*.o "$SRC"/nutq_bolaklari.*.o "$SRC"/tarjima_bridge.*.o "$SRC"/Kotib.arm64 "$SRC"/Kotib.x86_64

echo "[3/4] .app bundle..."
rm -rf "$APP"
# Eski nomdagi ilovalar qolib ketmasin — aks holda menyu satrida bir nechta ikonka
# chiqadi va qaysi biri ishlayotgani tushunarsiz boʻladi.
rm -rf "$HOME/Applications/RubaiSTT Dictation.app"
rm -rf "$HOME/Applications/Audio-Matnga.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"
cp "$SRC/Kotib" "$APP/Contents/MacOS/Kotib"
# Sparkle.framework. XPC servislari OLIB TASHLANADI: ular faqat sandbox'dagi
# ilovalar uchun, Kotib esa sandbox'siz (spec D1) — keraksiz kod imzolashni
# murakkablashtiradi va notarizatsiyada ortiqcha tekshiruv beradi.
ditto "$SPARKLE/Sparkle.framework" "$APP/Contents/Frameworks/Sparkle.framework"
rm -rf "$APP/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices" \
    "$APP/Contents/Frameworks/Sparkle.framework/XPCServices"
cp "$ROOT/assets/AppIcon.icns" "$APP/Contents/Resources/AppIcon.icns"
cp "$VAD" "$APP/Contents/Resources/$VAD_NOM"
# Oʻchirish skripti ilova ichida keladi — Sozlamalardagi "Ilovani oʻchirish…"
# foydalanuvchiga aynan shu faylni koʻrsatadi (u sudo talab qiladi, shuning
# uchun ilova uni oʻzi bajarmaydi).
cp "$ROOT/scripts/uninstall.sh" "$APP/Contents/Resources/uninstall.sh"
# Uchinchi tomon litsenziyalari (Sozlamalar → «Litsenziyalar…»).
"$ROOT/scripts/litsenziyalar.sh" "$APP/Contents/Resources/Litsenziyalar.txt"
chmod +x "$APP/Contents/Resources/uninstall.sh"

# Til modeli .app ichiga KIRMAYDI (1.2, S4). Ilova uni bundle'dan tashqarida
# qidiradi (`model_tanlov.swift`): ~/Library/Application Support/Kotib/models
# (ilova ichidagi yuklovchi), /Library/Application Support/Kotib/models (.pkg),
# eski yoʻllar. Nega: avto-yangilanish bundle'ni almashtiradi — model ichida
# boʻlsa har yangilanish 800 MB boʻlardi. `SKIP_MODEL` endi kerak emas.
if [ -n "${SKIP_MODEL:-}" ]; then
    echo "  (SKIP_MODEL endi kerak emas — model hech qachon .app ga solinmaydi)"
fi

# Yangilanish imzosining ochiq kaliti — repoda (scripts/yangilanish-kaliti.pub).
YANGILANISH_KALITI="$(tr -d '[:space:]' < "$ROOT/scripts/yangilanish-kaliti.pub")"
[[ "$YANGILANISH_KALITI" =~ ^[A-Za-z0-9+/]{43}=$ ]] \
    || { echo "Xato: scripts/yangilanish-kaliti.pub notoʻgʻri" >&2; exit 1; }

# Heredoc ataylab tirnoqsiz — faqat versiya va kalit oʻrniga qoʻyiladi.
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>Kotib</string>
    <key>CFBundleDisplayName</key><string>Kotib</string>
    <!-- Bundle ID ATAYLAB o'zgarmaydi: macOS'da Accessibility va Mikrofon
         ruxsatlari shu ID ga bog'langan. O'zgartirilsa mavjud foydalanuvchilarda
         ruxsatlar qaytadan so'raladi va ro'yxatda ikkita yozuv qoladi. -->
    <key>CFBundleIdentifier</key><string>com.rubaistt.dictation</string>
    <key>CFBundleVersion</key><string>${KOTIB_VERSIYA}</string>
    <key>CFBundleShortVersionString</key><string>${KOTIB_VERSIYA}</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleExecutable</key><string>Kotib</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>LSUIElement</key><true/>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSPrincipalClass</key><string>NSApplication</string>
    <key>NSMicrophoneUsageDescription</key><string>Ovozingizni matnga o'girish uchun mikrofon kerak.</string>
    <!-- Avto-yangilanish (Sparkle, S5; spec D1): ruxsat soʻralmaydi, fonda
         yuklanadi va boʻsh paytda oʻrnatiladi (yangilovchi.swift). Feed va
         yangilanishlar Ed25519 bilan imzolangan boʻlishi SHART. -->
    <key>SUFeedURL</key><string>https://stat.mirqobilov.com/v1/appcast/mac.xml</string>
    <key>SUPublicEDKey</key><string>${YANGILANISH_KALITI}</string>
    <key>SUEnableAutomaticChecks</key><true/>
    <key>SUAutomaticallyUpdate</key><true/>
    <key>SUAllowsAutomaticUpdates</key><true/>
    <key>SUScheduledCheckInterval</key><integer>86400</integer>
    <key>SUVerifyUpdateBeforeExtraction</key><true/>
    <key>SURequireSignedFeed</key><true/>
    <key>SUEnableSystemProfiling</key><false/>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeName</key><string>Audio va video fayllar</string>
            <key>LSItemContentTypes</key>
            <array>
                <string>public.audio</string>
                <string>public.movie</string>
            </array>
            <key>LSHandlerRank</key><string>Alternate</string>
        </dict>
    </array>
</dict>
</plist>
PLIST

# Login'da ishga tushirish uchun LaunchAgent — .app ichida keladi va u ilovani
# `--autostart` argumenti bilan chaqiradi. Ilova shu argumentga qarab oynani
# ochish/ochmaslikni hal qiladi (avtostart.swift'dagi LoginItem izohiga qarang).
# `SMAppService.agent(plistName:)` plistni aynan shu papkadan qidiradi va Label
# fayl nomi bilan bir xil boʻlishi SHART.
mkdir -p "$APP/Contents/Library/LaunchAgents"
cat > "$APP/Contents/Library/LaunchAgents/com.rubaistt.dictation.autostart.plist" <<'AGENT'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.rubaistt.dictation.autostart</string>
  <!-- BundleProgram — .app ichidagi nisbiy yoʻl. Absolyut yoʻl yozib boʻlmaydi:
       ilova /Applications da ham, ~/Applications da ham turishi mumkin. -->
  <key>BundleProgram</key><string>Contents/MacOS/Kotib</string>
  <key>ProgramArguments</key>
  <array>
    <string>Contents/MacOS/Kotib</string>
    <string>--autostart</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><false/>
  <key>ProcessType</key><string>Interactive</string>
  <!-- Sozlamalar > Login Items da "Kotib" nomi bilan koʻrinishi uchun -->
  <key>AssociatedBundleIdentifiers</key><array><string>com.rubaistt.dictation</string></array>
</dict></plist>
AGENT

# TCC (Accessibility/Mikrofon) ruxsati imzo KIMLIGIGA bogʻlanadi. Ad-hoc imzoda
# Team ID ham, barqaror designated requirement ham yoʻq — shuning uchun tccd grantni
# faylning cdhash'iga bogʻlaydi. Har qayta build'da cdhash oʻzgaradi va eski grant
# mos kelmay qoladi: Sozlamalarda tugma YOQILGAN turadi, lekin AXIsProcessTrusted()
# false qaytaradi va ilova ruxsatni qaytadan soʻraydi (roʻyxatda esa oʻlik yozuvlar
# yigʻiladi). Developer ID bilan imzolansa requirement bundle ID + Team ID ga
# bogʻlanadi va ruxsat barcha keyingi build'larda saqlanib qoladi.
IDENT="${CODESIGN_IDENTITY:-}"
if [ -z "$IDENT" ]; then
    # `|| true`: pipefail ostida sertifikat yoʻqligi skriptni toʻxtatmasin —
    # pastda ad-hoc imzoga tushamiz.
    IDENT=$(security find-identity -v -p codesigning 2>/dev/null \
        | sed -n 's/.*"\(Developer ID Application: .*\)"/\1/p' | head -1) || true
fi
if [ -n "$IDENT" ]; then
    echo "[4/4] Imzolash: $IDENT"
    # --timestamp=none — dev build'da Apple TSA'ga tarmoq soʻrovi kerak emas
    # (notarizatsiya faqat scripts/make_pkg.sh da, u oʻzi qayta imzolaydi).
    SIGN_ARGS=(--force --timestamp=none --sign "$IDENT")
else
    echo "[4/4] Ad-hoc imzolash (Developer ID sertifikati topilmadi)"
    echo "  DIQQAT: ad-hoc build'da Accessibility ruxsati har build'dan keyin tushadi." >&2
    SIGN_ARGS=(--force --sign -)
fi
xattr -cr "$APP" 2>/dev/null || true
# Ichdan tashqariga: avval Sparkle yordamchilari va framework (bizning
# entitlement'larimizsiz — mikrofon/JIT ularga kerak emas), keyin ilova.
# `--deep` ATAYLAB ishlatilmaydi: u entitlement'larni ichki kodga ham qoʻyardi.
FW="$APP/Contents/Frameworks/Sparkle.framework"
codesign "${SIGN_ARGS[@]}" "$FW/Versions/B/Autoupdate"
codesign "${SIGN_ARGS[@]}" "$FW/Versions/B/Updater.app"
codesign "${SIGN_ARGS[@]}" "$FW"
codesign "${SIGN_ARGS[@]}" --entitlements "$SRC/entitlements.plist" "$APP" 2>/dev/null \
    || codesign "${SIGN_ARGS[@]}" "$APP"

echo "Tayyor: $APP  (versiya $KOTIB_VERSIYA)"
