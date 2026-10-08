// Diktovka tarixi — Ctrl+Alt+D bilan aytilgan matnlar roʻyxati.
//
// macOS'dagi `src/diktovka_tarixi.swift` ning ekvivalenti.
//
// `HujjatOmbori` dan ATAYLAB alohida turadi: u fayl transkripsiyalari uchun —
// manba fayl yoʻli, segment vaqt belgilari, natijalar papkasi. Diktovkada
// bularning hech biri yoʻq, va har bir 3 soniyalik diktovka uchun papka
// yasash kutubxonani axlatga toʻldirardi.
//
// Nosozlikda hech qachon xato bermaydi: tarix — qoʻshimcha qulaylik, uning
// buzilishi diktovkani toʻxtatmasligi kerak.
#pragma once

#include <string>
#include <vector>

namespace rubai {

struct DiktovkaYozuvi {
    std::wstring id;
    std::wstring matn;
    long long sana = 0;     // Unix soniya
    double davomiylik = 0;  // soniya
};

class DiktovkaTarixi {
public:
    // Roʻyxatda saqlanadigan eng koʻp yozuv soni. Undan oshgani tushib ketadi.
    static constexpr size_t kChegara = 200;

    // %APPDATA%\Kotib\diktovka-tarixi.json
    static DiktovkaTarixi& birgalik();

    explicit DiktovkaTarixi(std::wstring fayl);

    // Eng yangisi birinchi. Fayl yoʻq yoki buzuq boʻlsa — boʻsh roʻyxat.
    std::vector<DiktovkaYozuvi> oqi() const;

    // Yangi yozuvni roʻyxat boshiga qoʻyadi va chegaradan oshganini kesadi.
    // `sana` — Unix soniya; 0 — hozir. Saqlangan ovoz keyinroq matnga
    // oʻgirilganda asl yozilgan vaqti beriladi (`saqlanmagan.h`).
    bool qoshish(const std::wstring& matn, double davomiylik, long long sana = 0);

    bool ochir(const std::wstring& id);
    bool tozala();

private:
    bool yoz(const std::vector<DiktovkaYozuvi>& royxat) const;
    std::wstring fayl_;
};

}  // namespace rubai
