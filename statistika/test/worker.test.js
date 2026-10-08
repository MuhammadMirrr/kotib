// Worker testlari: avto-yangilanish endpoint'lari va /v1/ping ning D1'dan
// mustaqilligi (barqarorlik spec'i, H1).
import { createExecutionContext, waitOnExecutionContext } from 'cloudflare:test';
import { env, exports } from 'cloudflare:workers';
import { beforeAll, describe, expect, it, vi } from 'vitest';
import worker, { ipTarmogi } from '../src/index.js';
import sxema from '../schema.sql?raw';

const ID = '0123456789abcdef0123456789abcdef';
const JORIY = {
  mac: { versiya: '1.1.0', url: 'https://uzb.mirqobilov.com', izoh: 'mac izohi' },
  win: { versiya: '1.1.0', url: 'https://uzb.mirqobilov.com', izoh: 'win izohi' },
};
const SIYOSAT = '{"m":"eyJwbGF0Zm9ybWEiOiJ3aW4ifQ==","s":"' + 'A'.repeat(86) + '=="}';

const soʻra = (yol, init) => exports.default.fetch(new Request('https://stat.mirqobilov.com' + yol, init));
const ping = (tana = { id: ID, platforma: 'win', versiya: '1.1.0', os: 'Windows 11' }) =>
  new Request('https://stat.mirqobilov.com/v1/ping', { method: 'POST', body: JSON.stringify(tana) });

beforeAll(async () => {
  // schema.sql ni D1 ga: izohlar olib tashlanadi, `;` boʻyicha boʻlinadi.
  const toza = sxema.replace(/--.*$/gm, '');
  const soʻrovlar = toza.split(';').map((s) => s.trim()).filter(Boolean);
  await env.DB.batch(soʻrovlar.map((s) => env.DB.prepare(s)));
});

describe('avto-yangilanish endpoint\'lari', () => {
  it('reliz boʻlmasa — 404, lekin keshlanadi', async () => {
    const j = await soʻra('/v1/yangilanish/win.json');
    expect(j.status).toBe(404);
    expect(j.headers.get('Cache-Control')).toBe('public, max-age=300');
  });

  it('siyosat KV dagi baytlarni aynan qaytaradi (imzo buzilmasin)', async () => {
    await env.VERSIYALAR.put('siyosat-win', SIYOSAT);
    const j = await soʻra('/v1/yangilanish/win.json');
    expect(j.status).toBe(200);
    expect(j.headers.get('Content-Type')).toBe('application/json; charset=utf-8');
    expect(j.headers.get('Cache-Control')).toBe('public, max-age=300');
    expect(await j.text()).toBe(SIYOSAT);
  });

  it('mac va win siyosati alohida kalitlarda', async () => {
    await env.VERSIYALAR.put('siyosat-mac', '{"m":"bWFj","s":"x"}');
    expect(await (await soʻra('/v1/yangilanish/mac.json')).text()).toBe('{"m":"bWFj","s":"x"}');
  });

  it('sinov kanali — alohida KV yozuvi, asosiysiga tegmaydi', async () => {
    await env.VERSIYALAR.put('siyosat-win', SIYOSAT);
    await env.VERSIYALAR.put('siyosat-win-sinov', '{"m":"c2lub3Y=","s":"x"}');
    expect(await (await soʻra('/v1/yangilanish/win-sinov.json')).text()).toBe('{"m":"c2lub3Y=","s":"x"}');
    expect(await (await soʻra('/v1/yangilanish/win.json')).text()).toBe(SIYOSAT);
    expect((await soʻra('/v1/yangilanish/mac-sinov.json')).status).toBe(404);
    expect((await soʻra('/v1/appcast/mac-sinov.xml')).status).toBe(404);
  });

  it('appcast XML sifatida beriladi', async () => {
    const xml = '<?xml version="1.0"?><rss><channel><item/></channel></rss>';
    await env.VERSIYALAR.put('appcast-mac', xml);
    const j = await soʻra('/v1/appcast/mac.xml');
    expect(j.status).toBe(200);
    expect(j.headers.get('Content-Type')).toBe('application/xml; charset=utf-8');
    expect(await j.text()).toBe(xml);
  });

  it('HEAD — GET bilan bir xil holat va sarlavhalar', async () => {
    await env.VERSIYALAR.put('siyosat-win', SIYOSAT);
    const j = await soʻra('/v1/yangilanish/win.json', { method: 'HEAD' });
    expect(j.status).toBe(200);
    expect(j.headers.get('Cache-Control')).toBe('public, max-age=300');
  });

  it('faqat GET/HEAD; boshqa platforma yoʻq', async () => {
    expect((await soʻra('/v1/yangilanish/win.json', { method: 'POST', body: '{}' })).status).toBe(404);
    expect((await soʻra('/v1/yangilanish/linux.json')).status).toBe(404);
  });

  it('KV yiqilsa — 503 va keshlanmaydi', async () => {
    const buzuqEnv = { ...env, VERSIYALAR: { get: async () => { throw new Error('KV yoʻq'); } } };
    const xato = vi.spyOn(console, 'error').mockImplementation(() => {});
    const j = await worker.fetch(new Request('https://x/v1/yangilanish/win.json'), buzuqEnv, createExecutionContext());
    expect(j.status).toBe(503);
    expect(j.headers.get('Cache-Control')).toBe('no-store');
    xato.mockRestore();
  });
});

