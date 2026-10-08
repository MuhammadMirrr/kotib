// Kotib — anonim foydalanish statistikasi va versiya xabari.
//
// Bitta soʻrov ikki ishni bajaradi: ilova kuniga bir marta «men tirikman»
// deb belgi yuboradi va javobda oxirgi versiya haqida maʼlumot oladi.
// Ikkitasini birlashtirish ataylab: alohida qilinsa ilova kuniga ikki marta
// tarmoqqa chiqardi va foydalanuvchi uchun hech qanday farq boʻlmasdi.
//
// ---------------------------------------------------------------------------
// MAXFIYLIK — bu yerdagi eng muhim qism, kodni oʻzgartirishdan oldin oʻqing.
// Bu chegara maxfiylik siyosati (PRIVACY.md) bilan MOS boʻlishi SHART.
//
// Yigʻiladi:  tasodifiy oʻrnatma ID (ilova oʻzi yasaydi), platforma, ilova
//             versiyasi, OS, qurilma muhiti (arxitektura, CPU/GPU modeli, RAM,
//             yadro soni), taxminiy joylashuv (mamlakat/viloyat/shahar —
//             Cloudflare IP'dan aniqlaydi), va har transkripsiya/tarjima uchun
//             OʻLCHOVLAR: ovoz uzunligi, ishlov vaqti, tezlik, backend, natija.
//
// YIGʻILMAYDI — hech qachon:  ovoz, transkripsiya/tarjima MATNI, fayl nomlari,
//             mikrofon nomi, email, hisob maʼlumotlari, XOM IP manzil.
//
// XOM IP saqlanmaydi. `cf` dan taxminiy joylashuv olinadi; IP'ning oʻzi esa
// kesiladi (IPv4 /24, IPv6 /48) va sir tuz bilan hashlanadi (`ip_hash`) —
// undan xom IP'ni tiklab boʻlmaydi, u faqat «nechta xil tarmoq» degan savolga
// javob beradi. Oʻrnatma ID qurilma yoki shaxs identifikatori emas: ilova
// birinchi ochilganda yasaladi, qayta oʻrnatishda yangisi paydo boʻladi.
//
// Statistika DOIM yoqiq (opt-out yoʻq). Bu — ega qabul qilgan qaror; uning
// yagona huquqiy asosi — foydalanish shartlari va maxfiylik siyosatida
// OCHIQ eʼlon (oʻrnatishda havola koʻrsatiladi). Shu sabab bu izoh va
// PRIVACY.md doim haqiqatni aytishi SHART — aks holda hech qanday himoya
// qolmaydi.
// ---------------------------------------------------------------------------

import { panel } from './panel.js';

// CORS YOʻQ (barqarorlik spec'i, H2). Ilgari `Access-Control-Allow-Origin: *`
// edi — istalgan sayt foydalanuvchi brauzeri orqali soxta ping/amal yuborib
// D1 kvotasini yeyishi mumkin edi. Ilova brauzer emas, sayt esa bu manzilga
// murojaat qilmaydi — CORS hech kimga kerak emas.
const SARLAVHALAR = {
  'Content-Type': 'application/json; charset=utf-8',
};

// POST tanasining chegarasi. Haqiqiy ping ~400 bayt, amal ~200 bayt.
const TANA_CHEGARASI = 2048;

// Oʻrnatma ID uchun qatʼiy shakl: 32 belgili hex. Boshqa har qanday narsa —
// xato yoki suiisteʼmol, bazaga tushmaydi.
const ID_SHAKLI = /^[0-9a-f]{32}$/;

const PLATFORMALAR = new Set(['mac', 'win']);
const AMAL_TURLARI = new Set(['stt', 'tarjima']);
const BACKENDLAR = new Set(['metal', 'vulkan', 'cpu', 'cuda']);

// Versiya raqami: 1.2.3 koʻrinishida, har boʻlagi 3 raqamgacha.
const VERSIYA_SHAKLI = /^\d{1,3}\.\d{1,3}\.\d{1,3}$/;

// Erkin matnli maydonlar (CPU/GPU nomi, shahar) uchun tozalash: uzunlikni
// cheklaymiz va boshqaruv belgilarini olib tashlaymiz. Bu maydonlarga hech
// qachon foydalanuvchi kontenti tushmasligi kerak, lekin ehtiyot uchun.
function matn(x, uzunlik = 80) {
  return String(x || '').replace(/[\x00-\x1f\x7f]/g, '').slice(0, uzunlik) || null;
}

