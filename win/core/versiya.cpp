// Versiya raqamlarini solishtirish.
//
// Nega alohida fayl: bu sof mantiq va u serverdan kelgan javobga qarab
// «yangilanish bormi» degan qarorni beradi — xato boʻlsa foydalanuvchi
// yangilanishni umuman koʻrmaydi yoki har kuni koʻraveradi. Alohida turgani
// uchun testlar uni **macOS'da** ham ishga tushira oladi
// (`win/tests/mac/sinov.sh`); yangilovchi va statistika esa WinHTTP'ga
// bogʻlangan.
//
// macOS'dagi egizagi — `src/yangilanish_siyosat.swift` dagi `taqqosla` va
// `versiyaFormatimi`. Holatlar umumiy: `tests/umumiy/yangilanish_holatlari.def`.
#include "yangilanish_siyosat.h"

#include <string>
#include <vector>

namespace rubai {

namespace {

bool raqammi(char c) { return c >= '0' && c <= '9'; }
bool harfmi(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// "1.2.0-rc1" → raqam "1.2.0", qoʻshimcha "rc1" (bor = true).
void ikkigaBol(std::string_view v, std::string_view& raqam, std::string_view& qoshimcha,
               bool& bor) {
    const size_t i = v.find('-');
    bor = i != std::string_view::npos;
    raqam = bor ? v.substr(0, i) : v;
    qoshimcha = bor ? v.substr(i + 1) : std::string_view{};
}

// '.' boʻyicha boʻlish; boʻsh boʻlaklar ham saqlanadi (Swift'dagi
// `omittingEmptySubsequences: false` bilan bir xil).
std::vector<std::string_view> bol(std::string_view s, char ajratgich) {
    std::vector<std::string_view> v;
    size_t boshi = 0;
    while (true) {
        const size_t i = s.find(ajratgich, boshi);
        if (i == std::string_view::npos) {
            v.push_back(s.substr(boshi));
            break;
        }
        v.push_back(s.substr(boshi, i - boshi));
        boshi = i + 1;
    }
    return v;
}

// Faqat raqamlardan iborat boʻlsa — son (999 999 999 da toʻxtaydi, toshmasin),
// aks holda 0. Avvalgi `std::stoi` "1a" ni 1 deb oʻqirdi, Swift esa 0 —
// endi ikkalasi bir xil.
int sonYokiNol(std::string_view s) {
    if (s.empty()) return 0;
    long long n = 0;
    for (char c : s) {
        if (!raqammi(c)) return 0;
        n = n * 10 + (c - '0');
        if (n > 999999999) return 999999999;
    }
    return static_cast<int>(n);
}

// "sinov9" va "sinov10": raqam va harf parchalari alohida, raqamlar son
// sifatida. Raqam parchasi harf parchasidan kichik (semver).
int identifikatorTaqqosla(std::string_view a, std::string_view b) {
    auto parchalar = [](std::string_view s) {
        std::vector<std::string_view> v;
        size_t i = 0;
        while (i < s.size()) {
            size_t j = i;
            while (j < s.size() && raqammi(s[j]) == raqammi(s[i])) ++j;
            v.push_back(s.substr(i, j - i));
            i = j;
        }
        return v;
    };
    const auto x = parchalar(a), y = parchalar(b);
    for (size_t i = 0; i < x.size() && i < y.size(); ++i) {
        const bool xr = raqammi(x[i][0]), yr = raqammi(y[i][0]);
        if (xr && yr) {
            const int m = sonYokiNol(x[i]), n = sonYokiNol(y[i]);
            if (m != n) return m < n ? -1 : 1;
        } else if (xr != yr) {
            return xr ? -1 : 1;
        } else if (x[i] != y[i]) {
            // Baytlar boʻyicha (unsigned) — Swift'dagi
            // `lexicographicallyPrecedes` bilan bir xil.
            return x[i].compare(y[i]) < 0 ? -1 : 1;
        }
    }
    if (x.size() == y.size()) return 0;
    return x.size() < y.size() ? -1 : 1;
}

}  // namespace

int versiyaTaqqosla(std::string_view a, std::string_view b) {
    std::string_view ar, aq, br, bq;
    bool aBor = false, bBor = false;
    ikkigaBol(a, ar, aq, aBor);
    ikkigaBol(b, br, bq, bBor);

    const auto x = bol(ar, '.'), y = bol(br, '.');
    for (size_t i = 0; i < (x.size() > y.size() ? x.size() : y.size()); ++i) {
        const int xi = i < x.size() ? sonYokiNol(x[i]) : 0;
        const int yi = i < y.size() ? sonYokiNol(y[i]) : 0;
        if (xi != yi) return xi < yi ? -1 : 1;
    }
    if (!aBor && !bBor) return 0;
    if (!aBor) return 1;  // 1.2.0 > 1.2.0-rc1
    if (!bBor) return -1;

    const auto p = bol(aq, '.'), q = bol(bq, '.');
    for (size_t i = 0; i < p.size() && i < q.size(); ++i) {
        const int k = identifikatorTaqqosla(p[i], q[i]);
        if (k != 0) return k;
    }
    if (p.size() == q.size()) return 0;
    return p.size() < q.size() ? -1 : 1;
}

bool versiyaFormatimi(std::string_view v) {
    std::string_view r, q;
    bool bor = false;
    ikkigaBol(v, r, q, bor);
    const auto bolaklar = bol(r, '.');
    if (bolaklar.size() != 3) return false;
    for (auto b : bolaklar) {
        if (b.empty()) return false;
        for (char c : b)
            if (!raqammi(c)) return false;
    }
    if (!bor) return true;
    if (q.empty()) return false;
    for (char c : q)
        if (!raqammi(c) && !harfmi(c) && c != '.') return false;
    return true;
}

}  // namespace rubai
