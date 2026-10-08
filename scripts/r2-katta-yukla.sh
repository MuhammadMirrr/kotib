#!/usr/bin/env bash
#
# 300 MiB dan katta faylni CDN'ga (R2) yuklaydi.
#
#   ./scripts/r2-katta-yukla.sh <fayl> <kalit>      masalan: dist/Kotib-1.2.0-mac.pkg dl/mac/Kotib-1.2.0-mac.pkg
#
# NEGA ALOHIDA: `wrangler r2 object put` 300 MiB dan kattasini rad etadi, wrangler'ning
# OAuth tokenida esa S3 API uchun `r2` huquqi yoʻq (AGENTS.md → «Uploading anything
# over 300 MiB to R2»). Shuning uchun: R2 bogʻlanishli VAQTINCHALIK Worker deploy
# qilinadi (multipart: boshlash / boʻlak / yakunlash), fayl 50 MiB boʻlaklarda
# HTTPS orqali yuboriladi, keyin Worker OʻCHIRILADI (muvaffaqiyatsizlikda ham).
# Kirish — har ishga tushishda yangi tasodifiy token (faqat shu jarayonda yashaydi).
#
# Immutable qoidasi: kalit CDN'da allaqachon bor boʻlsa — TOʻXTAYDI (Cloudflare uni
# bir yil keshlaydi, ustidan yozish eski nusxani qoldiradi). Oxirida hajm CDN orqali
# tekshiriladi.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FAYL="${1:?fayl kerak}"
KALIT="${2:?R2 kaliti kerak (masalan dl/mac/Kotib-1.2.0-mac.pkg)}"
CDN_BUCKET="audio-matnga-cdn"
CDN_URL="https://cdn.mirqobilov.com"
BOLAK=$((50 * 1024 * 1024))
NOM="kotib-r2-yuklovchi-$(date +%s)"

[ -f "$FAYL" ] || { echo "Xato: $FAYL yoʻq" >&2; exit 1; }
HAJM=$(stat -f %z "$FAYL")
if [ "$(curl -s -o /dev/null -w '%{http_code}' -I "$CDN_URL/$KALIT")" = "200" ]; then
    echo "Xato: $CDN_URL/$KALIT allaqachon bor — immutable kalit ustidan yozilmaydi" >&2
    exit 1
fi

ISH="$(mktemp -d)"
chmod 700 "$ISH"
TOKEN="$(openssl rand -hex 32)"
URL=""
tozala() {
    if [ -n "$URL" ] || [ -f "$ISH/wrangler.toml" ]; then
        ( cd "$ISH" && npx --prefix "$ROOT/statistika" --no-install wrangler delete --name "$NOM" --force \
            >/dev/null 2>&1 ) && echo "==> vaqtinchalik Worker oʻchirildi ($NOM)" \
            || echo "DIQQAT: $NOM Worker'ini qoʻlda oʻchiring: wrangler delete --name $NOM" >&2
    fi
    rm -rf "$ISH"
}
trap tozala EXIT

cat > "$ISH/index.js" <<'JS'
// Vaqtinchalik R2 multipart yuklovchi — scripts/r2-katta-yukla.sh deploy qiladi va oʻchiradi.
export default {
  async fetch(req, env) {
    if (req.headers.get('x-token') !== env.TOKEN) return new Response('yoʻq', { status: 403 });
    const u = new URL(req.url);
    const kalit = u.searchParams.get('kalit');
    const yol = u.pathname;
    if (yol === '/salom') return new Response('ok');
    if (yol === '/boshla') {
      const m = await env.CDN.createMultipartUpload(kalit, {
        httpMetadata: { cacheControl: 'public, max-age=31536000, immutable',
                        contentType: u.searchParams.get('tur') || 'application/octet-stream' },
      });
      return Response.json({ id: m.uploadId });
    }
    const m = env.CDN.resumeMultipartUpload(kalit, u.searchParams.get('id'));
    if (yol === '/bolak') {
      const p = await m.uploadPart(Number(u.searchParams.get('n')), req.body);
      return Response.json(p);
    }
    if (yol === '/yakunla') {
      const o = await m.complete(await req.json());
      return Response.json({ hajm: o.size });
    }
    if (yol === '/bekor') { await m.abort(); return Response.json({}); }
    return new Response('?', { status: 404 });
  },
};
JS
cat > "$ISH/wrangler.toml" <<TOML
name = "$NOM"
main = "index.js"
compatibility_date = "2026-09-01"
workers_dev = true
[[r2_buckets]]
binding = "CDN"
bucket_name = "$CDN_BUCKET"
TOML

