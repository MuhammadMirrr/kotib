# shellcheck shell=bash
# Kotib versiyasini `VERSION` faylidan oʻqiydi. Bu fayl ishga tushirilmaydi —
# boshqa skriptlar uni `source` qiladi:
#
#   source "$ROOT/scripts/versiya.sh"
#   echo "$KOTIB_VERSIYA"   # 1.1.0  (yoki 1.2.0-sinov1)
#
# `VERSION` — versiyaning YAGONA manbai. Ilgari raqam olti joyda qoʻlda
# yozilardi (build.sh, app.rc, make_pkg.sh, rubai.iss, versiya.json, sayt) va
# hujjat «uch joy» derdi. Bittasi unutilsa ilova oʻziga oʻzini «yangi versiya»
# deb taklif qiladi. Endi yigʻiladigan hamma narsa shu fayldan oladi,
# `scripts/versiya-tekshir.sh` esa qolganini nazorat qiladi.
#
# Format: `KATTA.KICHIK.TUZATISH`, ixtiyoriy `-qoʻshimcha` bilan (sinov
# kanali uchun, masalan `1.2.0-sinov1`). Windows resursidagi raqamli
# versiya faqat uchta raqamni oladi — qoʻshimcha unga sigʻmaydi.

_kotib_ildiz="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ ! -f "$_kotib_ildiz/VERSION" ]; then
    echo "Xato: $_kotib_ildiz/VERSION topilmadi" >&2
    exit 1
fi

# Faqat birinchi qator; atrofidagi boʻshliq va CR (Windows'da tahrirlangan
# boʻlsa) olib tashlanadi.
KOTIB_VERSIYA="$(head -n 1 "$_kotib_ildiz/VERSION" | tr -d ' \t\r')"

if ! printf '%s' "$KOTIB_VERSIYA" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$'; then
    echo "Xato: VERSION notoʻgʻri: «$KOTIB_VERSIYA» (kutilgan: 1.2.0 yoki 1.2.0-sinov1)" >&2
    exit 1
fi

unset _kotib_ildiz