describe('/v1/ping — D1 dan mustaqil (H1)', () => {
  it('oddiy holat: versiya javobi va D1 yozuvi', async () => {
    await env.VERSIYALAR.put('joriy', JSON.stringify(JORIY));
    const ctx = createExecutionContext();
    const j = await worker.fetch(ping(), env, ctx);
    await waitOnExecutionContext(ctx);
    expect(j.status).toBe(200);
    expect(await j.json()).toEqual(JORIY.win);
    const qator = await env.DB.prepare('SELECT platforma, versiya FROM ornatmalar WHERE id = ?1').bind(ID).first();
    expect(qator).toEqual({ platforma: 'win', versiya: '1.1.0' });
  });

  it('D1 yiqilsa ham javob keladi — yangilanish xabari yoʻqolmaydi', async () => {
    await env.VERSIYALAR.put('joriy', JSON.stringify(JORIY));
    const yiqilgan = { prepare: () => ({ bind: () => ({ run: async () => { throw new Error('D1 kvota'); } }) }),
                       batch: async () => { throw new Error('D1 kvota tugadi'); } };
    const xato = vi.spyOn(console, 'error').mockImplementation(() => {});
    const ctx = createExecutionContext();
    const j = await worker.fetch(ping(), { ...env, DB: yiqilgan }, ctx);
    await waitOnExecutionContext(ctx);
    expect(j.status).toBe(200);
    expect(await j.json()).toEqual(JORIY.win);
    expect(xato).toHaveBeenCalledWith(expect.stringContaining('ping: D1 yozuvi muvaffaqiyatsiz'));
    xato.mockRestore();
  });

  it('KV ham yiqilsa — versiya null, lekin 200', async () => {
    const buzuqEnv = { ...env, VERSIYALAR: { get: async () => { throw new Error('KV'); } } };
    const xato = vi.spyOn(console, 'error').mockImplementation(() => {});
    const ctx = createExecutionContext();
    const j = await worker.fetch(ping(), buzuqEnv, ctx);
    await waitOnExecutionContext(ctx);
    expect(j.status).toBe(200);
    expect(await j.json()).toEqual({ versiya: null });
    xato.mockRestore();
  });

  it('notoʻgʻri soʻrov hali ham 400 (tekshiruv oʻzgarmadi)', async () => {
    const ctx = createExecutionContext();
    expect((await worker.fetch(ping({ id: 'yomon', platforma: 'win', versiya: '1.1.0' }), env, ctx)).status).toBe(400);
  });

  it('/v1/amal D1 yiqilsa ham ok qaytaradi', async () => {
    const yiqilgan = { prepare: () => ({ bind: () => ({ run: async () => { throw new Error('D1'); } }) }) };
    const xato = vi.spyOn(console, 'error').mockImplementation(() => {});
    const ctx = createExecutionContext();
    const soʻrov = new Request('https://x/v1/amal', { method: 'POST', body: JSON.stringify(
      { id: ID, platforma: 'mac', versiya: '1.1.0', tur: 'stt', ishlov_s: 1.5, natija: 'ok' }) });
    const j = await worker.fetch(soʻrov, { ...env, DB: yiqilgan }, ctx);
    await waitOnExecutionContext(ctx);
    expect(j.status).toBe(200);
    expect(await j.json()).toEqual({ ok: true });
    expect(xato).toHaveBeenCalledWith(expect.stringContaining('amal: D1 yozuvi muvaffaqiyatsiz'));
    xato.mockRestore();
  });
});

