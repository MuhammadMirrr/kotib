# Xavfsizlik / Security

## Zaiflik haqida xabar berish

Zaiflik topsangiz, **ochiq issue yozmang** — muhammadmirqobilov@gmail.com ga
xat yuboring (yoki GitHub'dagi «Report a vulnerability» — private advisory).
Iloji boricha: taʼsirlangan versiya va platforma, takrorlash qadamlari, taʼsiri.
Javob — bir necha kun ichida; tuzatish chiqqach, xohlasangiz, ismingiz
eslatiladi.

Ayniqsa muhim yoʻnalishlar:

- **Avto-yangilanish** — manifest imzosi, URL ruxsatnomasi, yangilovchi
  (`src/yangilanish_siyosat.swift`, `src/imzo.swift`, `win/core/yangilanish_siyosat.cpp`,
  `win/core/imzo.cpp`).
- **Statistika Worker'i** (`statistika/`) — kiruvchi maʼlumotni tekshirish,
  panel kirishi, maxfiylik vaʼdalari ([`PRIVACY.md`](PRIVACY.md)).
- **Oʻrnatuvchilar** — macOS `.pkg` skriptlari, Windows Inno Setup.
- **Maxfiylik** — ovoz yoki matn qurilmadan chiqib ketadigan har qanday yoʻl.

## Qoʻllab-quvvatlanadigan versiyalar

Faqat oxirgi reliz. Avto-yangilanish (1.2.0 dan) tuzatishlarni hamma
foydalanuvchiga yetkazadi.

## Imzo kaliti siyosati

- Yangilanishlar **Ed25519** bilan imzolanadi. Ochiq kalit repoda:
  [`scripts/yangilanish-kaliti.pub`](scripts/yangilanish-kaliti.pub); ilovalar faqat
  shu kalit bilan imzolangan manifestni qabul qiladi.
- Yopiq kalit **hech qachon** repoda, CI'da yoki bulutda emas — faqat
  muallifning Keychain'ida va ikki oflayn shifrlangan zaxirada
  (`scripts/imzo-kaliti.sh`). Kalit oshkor boʻlsa — yangi kalit bilan
  favqulodda reliz va ushbu hujjatda eʼlon.
- macOS ilovalari Developer ID bilan imzolangan va Apple notarizatsiyasidan oʻtgan.

---

**Reporting:** please do not open a public issue — email
muhammadmirqobilov@gmail.com or use GitHub private vulnerability reporting.
Only the latest release is supported. Updates are signed with Ed25519; the public key
is `scripts/yangilanish-kaliti.pub`, the private key never leaves the maintainer's
Keychain and two offline encrypted backups.
