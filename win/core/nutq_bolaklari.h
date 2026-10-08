// Ovozni whisper chaqiruvlariga boʻlish (S12) — sof C, whisper'siz.
//
// Nega: bu fine-tune modelda whisper oynasi 30 s qatʼiy suriladi
// (`no_timestamps=true`) va dekoder oynada chiqarmagan narsa butunlay
// yoʻqoladi — uzun ovozda jumlalar tushib qolardi (oʻlchov: 96 tadan 14).
// `no_speech_prob` esa bu modelda doim ~1e-10, shuning uchun jimlikdagi
// «musiqa» gallyutsinatsiyasini parametr bilan toʻxtatib boʻlmaydi. Yechim —
// Silero VAD: nutq yoʻq boʻlsa whisper chaqirilmaydi; uzun ovoz faqat VAD
// topgan jimliklardan ≤ 25 s boʻlaklarga boʻlinadi va har boʻlak alohida
// oʻgiriladi. Oʻlchov va qaror: docs/superpowers/specs (barqarorlik B2, S12).
//
// macOS (`src/`) va Windows (`win/core/`) nusxalari BAYT-BAYT bir xil —
// `win/tests/mac/hammasi.sh` solishtiradi; testlar `win/tests/test_nutq.cpp`.
#ifndef RUBAI_NUTQ_BOLAKLARI_H
#define RUBAI_NUTQ_BOLAKLARI_H

#ifdef __cplusplus
extern "C" {
#endif

// Shu uzunlikkacha (soniya) ovoz boʻlinmaydi: VAD faqat DARVOZA — nutq
// topilsa butun ovoz bitta chaqiruvda (odatdagi diktovka, sifat oʻzgarmaydi).
#define RUBAI_DARVOZA_S 30
// Uzun ovozda ketma-ket VAD boʻlaklari shu uzunlikkacha (soniya) jamlanadi:
// whisper oynasi 30 s, zaxira bilan. Mayda boʻlaklar alohida oʻgirilsa
// (jamlanmasa) shovqindagi qisqa «nutq» «musiqa» boʻlib chiqardi.
#define RUBAI_GURUH_S 25

typedef struct {
    int boshi;  // namuna indeksi (16 kHz), shu jumladan
    int oxiri;  // namuna indeksi, shu jumladan EMAS
} rubai_oraliq;

// VAD boʻlaklari (`t0`, `t1` — santisekund, oʻsish tartibida) → whisper
// chaqiruvlari uchun namuna oraliqlari. Natija `chiqish` ga yoziladi (kamida
// `n_bolak` joy; `n_bolak` 0 boʻlsa ham 1 joy berish xavfsiz), qaytgan qiymat —
// oraliqlar soni; 0 — nutq yoʻq (whisper chaqirilmasin).
int rubai_nutq_bolaklari(const float *t0, const float *t1, int n_bolak, int n_namuna,
                         rubai_oraliq *chiqish);

#ifdef __cplusplus
}
#endif

#endif