// Musbat butun son yoki null.
function son(x, chek = 100000) {
  const n = Math.floor(Number(x));
  return Number.isFinite(n) && n >= 0 && n <= chek ? n : null;
}

// Musbat kasr son yoki null (ovoz uzunligi, ishlov vaqti — soniyalarda).
function kasr(x, chek = 1000000) {
  const n = Number(x);
  return Number.isFinite(n) && n >= 0 && n <= chek ? Math.round(n * 100) / 100 : null;
}

// IP'ning tarmoq qismi: IPv4 → /24, IPv6 → /48 (toʻliq yozilgan 3 guruh).
//
// Ilgari IPv6 `split(':').slice(0, 3)` bilan kesilardi — qisqartirilgan
// manzilda (`2001:db8::1`) bu boshqa narsa beradi: «2001:db8:» va
// «2001:db8:0:…» bitta /48 boʻlsa ham ikki xil «tarmoq» boʻlib sanalardi
// (spec H4). Endi `::` avval kengaytiriladi.
export function ipTarmogi(ip) {
  if (!ip.includes(':')) return ip.split('.').slice(0, 3).join('.');
  const [bosh, oxir = ''] = ip.toLowerCase().split('::');
  const b = bosh ? bosh.split(':') : [];
  const o = ip.includes('::') && oxir ? oxir.split(':') : [];
  const guruhlar = ip.includes('::')
    ? [...b, ...Array(Math.max(0, 8 - b.length - o.length)).fill('0'), ...o]
    : b;
  return guruhlar.slice(0, 3).map((g) => (parseInt(g, 16) || 0).toString(16)).join(':');
}

// Kesilgan IP'ning sir tuz bilan hashi. Xom IP hech qayerda saqlanmaydi.
// Tuzsiz (muhit.IP_TUZ yoʻq boʻlsa) null qaytaramiz — hash tuzatib
// boʻlmaydigan boʻlib qolmasin.
async function ipHash(soʻrov, muhit) {
  const tuz = muhit.IP_TUZ;
  if (!tuz) return null;
  const ip = soʻrov.headers.get('CF-Connecting-IP') || '';
  if (!ip) return null;
  const tarmoq = ipTarmogi(ip);
  const bayt = new TextEncoder().encode(tuz + '|' + tarmoq);
  const xesh = await crypto.subtle.digest('SHA-256', bayt);
  return [...new Uint8Array(xesh)].slice(0, 8).map((b) => b.toString(16).padStart(2, '0')).join('');
}

function javob(obyekt, holat = 200) {
  return new Response(JSON.stringify(obyekt), { status: holat, headers: SARLAVHALAR });
}

// Bugungi sana UTC boʻyicha, YYYY-MM-DD. Kunlik hisob shu chegara bilan
// yuritiladi — foydalanuvchining mahalliy vaqti emas, aks holda bir odam
// vaqt mintaqasini oʻzgartirib ikki marta sanalardi.
function bugun() {
  return new Date().toISOString().slice(0, 10);
}

// D1 yozuvini javobdan AJRATADI: yozuv `waitUntil` da, xatosi faqat logga.
//
// Ilgari ping D1 ga yozib boʻlgandan keyingina javob berardi — D1 xatosi
// yoki bepul kvotaning tugashi 500 ga aylanardi va ilova bilan birga
// YANGILANISH XABARI ham kelmay qolardi (barqarorlik spec'i, H1). Statistika
// yoʻqolishi mumkin, yangilanish xabari — yoʻq.
function fondaYoz(kontekst, nom, ish) {
  kontekst.waitUntil(
    (async () => {
      try {
        await ish();
      } catch (x) {
        console.error(`${nom}: D1 yozuvi muvaffaqiyatsiz — ${x && x.message}`);
      }
    })()
  );
}

// POST tanasini oʻqiydi: hajm chegarasi (413) va JSON (400). Xato boʻlsa
// tayyor Response qaytaradi, aks holda { tana }.
async function tanaOqi(soʻrov) {
  const eʼlon = Number(soʻrov.headers.get('Content-Length') || 0);
  if (eʼlon > TANA_CHEGARASI) return { xato: javob({ xato: 'tana juda katta' }, 413) };
  const matn = await soʻrov.text();
  if (new TextEncoder().encode(matn).length > TANA_CHEGARASI) {
    return { xato: javob({ xato: 'tana juda katta' }, 413) };
  }
  try {
    return { tana: JSON.parse(matn) };
  } catch {
    return { xato: javob({ xato: 'json emas' }, 400) };
  }
}

