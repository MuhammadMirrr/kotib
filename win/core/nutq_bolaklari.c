// Ovozni whisper chaqiruvlariga boʻlish — `nutq_bolaklari.h` ga qarang.
// macOS va Windows nusxalari BAYT-BAYT bir xil.
#include "nutq_bolaklari.h"

static int namunaga(float santisekund, int n_namuna) {
    int s = (int)(santisekund / 100.0f * 16000.0f + 0.5f);
    if (s < 0) s = 0;
    if (s > n_namuna) s = n_namuna;
    return s;
}

int rubai_nutq_bolaklari(const float *t0, const float *t1, int n_bolak, int n_namuna,
                         rubai_oraliq *chiqish) {
    if (n_bolak <= 0 || n_namuna <= 0 || !t0 || !t1 || !chiqish) return 0;

    // Qisqa ovoz: nutq bor — butun ovoz, bitta chaqiruv.
    if (n_namuna <= RUBAI_DARVOZA_S * 16000) {
        chiqish[0].boshi = 0;
        chiqish[0].oxiri = n_namuna;
        return 1;
    }

    int n = 0;
    for (int i = 0; i < n_bolak;) {
        const float boshi = t0[i];
        float oxiri = t1[i];
        int j = i;
        // Keyingi boʻlak (orasidagi jimlik bilan) guruhga sigʻsa — qoʻshiladi.
        while (j + 1 < n_bolak && (t1[j + 1] - boshi) / 100.0f <= (float)RUBAI_GURUH_S) {
            j++;
            oxiri = t1[j];
        }
        const int s0 = namunaga(boshi, n_namuna);
        const int s1 = namunaga(oxiri, n_namuna);
        if (s1 > s0) {
            chiqish[n].boshi = s0;
            chiqish[n].oxiri = s1;
            n++;
        }
        i = j + 1;
    }
    return n;
}
