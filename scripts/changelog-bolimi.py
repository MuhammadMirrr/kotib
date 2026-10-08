#!/usr/bin/env python3
"""CHANGELOG.md dan bitta versiya boʻlimini chiqaradi (sarlavhasiz).

    python3 scripts/changelog-bolimi.py CHANGELOG.md 1.2.0

`reliz.sh mac` uni Sparkle feed izohi sifatida ishlatadi. Boʻlim topilmasa —
chiqish kodi 1, hech narsa chiqmaydi.
"""
import re
import sys

matn = open(sys.argv[1], encoding="utf-8").read()
versiya = re.escape(sys.argv[2])
m = re.search(r"^## \[" + versiya + r"\][^\n]*\n(.*?)(?=^## \[|\Z)", matn, re.S | re.M)
if not m or not m.group(1).strip():
    sys.exit(1)
print(m.group(1).strip())
