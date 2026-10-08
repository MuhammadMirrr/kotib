#!/usr/bin/env bash
#
# Avto-yangilanish relizi: siyosat manifestini imzolash, tekshirish, KV'ga
# yozish va qaytarib olish (spec: avto-yangilanish — «Arxitektura»,
# «Bosqichli tarqatish»).
#
#   ./scripts/reliz.sh imzola  <manifest.json> [-o <javob.json>]
#   ./scripts/reliz.sh tekshir <javob.json> <mac|win>
#   ./scripts/reliz.sh kv-yoz  <manifest.json> [--foiz N]
#   ./scripts/reliz.sh qaytar  <mac|win> <versiya>
#   ./scripts/reliz.sh mac     <versiya> [--bosqich-soat N] [--min V] [--muhlat N]
#   ./scripts/reliz.sh win     <versiya> [--foiz N] [--min V] [--muhlat N]
#
# `mac` (S6): `make_pkg.sh` yasagan `dist/Kotib-<v>-mac.zip` ni Sparkle arxiv
# papkasiga (`dist/sparkle-arxiv/`, oldingi versiyalar delta uchun saqlanadi)
# qoʻshadi, `generate_appcast` bilan imzolangan feed va delta'lar yasaydi
# (bosqichli tarqatish: `--bosqich-soat`, standart 4 → 7 guruh ~24 soatda),
# yangi fayllarni CDN'ga (`dl/mac/`) yuklaydi va feed'ni KV'ga yozadi:
# `appcast-mac` va `appcast-mac@<v>` (`qaytar` uchun).
#
# Nega bitta `dl/mac/` prefiksi (versiyaga alohida emas): `generate_appcast`
# feed'dagi ESKI elementlarning URL'ini ham joriy prefiksga koʻchirib yozadi —
# versiyali prefiksda eski versiyalar havolasi buzilardi (sinab koʻrilgan).
# Immutable qoidasi fayl nomi orqali saqlanadi: nomda versiya bor va mavjud
# kalit hech qachon qayta yozilmaydi (yuklashdan oldin tekshiriladi).
# Versiya FAQAT raqamli (`1.2.0`): Sparkle'ning `SUStandardVersionComparator`
# qoʻshimchani hisobga olmaydi — `1.2.0-sinov2` = `1.2.0-sinov1` = `1.2.0`
# (2026-10-08 da ilovadagi Sparkle 2.10.0 bilan tekshirilgan). Qoʻshimchali
# element hech kimni yangilamaydi, oʻrnatilgan `1.2.0-sinovN` esa haqiqiy
# 1.2.0 ni ham «yangi» deb koʻrmaydi. Sinov kanali uchun relizdan KICHIK
# raqamli versiyalar: `1.1.90` → `1.1.91` (delta ham shunda sinaladi).
# `mac` siyosat manifestini (`siyosat-mac`) ham yozadi — har relizda, feed bilan
# birga, shunda eski `min_versiya` qolib ketmaydi. `--min V` (S9) — majburiy:
# manifestga `min_versiya`, feed'dagi element esa «kritik»
# (`--critical-update-version V`): Sparkle kritik elementni bosqichli
# tarqatishdan chetlab oʻtadi, aks holda foydalanuvchi navbati kelmasdan
# muhlat tugab, bloklanib qolishi mumkin edi.
# 300 MiB dan katta fayl (.pkg) wrangler bilan yuklanmaydi — yoʻriqnoma chiqadi.
#
# `win` (S8): `dist/Kotib-<v>-win-yangilash.exe` (modelsiz, S7) ni imzolaydi
# (Ed25519, faylning butun baytlari — ilova `faylniTekshir` bilan tekshiradi),
# CDN'ga `dl/win/` ga yuklaydi va siyosat manifestini yasab `kv-yoz` yoʻlidan
# oʻtkazadi. Oʻrnatuvchi universal (x64 + ARM64 ichida), shuning uchun ikkala
# arxitektura bitta faylga ishora qiladi. Izoh — CHANGELOG.md boʻlimi.
#
# Bayroqlar (hamma buyruqda):
#   --quruq          hech narsa YOZILMAYDI — faqat nima qilinishi koʻrsatiladi
#                    (imzolash va tekshiruv baribir bajariladi)
#   --mahalliy       KV — `wrangler dev` ning mahalliy nusxasi
#                    (statistika/.wrangler/), CDN tekshiruvi oʻtkaziladi
#   --kalit-fayli F  imzo uchun maxfiy kalit fayli (sinov kaliti);
#                    standart — Keychain (`scripts/imzo-kaliti.sh`, hisob kotib)
#   --ochiq B64      tekshiruv uchun ochiq kalit; standart —
#                    scripts/yangilanish-kaliti.pub
#   --kanal sinov    SINOV KANALI: KV kalitlari `…-sinov` (Worker
#                    /v1/yangilanish/<p>-sinov.json, /v1/appcast/mac-sinov.xml
#                    beradi). Oʻsha kalit bilan imzolanadi, foydalanuvchilarga
#                    taʼsir qilmaydi — haqiqiy CDN/Worker orqali uchdan-uchga sinov.
#
# KV kalitlari (VERSIYALAR):
#   siyosat-<p>             ilovalar oʻqiydigan joriy siyosat (Worker beradi)
#   siyosat-<p>@<versiya>   har chiqarilgan siyosatning nusxasi — `qaytar` uchun
#   (`--kanal sinov` da: siyosat-<p>-sinov, siyosat-<p>-sinov@<versiya>,
#   appcast-mac-sinov)
#
# Tartib: har yozuvdan OLDIN javob ilova bilan bir xil kod orqali tekshiriladi
# (`scripts/manifest-tekshir`, u `src/yangilanish_siyosat.swift` ni ishlatadi).
# Tekshiruvdan oʻtmagan narsa — buzilgan imzo, yaroqsiz maydon, CDN'da
# boʻlmagan yoki hajmi mos kelmaydigan fayl — KV'ga yozilmaydi.
#
# `kv-yoz --foiz 10` → 24 soat kuzatuv (statistika paneli) → `kv-yoz --foiz 100`.
# Foiz manifest ICHIDA, shuning uchun har oʻzgarish qayta imzolanadi.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=bogliqliklar.sh
source "$ROOT/scripts/bogliqliklar.sh"

