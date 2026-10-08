#!/usr/bin/env bash
#
# Versiya raqami hamma joyda mosmi — tekshiradi.
#
#   ./scripts/versiya-tekshir.sh           ishlab chiqish rejimi (test.sh va hammasi.sh chaqiradi)
#   ./scripts/versiya-tekshir.sh --reliz   reliz rejimi: eʼlon qilingan versiya == VERSION
#
# Yagona manba — repo ildizidagi `VERSION`. Yigʻiladigan hamma narsa undan
# oladi va raqamni oʻzida saqlamaydi:
#   macOS:   src/build.sh (Info.plist), scripts/make_pkg.sh (.pkg nomi va versiyasi)
#   Windows: win/CMakeLists.txt → kotib_versiya.h → win/res/app.rc,
#            win/tools/ornatuvchi-yasa.sh va win/build.ps1 → rubai.iss (/DAppVersion),
#            win/tools/zip-yasa.sh
#
# Eʼlon qilingan versiya esa VERSION dan yasab boʻlmaydi — u CDN'dagi faylga
# ishora qiladi va kalit prefiksi ixtiyoriy (`dl/v1.1b/…`, immutable kesh
# sababli). Shuning uchun bu fayllar qoʻlda yangilanadi va shu yerda
# solishtiriladi:
#   statistika/versiya.json (KV `joriy`), web/index.html (softwareVersion va
#   yuklash havolalari), README.md va win/README.md (yuklash havolalari).
#
# Ikki xil nomuvofiqlik ushlanadi:
#   1. Raqam yana qoʻlda yozilib qoʻyilgan (regressiya) — har ikki rejimda xato.
#   2. Eʼlon qilingan versiya VERSION dan YANGI — har ikki rejimda xato: ilova
#      oʻziga oʻzini «yangi versiya» deb taklif qiladi. ESKI boʻlishi esa
#      ishlab chiqish paytida normal (VERSION koʻtarilgan, reliz hali chiqmagan),
#      shuning uchun faqat `--reliz` da xato.
#
# Reliz tartibi: VERSION, versiya.json, sayt va README havolalari BITTA
# commit'da yangilanadi; KV yozish va sayt deploy'idan oldin `--reliz`.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

RELIZ=0
case "${1:-}" in
    "") ;;
    --reliz) RELIZ=1 ;;
    *) echo "Nomaʼlum argument: $1" >&2; exit 2 ;;
esac

# shellcheck source=versiya.sh
source "$ROOT/scripts/versiya.sh"

xatolar=0
xato() { echo "  ✗ $*" >&2; xatolar=$((xatolar + 1)); }

# versiya_taqqosla A B → -1 | 0 | 1. Raqamli qism son sifatida solishtiriladi
# (1.10.0 > 1.9.0); teng boʻlsa qoʻshimchali versiya qoʻshimchasizidan eski
# (1.2.0-sinov1 < 1.2.0), ikkalasida boʻlsa — matn sifatida.
versiya_taqqosla() {
    local a="$1" b="$2"
    local ar="${a%%-*}" br="${b%%-*}"
    local a1 a2 a3 b1 b2 b3
    IFS=. read -r a1 a2 a3 <<<"$ar"
    IFS=. read -r b1 b2 b3 <<<"$br"
    for juft in "$a1 $b1" "$a2 $b2" "$a3 $b3"; do
        # shellcheck disable=SC2086  # "a b" juftligi ataylab ikkiga boʻlinadi
        set -- $juft
        if [ "$1" -gt "$2" ]; then echo 1; return; fi
        if [ "$1" -lt "$2" ]; then echo -1; return; fi
    done
    local aq="" bq=""
    [ "$a" != "$ar" ] && aq="${a#*-}"
    [ "$b" != "$br" ] && bq="${b#*-}"
    if [ "$aq" = "$bq" ]; then echo 0
    elif [ -z "$aq" ]; then echo 1
    elif [ -z "$bq" ]; then echo -1
    elif [[ "$aq" > "$bq" ]]; then echo 1
    else echo -1
    fi
}

