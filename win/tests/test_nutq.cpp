// `nutq_bolaklari.c` testlari (S12): ovozni whisper chaqiruvlariga boʻlish.
// Fayl macOS'dagi `src/nutq_bolaklari.c` bilan bayt-bayt bir xil
// (`hammasi.sh` solishtiradi), shuning uchun bu testlar ikkala platformani qamraydi.
#include "../core/nutq_bolaklari.h"

#include <string>
#include <vector>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
}  // namespace kotib_test

using kotib_test::tekshir;

namespace {

constexpr int kS = 16000;  // 1 soniya, namunada

// VAD boʻlaklari soniyada → santisekund; natija — oraliqlar.
std::vector<rubai_oraliq> bol(const std::vector<std::pair<float, float>>& b, int n) {
    std::vector<float> t0, t1;
    for (const auto& [a, z] : b) {
        t0.push_back(a * 100.0f);
        t1.push_back(z * 100.0f);
    }
    std::vector<rubai_oraliq> c(b.size() + 1);
    const int k =
        rubai_nutq_bolaklari(t0.data(), t1.data(), static_cast<int>(b.size()), n, c.data());
    c.resize(k > 0 ? k : 0);
    return c;
}

bool teng(const std::vector<rubai_oraliq>& c, const std::vector<std::pair<int, int>>& k) {
    if (c.size() != k.size()) return false;
    for (size_t i = 0; i < c.size(); i++)
        if (c[i].boshi != k[i].first || c[i].oxiri != k[i].second) return false;
    return true;
}

}  // namespace

void nutqTestlari() {
    tekshir(L"nutq: VAD hech narsa topmadi — whisper chaqirilmaydi", bol({}, 10 * kS).empty());
    tekshir(L"nutq: boʻsh ovoz", bol({{0, 1}}, 0).empty());
    tekshir(L"nutq: nullptr", rubai_nutq_bolaklari(nullptr, nullptr, 3, kS, nullptr) == 0);

    // ≤ 30 s: VAD faqat darvoza — butun ovoz bitta chaqiruvda.
    tekshir(L"nutq: qisqa diktovka butunligicha",
            teng(bol({{1.2f, 3.4f}}, 10 * kS), {{0, 10 * kS}}));
    tekshir(L"nutq: aniq 30 s — hali darvoza",
            teng(bol({{0, 10}, {20, 29}}, 30 * kS), {{0, 30 * kS}}));

    // > 30 s: jimliklardan ≤ 25 s guruhlar.
    tekshir(L"nutq: 30 s + 1 namuna — boʻlinadi",
            teng(bol({{0, 10}, {20, 29}}, 30 * kS + 1), {{0, 10 * kS}, {20 * kS, 29 * kS}}));
    tekshir(L"nutq: guruh 25 s gacha yigʻiladi",
            teng(bol({{0, 10}, {12, 20}, {22, 30}}, 120 * kS), {{0, 20 * kS}, {22 * kS, 30 * kS}}));
    tekshir(L"nutq: aniq 25 s — guruhga kiradi",
            teng(bol({{0, 10}, {15, 25}, {26, 40}}, 60 * kS), {{0, 25 * kS}, {26 * kS, 40 * kS}}));
    tekshir(L"nutq: 25 s dan uzun yakka boʻlak — oʻzi alohida",
            teng(bol({{5, 32}, {33, 40}}, 60 * kS), {{5 * kS, 32 * kS}, {33 * kS, 40 * kS}}));
    tekshir(L"nutq: chegaradan chiqqan vaqt qisiladi",
            teng(bol({{-0.5f, 10}, {50, 70}}, 60 * kS), {{0, 10 * kS}, {50 * kS, 60 * kS}}));
    tekshir(L"nutq: nol uzunlikdagi boʻlak tashlanadi",
            teng(bol({{40, 40}, {45, 50}}, 60 * kS), {{40 * kS, 50 * kS}}));
    tekshir(L"nutq: ovozdan keyin boshlangan boʻlak tashlanadi",
            teng(bol({{10, 20}, {70, 80}}, 60 * kS), {{10 * kS, 20 * kS}}));
    tekshir(L"nutq: santisekund → namuna (10 ms = 160)",
            teng(bol({{0.01f, 31.0f}}, 40 * kS), {{160, 31 * kS}}));
}
