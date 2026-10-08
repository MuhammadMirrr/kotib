// Matnga oʻgirib boʻlmagan diktovka ovozi — `saqlanmagan.h` ga qarang.
#include "saqlanmagan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace rubai {
namespace saqlanmagan {

namespace {

void u16(std::string& d, unsigned v) {
    d += static_cast<char>(v & 0xFF);
    d += static_cast<char>((v >> 8) & 0xFF);
}

void u32(std::string& d, unsigned long v) {
    u16(d, static_cast<unsigned>(v & 0xFFFF));
    u16(d, static_cast<unsigned>((v >> 16) & 0xFFFF));
}

unsigned oqiU16(const std::string& d, size_t i) {
    return static_cast<unsigned>(static_cast<unsigned char>(d[i])) |
           static_cast<unsigned>(static_cast<unsigned char>(d[i + 1])) << 8;
}

unsigned long oqiU32(const std::string& d, size_t i) {
    return static_cast<unsigned long>(oqiU16(d, i)) | static_cast<unsigned long>(oqiU16(d, i + 2))
                                                          << 16;
}

bool raqammi(wchar_t c) { return c >= L'0' && c <= L'9'; }

// Nom shakli: YYYY-MM-DD_HH-MM-SS-mmm.wav — 27 belgi.
constexpr wchar_t kShakl[] = L"0000-00-00_00-00-00-000.wav";

}  // namespace

std::string wav(const std::vector<float>& s, int chastota) {
    const unsigned long malumot = static_cast<unsigned long>(s.size()) * 2;
    std::string d;
    d.reserve(44 + malumot);
    d += "RIFF";
    u32(d, 36 + malumot);
    d += "WAVE";
    d += "fmt ";
    u32(d, 16);
    u16(d, 1);  // PCM
    u16(d, 1);  // mono
    u32(d, static_cast<unsigned long>(chastota));
    u32(d, static_cast<unsigned long>(chastota) * 2);
    u16(d, 2);
    u16(d, 16);  // blok, bit
    d += "data";
    u32(d, malumot);
    for (float x : s) {
        if (!std::isfinite(x)) x = 0;
        x = std::max(-1.0f, std::min(1.0f, x));
        const int16_t v = static_cast<int16_t>(std::lround(x * 32767.0f));
        u16(d, static_cast<uint16_t>(v));
    }
    return d;
}

bool namunalar(const std::string& b, std::vector<float>& chiqish, int chastota) {
    chiqish.clear();
    if (b.size() < 44) return false;
    if (b.compare(0, 4, "RIFF") != 0 || b.compare(8, 4, "WAVE") != 0 ||
        b.compare(12, 4, "fmt ") != 0 || oqiU32(b, 16) != 16 || oqiU16(b, 20) != 1 ||
        oqiU16(b, 22) != 1 || oqiU32(b, 24) != static_cast<unsigned long>(chastota) ||
        oqiU16(b, 34) != 16 || b.compare(36, 4, "data") != 0) {
        return false;
    }
    // Sarlavhada koʻrsatilgandan kam maʼlumot boʻlsa (yozish uzilgan) — borini oʻqiymiz.
    const size_t n = std::min<size_t>(oqiU32(b, 40), b.size() - 44) / 2;
    chiqish.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const int16_t v = static_cast<int16_t>(oqiU16(b, 44 + 2 * i));
        chiqish[i] = static_cast<float>(v) / 32767.0f;
    }
    return true;
}

std::wstring nom(long long unixMs) {
    const std::time_t t = static_cast<std::time_t>(unixMs / 1000);
    std::tm m{};
    localtime_s(&m, &t);
    wchar_t b[40];
    std::swprintf(b, 40, L"%04d-%02d-%02d_%02d-%02d-%02d-%03d.wav", m.tm_year + 1900, m.tm_mon + 1,
                  m.tm_mday, m.tm_hour, m.tm_min, m.tm_sec, static_cast<int>(unixMs % 1000));
    return b;
}

long long sana(const std::wstring& n) {
    const size_t uzunlik = sizeof(kShakl) / sizeof(kShakl[0]) - 1;
    if (n.size() != uzunlik) return -1;
    for (size_t i = 0; i < uzunlik; ++i) {
        if (kShakl[i] == L'0' ? !raqammi(n[i]) : n[i] != kShakl[i]) return -1;
    }
    auto son = [&](size_t i, size_t k) {
        int v = 0;
        for (size_t j = i; j < i + k; ++j) v = v * 10 + (n[j] - L'0');
        return v;
    };
    std::tm m{};
    m.tm_year = son(0, 4) - 1900;
    m.tm_mon = son(5, 2) - 1;
    m.tm_mday = son(8, 2);
    m.tm_hour = son(11, 2);
    m.tm_min = son(14, 2);
    m.tm_sec = son(17, 2);
    m.tm_isdst = -1;
    const std::time_t t = std::mktime(&m);
    if (t == static_cast<std::time_t>(-1)) return -1;
    return static_cast<long long>(t) * 1000 + son(20, 3);
}

std::vector<std::wstring> ortiqcha(const std::vector<std::wstring>& nomlar, long long hozir) {
    std::vector<std::pair<long long, std::wstring>> sanali;
    for (const auto& n : nomlar) {
        const long long s = sana(n);
        if (s >= 0) sanali.emplace_back(s, n);
    }
    std::sort(sanali.begin(), sanali.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::wstring> och;
    for (size_t i = 0; i < sanali.size(); ++i) {
        if (i >= kChegara || hozir - sanali[i].first > kMuddatMs) {
            och.push_back(sanali[i].second);
        }
    }
    return och;
}

std::vector<std::wstring> royxat(const std::wstring& papka) {
    std::vector<std::wstring> natija;
    std::error_code ec;
    fs::directory_iterator it(fs::path(papka), ec), oxir;
    for (; !ec && it != oxir; it.increment(ec)) {
        const std::wstring n = it->path().filename().wstring();
        if (sana(n) >= 0) natija.push_back(it->path().wstring());
    }
    // Toʻliq yoʻl bir xil papkada — alifbo tartibi = nom tartibi = vaqt tartibi.
    std::sort(natija.begin(), natija.end());
    return natija;
}

std::wstring saqla(const std::vector<float>& s, const std::wstring& papka, long long hozir) {
    std::error_code ec;
    fs::create_directories(fs::path(papka), ec);
    if (ec) return {};
    const fs::path yol = fs::path(papka) / nom(hozir);
    {
        std::ofstream f(yol, std::ios::binary | std::ios::trunc);
        const std::string d = wav(s);
        if (!f || !f.write(d.data(), static_cast<std::streamsize>(d.size()))) {
            f.close();
            fs::remove(yol, ec);
            return {};
        }
    }
    tozala(papka, hozir);
    return yol.wstring();
}

void tozala(const std::wstring& papka, long long hozir) {
    std::vector<std::wstring> nomlar;
    for (const auto& y : royxat(papka)) nomlar.push_back(fs::path(y).filename().wstring());
    std::error_code ec;
    for (const auto& n : ortiqcha(nomlar, hozir)) fs::remove(fs::path(papka) / n, ec);
}

bool oqi(const std::wstring& yol, std::vector<float>& chiqish) {
    std::ifstream f(fs::path(yol), std::ios::binary);
    if (!f) return false;
    const std::string d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return namunalar(d, chiqish);
}

long long hozirMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace saqlanmagan
}  // namespace rubai
