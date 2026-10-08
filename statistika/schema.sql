-- Kotib statistikasi. D1 (SQLite).
--
-- Uch jadval:
--   ornatmalar — «nechta odamda oʻrnatilgan, kim qancha kun ishlatgan» + qurilma muhiti
--   kunlik     — «qaysi kuni nechta faol foydalanuvchi boʻlgan»
--   amallar    — har transkripsiya/tarjima: uzunlik, vaqt, tezlik, muhit (KONTENTSIZ)
--
-- Kunlik jadval alohida turadi, chunki uni `ornatmalar` dan hisoblab boʻlmaydi:
-- u yerda faqat OXIRGI koʻrilgan sana bor, oraliqdagi kunlar emas.
--
-- Jonli bazaga ustun qoʻshish uchun `migrate-*.sql` ga qarang — bu fayl faqat
-- toza baza yaratish uchun.

CREATE TABLE IF NOT EXISTS ornatmalar (
    id        TEXT PRIMARY KEY,   -- ilova yasagan tasodifiy 32-belgili hex
    platforma TEXT NOT NULL,      -- 'mac' yoki 'win'
    versiya   TEXT NOT NULL,
    os        TEXT,               -- masalan "Windows 11 26100" yoki "macOS 15.2"
    mamlakat  TEXT,               -- Cloudflare beradigan ikki harfli kod
    birinchi  TEXT NOT NULL,      -- birinchi ping sanasi (YYYY-MM-DD)
    oxirgi    TEXT NOT NULL,      -- oxirgi ping sanasi
    kunlar    INTEGER NOT NULL DEFAULT 1,  -- necha xil kunda ishlatilgan
    -- Qurilma muhiti (ping bilan yangilanadi):
    arx       TEXT,               -- 'arm64' | 'x64'
    cpu       TEXT,               -- protsessor modeli
    gpu       TEXT,               -- videokarta modeli
    ram_gb    INTEGER,
    yadro     INTEGER,            -- CPU yadrolari soni
    viloyat   TEXT,               -- Cloudflare (cf.region)
    shahar    TEXT,               -- Cloudflare (cf.city) — taxminiy joylashuv
    ip_hash   TEXT                -- kesilgan IP'ning hashi; xom IP hech qachon saqlanmaydi
);

-- «Oxirgi 30 kunda faol» soʻrovi eng koʻp ishlatiladigani.
CREATE INDEX IF NOT EXISTS ornatmalar_oxirgi ON ornatmalar(oxirgi);
CREATE INDEX IF NOT EXISTS ornatmalar_platforma ON ornatmalar(platforma);

CREATE TABLE IF NOT EXISTS kunlik (
    sana      TEXT NOT NULL,
    platforma TEXT NOT NULL,
    versiya   TEXT NOT NULL,
    faol      INTEGER NOT NULL DEFAULT 0,  -- shu kuni kelgan pinglar soni
    PRIMARY KEY (sana, platforma, versiya)
);

-- Har transkripsiya/tarjima uchun BITTA qator. Kontent yoʻq: faqat oʻlchovlar.
-- Eski qatorlar 60 kundan keyin `scheduled` handlerda avtomatik oʻchiriladi.
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
