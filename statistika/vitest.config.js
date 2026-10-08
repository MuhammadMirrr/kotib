// Worker testlari Workers runtime'ining oʻzida (Miniflare) ishlaydi —
// bogʻlanishlar (D1, KV) har test uchun alohida va mahalliy, jonli bazaga tegmaydi.
//
//   npm test
import { defineConfig } from 'vitest/config';
import { cloudflareTest } from '@cloudflare/vitest-plugin';

export default defineConfig({
  plugins: [cloudflareTest({
    wrangler: { configPath: './wrangler.toml' },
    // Sirlar faqat test uchun (jonli qiymatlar `wrangler secret` da).
    miniflare: { bindings: { PANEL_KALIT: 'sinov-kaliti', IP_TUZ: 'sinov-tuzi' } },
  })],
});
