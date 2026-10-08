# Kotib — Maxfiylik siyosati va foydalanish shartlari

**Oxirgi yangilanish:** 2026-09-10

Kotib — oʻzbek tili uchun ovozdan matnga (diktovka), audio fayldan matnga
(studiya) va oflayn tarjima ilovasi. Ovoz tanish va tarjima **butunlay
sizning qurilmangizda**, internetsiz bajariladi. Bu hujjat ilova qanday
anonim maʼlumot yigʻishini toʻliq va halol tushuntiradi.

---

## Eng muhimi — nima HECH QACHON yuborilmaydi

Quyidagilar qurilmangizdan **hech qachon chiqmaydi**:

- **Ovoz** — mikrofon yozuvi yoki audio fayllaringiz.
- **Matn** — nimani diktovka qilganingiz, transkripsiya yoki tarjima natijasi.
- Fayl nomlari, mikrofon nomi.
- Email, hisob maʼlumotlari, parollar, AI kalitlaringiz.
- Xom IP manzil.
- Qurilmani shaxsan belgilaydigan identifikatorlar: seriya raqami,
  Hardware UUID, MachineGuid, MAC manzil, foydalanuvchi nomi.
- Oʻrnatilgan dasturlar yoki ishlayotgan jarayonlar roʻyxati.
- Klaviatura bosishlari, ekran tasviri, bufer (clipboard), oyna sarlavhalari.

Ovoz tanish va tarjima modellari qurilmangizda ishlaydi — matningiz
tahlil uchun ham hech qayerga yuborilmaydi.

---

## Nima yigʻiladi (anonim)

Ilovani yaxshilash — qaysi qurilmalarda sekin ishlashini bilish, xatolarni
topish, qaysi platformaga kuch berishni tushunish uchun quyidagi **anonim**
maʼlumot yuboriladi:

| Maʼlumot | Misol | Nima uchun |
|---|---|---|
| Tasodifiy oʻrnatma raqami | 32 belgili hex | «Bu bitta oʻrnatma» deyish uchun |
| Platforma va versiya | `mac` / `win`, `1.1.0` | Eski versiyada qolganlarni bilish |
| Tizim versiyasi | macOS 15.2 / Windows 11 26100 | Qoʻllab-quvvatlash uchun |
| Qurilma turi | arxitektura, protsessor va videokarta modeli, xotira hajmi, yadro soni | Ish unumini tushunish |
| Taxminiy joylashuv | mamlakat / viloyat / shahar | Til va tarqatish uchun |
| Amal oʻlchovlari | ovoz uzunligi, ishlov vaqti, tezlik, GPU/CPU, natija (ok/xato), natija matnining uzunligi (belgilar soni) | Ilova qanchalik tez ishlashini oʻlchash |

**Taxminiy joylashuv** IP manzildan aniqlanadi, lekin **xom IP saqlanmaydi** —
u qisqartiriladi va qaytarib boʻlmaydigan tarzda hashlanadi.

**Amal oʻlchovlari** — har transkripsiya yoki tarjima uchun faqat *qancha
vaqt*, *qanday tez* va natija *necha belgi* ekani yoziladi, *nima* qilingani
(matnning oʻzi) emas.

**Avto-yangilanish** (1.2.0 dan) kuniga bir marta yangi versiya bormi deb
soʻraydi. Bu soʻrovda **hech qanday identifikator yoʻq** — oʻrnatma raqami ham
yuborilmaydi; faqat ilova versiyasi (dastur nomi qatorida, standart). Yangilanish
fayllari imzolangan: ilova faqat Kotib muallifining kaliti bilan imzolangan
versiyani oʻrnatadi.

**Qurilmangizdagi log** (`~/Library/Logs/Kotib.log`, Windows'da
`%LOCALAPPDATA%\Kotib\dictation.log`) hech qayerga yuborilmaydi. 1.2.0 dan
boshlab unda ham diktovka **matni yozilmaydi** — faqat uzunligi va vaqti. Matn faqat
Sozlamalarda «Diagnostika rejimi» yoqilsa yoziladi (muammoni tekshirish uchun),
bu rejim 24 soatdan keyin oʻzi oʻchadi. Log 1 MB dan oshsa eskisi
almashtiriladi.

**Matnga oʻgirilmagan ovoz.** 1.2.0 dan boshlab diktovka matnga oʻgirilmasa
(masalan, model yuklanmagan boʻlsa) gapirganingiz yoʻqolmasligi uchun ovoz
qurilmangizda WAV fayl boʻlib saqlanadi
(`~/Library/Application Support/Kotib/saqlanmagan/`, Windows'da
`%LOCALAPPDATA%\Kotib\saqlanmagan\`). U hech qayerga yuborilmaydi, matnga
oʻgirilishi bilan oʻchiriladi; oʻgirilmasa ham 7 kundan ortiq va oxirgi 20 tadan
koʻp saqlanmaydi. Muvaffaqiyatli diktovka ovozi hech qachon saqlanmaydi.

---

## Bu anonim, shaxsingizga bogʻlanmaydi

Oʻrnatma raqami tasodifiy yasaladi va **qurilma yoki shaxs identifikatori
emas**. Ilovani oʻchirib qayta oʻrnatsangiz — yangisi paydo boʻladi va
eskisi bilan hech qanday aloqasi qolmaydi. Yigʻilgan maʼlumotdan sizning
kimligingizni aniqlab boʻlmaydi.

---

## Rozilik va nazorat

Bu anonim statistika **doim yoqiq** va uni ilova ichida oʻchirish tugmasi
yoʻq. Ilovani oʻrnatib va ishlatib, siz shu maxfiylik siyosati va foydalanish
shartlariga rozilik bildirasiz. Ilova oʻrnatilganda bu hujjatga havola
koʻrsatiladi.

Statistika ilovaning asosiy vazifasiga taʼsir qilmaydi: diktovka, studiya
va tarjima internetsiz ham toʻliq ishlaydi.

## Saqlash muddati

Amal oʻlchovlari **60 kundan** keyin serverda avtomatik oʻchiriladi.
Umumiy oʻrnatma va kunlik faollik jamlanmasi tarixiy grafik uchun saqlanadi.

## Maʼlumot qayerda

Maʼlumot Cloudflare (Worker + D1 bazasi) orqali qayta ishlanadi va faqat
ilova egasiga koʻrinadi. **Uchinchi shaxsga berilmaydi, sotilmaydi.**
Reklama yoki kuzatuv (tracking) uchun ishlatilmaydi.

## Bogʻlanish

Savollar yoki maʼlumotingizni oʻchirish soʻrovi uchun:
muhammadmirqobilov@gmail.com

---

*Kotib mustaqil loyiha. Apple, Microsoft yoki boshqa kompaniya bilan
aloqasi yoʻq.*
