// Statistika paneli — faqat egasi uchun.
//
// Himoya: `PANEL_KALIT` maxfiy qiymati (wrangler secret), HTTP Basic orqali —
// brauzer login oynasining PAROL maydoniga kiritiladi (foydalanuvchi nomi
// istalgan). Kalitsiz yoki notoʻgʻri kalit bilan 401 (`kirishTogrimi` ga qarang).
//
// Ranglar `web/index.html` dagi palitradan olingan, shunda panel ham saytning
// bir qismidek koʻrinadi. Sarlavhalar serif — saytdagidek.
//
// Tuzilishi: bitta `db.batch([...])` bilan hamma soʻrov BIR marta yuboriladi
// (Worker'da har D1 soʻrovi alohida subrequest sanaladi — batch bittaga
// aylantiradi). Keyin natija HTML'ga aylanadi; JavaScript yoʻq, tashqi
// soʻrov yoʻq — sahifa oʻzi bilan toʻliq.

// ───────────────────────────────────────────────────────────────── uslub

const USLUB = `
  :root {
    --bg:#F0EEE6; --surface:#FAF9F5; --surface2:#F5F2EA;
    --ink:#191917; --ink-soft:#4A4842; --ink-mute:#63605A;
    --line:#D6D1C2; --line-soft:#E3DED0;
    --accent:#A94E2E; --accent2:#4A6B8A; --ok:#2F6B41; --xato:#A32B2B;
    --v-apple:#5A5750; --v-nvidia:#4E7F35; --v-amd:#B4462F;
    --v-intel:#3D6E9E; --v-boshqa:#8A8478;
    --serif:"Iowan Old Style","Palatino Linotype",Palatino,Georgia,"Times New Roman",serif;
    --sans:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
    --mono:"SF Mono","Cascadia Mono","Segoe UI Mono",Consolas,monospace;
    --soya:0 1px 2px rgba(25,25,23,.04), 0 6px 18px rgba(25,25,23,.05);
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --bg:#16150F; --surface:#201E19; --surface2:#282620;
      --ink:#F2F0EA; --ink-soft:#CFCAC0; --ink-mute:#9C968A;
      --line:#37342C; --line-soft:#2C2A23;
      --accent:#D9805B; --accent2:#84A9C7; --ok:#6FBE88; --xato:#DE7A72;
      --v-apple:#9A948A; --v-nvidia:#7EAF60; --v-amd:#D9724F;
      --v-intel:#6E9CC7; --v-boshqa:#7A756B;
      --soya:0 1px 2px rgba(0,0,0,.25), 0 8px 24px rgba(0,0,0,.22);
    }
  }

  * { margin:0; padding:0; box-sizing:border-box; }
  html { scroll-behavior:smooth; -webkit-text-size-adjust:100%; }
  body { background:var(--bg); color:var(--ink); font:15px/1.55 var(--sans);
         -webkit-font-smoothing:antialiased; padding-bottom:80px; }
  a { color:var(--accent); text-decoration:none; }
  a:hover { text-decoration:underline; }

  /* ───────────────────────────────────────────────────────────── yon panel */
  .sahn { display:flex; align-items:flex-start; }
  .yon { position:sticky; top:0; z-index:20; flex:none; width:218px; height:100vh;
         overflow-y:auto; display:flex; flex-direction:column; gap:16px;
         padding:20px 14px 18px; border-right:1px solid var(--line);
         background:color-mix(in srgb, var(--surface) 50%, var(--bg)); }
  .asos { flex:1; min-width:0; }
  .ichi { max-width:1000px; margin:0 auto; padding:0 28px 80px; }

  .belgi { font-family:var(--serif); font-size:16.5px; font-weight:500; letter-spacing:-.01em;
           display:flex; align-items:center; gap:8px; white-space:nowrap; padding:0 8px; }
  .belgi s { width:8px; height:8px; border-radius:50%; background:var(--ok); flex:none;
             text-decoration:none;
             box-shadow:0 0 0 3px color-mix(in srgb, var(--ok) 22%, transparent); }

  .yol { display:flex; flex-direction:column; gap:2px; min-width:0; }
  .yol a { position:relative; display:flex; align-items:center; justify-content:space-between;
           gap:10px; font-size:13.5px; color:var(--ink-soft); padding:7px 11px;
           border-radius:9px; }
  .yol a:hover { background:var(--surface); color:var(--ink); text-decoration:none; }
  .yol a.joriy { background:var(--surface); color:var(--ink); font-weight:500; }
  .yol a.joriy::before { content:""; position:absolute; left:-14px; top:50%; width:3px;
                         height:17px; margin-top:-8.5px; border-radius:0 3px 3px 0;
                         background:var(--accent); }
  .yol em { font-style:normal; font-size:12px; color:var(--ink-mute);
            font-variant-numeric:tabular-nums; }
  .vaqt { margin-top:auto; padding:0 11px; font-size:11.5px; line-height:1.6;
          color:var(--ink-mute); font-variant-numeric:tabular-nums; }

  /* ─────────────────────────────────────────────────────────────── boʻlim */
  section { padding-top:30px; scroll-margin-top:64px; }
  .bolim-bosh { display:flex; align-items:baseline; gap:12px; flex-wrap:wrap;
                margin:0 0 2px; }
  h2 { font-family:var(--serif); font-size:27px; font-weight:500; letter-spacing:-.015em; }
  .bolim-bosh .raqam { font-size:13px; color:var(--ink-mute);
                       font-variant-numeric:tabular-nums; }
  h3 { font-size:13px; font-weight:600; letter-spacing:.04em; text-transform:uppercase;
       color:var(--ink-mute); margin:22px 0 9px; }
  h3:first-of-type { margin-top:16px; }
  h3 span { text-transform:none; letter-spacing:0; font-weight:400; }

  /* ────────────────────────────────────────────────────────────── kartalar */
  .setka { display:grid; gap:12px; }
  .setka.k4 { grid-template-columns:repeat(4,1fr); }
  .setka.k6 { grid-template-columns:repeat(6,1fr); }
  .setka.k2 { grid-template-columns:repeat(2,1fr); }
  .quti { background:var(--surface); border:1px solid var(--line); border-radius:14px;
          box-shadow:var(--soya); }
  .quti + .quti { margin-top:10px; }
  .setka .quti + .quti { margin-top:0; }
  .karta { padding:15px 16px 14px; display:flex; flex-direction:column; gap:2px;
           min-height:100%; }
  .karta .son { font-size:31px; font-weight:600; line-height:1.05; letter-spacing:-.02em;
                font-variant-numeric:tabular-nums; }
  .karta .son u { text-decoration:none; font-size:.58em; font-weight:500; color:var(--ink-mute);
                  margin-left:2px; }
  .karta .nom { color:var(--ink-soft); font-size:13px; margin-top:3px; }
  .karta .ost { color:var(--ink-mute); font-size:12px; margin-top:auto; padding-top:8px; }
  .karta.kichik .son { font-size:23px; }
  .karta.kichik .nom { font-size:12px; }
  .karta .uchqun { margin-top:9px; height:22px; }

  /* ──────────────────────────────────────────── ulush chizigʻi (bir qator) */
  .ulush { display:flex; height:12px; border-radius:999px; overflow:hidden;
           background:var(--surface2); border:1px solid var(--line); }
  .ulush i { display:block; }
  .afsona { display:flex; gap:16px; flex-wrap:wrap; margin-top:10px; font-size:13px;
            color:var(--ink-soft); }
  .afsona span { display:flex; align-items:center; gap:7px; }
  .afsona s { width:9px; height:9px; border-radius:3px; text-decoration:none; flex:none; }
  .afsona b { font-variant-numeric:tabular-nums; }
  .afsona em { color:var(--ink-mute); font-style:normal; }

  /* ─────────────────────────────────────────────────── roʻyxat + chiziqcha */
  .royxat { list-style:none; }
  .royxat li { display:grid; grid-template-columns:auto 1fr 34% auto; align-items:center;
               gap:12px; padding:9px 16px; border-bottom:1px solid var(--line-soft); }
  .royxat li:last-child { border-bottom:none; }
  .royxat li:hover { background:var(--surface2); }
  .royxat .oldi { font-size:19px; line-height:1; width:26px; text-align:center;
                  font-variant-emoji:emoji; }
  .royxat .matn { min-width:0; }
  .royxat .matn b { font-weight:500; display:block; overflow:hidden; text-overflow:ellipsis;
                    white-space:nowrap; }
  .royxat .matn i { font-style:normal; color:var(--ink-mute); font-size:12px; display:block;
                    overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
  .royxat .tasma { height:7px; border-radius:999px; background:var(--surface2); overflow:hidden; }
  .royxat .tasma i { display:block; height:100%; border-radius:999px; background:var(--accent);
                     min-width:3px; }
  .royxat .qiymat { text-align:right; font-variant-numeric:tabular-nums; white-space:nowrap; }
  .royxat .qiymat b { font-weight:600; }
  .royxat .qiymat i { font-style:normal; color:var(--ink-mute); font-size:12px; display:block; }

  /* ───────────────────────────────────────────────────────────── jadvallar */
  .jadval-oram { overflow-x:auto; }
  table { width:100%; border-collapse:collapse; }
  th, td { padding:10px 16px; text-align:left; font-size:14px;
           border-bottom:1px solid var(--line-soft); white-space:nowrap; }
  th { color:var(--ink-mute); font-weight:500; font-size:11.5px; letter-spacing:.05em;
       text-transform:uppercase; background:var(--surface2); }
  tr:last-child td { border-bottom:none; }
  td.r { text-align:right; font-variant-numeric:tabular-nums; }
  td.asos { font-weight:500; }
  tbody tr:hover td { background:var(--surface2); }

  .yaxshi { color:var(--ok); }
  .yomon { color:var(--xato); }

  /* ───────────────────────────────────────────────────────────── grafiklar */
  .ch { display:block; width:100%; height:auto; overflow:visible; }
  .ch .tor { stroke:var(--line); stroke-width:1; stroke-dasharray:2 4; fill:none; }
  .ch .tayanch { stroke:var(--line); stroke-width:1; fill:none; }
  .ch text { fill:var(--ink-mute); font-family:var(--sans); font-size:9.5px; }
  .ch .maydon { fill:url(#kotib-tus); }
  .ch .chiziq { fill:none; stroke:var(--accent); stroke-width:1.8;
                stroke-linejoin:round; stroke-linecap:round; }
  .ch .ustun { fill:var(--accent); }
  .ch .ustun.ik { fill:var(--accent2); }
  .ch .sezgir { fill:transparent; }
  .ch .sezgir:hover { fill:color-mix(in srgb, var(--ink) 7%, transparent); }
  .ch .bugun { stroke:var(--accent); stroke-width:1; stroke-dasharray:2 3; opacity:.5; }
  #kotib-tus stop.a { stop-color:var(--accent); stop-opacity:.30; }
  #kotib-tus stop.b { stop-color:var(--accent); stop-opacity:.02; }
  .uchqun path { fill:none; stroke:var(--accent); stroke-width:1.6; stroke-linejoin:round;
                 stroke-linecap:round; opacity:.75; }
  .uchqun .tag { fill:var(--accent); opacity:.13; stroke:none; }

  /* ───────────────────────────────────────────────────────────────── boshqa */
  .pastki { margin-top:38px; padding-top:20px; border-top:1px solid var(--line);
            color:var(--ink-mute); font-size:12.5px; }
  .yoq { padding:22px 18px; text-align:center; color:var(--ink-mute); font-size:13.5px;
         border:1px dashed var(--line); border-radius:14px; background:transparent; }
  .yoq b { display:block; color:var(--ink-soft); font-weight:500; margin-bottom:3px;
           font-size:14.5px; }

  @media (max-width:900px) {
    .setka.k6 { grid-template-columns:repeat(3,1fr); }
    .setka.k4 { grid-template-columns:repeat(2,1fr); }
    /* Tor ekranda yon panel yuqoridagi bitta qatorga aylanadi. */
    .sahn { display:block; }
    .yon { flex-direction:row; align-items:center; gap:12px; width:auto; height:auto;
           overflow:visible; padding:9px 16px; border-right:none;
           border-bottom:1px solid var(--line);
           background:color-mix(in srgb, var(--bg) 88%, transparent);
           backdrop-filter:saturate(1.4) blur(12px); }
    .belgi { padding:0; font-size:15.5px; }
    .yol { flex-direction:row; margin-left:auto; overflow-x:auto; scrollbar-width:none; }
    .yol::-webkit-scrollbar { display:none; }
    .yol a { padding:4px 10px; white-space:nowrap; }
    .yol a.joriy::before { display:none; }
    .yol em, .vaqt { display:none; }
    .ichi { padding:0 20px 64px; }
    section { scroll-margin-top:60px; }
  }
  @media (max-width:620px) {
    body { font-size:14.5px; }
    .ichi { padding:0 16px 64px; }
    .setka.k6, .setka.k4, .setka.k2 { grid-template-columns:repeat(2,1fr); }
    .royxat li { grid-template-columns:auto 1fr auto; gap:10px; padding:9px 13px; }
    .royxat .tasma { display:none; }
    .vaqt { display:none; }
    h2 { font-size:23px; }
    th, td { padding:9px 12px; font-size:13px; }
  }
`;

