// Tarjimon qoʻllab-quvvatlaydigan tillar.
//
// macOS'dagi `src/tillar.swift` ning ekvivalenti. NLLB-200 modeli 202 tilni
// biladi va ularning HAMMASI bitta model faylida yotadi — qoʻshimcha til
// qoʻshish na hajm, na tezlik jihatidan hech narsa turmaydi. Shuning uchun
// roʻyxat cheklanmagan: interfeys qidiruvli.
//
// Nomlar QOʻLDA yozilmagan: ular macOS tomonida `Locale(identifier: "uz")`
// dan olinib, shu faylga koʻchirilgan — ikkala platformada bir xil boʻlishi
// uchun. Windows'da `uz` lokalining til nomlari yoʻq (`GetLocaleInfoEx`
// ruscha yoki inglizcha qaytaradi), shuning uchun boshqa yoʻl yoʻq.
//
// Roʻyxat AYNAN macOS'dagi tartibda — nom boʻyicha saralangan holda.
// Model almashtirilsa roʻyxat qayta olinishi kerak.
#pragma once

#include <string>
#include <vector>

namespace rubai {

struct Til {
    // NLLB kodi, masalan "uzn_Latn". Modelga aynan shu beriladi.
    std::string nllb;
    // Foydalanuvchi koʻradigan nom, oʻzbekcha.
    std::wstring nom;

    // ISO til kodi ("uzn").
    std::string kod() const { return nllb.substr(0, 3); }
    // Yozuv kodi ("Latn").
    std::string yozuv() const { return nllb.size() > 4 ? nllb.substr(4) : std::string(); }

    bool operator==(const Til& o) const { return nllb == o.nllb; }
};

namespace Tillar {

// Barcha tillar, oʻzbekcha nomi boʻyicha saralangan.
const std::vector<Til>& hammasi();

// Kod boʻyicha qidiradi. Topilmasa nullptr.
const Til* top(const std::string& nllb);

// Standart tanlovlar.
inline constexpr const char* kUz = "uzn_Latn";
inline constexpr const char* kRu = "rus_Cyrl";
inline constexpr const char* kEn = "eng_Latn";

}  // namespace Tillar
}  // namespace rubai