echo "==> vaqtinchalik Worker: $NOM"
CHIQISH="$(cd "$ISH" && npx --prefix "$ROOT/statistika" --no-install wrangler deploy 2>&1)"
URL="$(printf '%s\n' "$CHIQISH" | grep -oE 'https://[a-z0-9.-]+\.workers\.dev' | head -1)"
[ -n "$URL" ] || { printf '%s\n' "$CHIQISH" >&2; echo "Xato: Worker manzili topilmadi" >&2; exit 1; }
printf '%s' "$TOKEN" | ( cd "$ISH" && npx --prefix "$ROOT/statistika" --no-install wrangler secret put TOKEN \
    --name "$NOM" >/dev/null )
# workers.dev manzili va secret tarqalishini kutamiz: 403 yetarli emas — secret
# hali kuchga kirmagan Worker ham 403 beradi. Token bilan 200 kelishi shart.
tayyor=0
for _ in $(seq 1 60); do
    if [ "$(curl -s -o /dev/null -w '%{http_code}' -H "x-token: $TOKEN" "$URL/salom")" = "200" ]; then
        tayyor=1
        break
    fi
    sleep 2
done
[ "$tayyor" = 1 ] || { echo "Xato: vaqtinchalik Worker 2 daqiqada tayyor boʻlmadi" >&2; exit 1; }

case "$FAYL" in
    *.pkg) TUR="application/octet-stream" ;;
    *.exe) TUR="application/vnd.microsoft.portable-executable" ;;
    *) TUR="application/octet-stream" ;;
esac
SO="$(python3 -c 'import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))' "$KALIT")"
JAVOB="$(curl -s -X POST -H "x-token: $TOKEN" "$URL/boshla?kalit=$SO&tur=$TUR")"
ID="$(printf '%s' "$JAVOB" | python3 -c 'import json,sys;print(json.load(sys.stdin)["id"])' 2>/dev/null)" || {
    echo "Xato: multipart boshlanmadi: $JAVOB" >&2
    exit 1
}
IDQ="$(python3 -c 'import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))' "$ID")"

SONI=$(( (HAJM + BOLAK - 1) / BOLAK ))
echo "==> $FAYL ($HAJM bayt) → $KALIT, $SONI boʻlak"
: > "$ISH/qismlar.jsonl"
for n in $(seq 1 "$SONI"); do
    dd if="$FAYL" of="$ISH/bolak" bs="$BOLAK" skip=$((n - 1)) count=1 2>/dev/null
    for urinish in 1 2 3; do
        if curl -sf -X PUT -H "x-token: $TOKEN" --data-binary @"$ISH/bolak" \
            "$URL/bolak?kalit=$SO&id=$IDQ&n=$n" >> "$ISH/qismlar.jsonl"; then
            echo >> "$ISH/qismlar.jsonl"
            break
        fi
        [ "$urinish" = 3 ] && {
            curl -s -X POST -H "x-token: $TOKEN" "$URL/bekor?kalit=$SO&id=$IDQ" >/dev/null || true
            echo "Xato: $n-boʻlak yuklanmadi" >&2
            exit 1
        }
        sleep 3
    done
    printf '  %d/%d\n' "$n" "$SONI"
done
python3 -c 'import json,sys;print(json.dumps([json.loads(l) for l in open(sys.argv[1]) if l.strip()]))' \
    "$ISH/qismlar.jsonl" > "$ISH/qismlar.json"
curl -sf -X POST -H "x-token: $TOKEN" --data-binary @"$ISH/qismlar.json" "$URL/yakunla?kalit=$SO&id=$IDQ" >/dev/null

CDN_HAJM="$(curl -sI "$CDN_URL/$KALIT" | awk -F': ' 'tolower($1)=="content-length"{print $2}' | tr -d '\r')"
[ "$CDN_HAJM" = "$HAJM" ] || { echo "Xato: CDN hajmi $CDN_HAJM, kutilgan $HAJM" >&2; exit 1; }
echo "✓ $CDN_URL/$KALIT ($HAJM bayt)"