// IP boʻyicha soʻrov cheklovi (Workers Rate Limiting). Kalit — IP, u faqat
// Cloudflare'ning hisoblagichida turadi va hech qayerga yozilmaydi.
// Bogʻlanish yoʻq boʻlsa (masalan eski mahalliy sozlama) cheklov qoʻllanmaydi.
async function cheklovdanOtdimi(soʻrov, muhit) {
  if (!muhit.CHEKLOV) return true;
  const kalit = soʻrov.headers.get('CF-Connecting-IP') || 'nomalum';
  const { success } = await muhit.CHEKLOV.limit({ key: kalit });
  return success;
}

async function ping(soʻrov, muhit, kontekst) {
  const { tana, xato: tanaXatosi } = await tanaOqi(soʻrov);
  if (tanaXatosi) return tanaXatosi;

  const id = String(tana.id || '').toLowerCase();
  const platforma = String(tana.platforma || '');
  const versiya = String(tana.versiya || '');
  const os = matn(tana.os, 40);

  if (!ID_SHAKLI.test(id)) return javob({ xato: 'id notoʻgʻri' }, 400);
  if (!PLATFORMALAR.has(platforma)) return javob({ xato: 'platforma notoʻgʻri' }, 400);
  if (!VERSIYA_SHAKLI.test(versiya)) return javob({ xato: 'versiya notoʻgʻri' }, 400);

  // Qurilma muhiti — ilova yuboradi, hammasi tozalanadi.
  const arx    = matn(tana.arx, 12);
  const cpu    = matn(tana.cpu, 80);
  const gpu    = matn(tana.gpu, 80);
  const ram_gb = son(tana.ram_gb, 4096);
  const yadro  = son(tana.yadro, 1024);

  // Taxminiy joylashuv — Cloudflare IP'dan aniqlaydi (xom IP bizga kelmaydi).
  const cf = soʻrov.cf || {};
  const mamlakat = cf.country || 'XX';
  const viloyat = matn(cf.region, 60);
  const shahar = matn(cf.city, 60);
  const ip_hash = await ipHash(soʻrov, muhit);

  const sana = bugun();

  // Ikki yozuv: oʻrnatmaning oʻzi va kunlik jamlanma.
  //
  // `kunlar` faqat sana oʻzgarganda oshadi — shuning uchun ilova kuniga necha
  // marta ping yuborishidan qatʼi nazar, hisob toʻgʻri qoladi. Bu «necha kun
  // ishlatgan» degan savolga javob beradi. Qurilma muhiti har pingda
  // yangilanadi (foydalanuvchi kompyuterini almashtirsa yangisi yoziladi).
  fondaYoz(kontekst, 'ping', () => muhit.DB.batch([
    // Kunlik faol — bir oʻrnatma bir kunda BIR marta (spec H4). Ilgari har
    // ping sanalardi: 20 soatlik oraliq bir sutkaga ikki pingni sigʻdirib,
    // faollarni oshirib koʻrsatardi. Batch ketma-ket bajariladi: bu soʻrov
    // ornatmalar yangilanishidan OLDIN turadi, shuning uchun «bugun allaqachon
    // ping qilganmi» degan savolga hali eski `oxirgi` javob beradi.
    muhit.DB.prepare(
      `INSERT INTO kunlik (sana, platforma, versiya, faol)
       SELECT ?1, ?2, ?3, 1
       WHERE NOT EXISTS (SELECT 1 FROM ornatmalar WHERE id = ?4 AND oxirgi = ?1)
       ON CONFLICT(sana, platforma, versiya) DO UPDATE SET faol = faol + 1`
    ).bind(sana, platforma, versiya, id),

    muhit.DB.prepare(
      `INSERT INTO ornatmalar
         (id, platforma, versiya, os, mamlakat, birinchi, oxirgi, kunlar,
          arx, cpu, gpu, ram_gb, yadro, viloyat, shahar, ip_hash)
       VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?6, 1,
               ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)
       ON CONFLICT(id) DO UPDATE SET
         versiya = ?3,
         os      = ?4,
         mamlakat = ?5,
         kunlar  = kunlar + (CASE WHEN oxirgi <> ?6 THEN 1 ELSE 0 END),
         oxirgi  = ?6,
         arx     = COALESCE(?7,  arx),
         cpu     = COALESCE(?8,  cpu),
         gpu     = COALESCE(?9,  gpu),
         ram_gb  = COALESCE(?10, ram_gb),
         yadro   = COALESCE(?11, yadro),
         viloyat = COALESCE(?12, viloyat),
         shahar  = COALESCE(?13, shahar),
         ip_hash = COALESCE(?14, ip_hash)`
    ).bind(id, platforma, versiya, os, mamlakat, sana,
           arx, cpu, gpu, ram_gb, yadro, viloyat, shahar, ip_hash),
  ]));

  return javob(await versiyaMalumoti(muhit, platforma));
}

