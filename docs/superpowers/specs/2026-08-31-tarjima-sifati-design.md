# Tarjima sifati: doimiy yechim

Sana: 2026-08-31
Holat: dizayn tasdiqlangan, implementatsiya kutilmoqda

## Muammo

Ilova tibbiy maqolani o'zbekchadan ruschaga tarjima qildi. Natijada olti xil
nuqson topildi, ulardan ikkitasi tibbiy jihatdan xavfli:

- `bachadon saqlangan` → `беременность сохранена` (matka emas, homiladorlik)
- `issiq bosishi` → `жар` (приливы o'rniga isitma)
- `t.me/dr_azamoff` → `п . me/dr_azamoff` (havola ishlamaydi)
- `miya o'zgarishlari` → `болезней мозга` (o'zgarish kasallik emas)
- kesimsiz gap, ` ,` kabi tinish belgisi nuqsonlari
- osilgan olmosh: `Olimlar` → `Они`

Talab: vaqtinchalik yamoq emas, doimiy yechim. Har bir foydalanuvchi, dunyoning
istalgan nuqtasida, «Tarjima» tugmasini bosganda to'g'ri natija olishi kerak.
Tezlikni qurbon qilish mumkin.

## O'lchovlar

Hammasi shu mashinada (M5, 16 GB) o'lchandi. Harness: FLORES-200 devtest,
200 jumla, uzn_Latn → rus_Cyrl, chrF++ (sacrebleu, `word_order=2`).

### Sifat

| Konfiguratsiya | chrF++ |
|---|---|
| 1.3B int8, beam 4, `max_decoding_length=256` (hozirgi ilova) | 47,9 |
| 1.3B int8, beam 4, dinamik uzunlik | 47,9 (chiqish bayt-ma-bayt bir xil) |
| 3.3B int8, beam 4, dinamik uzunlik | **49,1** |

chrF++ 40-50 oralig'i — «tushunarli, lekin tahrirsiz e'lon qilib bo'lmaydi».

Korpus balli grammatik tuzatishlarni kam baholaydi: `удалён` → `удалена` ikki
harflik farq, ballga deyarli ta'sir qilmaydi, o'quvchi uchun esa savodli va
savodsiz matn farqi. Shuning uchun raqamdan tashqari real jumlalar sinovi ham
o'tkazildi — pastga qarang.

### Hajm, RAM va tezlik

CTranslate2 protsessorda faqat uch rejimni qo'llaydi: `float32`, `int8`,
`int8_float32`. `float16` va `bfloat16` — GPU uchun; `int16` — ARM'da yo'q.

| Model | Rejim | `model.bin` | Cho'qqi RAM (partiya=1) | Cho'qqi RAM (partiya=16) | Tezlik |
|---|---|---|---|---|---|
| 1.3B | `int8` | 1,38 GB | 1,40 GB | 2,93 GB | 0,75 jumla/s |
| 1.3B | `int8_float32` | 1,38 GB | — | 3,35 GB | — |
| 1.3B | `float32` | ~5,5 GB | — | 5,36 GB | 0,37 jumla/s |
| **3.3B** | **`int8`** | **3,36 GB** | **2,65 GB** | **6,00 GB** | **0,33 jumla/s** |
| 3.3B | `int8_float32` | 3,36 GB | — | 5,62 GB | — |

Partiya hajmi 1 dan 16 ga o'tsa RAM 2,65 → 6,00 GB ga sakraydi. Ya'ni
«tezlashtiramiz, jumlalarni to'p-to'p beramiz» degan optimizatsiya 3.3B bilan
**taqiqlangan** — u 8 GB mashinani swap'ga tushiradi. Partiya 1 da qoladi.

### Real jumlalar sinovi

Muammoli maqoladan sakkiz jumla. 3.3B sakkiztadan beshtasini tuzatdi:

| Manba nuqson | 1.3B | 3.3B |
|---|---|---|
| osilgan olmosh | `Они проанализировали` | `Учёные проанализировали` |
| superlativ | `крупнейших` | `крупных` |
| foiz konstruksiyasi | `35% меньше вероятность` | `на 35% меньше` |
| modallik | `не только облегчает… но и может` | `может не только облегчить… но и сыграть` |
| rod kelishigi | `он назначается`, `удалён матка` | `Она применяется`, `удалили матку` |
| leksik tanlov | `шагом`, `после менопаузы` | `направлением`, `в период менопаузы` |

Tuzalmagani: `issiq bosishi` → `перегрев` (hamon xato), `bachadon saqlangan`
→ `сохранившимся в матке` (hamon xato).

## Rad etilgan yondashuvlar

Bu bo'lim kelajakda «nega qilmadik?» degan savol chiqmasligi uchun. Uchala
g'oya ham o'lchab ko'rildi va qulaydi.

### 1. Teskari tarjima bilan qayta saralash — RAD ETILDI

G'oya: N variant chiqarib, har birini orqaga tarjima qilib, manbaga eng
yaqinini tanlash. Narxi ~6x vaqt.

O'lchov natijasi — signal **teskari** ishladi:

| Jumla | Holati | Model balli | Teskari chrF |
|---|---|---|---|
| `перегрев` (xato) | ❌ | −0,277 (eng yuqori ishonch) | 65,2 |
| `сохранившимся в матке` (xato) | ❌ | −0,395 | 64,8 |
| `на 35% меньше` (to'g'ri) | ✅ | −0,425 | 32,2 (eng past) |

Sababi: chrF — sirt o'lchovi. Xato 25 so'zdan bittasida, teskari tarjimada
`сохранившимся в матке` → `bachadonda saqlangan` bo'lib qaytadi, manba
`Bachadon saqlangan` bilan harflar darajasida deyarli bir xil. **Bir so'zlik
ma'no xatosi uzun jumla ichida eriydi.**

### 2. Variantlar kelishmovchiligi — RAD ETILDI

G'oya: 5 variantdan biri boshqasidan farq qilsa, bu shubha belgisi.

O'lchov: `issiq bosishi` uchun beshala variant ham `перегрев` dedi; farq faqat
`при менопаузе` / `менопаузы` kabi bezakda. `Bachadon saqlangan` uchun beshalasi
ham xato (`в матке`, `в утробе`). To'g'ri variant ro'yxatda umuman yo'q.

Beam search ma'no muqobillarini emas, sirt bezaklarini chiqaradi. Modelning
bitta noto'g'ri e'tiqodi bor va beshala nur o'sha e'tiqodni baham ko'radi.
Xato tasodifiy emas — tizimli.

### 3. Uzun jumlani ichki bo'laklarga bo'lish — RAD ETILDI

G'oya: model uzun gapda mazmun tashlaydi, demak qisqartirsak yaxshilanadi.

O'lchov: qisqartirilgan jumla natijani **yomonlashtirdi**.

- `Menopauzada issiq bosishi ko'p uchraydigan belgi.` → `Горячее давление`
- `Bachadon saqlangan ayollarga…` → `Женщинам, находящимся в матке`

### Xulosa

Model `issiq bosishi = приливы` ekanini bilmaydi. Bilmagan so'zni hech qanday
nur kengligi, teskari tarjima yoki qayta saralash tiklab bera olmaydi.
**Terminologiya xatosini offline dekodlash bilan hal qilib bo'lmaydi.**

Shuning uchun tekshiruv qatlami qurilmaydi: u hisoblash vaqtini yeydi va bu
xatoni ushlamaydi.

## Qaror

**3.3B int8, to'g'ridan-to'g'ri tarjima, bitta o'tishda, beam 4.** Ustiga
faqat deterministik tuzatishlar. Hech qanday qayta saralash, N-variant yoki
teskari tarjima yo'q.

Xatolar uch sinfga bo'linadi va ularning yechimi butunlay har xil:

| Sinf | Misol | Yechim | Ishonchlilik |
|---|---|---|---|
| Pipeline | `t.me` → `п . me`, ` ,`, kesilish | Deterministik kod | 100% |
| Grammatika / tushib qolish | `удалён матка`, `Они` | 3.3B modeli | 8 dan 5 tasi |
| Terminologiya | `issiq bosishi`, `bachadon` | Offline yechim yo'q | — |

## Komponentlar

### 1. Model almashtirish

Arxiv: `model.bin` + `config.json` + `shared_vocabulary.json`
(`OpenNMT/nllb-200-3.3B-ct2-int8` dan) + `sentencepiece.bpe.model` (hozirgi
1.3B papkasidan — sha256 `14bb8dfb35c0…` aynan mos, tekshirilgan).

CDN kaliti: `dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz`. `v1` prefiksi
`Cache-Control: immutable` — hech qachon ustiga yozilmaydi. Yuklash AGENTS.md
dagi multipart Worker usuli bilan (300 MiB dan katta obyekt uchun boshqa yo'l
yo'q).

Kod o'zgarishlari:

| Fayl | O'zgarish |
|---|---|
| `tarjima_yuklovchi.swift:23` | URL → `v2/nllb-200-3.3B-int8.tar.gz` |
| `tarjima_yuklovchi.swift:208` | «Model ~1,3 GB» → «~3,4 GB» |
| `tarjima_model.swift` | `taxminiyBayt` → arxivning aniq hajmi |
| `yollar.swift` | Papka nomi `tarjima-model` → `tarjima-model-33b` |

Eski foydalanuvchilar: hozir `TarjimaModel.tayyor` eski 1.3B papkasini ham
«tayyor» deydi, ya'ni yangilanish hech qachon boshlanmaydi. Papka nomi
o'zgargani buni hal qiladi. Eski papka topilsa **o'chiriladi** — foydalanuvchi
diskida 1,38 GB bo'shaydi. Bu `yollar.swift` ning mavjud migratsiya naqshiga
mos (`Audio-Matnga → Kotib` ko'chirishi kabi).

Yangi tekshiruv: yuklashdan **oldin** disk joyi. Vaqtincha ~7 GB kerak
(3,2 GB arxiv + 3,4 GB ochilgan nusxa; ochilgandan keyin arxiv o'chiriladi).
Hozir bunday tekshiruv yo'q — to'la diskda yuklash 3 GB dan keyin quladi va
foydalanuvchi sababini bilmaydi.

### 2. Segmentatsiya — `matn_boluvchi.swift`

Hozirgi `jumlalargaBol` har qanday `.` dan keyin kesadi. Shu sabab
`t.me/dr_azamoff` → `["t.", "me/dr_azamoff"]` bo'lib, `t.` alohida «jumla»
sifatida modelga ketdi va `п.` bo'lib qaytdi. Bir xil nuqson `3.14`, `v1.0`,
`PhD.` va har qanday URL'ni buzadi.

Yangi: ICU jumla chegaralari —
`String.enumerateSubstrings(in:options:.bySentences)`. Foundation'ning bir
qismi, ya'ni `test.sh` uni qamrab oladi. 202 tilga baravar ishlaydi: xitoy
`。`, arab `؟`, tay probelsiz matni — hammasi. Qisqartma va URL ichidagi
nuqtani kesmaydi.

Qo'shimcha: **himoyalangan bo'laklar**. URL, email, `@handle`, `#hashtag`,
fayl yo'li — `Bolak.xom` bo'ladi, modelga umuman berilmaydi va o'z joyiga
qaytariladi. Bu til-betaraf: hech qanday ruscha yoki o'zbekcha qoida yo'q.

### 3. Dekodlash — `tarjima_bridge.cpp`

`max_decoding_length` hozir doimiy 256. Uzun jumla **jimgina kesiladi** —
foydalanuvchi na xato, na ogohlantirish ko'radi. FLORES bu bug'ni ko'rmaydi
(uning jumlalari qisqa, ~50 token), shuning uchun u shu paytgacha sezilmagan.

Yangi: `max_decoding_length = manba_token_soni * 2 + 30`.

Beam 4 da qoladi (AGENTS.md dagi sifat o'lchovlari shu qiymatda olingan).
Partiya 1 da qoladi (RAM o'lchovi majbur qildi).

### 4. Normalizatsiya — chiqishda

Yozuv (script) bo'yicha tinish belgilari jadvali:

- ` ,` → `,`, ` .` → `.`, ` :` → `:`, ` )` → `)` — barcha yozuvlar uchun
- `bolakYasa` `—` ni `-` ga almashtiradi (model `—` ni bilmaydi); chiqishda
  uzun tire tiklanadi
- CJK: so'zlar orasiga probel qo'yilmaydi
- Fransuz: `!?:;` oldidan tor probel

Til-betaraf: qoidalar maqsad tilning **yozuviga** bog'lanadi, tilning o'ziga
emas.

### 5. Doimiy o'lchov harness'i — `scripts/eval/`

Bu eng muhim komponent. «Doimiy yechim» degani — o'zgarish sifatni
tushirganda buni **bilib turish**.

- FLORES-200 devtest korpusi + real uzun matn korpusi. Ikkinchisi shart:
  FLORES jumlalari qisqa va `max_decoding_length` bug'ini ko'rmaydi. O'lchov
  ko'rmaydigan narsani tuzatib, o'lchov ko'rsatmaydigan natijaga erishish —
  aynan shu tuzoqqa tushmaslik kerak.
- Harness ilova pipeline'ini **takrorlashi** shart: matnni avval
  `MatnBoluvchi` mantiqi bilan jumlalarga bo'lib, keyin modelga berish. Bu
  spec uchun o'tkazilgan o'lchovlar FLORES qatorini butunlay modelga berdi —
  ya'ni ilovadan qattiqroq shart. Haqiqiy sifat 47,9 dan biroz yuqori.
- Natijalar repoda saqlanadi, regressiya ko'rinsin.

## Testlar

`test.sh` ning `UNDER_TEST` ro'yxatiga tushadigan sof mantiq (AppKit,
AVFoundation, whisper, Keychain, fayl tizimi import qilmaydigan):

- `matn_boluvchi.swift` — ICU segmentatsiyasi: `t.me/dr_azamoff`, `3.14`,
  `v1.0`, `PhD.`, email, `@handle` bir butun qolishi; xitoy `。`, arab `؟`
  chegaralari; himoyalangan bo'laklar o'z joyiga qaytishi
- normalizatsiya: ` ,` → `,`, uzun tire tiklash, CJK probellari
- `tarjima_model.swift` — yangi papka nomi bilan to'liqlik tekshiruvi
- `yollar.swift` — eski `tarjima-model` papkasining o'chirilishi

AppKit'ga bog'liq qismlar (`tarjima_yuklovchi.swift`, `tarjimon.swift`) —
build va qo'lda sinov, AGENTS.md dagi qoida bo'yicha.

## Ochiq masalalar

**Zaif Windows mashinalarida tezlik.** M5 da 3.3B 0,33 jumla/s beradi, ya'ni
A4 sahifa ~2 daqiqa. Install base'ning ~76% zaif Windows mashinalari; ularda
bu 6-10 daqiqagacha cho'zilishi mumkin. O'lchanmagan. Agar chidab bo'lmas
bo'lsa, zaxira variant — «zaif mashina uchun 1.3B» tanlovi.

**GPU yo'li (Windows, CUDA).** Hozirgi qurilma `tarjima_bridge.cpp` da
`Device::CPU` bilan qat'iy belgilangan. CTranslate2 CUDA'ni qo'llaydi va 3.3B
int8 Ampere kartalarida ishlaydi. RTX 3060 Laptop (6 GB, ~336 GB/s) uchun
hisob-kitob (o'lchanmagan): bitta jumla — ~1,5-2,5 jumla/s, partiya 8-16 bilan
— ~10-15 jumla/s, ya'ni A4 sahifa ~3-4 soniya. M5 protsessoriga nisbatan
6x dan 40x gacha. Narxi: Windows uchun CUDA build, CUDA/cuDNN kutubxonalari
(~1-2 GB), drayver mosligi muammolari va CPU'ga qaytish yo'lini saqlash.

**4 GB RAM li mashinalar.** 3.3B int8 ~2,7 GB cho'qqi RAM talab qiladi. 8 GB
da ishlaydi, 4 GB da ishlamaydi. Bunday foydalanuvchiga aniq xabar kerak.

**Terminologiya lug'ati.** Hozir qilinmaydi — hozirgi ishni kechiktirmasin.
Keyinroq ko'riladi: sozlamalarda foydalanuvchi tahrirlaydigan `manba → maqsad`
jadvali, deterministik qo'llanadi. Cheklovi: rus tilida kelishik
(`приливы` / `приливов`) — sodda holatlar ishlaydi, murakkabi yo'q.