QURUQ=0
JOY="--remote"
KALIT_FAYLI=""
OCHIQ=""
FOIZ=""
MIN=""
MUHLAT=""
KANAL=""
BOSQICH_SOAT=4
CHIQISH=""
POZ=()

[ $# -gt 0 ] || { sed -n '3,25p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
BUYRUQ="$1"; shift
while [ $# -gt 0 ]; do
    case "$1" in
        --quruq) QURUQ=1 ;;
        --mahalliy) JOY="--local" ;;
        --kalit-fayli) KALIT_FAYLI="$2"; shift ;;
        --ochiq) OCHIQ="$2"; shift ;;
        --foiz) FOIZ="$2"; shift ;;
        --min) MIN="$2"; shift ;;
        --muhlat) MUHLAT="$2"; shift ;;
        --kanal)
            [ "${2:-}" = sinov ] || { echo "Xato: --kanal faqat «sinov»" >&2; exit 2; }
            KANAL="-sinov"; shift ;;
        --bosqich-soat) BOSQICH_SOAT="$2"; shift ;;
        -o) CHIQISH="$2"; shift ;;
        -*) echo "Nomaʼlum bayroq: $1" >&2; exit 2 ;;
        *) POZ+=("$1") ;;
    esac
    shift
done
[ -n "$OCHIQ" ] || OCHIQ="$(tr -d '[:space:]' < "$ROOT/scripts/yangilanish-kaliti.pub")"

ISH="$(mktemp -d)"
chmod 700 "$ISH"
trap 'rm -rf "$ISH"' EXIT

qadam() { printf '==> %s\n' "$*"; }

# ---- vositalar

# Ilova bilan bir xil tekshiruvchi. Manbalar oʻzgarganda qayta yigʻiladi.
TEKSHIRUVCHI="$ROOT/tests/.build/manifest-tekshir"
tekshiruvchi_tayyorla() {
    local manbalar=("$ROOT/scripts/manifest-tekshir/main.swift"
                    "$ROOT/src/yangilanish_siyosat.swift" "$ROOT/src/imzo.swift")
    local kerak=0 f
    [ -x "$TEKSHIRUVCHI" ] || kerak=1
    for f in "${manbalar[@]}"; do [ "$f" -nt "$TEKSHIRUVCHI" ] && kerak=1; done
    if [ "$kerak" = 1 ]; then
        mkdir -p "$(dirname "$TEKSHIRUVCHI")"
        swiftc -O "${manbalar[@]}" -o "$TEKSHIRUVCHI"
    fi
}