// ───────────────────────────────────────────── mamlakat nomlari va bayroqlar
//
// ISO 3166-1 alpha-2 → oʻzbekcha nom. Cloudflare shu ikki harfli kodni beradi.
// Bayroq emoji koddan hisoblanadi (regional indicator harflari), shuning uchun
// tashqi rasm yoki shrift talab qilinmaydi. Bayroq koʻrinmaydigan tizimda
// yonidagi kod oʻqiladi — shu sabab kod DOIM yoziladi.

const MAMLAKAT = {
  AD:'Andorra', AE:'Birlashgan Arab Amirliklari', AF:'Afgʻoniston',
  AG:'Antigua va Barbuda', AI:'Angilya', AL:'Albaniya', AM:'Armaniston',
  AO:'Angola', AQ:'Antarktida', AR:'Argentina', AS:'Amerika Samoasi',
  AT:'Avstriya', AU:'Avstraliya', AW:'Aruba', AX:'Aland orollari',
  AZ:'Ozarbayjon', BA:'Bosniya va Gertsegovina', BB:'Barbados', BD:'Bangladesh',
  BE:'Belgiya', BF:'Burkina-Faso', BG:'Bolgariya', BH:'Bahrayn', BI:'Burundi',
  BJ:'Benin', BL:'Sen-Bartelemi', BM:'Bermuda', BN:'Bruney', BO:'Boliviya',
  BQ:'Boneyr', BR:'Braziliya', BS:'Bagama orollari', BT:'Butan',
  BW:'Botsvana', BY:'Belarus', BZ:'Beliz', CA:'Kanada', CC:'Kokos orollari',
  CD:'Kongo (DR)', CF:'Markaziy Afrika Respublikasi', CG:'Kongo',
  CH:'Shveytsariya', CI:'Kot-dʼIvuar', CK:'Kuk orollari', CL:'Chili',
  CM:'Kamerun', CN:'Xitoy', CO:'Kolumbiya', CR:'Kosta-Rika', CU:'Kuba',
  CV:'Kabo-Verde', CW:'Kyurasao', CX:'Rojdestvo oroli', CY:'Kipr',
  CZ:'Chexiya', DE:'Germaniya', DJ:'Jibuti', DK:'Daniya', DM:'Dominika',
  DO:'Dominikan Respublikasi', DZ:'Aljir', EC:'Ekvador', EE:'Estoniya',
  EG:'Misr', EH:'Gʻarbiy Sahara', ER:'Eritreya', ES:'Ispaniya', ET:'Efiopiya',
  FI:'Finlandiya', FJ:'Fiji', FK:'Folklend orollari', FM:'Mikroneziya',
  FO:'Farer orollari', FR:'Fransiya', GA:'Gabon', GB:'Buyuk Britaniya',
  GD:'Grenada', GE:'Gruziya', GF:'Fransuz Gvianasi', GG:'Gernsi', GH:'Gana',
  GI:'Gibraltar', GL:'Grenlandiya', GM:'Gambiya', GN:'Gvineya',
  GP:'Gvadelupa', GQ:'Ekvatorial Gvineya', GR:'Gretsiya', GT:'Gvatemala',
  GU:'Guam', GW:'Gvineya-Bisau', GY:'Gayana', HK:'Gonkong', HN:'Gonduras',
  HR:'Xorvatiya', HT:'Gaiti', HU:'Vengriya', ID:'Indoneziya', IE:'Irlandiya',
  IL:'Isroil', IM:'Men oroli', IN:'Hindiston', IO:'Britaniya Hind okeani hududi',
  IQ:'Iroq', IR:'Iron', IS:'Islandiya', IT:'Italiya', JE:'Jersi',
  JM:'Yamayka', JO:'Iordaniya', JP:'Yaponiya', KE:'Keniya', KG:'Qirgʻiziston',
  KH:'Kambodja', KI:'Kiribati', KM:'Komor orollari', KN:'Sent-Kits va Nevis',
  KP:'Shimoliy Koreya', KR:'Janubiy Koreya', KW:'Quvayt',
  KY:'Kayman orollari', KZ:'Qozogʻiston', LA:'Laos', LB:'Livan',
  LC:'Sent-Lyusiya', LI:'Lixtenshteyn', LK:'Shri-Lanka', LR:'Liberiya',
  LS:'Lesoto', LT:'Litva', LU:'Lyuksemburg', LV:'Latviya', LY:'Liviya',
  MA:'Marokash', MC:'Monako', MD:'Moldova', ME:'Chernogoriya',
  MF:'Sen-Marten', MG:'Madagaskar', MH:'Marshall orollari',
  MK:'Shimoliy Makedoniya', ML:'Mali', MM:'Myanma', MN:'Mongoliya',
  MO:'Makao', MP:'Shimoliy Mariana orollari', MQ:'Martinika',
  MR:'Mavritaniya', MS:'Montserrat', MT:'Malta', MU:'Mavritsiy',
  MV:'Maldiv orollari', MW:'Malavi', MX:'Meksika', MY:'Malayziya',
  MZ:'Mozambik', NA:'Namibiya', NC:'Yangi Kaledoniya', NE:'Niger',
  NF:'Norfolk oroli', NG:'Nigeriya', NI:'Nikaragua', NL:'Niderlandiya',
  NO:'Norvegiya', NP:'Nepal', NR:'Nauru', NU:'Niue', NZ:'Yangi Zelandiya',
  OM:'Ummon', PA:'Panama', PE:'Peru', PF:'Fransuz Polineziyasi',
  PG:'Papua — Yangi Gvineya', PH:'Filippin', PK:'Pokiston', PL:'Polsha',
  PM:'Sen-Pyer va Mikelon', PR:'Puerto-Riko', PS:'Falastin', PT:'Portugaliya',
  PW:'Palau', PY:'Paragvay', QA:'Qatar', RE:'Reyunion', RO:'Ruminiya',
  RS:'Serbiya', RU:'Rossiya', RW:'Ruanda', SA:'Saudiya Arabistoni',
  SB:'Solomon orollari', SC:'Seyshel orollari', SD:'Sudan', SE:'Shvetsiya',
  SG:'Singapur', SH:'Muqaddas Yelena oroli', SI:'Sloveniya', SJ:'Svalbard',
  SK:'Slovakiya', SL:'Syerra-Leone', SM:'San-Marino', SN:'Senegal',
  SO:'Somali', SR:'Surinam', SS:'Janubiy Sudan', ST:'San-Tome va Prinsipi',
  SV:'Salvador', SX:'Sint-Marten', SY:'Suriya', SZ:'Esvatini',
  TC:'Turks va Kaykos', TD:'Chad', TG:'Togo', TH:'Tailand', TJ:'Tojikiston',
  TK:'Tokelau', TL:'Timor-Leste', TM:'Turkmaniston', TN:'Tunis', TO:'Tonga',
  TR:'Turkiya', TT:'Trinidad va Tobago', TV:'Tuvalu', TW:'Tayvan',
  TZ:'Tanzaniya', UA:'Ukraina', UG:'Uganda', US:'AQSh', UY:'Urugvay',
  UZ:'Oʻzbekiston', VA:'Vatikan', VC:'Sent-Vinsent va Grenadin',
  VE:'Venesuela', VG:'Britaniya Virjin orollari', VI:'AQSh Virjin orollari',
  VN:'Vyetnam', VU:'Vanuatu', WF:'Uollis va Futuna', WS:'Samoa', YE:'Yaman',
  YT:'Mayotta', ZA:'Janubiy Afrika', ZM:'Zambiya', ZW:'Zimbabve',
};

