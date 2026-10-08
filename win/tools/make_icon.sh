#!/bin/bash
# assets/icon_1024.png dan Windows .ico yasaydi — make_icon.ps1 ning macOS varianti.
# Cross-compile macOS'da ketgani uchun kerak: windres .ico faylni build paytida
# talab qiladi, uni esa PowerShell'siz yasash kerak.
#
# ICO ichiga PNG siqilgan tasvirlar joylanadi (Windows Vista'dan beri ishlaydi).
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
src="$root/assets/icon_1024.png"
dst="$root/win/res/AppIcon.ico"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Windows shu o'lchamlarni turli joylarda ishlatadi:
# 16 - sarlavha/tray, 32 - ish stoli, 48 - Explorer, 256 - katta ko'rinish.
sizes=(16 24 32 48 64 128 256)
for s in "${sizes[@]}"; do
    sips -z "$s" "$s" "$src" --out "$tmp/$s.png" >/dev/null
done

python3 - "$tmp" "$dst" "${sizes[@]}" <<'PY'
import struct, sys, pathlib
tmp, dst, *sizes = sys.argv[1:]
sizes = [int(s) for s in sizes]
blobs = [pathlib.Path(f"{tmp}/{s}.png").read_bytes() for s in sizes]

out = struct.pack("<HHH", 0, 1, len(sizes))          # ICONDIR
offset = 6 + 16 * len(sizes)
for s, b in zip(sizes, blobs):                        # ICONDIRENTRY
    # 256 o'lchami baytga sig'maydi — 0 deb yoziladi, format shuni kutadi.
    out += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(b), offset)
    offset += len(b)
pathlib.Path(dst).write_bytes(out + b"".join(blobs))
print(f"{dst} — {len(sizes)} o'lcham, {offset} bayt")
PY