# imzola_fayl <fayl> → stdout: base64 Ed25519 imzo (Sparkle sign_update).
#
# Keychain'dagi kalitni sign_update TOʻGʻRIDAN-TOʻGʻRI oʻqisa macOS GUI ruxsat
# oynasini chiqaradi va skript osilib qoladi (yozuv ACL'i faqat
# generate_keys'ga ishonadi). Shuning uchun kalit vaqtinchalik faylga
# (0700 papka) eksport qilinadi va trap bilan darhol oʻchiriladi.
imzola_fayl() {
    sparkle_tayyorla "$ROOT/sparkle"
    local kalit="$KALIT_FAYLI"
    if [ -z "$kalit" ]; then
        kalit="$ISH/kalit"
        [ -f "$kalit" ] || "$ROOT/sparkle/bin/generate_keys" --account kotib -x "$kalit" >/dev/null
    fi
    "$ROOT/sparkle/bin/sign_update" --ed-key-file "$kalit" -p "$1" | tr -d '[:space:]'
}

# javob_yasa <manifest baytlari fayli> <chiqish> — {"m":…, "s":…}
javob_yasa() {
    local imzo
    imzo="$(imzola_fayl "$1")"
    printf '%s' "$imzo" | grep -Eq '^[A-Za-z0-9+/]{86}==$' || { echo "Xato: imzo olinmadi" >&2; exit 1; }
    printf '{"m":"%s","s":"%s"}' "$(base64 < "$1" | tr -d '\n')" "$imzo" > "$2"
}

# tekshir_javob <javob> <platforma> → manifest-tekshir chiqishi (stdout)
tekshir_javob() {
    tekshiruvchi_tayyorla
    "$TEKSHIRUVCHI" "$1" "$2" "$OCHIQ"
}

cdn_hajm() {  # cdn_hajm <url> → Content-Length yoki boʻsh (yoʻq)
    # `|| true` SHART: 404 da curl 22 qaytaradi va `pipefail` + `set -e`
    # butun skriptni jimgina toʻxtatardi — «yoʻq» esa bu yerda oddiy javob.
    { curl -sfIL "$1" 2>/dev/null || true; } | tr -d '\r' \
        | awk -F': ' 'tolower($1)=="content-length"{n=$2} END{print n}'
}

# cdn_tekshir <manifest-tekshir chiqishi> — har fayl CDN'da bormi va hajmi mosmi.
# (sha256 ni toʻliq yuklab tekshirish — S6/S8 reliz quvurida; bu yerda
# HEAD bilan hajm: yanglish yuklangan yoki umuman yoʻq fayl ushlanadi.)
cdn_tekshir() {
    local tur arx url hajm bor
    while IFS=$'\t' read -r tur arx url hajm _; do
        [ "$tur" = fayl ] || continue
        bor="$(cdn_hajm "$url")"
        if [ "$bor" != "$hajm" ]; then
            echo "Xato: $arx — $url CDN'da ${bor:-yoʻq} bayt, manifestda $hajm" >&2
            return 1
        fi
        echo "  CDN ✓ $arx $hajm bayt"
    done <<< "$1"
}

# CDN (R2 bucket, cdn.mirqobilov.com). Kalitlar immutable — hech qachon
# ustidan yozilmaydi (AGENTS.md → «never overwrite this key»).
CDN_BUCKET="audio-matnga-cdn"
CDN_URL="https://cdn.mirqobilov.com"


r2_yukla() {  # r2_yukla <fayl> <kalit>
    ( cd "$ROOT/statistika" && npx --no-install wrangler r2 object put "$CDN_BUCKET/$2" \
        --file "$1" --remote --cache-control "public, max-age=31536000, immutable" )
}

kv() {  # kv <put|get> … — statistika papkasidan, VERSIYALAR bogʻlanishi orqali
    ( cd "$ROOT/statistika" && npx --no-install wrangler kv key "$@" --binding VERSIYALAR "$JOY" )
}

maydon() {  # maydon <manifest-tekshir chiqishi> <nom>
    printf '%s\n' "$1" | awk -F'\t' -v n="$2" '$1==n{print $2; exit}'
}

