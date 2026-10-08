-- Kotib statistika — kengaytirilgan monitoring migratsiyasi (2026-09).
--
-- Bu fayl JONLI bazaga xavfsiz qoʻllanadi: mavjud maʼlumot oʻchmaydi.
-- `ALTER TABLE ADD COLUMN` SQLite'da mavjud qatorlarga NULL qiymat beradi.
-- Yangi oʻrnatmalar ping bilan bu ustunlarni toʻldiradi; eskilarida NULL qoladi.
--
--   wrangler d1 execute kotib-statistika --remote --file=migrate-2026-09-monitoring.sql

-- 1) Qurilma muhiti — ping bilan bir marta yoziladi va yangilanadi.
ALTER TABLE ornatmalar ADD COLUMN arx     TEXT;   -- 'arm64' | 'x64'
ALTER TABLE ornatmalar ADD COLUMN cpu     TEXT;   -- protsessor modeli
ALTER TABLE ornatmalar ADD COLUMN gpu     TEXT;   -- videokarta modeli
ALTER TABLE ornatmalar ADD COLUMN ram_gb  INTEGER;
ALTER TABLE ornatmalar ADD COLUMN yadro   INTEGER; -- CPU yadrolari soni
ALTER TABLE ornatmalar ADD COLUMN viloyat TEXT;   -- Cloudflare (cf.region)
ALTER TABLE ornatmalar ADD COLUMN shahar  TEXT;   -- Cloudflare (cf.city) — taxminiy
ALTER TABLE ornatmalar ADD COLUMN ip_hash TEXT;   -- kesilgan IP'ning hashi; xom IP EMAS

-- 2) Amallar — har transkripsiya/tarjima uchun BITTA qator (maksimal keng).
--    Kontent yoʻq: faqat uzunlik, vaqt, tezlik, muhit.
CREATE TABLE IF NOT EXISTS amallar (
    id        TEXT NOT NULL,      -- anonim oʻrnatma ID (ornatmalar.id)
    vaqt      INTEGER NOT NULL,   -- unix epoch (soniya) — server qoʻyadi
    sana      TEXT NOT NULL,      -- YYYY-MM-DD — soʻrov va tozalash uchun
    platforma TEXT NOT NULL,      -- 'mac' | 'win'
    versiya   TEXT NOT NULL,
    tur       TEXT NOT NULL,      -- 'stt' | 'tarjima'
    ovoz_s    REAL,               -- ovoz uzunligi, soniya (stt)
    belgi     INTEGER,            -- belgi soni (tarjima)
    ishlov_s  REAL NOT NULL,      -- qancha vaqtda tayyor boʻldi
    backend   TEXT,               -- 'metal' | 'vulkan' | 'cpu'
    natija    TEXT NOT NULL       -- 'ok' | 'xato:<kod>'
);
CREATE INDEX IF NOT EXISTS amallar_sana ON amallar(sana);
CREATE INDEX IF NOT EXISTS amallar_id   ON amallar(id);
CREATE INDEX IF NOT EXISTS amallar_tur  ON amallar(tur);
