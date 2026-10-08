# Ochiq kod auditi — joriy daraxt (2026-10-08)

Spec: barqarorlik O1. Q5 qaroriga koʻra ochiq repo **bitta boshlangʻich
commit**dan boshlanadi — eski tarix chiqmaydi, shuning uchun faqat joriy daraxt
tekshirildi (`git ls-files`, 287 fayl).

## Natija: toza — sir va shaxsiy maʼlumot topilmadi

| Tekshiruv | Usul | Natija |
|---|---|---|
| Sirlar | regex: AWS (`AKIA…`), GitHub (`ghp_`, `github_pat_`), OpenAI/Anthropic (`sk-…`), Slack (`xox?-`), Google (`AIza…`), JWT, `-----BEGIN … PRIVATE KEY-----`, `PANEL_KALIT`/`IP_TUZ` qiymatlari, `password=` | 0 |
| Shaxsiy mutlaq yoʻllar | `/Users/<nom>/` | 0 (faqat spec'dagi qoidaning oʻzi) |
| IP manzillar | IPv4 (127.0.0.1 va versiyalardan tashqari) | 0 |
| Telefon, karta raqamlari | `+998…`, `NNNN NNNN NNNN NNNN` | 0 |
| Email | — | faqat ataylab: aloqa manzili (`PRIVACY.md`, `web/maxfiylik.html`, `SECURITY.md`, `CODE_OF_CONDUCT.md`) |
| Katta fayllar | eng kattasi 208 KB (`win/cmake/vulkan-lib/*.a` — Vulkan import kutubxonasi, yigʻish uchun kerak) | muammo yoʻq |
| `.gitignore` | `.dev.vars*`, model (`*.bin`), build papkalari, `.stt-baho/` | bor |

`gitleaks` bu mashinada oʻrnatilmagan; yuqoridagi naqshlar uning asosiy
qoidalarini qamraydi. Ochishdan oldin (S24) CI'da `gitleaks` bilan yana bir bor
oʻtkazish tavsiya etiladi.

## Sir emas — qoladi

- **Apple Team ID** (`scripts/make_pkg.sh`) — har imzolangan binarda ochiq koʻrinadi.
- **Cloudflare D1/KV identifikatorlari** (`statistika/wrangler.toml`) — hisobga
  kirmasdan ulardan foydalanib boʻlmaydi.
- **Imzo ochiq kaliti** (`scripts/yangilanish-kaliti.pub`) — ataylab ochiq.
- **Donat karta raqamlari** — ataylab ochiq (ilova va saytda koʻrsatiladi).

## Egasining qarori kerak (ochishdan oldin, S24)

| Fayl | Nima | Tavsiya |
|---|---|---|
| `KEYINGI-ISH.md` | Ichki ish roʻyxati | Ochishdan oldin oʻchirish yoki `CHANGELOG.md`/issue'larga koʻchirish |
| `WINDOWS-PORT-PLAN.md`, `WINDOWS-PARITET.md` | Windows porti tarixi va VM holati | `docs/windows/` ga koʻchirish — muhandislik tarixi sifatida foydali |
| `docs/superpowers/` | Reja va spec'lar (oʻzbekcha) | Qoldirish mumkin — qarorlarning «nega»si; ichida shaxsiy maʼlumot yoʻq |
| `CLAUDE.md`, `AGENTS.md` | Agent/muhandislik qoʻllanmasi | Qoldirish — hissa qoʻshuvchilar uchun ham eng toʻliq hujjat |
| `statistika/` | Statistika Worker'i | Qoldirish — maxfiylik vaʼdalarini tekshirib boʻladigan qiladi |
| Klon havolalari (`MuhammadMirrr/uzbek-dictation`) | README va boshqalar | `kotib` repo yaratilganda almashtiriladi (O6) |

**Qaror (egasi, 2026-10-08):** tavsiyalar qabul qilindi — `KEYINGI-ISH.md` ochiq
repoga kirmaydi (`scripts/ochiq-repoga.sh`), `WINDOWS-*.md` → `docs/windows/`, klon
havolalari va `rubai.iss` dagi `AppUrl` → `MuhammadMirrr/kotib`. Ochishdan oldin
`gitleaks` 8.30.1 butun daraxtni tekshirdi: faqat KV namespace identifikatori (sir emas).

## Litsenziyalar

`LICENSE` — standart MIT matni (GitHub taniydi). Oxiridagi «Note: … rubaiSTT … whisper.cpp» paragrafi olib tashlandi: GitHub litsenziyani shu sababli tanimasligi mumkin edi, mazmuni esa endi `THIRD_PARTY_NOTICES.md` da toʻliqroq. Uchinchi tomon: `THIRD_PARTY_NOTICES.md`
(jadval) + ilova ichidagi `Litsenziyalar.txt` (`scripts/litsenziyalar.sh` pinlangan
manbalarning oʻz LICENSE fayllaridan yigʻadi). Eʼtibor: **NLLB-200 — CC-BY-NC-4.0**
(faqat notijorat) — Kotib bepul, lekin model tijorat maqsadida qayta tarqatilmasligi
kerak; bu `THIRD_PARTY_NOTICES.md` da yozilgan.