# siyosat_tayyorla <manifest.json> <cdn_tekshir: 1|0> — manifestni ixcham va
# barqaror shaklga keltiradi (--foiz berilsa almashtiriladi), imzolaydi va ilova
# bilan bir xil kod orqali tekshiradi. Natija: $ISH/javob.json, SIYOSAT_NATIJA,
# SIYOSAT_PLATFORMA. Hech narsa YOZMAYDI — tekshiruvdan oʻtmasa skript toʻxtaydi.
siyosat_tayyorla() {
    python3 - "$1" "$ISH/m.json" "$FOIZ" <<'PY'
import json, sys
m = json.load(open(sys.argv[1], encoding="utf-8"))
if sys.argv[3]:
    f = int(sys.argv[3])
    if not 0 <= f <= 100:
        sys.exit("Xato: --foiz 0–100 oraligʻida boʻlishi kerak")
    m["tarqatish_foiz"] = f
open(sys.argv[2], "w", encoding="utf-8").write(json.dumps(m, ensure_ascii=False, separators=(",", ":")))
PY
    SIYOSAT_PLATFORMA="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["platforma"])' "$ISH/m.json")"
    qadam "siyosat: imzolash"
    javob_yasa "$ISH/m.json" "$ISH/javob.json"
    qadam "siyosat: tekshirish (ilova bilan bir xil kod)"
    SIYOSAT_NATIJA="$(tekshir_javob "$ISH/javob.json" "$SIYOSAT_PLATFORMA")" || {
        echo "Xato: imzolangan javob tekshiruvdan oʻtmadi — KV'ga YOZILMADI" >&2; exit 1; }
    printf '%s\n' "$SIYOSAT_NATIJA" | sed 's/^/  /'
    if [ "$JOY" != "--local" ] && [ "$2" = 1 ]; then cdn_tekshir "$SIYOSAT_NATIJA"; fi
}

# siyosat_kvga — `siyosat_tayyorla` tekshirgan javobni KV'ga yozadi:
# siyosat-<p><kanal> va siyosat-<p><kanal>@<v>.
siyosat_kvga() {
    local versiya kalit
    versiya="$(maydon "$SIYOSAT_NATIJA" versiya)"
    kalit="siyosat-$SIYOSAT_PLATFORMA$KANAL"
    if [ "$QURUQ" = 1 ]; then
        qadam "QURUQ: yozilardi → $kalit@$versiya va $kalit (${JOY#--})"
    else
        qadam "KV ga yozish (${JOY#--})"
        kv put "$kalit@$versiya" --path "$ISH/javob.json" >/dev/null
        kv put "$kalit" --path "$ISH/javob.json" >/dev/null
        echo "✓ $kalit = $versiya (foiz $(maydon "$SIYOSAT_NATIJA" foiz))"
    fi
}

siyosat_yoz() {  # siyosat_yoz <manifest.json> <cdn_tekshir: 1|0>
    siyosat_tayyorla "$1" "$2"
    siyosat_kvga
}

# cdn_joyla <fayl> <kalit> — immutable: mavjud kalit hech qachon qayta
# yozilmaydi; bor boʻlsa hajmi mos kelishi shart.
cdn_joyla() {
    local hajm bor
    hajm="$(stat -f %z "$1")"
    [ "$hajm" -lt $((300 * 1024 * 1024)) ] || {
        echo "Xato: $(basename "$1") 300 MiB dan katta — wrangler yuklamaydi (AGENTS.md → R2)" >&2; exit 1; }
    bor="$(cdn_hajm "$CDN_URL/$2")"
    if [ -n "$bor" ]; then
        [ "$bor" = "$hajm" ] || {
            echo "Xato: $2 CDN'da allaqachon bor ($bor bayt, yangisi $hajm) — ustidan yozilmaydi" >&2; exit 1; }
        qadam "CDN'da bor: $2 ($hajm bayt) — oʻtkazib yuborildi"
    elif [ "$QURUQ" = 1 ]; then
        qadam "QURUQ: R2 ← $2 ($hajm bayt)"
    else
        qadam "R2 ← $2"
        r2_yukla "$1" "$2" >/dev/null
        [ "$(cdn_hajm "$CDN_URL/$2")" = "$hajm" ] || { echo "Xato: CDN'da $2 hajmi mos emas" >&2; exit 1; }
    fi
}

# ---- buyruqlar