// 'XX' — Cloudflare mamlakatni aniqlay olmaganda (masalan Tor yoki sunʼiy
// yoʻldosh tarmogʻi). Bunda bayroq oʻrniga globus koʻrsatiladi.
function mamlakatNomi(kod) {
  const k = String(kod || '').toUpperCase();
  return MAMLAKAT[k] || (k && k !== 'XX' ? k : 'Aniqlanmagan');
}

function bayroq(kod) {
  const k = String(kod || '').toUpperCase();
  if (!/^[A-Z]{2}$/.test(k) || k === 'XX' || k === 'T1') return '🌐';
  return String.fromCodePoint(...[...k].map((c) => 0x1f1e6 + c.charCodeAt(0) - 65));
}

// ───────────────────────────────────────────────────────────── yordamchilar

function q(s) {
  return String(s).replace(/[<>&"']/g, (c) =>
    ({ '<': '&lt;', '>': '&gt;', '&': '&amp;', '"': '&quot;', "'": '&#39;' }[c]));
}

// Mingliklarni ingichka boʻshliq (U+202F) bilan ajratamiz — 12 345 koʻrinishi
// tabular-nums bilan birga ustunlarni tekis saqlaydi.
function raqam(n) {
  const x = Number(n);
  if (!Number.isFinite(x)) return '—';
  const [butun, kasr] = String(Math.abs(x)).split('.');
  const ajratilgan = butun.replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
  return (x < 0 ? '−' : '') + ajratilgan + (kasr ? '.' + kasr : '');
}

function foiz(n, jami) {
  if (!jami) return '0%';
  const p = (n / jami) * 100;
  if (p > 0 && p < 1) return '<1%';
  return (p >= 9.95 ? Math.round(p) : Math.round(p * 10) / 10) + '%';
}

// Soniyani odam oʻqiy oladigan koʻrinishga: 42s / 3 daq / 2 soat 10 daq.
function davomiylik(s) {
  const x = Number(s);
  if (!Number.isFinite(x) || x <= 0) return '—';
  if (x < 60) return (x < 10 ? Math.round(x * 10) / 10 : Math.round(x)) + ' s';
  if (x < 3600) return Math.round(x / 60) + ' daq';
  const soat = Math.floor(x / 3600);
  const daq = Math.round((x % 3600) / 60);
  return soat + ' soat' + (daq ? ' ' + daq + ' daq' : '');
}

const OY_QISQA = ['yan', 'fev', 'mar', 'apr', 'may', 'iyn', 'iyl', 'avg', 'sen', 'okt', 'noy', 'dek'];
const OY_TOLIQ = ['yanvar', 'fevral', 'mart', 'aprel', 'may', 'iyun', 'iyul',
                  'avgust', 'sentabr', 'oktabr', 'noyabr', 'dekabr'];

function sanaQisqa(s) {
  const [, o, k] = String(s).split('-');
  return `${Number(k)} ${OY_QISQA[Number(o) - 1] || ''}`.trim();
}

function sanaToliq(s) {
  const [y, o, k] = String(s).split('-');
  return `${Number(k)} ${OY_TOLIQ[Number(o) - 1] || ''} ${y}`;
}

// Bugundan N kun oldingi sana (UTC, YYYY-MM-DD) — bazadagi sanalar ham UTC.
function kunOldin(n) {
  const d = new Date();
  d.setUTCDate(d.getUTCDate() - n);
  return d.toISOString().slice(0, 10);
}

// Kunlik qatorlarda boʻsh kunlar yoʻq (ping boʻlmagan kun yozilmaydi). Grafik
// uzluksiz boʻlishi uchun yetmagan kunlarni nol bilan toʻldiramiz.
function kunlarniTolatir(qatorlar, kun, ustun = 'n') {
  const xarita = new Map((qatorlar || []).map((r) => [r.sana, Number(r[ustun]) || 0]));
  const chiqdi = [];
  for (let i = kun - 1; i >= 0; i--) {
    const s = kunOldin(i);
    chiqdi.push({ sana: s, n: xarita.get(s) || 0 });
  }
  return chiqdi;
}

// ───────────────────────────────────────────── qurilma nomlarini tozalash
//
// Windows xom holda «Intel(R) Core(TM) i5-12450H CPU @ 2.10GHz» qaytaradi.
// Bu qatorlar panelda oʻqilmaydi va bir xil protsessor turli yozuvlar bilan
// alohida qatorlarga boʻlinib ketadi. Shuning uchun nomni qisqartirib,
// keyin JS tomonida qayta jamlaymiz.
function qurilmaNomi(xom) {
  const s = String(xom || '').trim();
  if (!s || s === '?') return null;
  return s
    .replace(/\((?:R|TM|C)\)/gi, '')
    .replace(/\s*@\s*[\d.]+\s*GHz/i, '')
    .replace(/\s+w\/\s+.*$/i, '')          // «… w/ Radeon 780M Graphics»
    .replace(/\s+with\s+Radeon.*$/i, '')
    .replace(/\b(?:CPU|Processor)\b/gi, '')
    .replace(/\bCore\(TM\)\b/gi, 'Core')
    .replace(/\s{2,}/g, ' ')
    .replace(/\s+,/g, ',')
    .trim() || null;
}

// Ishlab chiqaruvchi — roʻyxatdagi chiziqchani rangli qilish uchun.
function ishlabChiqaruvchi(nom) {
  const s = String(nom || '').toLowerCase();
  if (/apple|\bm[1-9]\b/.test(s)) return 'apple';
  if (/nvidia|geforce|quadro|\brtx\b|\bgtx\b/.test(s)) return 'nvidia';
  if (/\bamd\b|radeon|ryzen|athlon/.test(s)) return 'amd';
  if (/intel|\biris\b|\buhd\b/.test(s)) return 'intel';
  return 'boshqa';
}

// RAM: ilova foydalanish uchun ochiq xotirani aytadi (32 GB mashina 31 deb
// koʻrinadi). Standart oʻlchamga yaxlitlaymiz, aks holda bitta 32 GB uchun
// uchta alohida qator chiqadi.
const RAM_POGONA = [2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 512];
function ramGuruh(gb) {
  const g = Number(gb);
  if (!Number.isFinite(g) || g <= 0) return null;
  for (const p of RAM_POGONA) if (g <= p + 0.6) return p;
  return Math.ceil(g / 64) * 64;
}

// OS nomi. Oxiridagi yakka qurilish raqami («Windows 11 26100») olib
// tashlanadi — u panelda shovqin va bitta Windows'ni bir necha qatorga
// boʻlib yuboradi. Nuqtali versiya («macOS 15.2») maʼnoli, u qoladi.
function osNomi(xom) {
  const s = String(xom || '').trim();
  if (!s || s === '?') return 'Aniqlanmagan';
  return s.replace(/\s+\d{4,}$/, '').trim() || 'Aniqlanmagan';
}

// Platformani hamma joyda bir xil koʻrsatish uchun: nomi va chiziqcha rangi.
const PLATFORMA_NOMI = { mac: 'macOS', win: 'Windows' };
const PLATFORMA_RANGI = { mac: 'var(--v-apple)', win: 'var(--v-intel)' };

// ─────────────────────────────────────────────────────────── qismlar (HTML)

function quti(ichi, sinf = '') {
  return `<div class="quti ${sinf}" style="overflow:hidden">${ichi}</div>`;
}

function karta({ son, nom, ost = '', birlik = '', izoh = '', sinf = '', uchqunlar = null }) {
  const t = izoh ? ` title="${q(izoh)}"` : '';
  return `<div class="quti"><div class="karta ${sinf}"${t}>
    <div class="son">${q(son)}${birlik ? `<u>${q(birlik)}</u>` : ''}</div>
    <div class="nom">${q(nom)}</div>
    ${uchqunlar ? uchqun(uchqunlar) : ''}
    ${ost ? `<div class="ost">${ost}</div>` : ''}
  </div></div>`;
}

// Kichkina grafik — kartaning ichida, oʻqlarsiz.
function uchqun(nuqtalar) {
  const n = nuqtalar.length;
  if (n < 2 || nuqtalar.every((p) => !p.n)) return '';
  const W = 120, H = 22;
  const eng = Math.max(1, ...nuqtalar.map((p) => p.n));
  const X = (i) => (i * W) / (n - 1);
  const Y = (v) => H - 1.5 - (v / eng) * (H - 3);
  const yol = nuqtalar.map((p, i) => `${i ? 'L' : 'M'}${X(i).toFixed(1)} ${Y(p.n).toFixed(1)}`).join(' ');
  return `<svg class="uchqun" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none" aria-hidden="true">
    <path class="tag" d="${yol} L${W} ${H} L0 ${H} Z"/><path d="${yol}"/></svg>`;
}

// Oʻq belgisi uchun «chiroyli» yuqori chegara: 13 → 15, 57 → 60, 480 → 500.
function chiroyliChegara(eng) {
  if (eng <= 5) return Math.max(1, Math.ceil(eng));
  const daraja = Math.pow(10, Math.floor(Math.log10(eng)));
  for (const k of [1, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10]) {
    if (eng <= k * daraja) return k * daraja;
  }
  return 10 * daraja;
}

// Ikkala grafik uchun umumiy: gorizontal yoʻnaltiruvchi chiziqlar va chapdagi
// oʻq belgilari. Chegara 1 boʻlsa oʻrtadagi chiziq tashlanadi (aks holda
// «1 / 1 / 0» kabi takror chiqadi); qolganda belgi chiziq turgan aniq
// qiymatni koʻrsatadi — 15 ning oʻrtasi 8 emas, 7.5.
function torVaBelgi(chegara, P, gH, W) {
  const ulush = chegara >= 2 ? [0, 0.5, 1] : [0, 1];
  return ulush.map((k) => {
    const qiy = chegara * k;
    const y = (P.tepa + gH - k * gH).toFixed(1);
    const matn = raqam(Number.isInteger(qiy) ? qiy : Math.round(qiy * 10) / 10);
    return `<line class="${k ? 'tor' : 'tayanch'}" x1="${P.chap}" y1="${y}" x2="${W - P.ong}" y2="${y}"/>
            <text x="${P.chap - 6}" y="${Number(y) + 3.2}" text-anchor="end">${matn}</text>`;
  }).join('');
}

// Maydon grafigi (kunlik qatorlar uchun). Sichqoncha ustiga kelganda har kun
// uchun brauzerning oʻz maslahat oynasi chiqadi — JavaScript kerak emas.
function maydonChizma(nuqtalar, { balandlik = 168, birlik = '' } = {}) {
  const n = nuqtalar.length;
  if (!n) return '';
  const W = 760, H = balandlik;
  const P = { chap: 30, ong: 10, tepa: 14, past: 24 };
  const gW = W - P.chap - P.ong, gH = H - P.tepa - P.past;
  const chegara = chiroyliChegara(Math.max(...nuqtalar.map((p) => p.n)));
  const X = (i) => P.chap + (n === 1 ? gW / 2 : (i * gW) / (n - 1));
  const Y = (v) => P.tepa + gH - (v / chegara) * gH;

  const chiziq = nuqtalar
    .map((p, i) => `${i ? 'L' : 'M'}${X(i).toFixed(1)} ${Y(p.n).toFixed(1)}`).join(' ');
  const maydon = `${chiziq} L${X(n - 1).toFixed(1)} ${P.tepa + gH} L${X(0).toFixed(1)} ${P.tepa + gH} Z`;

  const tor = torVaBelgi(chegara, P, gH, W);

  const belgiIdx = n > 2 ? [0, Math.floor((n - 1) / 2), n - 1] : [0, n - 1];
  const xBelgi = [...new Set(belgiIdx)].map((i, j, a) => {
    const tayanch = j === 0 ? 'start' : j === a.length - 1 ? 'end' : 'middle';
    return `<text x="${X(i).toFixed(1)}" y="${H - 6}" text-anchor="${tayanch}">${q(sanaQisqa(nuqtalar[i].sana))}</text>`;
  }).join('');

  const kenglik = n > 1 ? gW / (n - 1) : gW;
  const sezgir = nuqtalar.map((p, i) =>
    `<rect class="sezgir" x="${(X(i) - kenglik / 2).toFixed(1)}" y="${P.tepa}" width="${kenglik.toFixed(1)}" height="${gH}"><title>${q(sanaToliq(p.sana))} — ${q(raqam(p.n))}${birlik ? ' ' + q(birlik) : ''}</title></rect>`).join('');

  return `<svg class="ch" viewBox="0 0 ${W} ${H}" role="img">
    ${tor}<path class="maydon" d="${maydon}"/><path class="chiziq" d="${chiziq}"/>${xBelgi}${sezgir}</svg>`;
}

// Ustunli grafik. `qatlamlar` bir nechta boʻlsa ustunlar bir-birining ustiga
// yigʻiladi (masalan transkripsiya + tarjima bitta kunda).
function ustunChizma(nuqtalar, qatlamlar, { balandlik = 168, xHar = 5, birlik = '' } = {}) {
  const n = nuqtalar.length;
  if (!n) return '';
  const W = 760, H = balandlik;
  const P = { chap: 30, ong: 10, tepa: 14, past: 24 };
  const gW = W - P.chap - P.ong, gH = H - P.tepa - P.past;
  const jamilar = nuqtalar.map((p) => qatlamlar.reduce((s, l) => s + (Number(p.qiymat[l.kalit]) || 0), 0));
  const chegara = chiroyliChegara(Math.max(...jamilar));
  const qadam = gW / n;
  const kenglik = Math.max(2, Math.min(qadam * 0.68, 26));

  const tor = torVaBelgi(chegara, P, gH, W);

  const ustunlar = nuqtalar.map((p, i) => {
    const x = P.chap + qadam * i + (qadam - kenglik) / 2;
    let past = P.tepa + gH;
    const boloq = qatlamlar.map((l) => {
      const v = Number(p.qiymat[l.kalit]) || 0;
      if (v <= 0) return '';
      const h = Math.max(1.2, (v / chegara) * gH);
      past -= h;
      return `<rect class="ustun ${l.sinf || ''}" x="${x.toFixed(1)}" y="${past.toFixed(1)}" width="${kenglik.toFixed(1)}" height="${h.toFixed(1)}" rx="1.5"/>`;
    }).join('');
    const jami = jamilar[i];
    const tafsil = qatlamlar.length > 1
      ? qatlamlar.filter((l) => p.qiymat[l.kalit]).map((l) => `${l.nom}: ${p.qiymat[l.kalit]}`).join(', ')
      : '';
    const maslahat = `${p.tavsif} — ${raqam(jami)}${birlik ? ' ' + birlik : ''}${tafsil && qatlamlar.length > 1 ? ' (' + tafsil + ')' : ''}`;
    return `<g><rect class="sezgir" x="${(P.chap + qadam * i).toFixed(1)}" y="${P.tepa}" width="${qadam.toFixed(1)}" height="${gH}"><title>${q(maslahat)}</title></rect>${boloq}</g>`;
  }).join('');

  const xBelgi = nuqtalar.map((p, i) => (i % xHar === 0 || i === n - 1)
    ? `<text x="${(P.chap + qadam * i + qadam / 2).toFixed(1)}" y="${H - 6}" text-anchor="middle">${q(p.yorliq)}</text>` : '').join('');

  return `<svg class="ch" viewBox="0 0 ${W} ${H}" role="img">${tor}${ustunlar}${xBelgi}</svg>`;
}

function afsona(bolaklar, jami) {
  return `<div class="afsona">${bolaklar.map((b) =>
    `<span><s style="background:${b.rang}"></s>${q(b.nom)} <b>${q(raqam(b.n))}</b>${jami ? ` <em>· ${q(foiz(b.n, jami))}</em>` : ''}</span>`).join('')}</div>`;
}

// Bitta qatorli ulush chizigʻi — «85% Windows / 15% macOS» kabi taqsimotlar
// uchun roʻyxatdan koʻra tezroq oʻqiladi.
function ulushChiziq(bolaklar) {
  const jami = bolaklar.reduce((s, b) => s + b.n, 0) || 1;
  const tasma = bolaklar.map((b) =>
    `<i style="width:${((b.n / jami) * 100).toFixed(2)}%;background:${b.rang}" title="${q(b.nom)}: ${q(raqam(b.n))} (${q(foiz(b.n, jami))})"></i>`).join('');
  return `<div class="ulush">${tasma}</div>${afsona(bolaklar, jami)}`;
}

// Chiziqchali roʻyxat — panelning asosiy qismi. `oldi` — bayroq (ixtiyoriy),
// `izoh` — ikkinchi qator (masalan viloyat nomi), `rang` — chiziqcha rangi.
function royxat(elementlar, { jami = null } = {}) {
  if (!elementlar.length) return '';
  const eng = Math.max(1, ...elementlar.map((e) => e.n));
  const butun = jami ?? elementlar.reduce((s, e) => s + e.n, 0);
  const qatorlar = elementlar.map((e) => {
    const oldi = e.oldi ? `<span class="oldi">${q(e.oldi)}</span>` : '';
    const tasma =
      `<span class="tasma"><i style="width:${((e.n / eng) * 100).toFixed(1)}%${e.rang ? ';background:' + e.rang : ''}"></i></span>`;
    return `<li>${oldi}
      <span class="matn"><b>${q(e.yorliq)}</b>${e.izoh ? `<i>${q(e.izoh)}</i>` : ''}</span>
      ${tasma}
      <span class="qiymat"><b>${q(raqam(e.n))}</b>${butun ? `<i>${q(foiz(e.n, butun))}</i>` : ''}</span></li>`;
  }).join('');
  return quti(`<ul class="royxat">${qatorlar}</ul>`);
}

function jadval(sarlavhalar, qatorlar) {
  if (!qatorlar.length) return '';
  const th = sarlavhalar.map((s, i) => `<th${i ? ' style="text-align:right"' : ''}>${q(s)}</th>`).join('');
  const tr = qatorlar.map((r) => '<tr>' + r.map((v, i) =>
    `<td class="${i ? 'r' : 'asos'}">${v && v.xom ? v.html : q(v ?? '—')}</td>`).join('') + '</tr>').join('');
  return quti(`<div class="jadval-oram"><table><thead><tr>${th}</tr></thead><tbody>${tr}</tbody></table></div>`);
}

function yoq(sarlavha, matn) {
  return `<div class="yoq"><b>${q(sarlavha)}</b>${q(matn)}</div>`;
}

// ─────────────────────────────────────────────────────────────── soʻrovlar
//
// Hammasi bitta `batch` bilan ketadi. Tartib muhim emas — natijalar nom
// boʻyicha xaritaga yigʻiladi.
function soʻrovRoyxati() {
  const bugun = kunOldin(0), hafta = kunOldin(6), oy = kunOldin(29), haftaOldin = kunOldin(7);
  return [
    ['jami',      'SELECT COUNT(*) AS n FROM ornatmalar'],
    ['faolBugun', 'SELECT COUNT(*) AS n FROM ornatmalar WHERE oxirgi >= ?1', bugun],
    ['faolKecha', 'SELECT COUNT(*) AS n FROM ornatmalar WHERE oxirgi = ?1', kunOldin(1)],
    ['faol7',     'SELECT COUNT(*) AS n FROM ornatmalar WHERE oxirgi >= ?1', hafta],
    ['faol30',    'SELECT COUNT(*) AS n FROM ornatmalar WHERE oxirgi >= ?1', oy],
    ['yangi7',    'SELECT COUNT(*) AS n FROM ornatmalar WHERE birinchi >= ?1', hafta],
    ['yangi30',   'SELECT COUNT(*) AS n FROM ornatmalar WHERE birinchi >= ?1', oy],
    ['ortacha',   'SELECT ROUND(AVG(kunlar),1) AS o, MAX(kunlar) AS eng FROM ornatmalar'],
    ['tarmoq',    'SELECT COUNT(DISTINCT ip_hash) AS n FROM ornatmalar WHERE ip_hash IS NOT NULL'],
    // Haftalik qaytish: bir haftadan avval kelganlarning qanchasi shu haftada ham koʻrindi.
    ['qaytish',   `SELECT SUM(CASE WHEN birinchi <= ?1 THEN 1 ELSE 0 END) AS eski,
                          SUM(CASE WHEN birinchi <= ?1 AND oxirgi >= ?2 THEN 1 ELSE 0 END) AS qaytgan
                     FROM ornatmalar`, haftaOldin, hafta],
    ['chuqurlik', `SELECT CASE WHEN kunlar<=1 THEN 1 WHEN kunlar<=3 THEN 2
                               WHEN kunlar<=7 THEN 3 WHEN kunlar<=30 THEN 4 ELSE 5 END AS guruh,
                          COUNT(*) AS n
                     FROM ornatmalar GROUP BY guruh ORDER BY guruh`],

    ['platformalar', 'SELECT platforma, COUNT(*) AS n FROM ornatmalar GROUP BY platforma ORDER BY n DESC'],
    ['versiyalar',   'SELECT versiya, platforma, COUNT(*) AS n FROM ornatmalar GROUP BY versiya, platforma ORDER BY n DESC LIMIT 20'],
    ['oslar',        'SELECT os, platforma, COUNT(*) AS n FROM ornatmalar GROUP BY os, platforma ORDER BY n DESC LIMIT 16'],

    ['mamlakatlar', 'SELECT mamlakat, COUNT(*) AS n FROM ornatmalar GROUP BY mamlakat ORDER BY n DESC LIMIT 40'],
    ['shaharlar',   'SELECT shahar, viloyat, mamlakat, COUNT(*) AS n FROM ornatmalar GROUP BY shahar, viloyat, mamlakat ORDER BY n DESC LIMIT 20'],

    ['kunlikFaol',  'SELECT sana, SUM(faol) AS n FROM kunlik WHERE sana >= ?1 GROUP BY sana ORDER BY sana', oy],
    ['kunlikYangi', 'SELECT birinchi AS sana, COUNT(*) AS n FROM ornatmalar WHERE birinchi >= ?1 GROUP BY birinchi ORDER BY birinchi', oy],

    ['amalJami', `SELECT COUNT(*) AS n, SUM(CASE WHEN natija='ok' THEN 1 ELSE 0 END) AS ok,
                         COUNT(DISTINCT id) AS odam FROM amallar`],
    ['amalTur',  `SELECT tur, COUNT(*) AS n,
                         SUM(CASE WHEN natija='ok' THEN 1 ELSE 0 END) AS ok,
                         ROUND(AVG(CASE WHEN ovoz_s>0 AND ishlov_s>0 THEN ovoz_s/ishlov_s END),1) AS rtf,
                         ROUND(AVG(ishlov_s),1) AS ort_vaqt,
                         ROUND(MAX(ishlov_s),1) AS eng_vaqt,
                         ROUND(SUM(ovoz_s),0) AS ovoz_s,
                         SUM(belgi) AS belgi,
                         ROUND(AVG(CASE WHEN belgi>0 AND ishlov_s>0 THEN belgi/ishlov_s END),0) AS belgi_s
                    FROM amallar GROUP BY tur ORDER BY n DESC`],
    ['amalBackend', `SELECT backend, COUNT(*) AS n,
                            ROUND(AVG(CASE WHEN ovoz_s>0 AND ishlov_s>0 THEN ovoz_s/ishlov_s END),1) AS rtf,
                            ROUND(AVG(ishlov_s),1) AS ort_vaqt,
                            ROUND(SUM(ovoz_s)/3600.0,2) AS soat
                       FROM amallar WHERE tur='stt' GROUP BY backend ORDER BY n DESC`],
    ['amalKunlik', 'SELECT sana, tur, COUNT(*) AS n FROM amallar WHERE sana >= ?1 GROUP BY sana, tur', oy],
    ['amalSoat',   "SELECT CAST(strftime('%H', vaqt, 'unixepoch') AS INTEGER) AS soat, COUNT(*) AS n FROM amallar GROUP BY soat"],
    ['amalXato',   "SELECT natija, tur, COUNT(*) AS n FROM amallar WHERE natija <> 'ok' GROUP BY natija, tur ORDER BY n DESC LIMIT 12"],
    ['ovozUzun',   `SELECT CASE WHEN ovoz_s<10 THEN 1 WHEN ovoz_s<30 THEN 2 WHEN ovoz_s<120 THEN 3
                                WHEN ovoz_s<600 THEN 4 ELSE 5 END AS guruh,
                           COUNT(*) AS n, ROUND(SUM(ovoz_s)/60.0,1) AS daq
                      FROM amallar WHERE tur='stt' AND ovoz_s>0 GROUP BY guruh ORDER BY guruh`],

    ['qurArx',   'SELECT arx, COUNT(*) AS n FROM ornatmalar GROUP BY arx ORDER BY n DESC'],
    ['qurGpu',   'SELECT gpu, COUNT(*) AS n FROM ornatmalar GROUP BY gpu'],
    ['qurCpu',   'SELECT cpu, COUNT(*) AS n FROM ornatmalar GROUP BY cpu'],
    ['qurRam',   'SELECT ram_gb, COUNT(*) AS n FROM ornatmalar WHERE ram_gb IS NOT NULL GROUP BY ram_gb'],
    ['qurYadro', 'SELECT yadro, COUNT(*) AS n FROM ornatmalar WHERE yadro IS NOT NULL GROUP BY yadro ORDER BY yadro'],
  ];
}

// Xom nomlarni tozalab, bir xil boʻlib qolganlarni qoʻshib yuboradi.
function nomlarniJamla(qatorlar, ustun) {
  const xarita = new Map();
  for (const r of qatorlar) {
    const nom = qurilmaNomi(r[ustun]) || 'Aniqlanmagan';
    xarita.set(nom, (xarita.get(nom) || 0) + Number(r.n));
  }
  return [...xarita].map(([nom, n]) => ({ nom, n })).sort((a, b) => b.n - a.n);
}

function ishlabChiqaruvchiUlushi(elementlar) {
  const nomlar = { apple: 'Apple', nvidia: 'NVIDIA', amd: 'AMD', intel: 'Intel', boshqa: 'Boshqa' };
  const xarita = new Map();
  for (const e of elementlar) {
    const v = e.nom === 'Aniqlanmagan' ? 'boshqa' : ishlabChiqaruvchi(e.nom);
    xarita.set(v, (xarita.get(v) || 0) + e.n);
  }
  return [...xarita].sort((a, b) => b[1] - a[1])
    .map(([v, n]) => ({ nom: nomlar[v] || v, n, rang: `var(--v-${v})` }));
}

const CHUQURLIK_NOMI = { 1: 'Faqat 1 kun', 2: '2–3 kun', 3: '4–7 kun', 4: '8–30 kun', 5: '30 kundan koʻp' };
const OVOZ_NOMI = {
  1: '10 soniyagacha', 2: '10–30 soniya', 3: '30 soniya – 2 daqiqa',
  4: '2–10 daqiqa', 5: '10 daqiqadan uzun',
};
const XATO_NOMI = {
  'xato:nomalum': 'Aniqlanmagan xato',
  'xato:model': 'Model yuklanmadi',
  'xato:ovoz': 'Ovoz olinmadi',
  'xato:fayl': 'Fayl oʻqilmadi',
  'xato:xotira': 'Xotira yetmadi',
  'xato:bekor': 'Foydalanuvchi bekor qildi',
};

const BACKEND_NOMI = {
  metal: 'Metal', vulkan: 'Vulkan', cuda: 'CUDA', cpu: 'CPU',
};
const BACKEND_IZOHI = {
  metal: 'macOS videokartasi', vulkan: 'Windows videokartasi',
  cuda: 'NVIDIA videokartasi', cpu: 'faqat protsessor',
};

// ────────────────────────────────────────────────────────────────── sahifa

// Kirish — HTTP Basic: brauzer oʻzi login oynasini ochadi, kalit (parol
// maydoniga) `Authorization` sarlavhasida keladi. Foydalanuvchi nomi
// eʼtiborsiz.
//
// Ilgari kalit URL'da edi (`/panel?kalit=…`) — u observability loglariga,
// brauzer tarixiga va Referer'ga tushardi; solishtirish ham oddiy `!==` edi
// (vaqt boʻyicha sizib chiqadi). Endi URL'dagi kalit qabul QILINMAYDI va
// solishtirish ikkala tomonning SHA-256 i ustidan `timingSafeEqual` bilan
// (uzunlik ham sizmaydi). Spec H3.
async function kirishTogrimi(soʻrov, muhit) {
  if (!muhit.PANEL_KALIT) return false;
  const m = /^Basic\s+([A-Za-z0-9+/=]+)$/.exec(soʻrov.headers.get('Authorization') || '');
  if (!m) return false;
  let juft;
  try {
    juft = new TextDecoder().decode(Uint8Array.from(atob(m[1]), (c) => c.charCodeAt(0)));
  } catch {
    return false;
  }
  const parol = juft.slice(juft.indexOf(':') + 1);
  const xesh = (x) => crypto.subtle.digest('SHA-256', new TextEncoder().encode(x));
  const [a, b] = await Promise.all([xesh(parol), xesh(muhit.PANEL_KALIT)]);
  return crypto.subtle.timingSafeEqual(a, b);
}

export async function panel(soʻrov, muhit) {
  if (!(await kirishTogrimi(soʻrov, muhit))) {
    return new Response(JSON.stringify({ xato: 'kirish kerak' }), {
      status: 401,
      headers: {
        'Content-Type': 'application/json; charset=utf-8',
        'WWW-Authenticate': 'Basic realm="Kotib statistika", charset="UTF-8"',
        'Cache-Control': 'no-store',
      },
    });
  }

  const S = soʻrovRoyxati();
  const [chiqdi, joriy] = await Promise.all([
    muhit.DB.batch(S.map(([, sql, ...p]) =>
      (p.length ? muhit.DB.prepare(sql).bind(...p) : muhit.DB.prepare(sql)))),
    muhit.VERSIYALAR.get('joriy', { type: 'json' }).catch(() => null),
  ]);

  const d = {};
  S.forEach(([nom], i) => { d[nom] = (chiqdi[i] && chiqdi[i].results) || []; });
  const bir = (nom, ustun, standart = 0) => {
    const r = d[nom][0];
    const v = r ? r[ustun] : null;
    return v === null || v === undefined ? standart : Number(v);
  };

  // ── raqamlar ──────────────────────────────────────────────────────────
  const jami = bir('jami', 'n');
  const faolBugun = bir('faolBugun', 'n');
  const faolKecha = bir('faolKecha', 'n');
  const faol7 = bir('faol7', 'n');
  const faol30 = bir('faol30', 'n');
  const yangi7 = bir('yangi7', 'n');
  const yangi30 = bir('yangi30', 'n');
  const ortKun = bir('ortacha', 'o');
  const engKun = bir('ortacha', 'eng');
  const tarmoq = bir('tarmoq', 'n');
  const eskiler = bir('qaytish', 'eski');
  const qaytgan = bir('qaytish', 'qaytgan');
  const amalJami = bir('amalJami', 'n');
  const amalOk = bir('amalJami', 'ok');
  const amalOdam = bir('amalJami', 'odam');

  const stt = d.amalTur.find((r) => r.tur === 'stt') || null;
  const tarjima = d.amalTur.find((r) => r.tur === 'tarjima') || null;

  // ── grafiklar uchun qatorlar ─────────────────────────────────────────
  const faolEgri = kunlarniTolatir(d.kunlikFaol, 30);
  const yangiEgri = kunlarniTolatir(d.kunlikYangi, 30);

  // Jami oʻrnatmaning oʻsishi: oxirgi kunda `jami`, orqaga qarab har kunning
  // yangi oʻrnatmalari ayriladi.
  const osish = new Array(yangiEgri.length);
  let qoldi = jami;
  for (let i = yangiEgri.length - 1; i >= 0; i--) {
    osish[i] = { sana: yangiEgri[i].sana, n: Math.max(0, qoldi) };
    qoldi -= yangiEgri[i].n;
  }

  const amalXarita = new Map();
  for (const r of d.amalKunlik) {
    const o = amalXarita.get(r.sana) || {};
    o[r.tur] = Number(r.n);
    amalXarita.set(r.sana, o);
  }
  const amalNuqta = [];
  for (let i = 29; i >= 0; i--) {
    const s = kunOldin(i);
    const o = amalXarita.get(s) || {};
    amalNuqta.push({
      yorliq: sanaQisqa(s), tavsif: sanaToliq(s),
      qiymat: { stt: o.stt || 0, tarjima: o.tarjima || 0 },
    });
  }
  const amalQatlam = [
    { kalit: 'stt', nom: 'Transkripsiya', sinf: '' },
    { kalit: 'tarjima', nom: 'Tarjima', sinf: 'ik' },
  ];

  // Soatlik faollik. Bazada UTC turadi; koʻrsatishda Toshkent vaqtiga
  // (UTC+5, yil boʻyi oʻzgarmaydi) surib chiqamiz — foydalanuvchilarning
  // asosiy qismi shu mintaqada.
  const soatlar = new Array(24).fill(0);
  for (const r of d.amalSoat) soatlar[(Number(r.soat) + 5) % 24] += Number(r.n);
  const ikki = (x) => String(x).padStart(2, '0');
  const soatNuqta = soatlar.map((n, s) => ({
    yorliq: ikki(s), tavsif: `${ikki(s)}:00–${ikki(s)}:59, Toshkent`, qiymat: { n },
  }));

  // ── qurilma roʻyxatlari ──────────────────────────────────────────────
  const gpular = nomlarniJamla(d.qurGpu, 'gpu');
  const cpular = nomlarniJamla(d.qurCpu, 'cpu');
  const gpuUlush = ishlabChiqaruvchiUlushi(gpular);
  const cpuUlush = ishlabChiqaruvchiUlushi(cpular);
  const rangla = (e) => `var(--v-${e.nom === 'Aniqlanmagan' ? 'boshqa' : ishlabChiqaruvchi(e.nom)})`;

  const ramXarita = new Map();
  for (const r of d.qurRam) {
    const g = ramGuruh(r.ram_gb);
    if (g) ramXarita.set(g, (ramXarita.get(g) || 0) + Number(r.n));
  }
  const ramlar = [...ramXarita].sort((a, b) => a[0] - b[0]).map(([g, n]) => ({ yorliq: `${g} GB`, n }));

  const ARX_NOMI = { arm64: 'ARM64', x64: 'x86-64 (Intel/AMD)' };
  const arxBolak = d.qurArx.map((r) => ({
    nom: ARX_NOMI[r.arx] || (r.arx || 'Aniqlanmagan'),
    n: Number(r.n),
    rang: r.arx === 'arm64' ? 'var(--v-apple)' : r.arx === 'x64' ? 'var(--v-intel)' : 'var(--v-boshqa)',
  }));

  const platformaBolak = d.platformalar.map((r) => ({
    nom: PLATFORMA_NOMI[r.platforma] || r.platforma,
    n: Number(r.n),
    rang: PLATFORMA_RANGI[r.platforma] || 'var(--v-boshqa)',
  }));

  // ── vaqt ─────────────────────────────────────────────────────────────
  const hozir = new Date();
  const utcVaqt = hozir.toISOString().slice(11, 16);
  const toshVaqt = new Date(hozir.getTime() + 5 * 3600 * 1000).toISOString().slice(11, 16);

  // ── kartalar ─────────────────────────────────────────────────────────
  // Har kartaning `title` izohi bor — sahifada yozuv kam boʻlsin deb
  // taʼriflar matnga emas, maslahat oynasiga chiqariladi.
  const boshKartalar = [
    karta({
      son: raqam(jami), nom: 'Jami oʻrnatma',
      izoh: 'Ilova nechta xil kompyuterda ishga tushgan. Qayta oʻrnatilsa yangisi sanaladi.',
      ost: yangi30 ? `30 kunda <b>+${q(raqam(yangi30))}</b>` : '',
      uchqunlar: osish,
    }),
    karta({
      son: raqam(faolBugun), nom: 'Bugun faol',
      izoh: 'Bugun (UTC) ilovani kamida bir marta ochganlar.',
      ost: `kecha ${q(raqam(faolKecha))}`,
    }),
    karta({
      son: raqam(faol30), nom: '30 kunda faol',
      izoh: 'Oxirgi 30 kunda kamida bir kun ilovani ochganlar.',
      ost: `7 kunda ${q(raqam(faol7))}`,
      uchqunlar: faolEgri,
    }),
    karta({
      son: raqam(amalJami), nom: 'Jami amal',
      izoh: 'Transkripsiya va tarjimalar soni. Oxirgi 60 kun saqlanadi.',
      ost: amalJami ? `${q(foiz(amalOk, amalJami))} muvaffaqiyatli` : '',
      uchqunlar: kunlarniTolatir(
        [...amalXarita].map(([sana, o]) => ({ sana, n: (o.stt || 0) + (o.tarjima || 0) })), 30),
    }),
  ].join('');

  const kichikKartalar = [
    karta({ sinf: 'kichik', son: raqam(yangi7), nom: '7 kunda yangi',
      izoh: 'Oxirgi 7 kunda birinchi marta ishga tushgan oʻrnatmalar.' }),
    karta({ sinf: 'kichik', son: (ortKun || 0).toFixed(1), nom: 'Oʻrtacha faol kun',
      izoh: `Bitta oʻrnatma oʻrtacha necha xil kunda ishlatilgan. Eng koʻpi — ${engKun} kun.` }),
    karta({ sinf: 'kichik', son: raqam(d.mamlakatlar.length), nom: 'Mamlakat',
      izoh: 'Nechta xil mamlakatdan foydalanilgan (taxminiy, IP boʻyicha).',
      ost: `${q(raqam(d.shaharlar.length))} shahar` }),
    karta({ sinf: 'kichik', son: eskiler ? foiz(qaytgan, eskiler) : '—', nom: 'Haftalik qaytish',
      izoh: 'Bir haftadan avval oʻrnatganlarning qanchasi shu hafta ham ilovani ochgan — ilova odat boʻlyaptimi degan savolga javob.' }),
    karta({ sinf: 'kichik', son: stt && stt.rtf ? stt.rtf : '—', birlik: stt && stt.rtf ? '×' : '',
      nom: 'Transkripsiya tezligi',
      izoh: 'Ovoz uzunligining ishlov vaqtiga nisbati. Katta boʻlishi yaxshi.',
      ost: stt && stt.rtf ? `1 daq ovoz ≈ ${q(Math.round(60 / stt.rtf))} s` : '' }),
    karta({ sinf: 'kichik', son: stt && stt.ovoz_s ? davomiylik(stt.ovoz_s) : '—',
      nom: 'Jami yozilgan ovoz',
      izoh: 'Barcha transkripsiyalarning umumiy davomiyligi. Ovozning oʻzi saqlanmaydi — faqat uzunligi.' }),
  ].join('');

  // ── versiya boʻlimi ──────────────────────────────────────────────────
  const oxirgiVer = (p) => (joriy && joriy[p] && joriy[p].versiya) || null;
  const platformaKartalar = d.platformalar.map((r) => {
    const p = r.platforma, jamiP = Number(r.n), ox = oxirgiVer(p);
    const shu = d.versiyalar
      .filter((v) => v.platforma === p && v.versiya === ox)
      .reduce((s, v) => s + Number(v.n), 0);
    return karta({
      sinf: 'kichik',
      son: ox ? foiz(shu, jamiP) : '—',
      nom: `${PLATFORMA_NOMI[p] || p} — oxirgi versiyada`,
      izoh: 'Yangilanishni oʻtkazib yuborganlar ulushi shu raqamdan koʻrinadi.',
      ost: ox ? `${q(raqam(shu))} / ${q(raqam(jamiP))} · <b>${q(ox)}</b>` : '',
    });
  }).join('');

  const versiyaRoyxat = royxat(d.versiyalar.map((r) => {
    const ox = oxirgiVer(r.platforma);
    return {
      yorliq: r.versiya,
      izoh: (PLATFORMA_NOMI[r.platforma] || r.platforma) + (ox && r.versiya !== ox ? ' · eskirgan' : ''),
      n: Number(r.n),
      rang: PLATFORMA_RANGI[r.platforma] || 'var(--v-boshqa)',
    };
  }), { jami });

  // Qurilish raqami olingach bir xil boʻlib qolgan qatorlarni qoʻshamiz.
  const osXarita = new Map();
  for (const r of d.oslar) {
    const nom = osNomi(r.os);
    const kalit = nom + '|' + r.platforma;
    const bor = osXarita.get(kalit);
    if (bor) bor.n += Number(r.n);
    else osXarita.set(kalit, { nom, platforma: r.platforma, n: Number(r.n) });
  }
  const osRoyxat = royxat([...osXarita.values()].sort((a, b) => b.n - a.n).slice(0, 8)
    .map((e) => ({
      yorliq: e.nom,
      n: e.n,
      rang: PLATFORMA_RANGI[e.platforma] || 'var(--v-boshqa)',
    })), { jami });

  // ── HTML ─────────────────────────────────────────────────────────────
  const html = `<!DOCTYPE html><html lang="uz"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex,nofollow">
<meta name="referrer" content="no-referrer">
<meta name="color-scheme" content="light dark">
<title>Kotib — statistika</title>
<style>${USLUB}</style></head><body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>
  <linearGradient id="kotib-tus" x1="0" y1="0" x2="0" y2="1">
    <stop class="a" offset="0"/><stop class="b" offset="1"/>
  </linearGradient>
</defs></svg>

<div class="sahn">
<aside class="yon">
  <div class="belgi"><s></s>Kotib · statistika</div>
  <nav class="yol">${[
    ['umumiy', 'Umumiy', jami],
    ['joylashuv', 'Joylashuv', d.mamlakatlar.length],
    ['versiya', 'Versiya', d.versiyalar.length],
    ['amallar', 'Amallar', amalJami],
    ['qurilma', 'Qurilma', cpular.length],
  ].map(([id, nom, son]) =>
    `<a href="#${id}"><span>${q(nom)}</span><em>${q(raqam(son))}</em></a>`).join('')}
  </nav>
  <div class="vaqt">Toshkent ${q(toshVaqt)}<br>UTC ${q(utcVaqt)}</div>
</aside>

<main class="asos"><div class="ichi">
<section id="umumiy">
  <div class="bolim-bosh"><h2>Umumiy holat</h2>
    <span class="raqam">${q(raqam(jami))} oʻrnatma · ${q(raqam(amalJami))} amal</span></div>

  <div class="setka k4">${boshKartalar}</div>
  <div class="setka k6" style="margin-top:12px">${kichikKartalar}</div>

  <h3>Kunlik faollik <span>· 30 kun</span></h3>
  ${quti(`<div style="padding:12px 14px 6px">${maydonChizma(faolEgri, { birlik: 'faol oʻrnatma' })}</div>`)}

  <h3>Yangi oʻrnatma <span>· 30 kun</span></h3>
  ${quti(`<div style="padding:12px 14px 6px">${ustunChizma(
      yangiEgri.map((p) => ({ yorliq: sanaQisqa(p.sana), tavsif: sanaToliq(p.sana), qiymat: { n: p.n } })),
      [{ kalit: 'n', nom: 'Yangi' }], { xHar: 5, birlik: 'yangi oʻrnatma' })}</div>`)}

  <h3>Necha kun ishlatilgan</h3>
  ${d.chuqurlik.length
      ? royxat(d.chuqurlik.map((r) => ({
          yorliq: CHUQURLIK_NOMI[r.guruh] || String(r.guruh), n: Number(r.n),
        })), { jami })
      : yoq('Hali maʼlumot yoʻq', '')}
</section>

<section id="joylashuv">
  <div class="bolim-bosh"><h2>Joylashuv</h2>
    <span class="raqam">${q(raqam(d.mamlakatlar.length))} mamlakat · ${q(raqam(d.shaharlar.length))} shahar ·
      ${q(raqam(tarmoq))} tarmoq</span></div>

  <h3>Mamlakatlar</h3>
  ${d.mamlakatlar.length
      ? royxat(d.mamlakatlar.slice(0, 12).map((r) => ({
          oldi: bayroq(r.mamlakat),
          yorliq: mamlakatNomi(r.mamlakat),
          izoh: String(r.mamlakat || '').toUpperCase(),
          n: Number(r.n),
        })), { jami })
      : yoq('Hali maʼlumot yoʻq', '')}

  <h3>Shaharlar</h3>
  ${d.shaharlar.length
      ? royxat(d.shaharlar.slice(0, 12).map((r) => ({
          oldi: bayroq(r.mamlakat),
          yorliq: r.shahar || 'Aniqlanmagan',
          izoh: mamlakatNomi(r.mamlakat),
          n: Number(r.n),
        })), { jami })
      : yoq('Hali maʼlumot yoʻq', '')}
</section>

<section id="versiya">
  <div class="bolim-bosh"><h2>Platforma va versiya</h2>
    <span class="raqam">${platformaBolak.map((b) => q(b.nom) + ' ' + q(raqam(b.n))).join(' · ')}</span></div>

  ${quti(`<div style="padding:15px 16px">${ulushChiziq(platformaBolak)}</div>`)}

  <h3>Oxirgi versiyada</h3>
  <div class="setka k2">${platformaKartalar}</div>

  <h3>Versiyalar</h3>
  ${versiyaRoyxat || yoq('Hali maʼlumot yoʻq', '')}

  <h3>Operatsion tizim</h3>
  ${osRoyxat || yoq('Hali maʼlumot yoʻq', '')}
</section>

<section id="amallar">
  <div class="bolim-bosh"><h2>Amallar</h2>
    <span class="raqam">${q(raqam(amalJami))} amal · ${q(raqam(amalOdam))} oʻrnatmadan</span></div>

  <div class="setka k4">
    ${karta({ sinf: 'kichik', son: raqam(stt ? stt.n : 0), nom: 'Transkripsiya',
      izoh: 'Ovozdan matnga oʻgirilgan hollar soni.',
      ost: `${q(foiz(stt ? Number(stt.n) : 0, amalJami))}` })}
    ${karta({ sinf: 'kichik', son: raqam(tarjima ? tarjima.n : 0), nom: 'Tarjima',
      izoh: 'Oflayn tarjimon ishlatilgan hollar soni.',
      ost: `${q(foiz(tarjima ? Number(tarjima.n) : 0, amalJami))}` })}
    ${karta({ sinf: 'kichik', son: amalJami ? foiz(amalOk, amalJami) : '—', nom: 'Muvaffaqiyat',
      izoh: 'Xatosiz tugagan amallar ulushi.',
      ost: amalJami - amalOk
        ? `<span class="yomon">${q(raqam(amalJami - amalOk))} xato</span>`
        : '<span class="yaxshi">xato yoʻq</span>' })}
    ${karta({ sinf: 'kichik', son: stt && stt.ort_vaqt ? davomiylik(stt.ort_vaqt) : '—',
      nom: 'Oʻrtacha ishlov',
      izoh: 'Bitta transkripsiya oʻrtacha qancha vaqt olgan.',
      ost: stt && stt.eng_vaqt ? `eng uzuni ${q(davomiylik(stt.eng_vaqt))}` : '' })}
  </div>

  <h3>Kunlik amallar <span>· 30 kun</span></h3>
  ${!amalJami ? yoq('Hali amal yoʻq', '') : quti(`<div style="padding:12px 14px 10px">
    ${ustunChizma(amalNuqta, amalQatlam, { xHar: 5, birlik: 'amal' })}
    <div style="padding:0 2px">${afsona([
      { nom: 'Transkripsiya', n: stt ? Number(stt.n) : 0, rang: 'var(--accent)' },
      { nom: 'Tarjima', n: tarjima ? Number(tarjima.n) : 0, rang: 'var(--accent2)' },
    ], amalJami)}</div></div>`)}

  <h3>Kun boʻyi <span>· Toshkent vaqti</span></h3>
  ${amalJami
      ? quti(`<div style="padding:12px 14px 6px">${ustunChizma(soatNuqta,
          [{ kalit: 'n', nom: 'Amal' }], { xHar: 3, balandlik: 140, birlik: 'amal' })}</div>`)
      : yoq('Hali amal yoʻq', '')}

  <h3>Tur boʻyicha</h3>
  ${d.amalTur.length
      ? jadval(['Tur', 'Soni', 'Tezlik', 'Oʻrtacha', 'Eng uzun', 'Hajm'],
          d.amalTur.map((r) => [
            r.tur === 'stt' ? 'Transkripsiya' : r.tur === 'tarjima' ? 'Tarjima' : r.tur,
            raqam(r.n),
            r.tur === 'stt'
              ? (r.rtf ? r.rtf + '×' : '—')
              : (r.belgi_s ? raqam(r.belgi_s) + ' belgi/s' : '—'),
            davomiylik(r.ort_vaqt),
            davomiylik(r.eng_vaqt),
            r.tur === 'stt' ? davomiylik(r.ovoz_s) : (r.belgi ? raqam(r.belgi) + ' belgi' : '—'),
          ]))
      : yoq('Hali amal yoʻq', '')}

  <h3>Backend</h3>
  ${d.amalBackend.length
      ? jadval(['Backend', 'Amal', 'Tezlik', 'Oʻrtacha', 'Jami ovoz'],
          d.amalBackend.map((r) => [
            { xom: true, html: `<b>${q(BACKEND_NOMI[r.backend] || r.backend || 'Aniqlanmagan')}</b>` +
              (BACKEND_IZOHI[r.backend] ? ` <span style="color:var(--ink-mute)">— ${q(BACKEND_IZOHI[r.backend])}</span>` : '') },
            raqam(r.n),
            r.rtf ? r.rtf + '×' : '—',
            davomiylik(r.ort_vaqt),
            r.soat ? r.soat + ' soat' : '—',
          ]))
      : yoq('Hali maʼlumot yoʻq', '')}

  <h3>Ovoz uzunligi</h3>
  ${d.ovozUzun.length
      ? royxat(d.ovozUzun.map((r) => ({
          yorliq: OVOZ_NOMI[r.guruh] || String(r.guruh), n: Number(r.n),
        })), { jami: stt ? Number(stt.n) : null })
      : yoq('Hali maʼlumot yoʻq', '')}

  <h3>Xatolar</h3>
  ${d.amalXato.length
      ? royxat(d.amalXato.map((r) => ({
          yorliq: XATO_NOMI[r.natija] || r.natija,
          izoh: r.tur === 'stt' ? 'transkripsiyada' : 'tarjimada',
          n: Number(r.n), rang: 'var(--xato)',
        })), { jami: amalJami })
      : yoq('Xato yoʻq', amalJami ? 'Hamma amal muvaffaqiyatli tugagan.' : '')}
</section>

<section id="qurilma">
  <div class="bolim-bosh"><h2>Qurilma muhiti</h2>
    <span class="raqam">${q(raqam(gpular.length))} videokarta · ${q(raqam(cpular.length))} protsessor</span></div>

  <h3>Arxitektura</h3>
  ${arxBolak.length
      ? quti(`<div style="padding:15px 16px">${ulushChiziq(arxBolak)}</div>`)
      : yoq('Hali maʼlumot yoʻq', '')}

  <h3>Videokarta</h3>
  ${gpuUlush.length ? quti(`<div style="padding:15px 16px">${ulushChiziq(gpuUlush)}</div>`) : ''}
  ${gpular.length
      ? royxat(gpular.slice(0, 10).map((e) => ({ yorliq: e.nom, n: e.n, rang: rangla(e) })), { jami })
      : yoq('Hali maʼlumot yoʻq', '')}

  <h3>Protsessor</h3>
  ${cpuUlush.length ? quti(`<div style="padding:15px 16px">${ulushChiziq(cpuUlush)}</div>`) : ''}
  ${cpular.length
      ? royxat(cpular.slice(0, 10).map((e) => ({ yorliq: e.nom, n: e.n, rang: rangla(e) })), { jami })
      : yoq('Hali maʼlumot yoʻq', '')}

  <h3>Xotira va yadro</h3>
  <div class="setka k2">
    ${ramlar.length ? royxat(ramlar, { jami }) : yoq('Maʼlumot yoʻq', '')}
    ${d.qurYadro.length
      ? royxat(d.qurYadro.map((r) => ({ yorliq: `${r.yadro} yadro`, n: Number(r.n) })), { jami })
      : yoq('Maʼlumot yoʻq', '')}
  </div>
</section>

<p class="pastki">Anonim: ovoz, matn, fayl nomlari va xom IP yigʻilmaydi. Kunlar UTC boʻyicha,
amallar 60 kun saqlanadi. Raqam nimani bildirishini bilish uchun karta ustiga sichqonchani olib boring.</p>
</div></main>
</div>

<script>
// Sahifadagi yagona skript: yon panelda hozir oʻqilayotgan boʻlimni belgilaydi.
// U ishlamay qolsa ham havolalar oddiy langar sifatida ishlayveradi.
(function () {
  var havolalar = [].slice.call(document.querySelectorAll('.yol a'));
  var bolimlar = havolalar.map(function (a) { return document.querySelector(a.hash); });
  function yangila() {
    var joriy = 0;
    bolimlar.forEach(function (b, i) {
      if (b && b.getBoundingClientRect().top <= 120) joriy = i;
    });
    havolalar.forEach(function (a, i) { a.classList.toggle('joriy', i === joriy); });
  }
  addEventListener('scroll', yangila, { passive: true });
  yangila();
})();
</script>
</body></html>`;

  return new Response(html, {
    headers: {
      'Content-Type': 'text/html; charset=utf-8',
      'Cache-Control': 'no-store',
      'Referrer-Policy': 'no-referrer',
      'X-Robots-Tag': 'noindex, nofollow',
    },
  });
}
