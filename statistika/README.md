# Kotib — statistika va yangilanish xizmati

Cloudflare Worker. Ikkita ish qiladi:

1. **Yangilanish xabari** — ilova oxirgi versiyani soʻraydi, yangisi boʻlsa
   foydalanuvchiga «Yangi versiya chiqdi» chizigʻi koʻrsatiladi.
2. **Anonim statistika** — nechta oʻrnatma bor, ular qancha kun ishlatilgan.

Manzil: `https://stat.mirqobilov.com`

---

## Maxfiylik

Bu bandni oʻzgartirishdan oldin oʻylang: u huquqiy talab ham, foydalanuvchi
bilan tuzilgan kelishuv ham.

**Yigʻiladi:**

| Nima | Nima uchun |
|---|---|
| Tasodifiy oʻrnatma ID (32 belgi hex) | «Bu bitta oʻrnatma» deyish uchun |
| Platforma (`mac` / `win`) | Qaysi tomonga kuch sarflashni bilish uchun |
| Ilova versiyasi | Eski versiyada qolganlar qanchaligini bilish uchun |
| OS versiyasi | Qaysi Windows'ni qoʻllab-quvvatlash kerakligini bilish uchun |
| Mamlakat (Cloudflare beradi) | Til va tarqatish uchun |

**Yigʻilmaydi — hech qachon:** ovoz, matn, diktovka tarixi, fayl nomlari,
mikrofon nomi, email, hisob maʼlumotlari, IP manzil.

Oʻrnatma ID **qurilma identifikatori emas**: u ilova birinchi ochilganda
tasodifiy yasaladi va hech qanday shaxs bilan bogʻlanmaydi. Ilova oʻchirib
qayta oʻrnatilsa — yangisi paydo boʻladi va eskisi bilan aloqasi qolmaydi.

Foydalanuvchi statistikani Sozlamalardan **butunlay oʻchira oladi**. Oʻshanda
ilova `/v1/ping` ga umuman murojaat qilmaydi — faqat `/v1/versiya` ni
soʻraydi, unda hech qanday maʼlumot yuborilmaydi. Versiya tekshiruvi
ataylab oʻchirilmaydi: xavfsizlik tuzatishi chiqqanini bilish
foydalanuvchining oʻz manfaatida.

---

## Manzillar

| Yoʻl | Kim chaqiradi |
|---|---|
| `POST /v1/ping` | Ilova, kuniga bir marta. Javobda oxirgi versiya keladi (1.1.0 uchun «koʻprik»). D1 yiqilsa ham javob beradi — yozuv `waitUntil` da. |
| `POST /v1/amal` | Har transkripsiya/tarjimadan keyin oʻlchov (kontentsiz). |
| `GET /v1/yangilanish/{mac,win}.json` | 1.2+ avto-yangilovchi. KV `siyosat-<p>` — imzolangan `{"m","s"}`, aynan baytlari bilan. Yoʻq boʻlsa 404. |
| `GET /v1/appcast/mac.xml` | Sparkle (1.2+). KV `appcast-mac`. |
| `GET /panel` | Faqat egasi. Brauzer login oynasi chiqadi: **parol** maydoniga `PANEL_KALIT` (foydalanuvchi nomi istalgan). Kalit URL'da qabul qilinmaydi. |

Avto-yangilanish javoblari `Cache-Control: public, max-age=300` bilan —
qaytarib olish (`reliz.sh qaytar`) 5 daqiqada hamma joyga yetadi. Worker
ularni imzolamaydi va oʻzgartirmaydi: imzoni `scripts/reliz.sh` qoʻyadi,
ilova tekshiradi. Worker buzilsa ham soxta yangilanish berib boʻlmaydi.

POST'larda CORS yoʻq (sayt bu manzilga murojaat qilmaydi), tana ≤ 2 KB
(aks holda 413), bitta IP'dan daqiqasiga 60 tagacha (`CHEKLOV`, aks holda 429).
Kunlik faol — bir oʻrnatma bir kunda bir marta sanaladi.

---

## Testlar

```bash
cd statistika && npm install && npm test
```

Worker Workers runtime'ining oʻzida (Miniflare, `@cloudflare/vitest-plugin`)
sinaladi; D1 va KV har test uchun mahalliy, jonli bazaga tegmaydi.
D1/KV yiqilgan holatlar ham sinaladi.

---

## Avto-yangilanish siyosatini chiqarish (1.2+)

```bash
./scripts/reliz.sh kv-yoz manifest.json --foiz 10   # imzolaydi, tekshiradi, KV'ga yozadi
./scripts/reliz.sh kv-yoz manifest.json --foiz 100  # 24 soat kuzatuvdan keyin
./scripts/reliz.sh qaytar win 1.2.0                 # nosoz reliz — oldingisiga qaytarish
```