case "$BUYRUQ" in
    imzola)
        [ ${#POZ[@]} -eq 1 ] || { echo "ishlatish: $0 imzola <manifest.json> [-o <javob.json>]" >&2; exit 2; }
        javob_yasa "${POZ[0]}" "$ISH/javob.json"
        if [ -n "$CHIQISH" ]; then cp "$ISH/javob.json" "$CHIQISH"; else cat "$ISH/javob.json"; echo; fi
        ;;

    tekshir)
        [ ${#POZ[@]} -eq 2 ] || { echo "ishlatish: $0 tekshir <javob.json> <mac|win>" >&2; exit 2; }
        natija="$(tekshir_javob "${POZ[0]}" "${POZ[1]}")"
        printf '%s\n' "$natija"
        [ "$JOY" = "--local" ] || cdn_tekshir "$natija"
        echo "✓ ilova bu javobni qabul qiladi"
        ;;

    kv-yoz)
        [ ${#POZ[@]} -eq 1 ] || { echo "ishlatish: $0 kv-yoz <manifest.json> [--foiz N]" >&2; exit 2; }
        siyosat_yoz "${POZ[0]}" 1
        ;;

    qaytar)
        [ ${#POZ[@]} -eq 2 ] || { echo "ishlatish: $0 qaytar <mac|win> <versiya>" >&2; exit 2; }
        platforma="${POZ[0]}"; versiya="${POZ[1]}"
        qadam "siyosat-$platforma$KANAL@$versiya oʻqilmoqda (${JOY#--})"
        kv get "siyosat-$platforma$KANAL@$versiya" --text > "$ISH/javob.json" 2>/dev/null || true
        if ! { [ -s "$ISH/javob.json" ] && grep -q '"m"' "$ISH/javob.json"; }; then
            echo "Xato: siyosat-$platforma$KANAL@$versiya KV'da yoʻq" >&2; exit 1
        fi
        qadam "tekshirish"
        natija="$(tekshir_javob "$ISH/javob.json" "$platforma")" || {
            echo "Xato: saqlangan javob tekshiruvdan oʻtmadi — qaytarilMADI" >&2; exit 1; }
        [ "$(maydon "$natija" versiya)" = "$versiya" ] || {
            echo "Xato: saqlangan javob boshqa versiyaniki" >&2; exit 1; }
        if [ "$QURUQ" = 1 ]; then
            qadam "QURUQ: siyosat-$platforma$KANAL ← $versiya qilinardi"
        else
            kv put "siyosat-$platforma$KANAL" --path "$ISH/javob.json" >/dev/null
            echo "✓ siyosat-$platforma$KANAL qaytarildi → $versiya"
        fi
        # macOS: Sparkle feed'i ham shu versiyaga (yangisi undan tushib qoladi).
        if [ "$platforma" = mac ]; then
            kv get "appcast-mac$KANAL@$versiya" --text > "$ISH/appcast.xml" 2>/dev/null || true
            if ! grep -q "sparkle-signatures" "$ISH/appcast.xml" 2>/dev/null; then
                echo "Ogohlantirish: appcast-mac$KANAL@$versiya KV'da yoʻq yoki imzosiz — feed qaytarilmadi" >&2
            elif [ "$QURUQ" = 1 ]; then
                qadam "QURUQ: appcast-mac$KANAL ← $versiya qilinardi"
            else
                kv put "appcast-mac$KANAL" --path "$ISH/appcast.xml" >/dev/null
                echo "✓ appcast-mac$KANAL qaytarildi → $versiya"
            fi
        fi
        ;;

    mac)
        [ ${#POZ[@]} -eq 1 ] || { echo "ishlatish: $0 mac <versiya> [--bosqich-soat N]" >&2; exit 2; }
        versiya="${POZ[0]}"
        zip="$ROOT/dist/Kotib-$versiya-mac.zip"
        [[ "$versiya" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
            echo "Xato: macOS feed'iga faqat raqamli versiya ($versiya emas) — Sparkle" \
                "qoʻshimchani solishtirmaydi (1.2.0-sinov2 = 1.2.0). Sinov uchun: 1.1.90, 1.1.91" >&2
            exit 2
        }
        [ -f "$zip" ] || { echo "Xato: $zip yoʻq — avval ./scripts/make_pkg.sh" >&2; exit 1; }
        [[ "$BOSQICH_SOAT" =~ ^[0-9]+$ ]] || { echo "Xato: --bosqich-soat butun son" >&2; exit 2; }
        [ -z "$MUHLAT" ] || [[ "$MUHLAT" =~ ^[0-9]+$ ]] || { echo "Xato: --muhlat butun son (soat)" >&2; exit 2; }
        sparkle_tayyorla "$ROOT/sparkle"

        # Siyosat manifesti (majburiy talab yoki uning yoʻqligi) HAMMA narsadan
        # OLDIN imzolanib tekshiriladi: notoʻgʻri `--min` (masalan, versiyadan
        # yuqori) feed «kritik» deb chiqib ketgandan keyin emas, hozir toʻxtatsin.
        python3 - "$ISH/manifest-mac.json" "$versiya" "$MIN" "$MUHLAT" <<'PY'
import json, sys
yol, v, mn, muhlat = sys.argv[1:]
m = {"platforma": "mac", "versiya": v}
if mn:
    m["min_versiya"] = mn
if muhlat:
    m["majburiy_muhlat_soat"] = int(muhlat)
json.dump(m, open(yol, "w", encoding="utf-8"), ensure_ascii=False)
PY
        siyosat_tayyorla "$ISH/manifest-mac.json" 0

        # Arxiv papkasi doimiy: oldingi versiyalar shu yerda turadi (delta'lar
        # ulardan yasaladi). --quruq da uning NUSXASI ustida ishlanadi. Sinov
        # kanali — ALOHIDA arxiv: aks holda sinov build'lari keyingi asosiy
        # feed'ga tushib, hammaga taklif qilinishi mumkin edi.
        arxiv="${SPARKLE_ARXIV:-$ROOT/dist/sparkle-arxiv$KANAL}"
        mkdir -p "$arxiv"
        ish_arxiv="$arxiv"
        if [ "$QURUQ" = 1 ]; then
            ish_arxiv="$ISH/arxiv"
            cp -R "$arxiv" "$ish_arxiv"
        fi
        cp "$zip" "$ish_arxiv/"
        # Izoh — CHANGELOG.md dagi shu versiya boʻlimi (boʻlmasa — izohsiz).
        python3 "$ROOT/scripts/changelog-bolimi.py" "$ROOT/CHANGELOG.md" "$versiya" \
            > "$ish_arxiv/Kotib-$versiya-mac.md" || rm -f "$ish_arxiv/Kotib-$versiya-mac.md"
        oldin="$(find "$ish_arxiv" -maxdepth 1 -type f -name '*.delta' | sort)"

        qadam "generate_appcast (delta'lar, imzolangan feed, bosqich ${BOSQICH_SOAT} soat)"
        kalit="$KALIT_FAYLI"
        if [ -z "$kalit" ]; then
            kalit="$ISH/kalit"
            # Siyosat imzosi (yuqorida) kalitni shu faylga eksport qilgan boʻlishi
            # mumkin — `generate_keys -x` mavjud fayl ustidan yozmaydi va yiqiladi.
            [ -f "$kalit" ] || "$ROOT/sparkle/bin/generate_keys" --account kotib -x "$kalit" >/dev/null
        fi
        kritik=()
        [ -z "$MIN" ] || kritik=(--critical-update-version "$MIN")
        "$ROOT/sparkle/bin/generate_appcast" --ed-key-file "$kalit" \
            --download-url-prefix "$CDN_URL/dl/mac/" \
            --embed-release-notes --maximum-deltas 3 \
            --phased-rollout-interval $((BOSQICH_SOAT * 3600)) \
            ${kritik[@]+"${kritik[@]}"} \
            -o "$ish_arxiv/appcast-mac.xml" "$ish_arxiv"
        grep -q "sparkle-signatures" "$ish_arxiv/appcast-mac.xml" || {
            echo "Xato: feed imzolanmadi (SURequireSignedFeed ilovada yoqiq) — toʻxtatildi" >&2; exit 1; }
        grep -q "<sparkle:version>$versiya</sparkle:version>" "$ish_arxiv/appcast-mac.xml" || {
            echo "Xato: feed'da $versiya yoʻq" >&2; exit 1; }

        # Yangi fayllar: shu versiya .zip i va shu safar yasalgan delta'lar.
        yangilar=("$ish_arxiv/Kotib-$versiya-mac.zip")
        while IFS= read -r f; do
            [ -n "$f" ] || continue
            grep -qxF "$f" <<< "$oldin" || yangilar+=("$f")
        done < <(find "$ish_arxiv" -maxdepth 1 -type f -name '*.delta' | sort)

        for f in "${yangilar[@]}"; do
            cdn_joyla "$f" "dl/mac/$(basename "$f")"
        done
        echo "  .pkg (yangi oʻrnatish uchun) 300 MiB dan katta — AGENTS.md → «Uploading anything over 300 MiB to R2»"

        if [ "$QURUQ" = 1 ]; then
            qadam "QURUQ: KV ← appcast-mac$KANAL va appcast-mac$KANAL@$versiya (${JOY#--})"
            printf '  feed elementlari: %s, delta: %s\n' \
                "$(grep -c '<item>' "$ish_arxiv/appcast-mac.xml")" \
                "$(grep -c 'sparkle:deltaFrom' "$ish_arxiv/appcast-mac.xml" || true)"
        else
            qadam "KV ← appcast-mac$KANAL (${JOY#--})"
            kv put "appcast-mac$KANAL@$versiya" --path "$ish_arxiv/appcast-mac.xml" >/dev/null
            kv put "appcast-mac$KANAL" --path "$ish_arxiv/appcast-mac.xml" >/dev/null
            echo "✓ appcast-mac$KANAL = $versiya (bosqich ${BOSQICH_SOAT} soat). Qaytarish: $0 qaytar mac <oldingi>"
        fi

        # Siyosat — feed bilan birga (yuqorida allaqachon tekshirilgan).
        siyosat_kvga
        ;;

    win)
        [ ${#POZ[@]} -eq 1 ] || {
            echo "ishlatish: $0 win <versiya> [--foiz N] [--min V] [--muhlat N] [--kanal sinov]" >&2; exit 2; }
        versiya="${POZ[0]}"
        exe="$ROOT/dist/Kotib-$versiya-win-yangilash.exe"
        [ -f "$exe" ] || { echo "Xato: $exe yoʻq — avval ./win/tools/ornatuvchi-yasa.sh (S7)" >&2; exit 1; }
        [ -z "$MUHLAT" ] || [[ "$MUHLAT" =~ ^[0-9]+$ ]] || { echo "Xato: --muhlat butun son (soat)" >&2; exit 2; }

        qadam "fayl imzosi (Ed25519, butun fayl)"
        nom="dl/win/$(basename "$exe")"
        hajm="$(stat -f %z "$exe")"
        sha="$(shasum -a 256 "$exe" | awk '{print $1}')"
        imzo="$(imzola_fayl "$exe")"
        printf '%s' "$imzo" | grep -Eq '^[A-Za-z0-9+/]{86}==$' || { echo "Xato: fayl imzosi olinmadi" >&2; exit 1; }
        echo "  $nom: $hajm bayt, sha256 $sha"

        cdn_joyla "$exe" "$nom"

        izoh="$(python3 "$ROOT/scripts/changelog-bolimi.py" "$ROOT/CHANGELOG.md" "$versiya" 2>/dev/null || true)"
        python3 - "$ISH/manifest.json" "$versiya" "$MIN" "$MUHLAT" "$CDN_URL/$nom" "$hajm" "$sha" "$imzo" "$izoh" <<'PY'
import json, sys
yol, v, mn, muhlat, url, hajm, sha, imzo, izoh = sys.argv[1:]
f = {"url": url, "hajm": int(hajm), "sha256": sha, "imzo": imzo}
m = {"platforma": "win", "versiya": v}
if mn:
    m["min_versiya"] = mn
if muhlat:
    m["majburiy_muhlat_soat"] = int(muhlat)
m["tarqatish_foiz"] = 100
if izoh.strip():
    m["izoh"] = izoh.strip()
m["fayllar"] = {"x64": f, "arm64": f}
json.dump(m, open(yol, "w", encoding="utf-8"), ensure_ascii=False)
PY
        # Quruq rejimda fayl CDN'ga yuklanmagan — HEAD tekshiruvi oʻtkazilmaydi.
        cdn=1
        [ "$QURUQ" = 1 ] && cdn=0
        siyosat_yoz "$ISH/manifest.json" "$cdn"
        ;;

    *)
        echo "Nomaʼlum buyruq: $BUYRUQ" >&2
        sed -n '3,25p' "$0" | sed 's/^# \{0,1\}//'
        exit 2
        ;;
esac