// Har transkripsiya/tarjima tugagach yuboriladi. Kontent yoʻq — faqat oʻlchov.
async function amal(soʻrov, muhit, kontekst) {
  const { tana, xato: tanaXatosi } = await tanaOqi(soʻrov);
  if (tanaXatosi) return tanaXatosi;

  const id = String(tana.id || '').toLowerCase();
  const platforma = String(tana.platforma || '');
  const versiya = String(tana.versiya || '');
  const tur = String(tana.tur || '');

  if (!ID_SHAKLI.test(id)) return javob({ xato: 'id notoʻgʻri' }, 400);
  if (!PLATFORMALAR.has(platforma)) return javob({ xato: 'platforma notoʻgʻri' }, 400);
  if (!VERSIYA_SHAKLI.test(versiya)) return javob({ xato: 'versiya notoʻgʻri' }, 400);
  if (!AMAL_TURLARI.has(tur)) return javob({ xato: 'tur notoʻgʻri' }, 400);

  const ovoz_s = kasr(tana.ovoz_s);
  const belgi = son(tana.belgi, 100000000);
  const ishlov_s = kasr(tana.ishlov_s);
  if (ishlov_s === null) return javob({ xato: 'ishlov_s kerak' }, 400);
  const backend = BACKENDLAR.has(String(tana.backend)) ? String(tana.backend) : null;
  // Natija: 'ok' yoki 'xato:<kod>'. Kod faqat harf/raqam/pastki chiziq —
  // xom xato matni yoki fayl yoʻli hech qachon tushmaydi.
  let natija = String(tana.natija || 'ok');
  if (natija !== 'ok') {
    const m = /^xato:([a-z0-9_]{1,40})$/.exec(natija);
    natija = m ? 'xato:' + m[1] : 'xato:nomalum';
  }

  const sana = bugun();
  const vaqt = Math.floor(Date.now() / 1000);

  fondaYoz(kontekst, 'amal', () => muhit.DB.prepare(
    `INSERT INTO amallar
       (id, vaqt, sana, platforma, versiya, tur, ovoz_s, belgi, ishlov_s, backend, natija)
     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)`
  ).bind(id, vaqt, sana, platforma, versiya, tur, ovoz_s, belgi, ishlov_s, backend, natija).run());

  return javob({ ok: true });
}

// Oxirgi versiya maʼlumoti. KV'da saqlanadi — uni yangilash uchun kodni qayta
// joylash shart emas, bitta `wrangler kv key put` yetadi.
//
// 1.1.0 ilovalar uchun bu — «koʻprik»: ularda avto-yangilovchi yoʻq, 1.2.0 ga
// oxirgi marta shu xabar orqali qoʻlda oʻtiladi (avto-yangilanish spec'i).
async function versiyaMalumoti(muhit, platforma) {
  try {
    const xom = await muhit.VERSIYALAR.get('joriy', { type: 'json' });
    if (!xom || !xom[platforma]) return { versiya: null };
    return xom[platforma];
  } catch (x) {
    console.error(`versiya: KV oʻqilmadi — ${x && x.message}`);
    return { versiya: null };
  }
}