Har buyruq `--quruq` (hech narsa yozmaydi) va `--mahalliy` (`wrangler dev`
ning KV'si) bilan ham ishlaydi. Tekshiruv ilova bilan **aynan bir xil kod**
orqali (`scripts/manifest-tekshir` → `src/yangilanish_siyosat.swift`):
tekshiruvdan oʻtmagan javob KV'ga yozilmaydi.

---

## Yangi reliz chiqqanda

1.2 dan boshlab ilovalar yangilanishni **imzolangan siyosat** orqali oladi
(`scripts/reliz.sh mac|win`, yuqoriga qarang). KV'dagi `joriy` (`versiya.json`)
endi faqat **1.1.0 lar uchun koʻprik**: ularda avto-yangilovchi yoʻq va 1.2.0 ga
oxirgi marta ping javobidagi banner orqali qoʻlda oʻtiladi.

**1.2.0 relizida (S24)** — `versiya.json` ni `joriy` ga yozing:
undagi `url` sayt emas, **toʻgʻridan-toʻgʻri** oʻrnatuvchi (`dl/mac/…pkg`,
`dl/win/…-setup.exe` — avval CDN'da borligini tekshiring), izoh esa «bu oxirgi
qoʻlda yangilash» (1.2.0 relizida shu koʻrinishga keltirilgan; repo nusxasi
KV bilan bir xil turishi kerak). 1.1.0 ilovalari bu manzilni tekshirmasdan ochadi —
shuning uchun u faqat bizning CDN'imiz boʻlishi shart.

`versiya.json` ni tahrirlang, tekshiring va KV'ga yuklang. Kodni qayta
joylash SHART EMAS:

```bash
../scripts/versiya-tekshir.sh --reliz
npx wrangler kv key put --namespace-id=8570442129a44d89ab486d90b4c471fd \
    joriy --path=versiya.json --remote
```

Bir necha daqiqada barcha ilovalar yangi raqamni koʻradi.

⚠️ Ilovaning oʻz versiyasi repo ildizidagi `VERSION` faylida (Info.plist va
`app.rc` undan yigʻiladi). `versiya.json` dagi raqam undan **yangi** boʻlsa,
ilova oʻziga oʻzi yangilanish taklif qiladi — `versiya-tekshir.sh` buni
ushlaydi; `--reliz` bilan esa eski qolgan joylarni ham koʻrsatadi.

---

## Statistikani koʻrish

Brauzerda: `https://stat.mirqobilov.com/panel?kalit=<kalit>`

Sahifa `src/panel.js` da tayyorlanadi. Besh boʻlim: **Umumiy holat** (kartalar
va 30 kunlik grafiklar), **Joylashuv** (mamlakat bayrogʻi + oʻzbekcha nomi,
shaharlar), **Platforma va versiya** (yangilanish qamrovi KV'dagi `joriy` bilan
solishtiriladi), **Amallar** va **Qurilma muhiti**.

Sahifada yozuv ataylab kam: taʼriflar matnga emas, kartaning `title` maslahat
oynasiga yoziladi. JavaScript ham, tashqi soʻrov ham yoʻq — grafiklar ichma-ich
SVG, sichqoncha maslahatlari `title` orqali.

Barcha soʻrovlar bitta `db.batch([...])` bilan ketadi: Worker'da har D1 soʻrovi
alohida subrequest sanaladi, batch esa bittaga aylantiradi.

Panelni mahalliy sinash uchun tayyor buyruq yoʻq, lekin `node:sqlite` bilan
`schema.sql` dan xotirada baza yasab, `panel()` ni soxta `muhit` bilan chaqirish
yetadi — Worker API'sidan faqat `DB.batch`, `DB.prepare().bind()` va
`VERSIYALAR.get()` ishlatiladi.

Kalit Cloudflare secret'ida (`PANEL_KALIT`) va repoda **yoʻq**. Yoʻqolsa
yangisini qoʻying:

```bash
openssl rand -hex 24 | npx wrangler secret put PANEL_KALIT
```

Toʻgʻridan-toʻgʻri soʻrov ham mumkin:

```bash
npx wrangler d1 execute kotib-statistika --remote \
  --command="SELECT platforma, COUNT(*) FROM ornatmalar GROUP BY platforma"
```

---

## Birinchi oʻrnatish (bir marta bajarilgan)

```bash
npx wrangler d1 create kotib-statistika
npx wrangler d1 execute kotib-statistika --remote --file=schema.sql
npx wrangler kv namespace create VERSIYALAR
openssl rand -hex 24 | npx wrangler secret put PANEL_KALIT
npx wrangler deploy
```

`wrangler.toml` dagi ID'lar shu buyruqlardan olingan. Ular maxfiy emas —
hisobga kirish huquqisiz hech narsa bermaydi.

---

## Narxi

Cloudflare'ning bepul chegarasi: kuniga 100 000 soʻrov, D1 da 5 GB.
Har bir oʻrnatma kuniga bitta soʻrov yuboradi, yaʼni bepul chegara
**100 000 kunlik faol foydalanuvchi** gacha yetadi. Hozirgi hajm — bir necha
yuz.