describe('Worker himoyasi (S18: H2, H3, H4)', () => {
  const ID2 = 'fedcba9876543210fedcba9876543210';
  const post = (yol, tana, ip = '203.0.113.7', qoʻshimcha = {}) => exports.default.fetch(new Request(
    'https://stat.mirqobilov.com' + yol,
    { method: 'POST', body: typeof tana === 'string' ? tana : JSON.stringify(tana),
      headers: { 'CF-Connecting-IP': ip, ...qoʻshimcha } }));

  it('POST javobida CORS yoʻq; OPTIONS — 404', async () => {
    const j = await post('/v1/ping', { id: ID, platforma: 'mac', versiya: '1.1.0' }, '198.51.100.1');
    expect(j.status).toBe(200);
    expect(j.headers.get('Access-Control-Allow-Origin')).toBeNull();
    expect((await soʻra('/v1/ping', { method: 'OPTIONS' })).status).toBe(404);
  });

  it('katta tana — 413', async () => {
    const katta = JSON.stringify({ id: ID, platforma: 'mac', versiya: '1.1.0', os: 'x'.repeat(3000) });
    expect((await post('/v1/ping', katta, '198.51.100.2')).status).toBe(413);
    expect((await post('/v1/amal', katta, '198.51.100.2')).status).toBe(413);
  });

  it('bitta IP dan daqiqasiga 60 dan ortiq — 429', async () => {
    const tana = { id: ID, platforma: 'win', versiya: '1.1.0', tur: 'stt', ishlov_s: 1 };
    const holatlar = [];
    for (let i = 0; i < 65; i++) holatlar.push((await post('/v1/amal', tana, '192.0.2.99')).status);
    expect(holatlar.slice(0, 60).every((h) => h === 200)).toBe(true);
    expect(holatlar.slice(60)).toContain(429);
    // Boshqa IP'ga taʼsir qilmaydi.
    expect((await post('/v1/amal', tana, '192.0.2.100')).status).toBe(200);
  });

  it('/v1/versiya olib tashlangan; nomaʼlum yoʻl — JSON 404', async () => {
    const j = await soʻra('/v1/versiya?platforma=mac');
    expect(j.status).toBe(404);
    expect(await j.json()).toEqual({ xato: 'topilmadi' });
  });

  it('panel: kalitsiz va URL kalit bilan ochilmaydi (401), notoʻgʻri parol — 401', async () => {
    for (const yol of ['/panel', '/panel?kalit=sinov-kaliti']) {
      const j = await soʻra(yol);
      expect(j.status).toBe(401);
      expect(j.headers.get('WWW-Authenticate')).toContain('Basic');
      expect(j.headers.get('Cache-Control')).toBe('no-store');
    }
    const yomon = await soʻra('/panel', { headers: { Authorization: 'Basic ' + btoa('egasi:notogri-kalit') } });
    expect(yomon.status).toBe(401);
  });

  it('panel: toʻgʻri kalit (parol maydonida) — 200 HTML', async () => {
    const j = await soʻra('/panel', { headers: { Authorization: 'Basic ' + btoa('istalgan:sinov-kaliti') } });
    expect(j.status).toBe(200);
    expect(j.headers.get('Content-Type')).toContain('text/html');
  });

  it('kunlik faol: bir oʻrnatma bir kunda bir marta sanaladi', async () => {
    const yubor = async (id) => {
      const ctx = createExecutionContext();
      await worker.fetch(new Request('https://x/v1/ping', { method: 'POST',
        body: JSON.stringify({ id, platforma: 'win', versiya: '9.9.9' }) }), env, ctx);
      await waitOnExecutionContext(ctx);
    };
    await yubor(ID2); await yubor(ID2); await yubor(ID2);
    const faol = async () => (await env.DB.prepare(
      "SELECT faol FROM kunlik WHERE versiya = '9.9.9' AND platforma = 'win'").first())?.faol;
    expect(await faol()).toBe(1);
    await yubor('00000000000000000000000000000001');
    expect(await faol()).toBe(2);
  });

  it('IPv6 /48 kengaytirilgan holda kesiladi', () => {
    expect(ipTarmogi('2001:db8::1')).toBe('2001:db8:0');
    expect(ipTarmogi('2001:0db8:0000:1234::5')).toBe('2001:db8:0');
    expect(ipTarmogi('2001:db8:1::')).toBe('2001:db8:1');
    expect(ipTarmogi('::1')).toBe('0:0:0');
    expect(ipTarmogi('203.0.113.45')).toBe('203.0.113');
  });
});
