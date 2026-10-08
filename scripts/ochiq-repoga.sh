#!/usr/bin/env bash
#
# Joriy commit'ni ochiq `MuhammadMirrr/kotib` repo'ga koʻchiradi (bitta commit).
#
#   ./scripts/ochiq-repoga.sh "<commit xabari>" [--quruq]
#
# Ichki repo (`uzbek-dictation`, private) — toʻliq tarix va `KEYINGI-ISH.md`.
# Ochiq repo — shu daraxt, faqat CHIQARILADIGANLARsiz (pastdagi roʻyxat).
# Avval `gitleaks` daraxtni tekshiradi; topilma boʻlsa toʻxtaydi (KV namespace
# identifikatori — sir emas, `.gitleaksignore` da emas, shu yerda filtrlanadi).
# Faqat commit qilingan holat koʻchadi (`git archive HEAD`) — ishchi nusxadagi
# oʻzgarishlar emas.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
XABAR="${1:?commit xabari kerak}"
QURUQ=0; [ "${2:-}" = "--quruq" ] && QURUQ=1
OCHIQ="https://github.com/MuhammadMirrr/kotib.git"
# Ochiq repoga KIRMAYDIGANLAR (audit: docs/audit/2026-10-08-ochiq-kod-auditi.md).
CHIQARILADI=(KEYINGI-ISH.md)

[ -z "$(git -C "$ROOT" status --porcelain --untracked-files=no)" ] ||
    echo "Eslatma: commit qilinmagan oʻzgarishlar bor — ular koʻchmaydi" >&2

ISH="$(mktemp -d)"
trap 'rm -rf "$ISH"' EXIT
git clone -q "$OCHIQ" "$ISH/repo"
# Ochiq repodagi hamma narsa (.git dan tashqari) oʻchiriladi va daraxt qaytadan yoziladi —
# ichki repoda oʻchirilgan fayl ochiq repoda qolib ketmasin.
find "$ISH/repo" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
git -C "$ROOT" archive HEAD | tar -x -C "$ISH/repo"
for f in "${CHIQARILADI[@]}"; do rm -rf "${ISH:?}/repo/$f"; done

command -v gitleaks >/dev/null || { echo "Xato: gitleaks yoʻq (brew install gitleaks)" >&2; exit 1; }
topilma="$(gitleaks dir "$ISH/repo" --no-banner --redact -v 2>/dev/null |
    grep '^Finding:' | grep -v 'wrangler kv key put --namespace-id=' || true)"
[ -z "$topilma" ] || { printf '%s\n' "$topilma" >&2; echo "Xato: gitleaks topilmasi — toʻxtatildi" >&2; exit 1; }

cd "$ISH/repo"
git add -A
if git diff --cached --quiet; then echo "Oʻzgarish yoʻq."; exit 0; fi
git diff --cached --stat | tail -1
if [ "$QURUQ" = 1 ]; then echo "QURUQ: push qilinmadi"; exit 0; fi
git commit -q -m "$XABAR"
git push -q origin HEAD:main
echo "✓ $OCHIQ ← $(git rev-parse --short HEAD) ($(git -C "$ROOT" rev-parse --short HEAD) dan)"