# --- 1. Yigʻiladigan fayllarda qoʻlda yozilgan raqam yoʻq
#
# Har qator: fayl | taqiqlangan andoza (grep -E) | izoh
qatiy_raqam_yoq() {
    local fayl="$1" andoza="$2" izoh="$3"
    if grep -nE "$andoza" "$fayl" >/dev/null; then
        xato "$fayl: versiya qoʻlda yozilgan ($izoh) — VERSION dan olinishi kerak:"
        grep -nE "$andoza" "$fayl" | sed 's/^/        /' >&2
    fi
}
qatiy_raqam_yoq src/build.sh 'CFBundle(Short)?Version(String)?</key><string>[0-9]' "Info.plist"
qatiy_raqam_yoq scripts/make_pkg.sh '^[[:space:]]*VERSION=["'"'"']?[0-9]' "VERSION="
qatiy_raqam_yoq win/res/app.rc '^[[:space:]]*(FILE|PRODUCT)VERSION[[:space:]]+[0-9]' "FILEVERSION"
qatiy_raqam_yoq win/res/app.rc '"(File|Product)Version",[[:space:]]*"[0-9]' "VALUE"
qatiy_raqam_yoq win/installer/rubai.iss '^[[:space:]]*#define[[:space:]]+AppVersion[[:space:]]' "#define AppVersion"
qatiy_raqam_yoq win/tools/zip-yasa.sh 'Kotib-[0-9]+\.[0-9]+' "fayl nomi"

# --- 2. Eʼlon qilingan versiyalar
#
# «fayl: qiymat» juftlarini yigʻamiz, keyin har birini VERSION bilan solishtiramiz.
elonlar=()

# versiya.json: {"mac": {"versiya": …}, "win": {"versiya": …}} — qaysi
# platformaniki ekanini xato matnida koʻrsatish uchun oxirgi kalitni eslab qolamiz.
if [ -f statistika/versiya.json ]; then
    while read -r platforma v; do
        elonlar+=("statistika/versiya.json ($platforma):$v")
    done < <(awk '
        match($0, /"(mac|win)"[[:space:]]*:/) { p = substr($0, RSTART + 1, 3) }
        match($0, /"versiya"[[:space:]]*:[[:space:]]*"[^"]*"/) {
            v = substr($0, RSTART, RLENGTH)
            sub(/^"versiya"[[:space:]]*:[[:space:]]*"/, "", v); sub(/"$/, "", v)
            print p, v
        }' statistika/versiya.json)
fi

while IFS= read -r v; do
    elonlar+=("web/index.html (softwareVersion):$v")
done < <(grep -oE '"softwareVersion"[[:space:]]*:[[:space:]]*"[^"]*"' web/index.html \
         | sed -E 's/.*"([^"]*)"$/\1/')

# Yuklash havolalaridagi fayl nomlari: Kotib-<versiya>-mac.pkg, Kotib-<versiya>-win-setup.exe.
# `Kotib-<versiya>-…` kabi shablon yozuvlar raqam bilan boshlanmagani uchun tushmaydi.
for fayl in web/index.html README.md win/README.md; do
    [ -f "$fayl" ] || continue
    while IFS= read -r v; do
        elonlar+=("$fayl (havola):$v")
    done < <(grep -oE 'Kotib-[0-9][0-9A-Za-z.-]*-(mac\.pkg|win-setup\.exe)' "$fayl" \
             | sed -E 's/^Kotib-(.*)-(mac\.pkg|win-setup\.exe)$/\1/' | sort -u)
done

# macOS'dagi bash 3.2 da `set -u` ostida boʻsh massivni yoyish xato beradi —
# shuning uchun boʻsh holat alohida.
if [ ${#elonlar[@]} -eq 0 ]; then
    xato "eʼlon qilingan versiya umuman topilmadi (andozalar eskirganmi?)"
    elonlar=("")
fi

for juft in "${elonlar[@]}"; do
    [ -n "$juft" ] || continue
    joy="${juft%:*}"
    v="${juft##*:}"
    if ! printf '%s' "$v" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$'; then
        xato "$joy: «$v» versiyaga oʻxshamaydi"
        continue
    fi
    natija="$(versiya_taqqosla "$v" "$KOTIB_VERSIYA")"
    if [ "$natija" = 1 ]; then
        xato "$joy: $v eʼlon qilingan, kod esa $KOTIB_VERSIYA — ilova oʻzini oʻziga yangilanish deb taklif qiladi"
    elif [ "$natija" = -1 ] && [ "$RELIZ" = 1 ]; then
        xato "$joy: hali $v (reliz $KOTIB_VERSIYA)"
    fi
done

if [ "$xatolar" -gt 0 ]; then
    echo "✗ versiya: $xatolar ta nomuvofiqlik (VERSION = $KOTIB_VERSIYA)" >&2
    exit 1
fi
rejim="ishlab chiqish"; [ "$RELIZ" = 1 ] && rejim="reliz"
echo "✓ versiya $KOTIB_VERSIYA — ${#elonlar[@]} ta eʼlon mos ($rejim rejimi)"
