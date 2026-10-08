# shellcheck shell=bash
# Pinlangan bogʻliqliklarni (`scripts/bogliqliklar.env`) oʻqiydi va ularni
# klonlash/tekshirish yordamchisini beradi. Ishga tushirilmaydi — `source`
# qilinadi (setup.sh, win/build-mac.sh):
#
#   source "$ROOT/scripts/bogliqliklar.sh"
#   bogliqlik_tayyorla "$ROOT/whisper.cpp" "$WHISPER_URL" "$WHISPER_COMMIT"

# shellcheck source=bogliqliklar.env
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/bogliqliklar.env"

# bogliqlik_tayyorla <papka> <url> <commit> [--submodullar]
#
# Papka yoʻq boʻlsa — aynan shu commit'ni klonlaydi (faqat bitta commit,
# tarixsiz). Bor boʻlsa — HEAD pin bilan bir xilligini tekshiradi va mos
# kelmasa TOʻXTAYDI: boshqa commit'dan yigʻilgan kutubxona ikki platformada
# boshqacha natija beradi, buni jim oʻtkazib yuborib boʻlmaydi. Mahalliy
# nusxani skript oʻzi almashtirmaydi — unda tajriba oʻzgarishlari boʻlishi
# mumkin, build papkalarini qayta yigʻish esa 25 daqiqagacha oladi.
bogliqlik_tayyorla() {
    local papka="$1" url="$2" commit="$3" sub="${4:-}"
    local nom
    nom="$(basename "$papka")"

    if [ -e "$papka" ]; then
        # `.git` boʻlmasa `git -C` yuqoriga chiqib Kotib repo'sining oʻzini
        # topardi va chalkash xato berardi.
        if [ ! -e "$papka/.git" ]; then
            echo "Xato: $papka git nusxasi emas — oʻchirib, skriptni qayta ishga tushiring." >&2
            return 1
        fi
        local hozir
        hozir="$(git -C "$papka" rev-parse HEAD)"
        if [ "$hozir" != "$commit" ]; then
            local sub_buyruq=""
            [ "$sub" = --submodullar ] && sub_buyruq="
  git -C \"$papka\" submodule update --init --recursive"
            cat >&2 <<XATO
Xato: $nom pinlangan commit'da emas.
  hozir: $hozir
  pin:   $commit   (scripts/bogliqliklar.env)

Pinga oʻtish:
  git -C "$papka" fetch --depth 1 origin $commit
  git -C "$papka" checkout --detach $commit$sub_buyruq
Soʻng $nom ning build papkalarini oʻchirib, skriptni qayta ishga tushiring.
XATO
            return 1
        fi
        return 0
    fi

    echo "==> $nom klonlanmoqda (${commit:0:12})"
    # Vaqtinchalik papkaga: tarmoq uzilsa yarim klon asosiy nom bilan
    # qolmasin — aks holda keyingi ishga tushirish uni «tayyor» deb oʻylaydi.
    local vaqtinchalik="$papka.yuklanmoqda"
    rm -rf "$vaqtinchalik"
    git init -q "$vaqtinchalik"
    git -C "$vaqtinchalik" remote add origin "$url"
    # GitHub istalgan commit'ni SHA boʻyicha berishga ruxsat beradi, shuning
    # uchun teg yoki branch kerak emas va `--depth 1` yetadi.
    git -C "$vaqtinchalik" fetch -q --depth 1 origin "$commit"
    git -C "$vaqtinchalik" checkout -q --detach FETCH_HEAD
    if [ "$sub" = --submodullar ]; then
        git -C "$vaqtinchalik" submodule update -q --init --recursive --depth 1
    fi
    mv "$vaqtinchalik" "$papka"
}

# sparkle_tayyorla <papka>
#
# Sparkle relizini (`SPARKLE_URL`) yuklab, sha256 ni tekshirib, <papka> ga
# ochadi. Allaqachon shu versiya ochilgan boʻlsa — hech narsa qilmaydi.
# Papka gitignore'da: framework va vositalar repoga tushmaydi.
sparkle_tayyorla() {
    local papka="$1"
    if [ -f "$papka/.versiya" ] && [ "$(cat "$papka/.versiya")" = "$SPARKLE_VERSIYA" ]; then
        return 0
    fi
    echo "==> Sparkle $SPARKLE_VERSIYA yuklanmoqda"
    local arxiv="$papka.tar.xz.yuklanmoqda"
    curl -fL --progress-bar -o "$arxiv" "$SPARKLE_URL"
    local hash
    hash="$(shasum -a 256 "$arxiv" | cut -d' ' -f1)"
    if [ "$hash" != "$SPARKLE_SHA256" ]; then
        echo "Xato: Sparkle arxivining sha256 mos emas (olingan: $hash)" >&2
        rm -f "$arxiv"
        return 1
    fi
    rm -rf "$papka"
    mkdir -p "$papka"
    tar -xJf "$arxiv" -C "$papka"
    rm -f "$arxiv"
    echo "$SPARKLE_VERSIYA" > "$papka/.versiya"
}

# vad_tayyorla <fayl> — Silero VAD modeli (S12) `<fayl>` da: yoʻq yoki sha256
# mos kelmasa pinlangan manbadan yuklab olinadi va tekshiriladi.
vad_tayyorla() {
    local fayl="$1" hash
    if [ -f "$fayl" ]; then
        hash="$(shasum -a 256 "$fayl" | cut -d' ' -f1)"
        [ "$hash" = "$VAD_SHA256" ] && return 0
    fi
    echo "==> VAD modeli ($VAD_NOM) yuklanmoqda"
    mkdir -p "$(dirname "$fayl")"
    curl -fL --progress-bar -o "$fayl.yuklanmoqda" "$VAD_URL"
    hash="$(shasum -a 256 "$fayl.yuklanmoqda" | cut -d' ' -f1)"
    if [ "$hash" != "$VAD_SHA256" ]; then
        echo "Xato: VAD modelining sha256 mos emas (olingan: $hash)" >&2
        rm -f "$fayl.yuklanmoqda"
        return 1
    fi
    mv "$fayl.yuklanmoqda" "$fayl"
}