// ---------------------------------------------------------------------------
// Avto-yangilanish (1.2+). Qiymatlarni Worker yasamaydi va imzolamaydi —
// ularni `scripts/reliz.sh` imzolab, tekshirib KV'ga yozadi. Shuning uchun
// Worker buzilsa ham soxta yangilanish berib boʻlmaydi: ilova imzoni oʻzi
// tekshiradi (ochiq kalit ilova ichida).
//
//   siyosat-mac, siyosat-win — {"m": base64(manifest), "s": base64(imzo)}
//   appcast-mac              — Sparkle appcast XML (imzolangan feed)
//   …-sinov                  — SINOV KANALI: oʻsha kalit bilan imzolangan, lekin
//                              alohida yozuv. Foydalanuvchilarga taʼsir qilmasdan
//                              haqiqiy server va CDN orqali uchdan-uchga sinash
//                              uchun (Windows: settings.ini → yangilanish.kanal=sinov).
//
// `Cache-Control: max-age=300` — qaytarib olish (kill switch) 5 daqiqada
// hamma joyga yetadi, CDN keshi esa koʻp soʻrovni Worker'ga yetkazmaydi.
// ---------------------------------------------------------------------------

const YANGILANISH_KESHI = 'public, max-age=300';

const JSON_TURI = 'application/json; charset=utf-8';
const XML_TURI = 'application/xml; charset=utf-8';
const YANGILANISH_YOLLARI = {
  '/v1/yangilanish/mac.json': ['siyosat-mac', JSON_TURI],
  '/v1/yangilanish/win.json': ['siyosat-win', JSON_TURI],
  '/v1/appcast/mac.xml': ['appcast-mac', XML_TURI],
  '/v1/yangilanish/mac-sinov.json': ['siyosat-mac-sinov', JSON_TURI],
  '/v1/yangilanish/win-sinov.json': ['siyosat-win-sinov', JSON_TURI],
  '/v1/appcast/mac-sinov.xml': ['appcast-mac-sinov', XML_TURI],
};

async function kvdanBer(muhit, kalit, turi) {
  let qiymat = null;
  try {
    qiymat = await muhit.VERSIYALAR.get(kalit);
  } catch (x) {
    console.error(`${kalit}: KV oʻqilmadi — ${x && x.message}`);
    return new Response(JSON.stringify({ xato: 'vaqtincha mavjud emas' }), {
      status: 503,
      headers: { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' },
    });
  }
  if (qiymat === null) {
    // Hali reliz yoʻq — ilova buni «yangilanish yoʻq» deb tushunadi.
    return new Response(JSON.stringify({ xato: 'topilmadi' }), {
      status: 404,
      headers: { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': YANGILANISH_KESHI },
    });
  }
  return new Response(qiymat, {
    headers: { 'Content-Type': turi, 'Cache-Control': YANGILANISH_KESHI },
  });
}

export default {
  async fetch(soʻrov, muhit, kontekst) {
    const yol = new URL(soʻrov.url).pathname;

    if ((yol === '/v1/ping' || yol === '/v1/amal') && soʻrov.method === 'POST') {
      if (!(await cheklovdanOtdimi(soʻrov, muhit))) {
        return javob({ xato: 'juda koʻp soʻrov' }, 429);
      }
      // ping — kuniga bir marta; amal — har transkripsiya/tarjimadan keyin
      // oʻlchov yozuvi (kontentsiz).
      return yol === '/v1/ping' ? ping(soʻrov, muhit, kontekst) : amal(soʻrov, muhit, kontekst);
    }

    // Avto-yangilanish: imzolangan siyosat manifesti va Sparkle appcast.
    // HEAD ham — GET bilan bir xil sarlavhalar (tana runtime tomonidan olib tashlanadi).
    if (soʻrov.method === 'GET' || soʻrov.method === 'HEAD') {
      const y = YANGILANISH_YOLLARI[yol];
      if (y) return kvdanBer(muhit, y[0], y[1]);
    }

    // Egasi uchun statistika sahifasi — HTTP Basic (panel.js).
    if (yol === '/panel') return panel(soʻrov, muhit);

    return javob({ xato: 'topilmadi' }, 404);
  },

  // Kunlik tozalash (Cron Trigger). `amallar` har amal uchun qator yozadi va
  // tez oʻsadi — 60 kundan eski yozuvlar oʻchiriladi. Jamlangan `ornatmalar`
  // va `kunlik` tegilmaydi: ular kichik va tarixiy grafik uchun kerak.
  async scheduled(hodisa, muhit, kontekst) {
    kontekst.waitUntil(
      muhit.DB.prepare(
        `DELETE FROM amallar WHERE sana < date('now', '-60 days')`
      ).run()
    );
  },
};
