// `saqlanmagan.cpp` testlari — macOS'dagi `tests/test_saqlanmagan.swift` bilan
// bir xil holatlar: ikkala platforma bir xil WAV yozadi, bir xil nom beradi
// va bir xil fayllarni oʻchiradi.
#include "../core/saqlanmagan.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan);
}  // namespace kotib_test

using kotib_test::tekshir;
using kotib_test::tengmi;
using namespace rubai;
namespace sq = rubai::saqlanmagan;
namespace fs = std::filesystem;

void saqlanmaganTestlari() {
    // ---- WAV aylanmasi ----
    {
        const std::vector<float> s = {0, 0.5f, -0.5f, 1, -1, 0.25f, 1.7f, -3, NAN};
        const std::string d = sq::wav(s);
        tekshir(L"wav hajmi", d.size() == 44 + s.size() * 2);
        tekshir(L"wav RIFF", d.compare(0, 4, "RIFF") == 0);
        const unsigned long riff = (unsigned char)d[4] | (unsigned char)d[5] << 8 |
                                   (unsigned char)d[6] << 16 |
                                   (unsigned long)(unsigned char)d[7] << 24;
        tekshir(L"wav RIFF hajmi", riff == d.size() - 8);
        std::vector<float> q;
        tekshir(L"wav oʻqildi", sq::namunalar(d, q));
        tekshir(L"wav soni", q.size() == s.size());
        const std::vector<float> kutilgan = {0, 0.5f, -0.5f, 1, -1, 0.25f, 1, -1, 0};
        bool mos = q.size() == kutilgan.size();
        for (size_t i = 0; mos && i < q.size(); ++i) {
            mos = std::fabs(q[i] - kutilgan[i]) <= 0.5f / 32767 + 1e-7f;
        }
        tekshir(L"wav qiymatlar", mos);
        tekshir(L"wav boʻsh", sq::namunalar(sq::wav({}), q) && q.empty());
    }
    // ---- begona WAV ----
    {
        std::string d = sq::wav({0.1f, 0.2f});
        std::vector<float> q;
        tekshir(L"44 baytdan qisqa", !sq::namunalar(d.substr(0, 43), q));
        std::string stereo = d;
        stereo[22] = 2;
        tekshir(L"stereo rad", !sq::namunalar(stereo, q));
        tekshir(L"boshqa chastota rad", !sq::namunalar(sq::wav({0.1f}, 44100), q));
        tekshir(L"WAV emas", !sq::namunalar(std::string(100, '\0'), q));
        const std::string toliq = sq::wav({0.1f, 0.2f, 0.3f});
        tekshir(L"kesilgan — borini oʻqiydi",
                sq::namunalar(toliq.substr(0, 44 + 4), q) && q.size() == 2);
    }
    // ---- nom ↔ sana ----
    {
        const long long t = 1791000000123LL;
        const std::wstring n = sq::nom(t);
        tekshir(L"nom .wav", n.size() > 4 && n.compare(n.size() - 4, 4, L".wav") == 0);
        tekshir(L"nom aylanmasi", sq::sana(n) == t);
        tekshir(L"begona nom", sq::sana(L"ovoz.wav") == -1);
        tekshir(L"kengaytmasiz", sq::sana(n.substr(0, n.size() - 4)) == -1);
        std::wstring buzuq = n;
        buzuq[0] = L'x';
        tekshir(L"raqam oʻrnida harf", sq::sana(buzuq) == -1);
        tekshir(L"alifbo tartibi = vaqt tartibi", n < sq::nom(t + 1000));
    }
    // ---- 20 ta va 7 kun ----
    {
        const long long hozir = 1791000000000LL;
        auto f = [&](double soatOldin) {
            return sq::nom(hozir - (long long)(soatOldin * 3600 * 1000));
        };
        std::vector<std::wstring> yangilar;
        for (int i = 0; i < 25; ++i) yangilar.push_back(f(i));
        const auto och = sq::ortiqcha(yangilar, hozir);
        bool togri = och.size() == 5;
        for (int i = 20; togri && i < 25; ++i) {
            togri = std::find(och.begin(), och.end(), yangilar[i]) != och.end();
        }
        tekshir(L"20 dan ortigʻi", togri);
        const std::wstring eski = f(7 * 24 + 1), chegarada = f(7 * 24 - 1);
        const auto och7 = sq::ortiqcha({eski, chegarada, f(0)}, hozir);
        tekshir(L"7 kun", och7.size() == 1 && och7[0] == eski);
        tekshir(L"begona hech qachon", sq::ortiqcha({L"muhim.wav"}, hozir).empty());
    }
    // ---- fayl tizimi ----
    {
        std::mt19937 g(std::random_device{}());
        const fs::path papka =
            fs::temp_directory_path() / ("kotib-saqlanmagan-" + std::to_string(g()));
        const std::wstring p = papka.wstring();
        const long long hozir = 1791000000000LL;
        tekshir(L"papka yoʻq — boʻsh", sq::royxat(p).empty());
        const std::wstring birinchi = sq::saqla({0.1f, 0.2f}, p, hozir);
        tekshir(L"saqlandi", !birinchi.empty());
        { std::ofstream(papka / "begona.txt") << "x"; }
        for (int i = 1; i <= 21; ++i) sq::saqla({0.3f}, p, hozir + i * 1000);
        const auto r = sq::royxat(p);
        tekshir(L"20 tasi qoldi", r.size() == 20);
        tekshir(L"eskidan yangiga", std::is_sorted(r.begin(), r.end()));
        tekshir(L"eng eskisi oʻchdi", std::find(r.begin(), r.end(), birinchi) == r.end());
        tekshir(L"begona fayl joyida", fs::exists(papka / "begona.txt"));
        std::vector<float> q;
        tekshir(L"oxirgisi oʻqiladi", !r.empty() && sq::oqi(r.back(), q) && q.size() == 1);
        tekshir(L"yoʻq faylni oʻqish", !sq::oqi(p + L"/yoq.wav", q));
        std::error_code ec;
        fs::remove_all(papka, ec);
    }
}
