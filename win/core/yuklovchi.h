// Katta faylni yuklab olish: HTTP Range bilan davom ettiriladi, bekor
// qilinadi, jarayon xabar qilinadi.
//
// Ikki joyda ishlatiladi va shuning uchun alohida turadi:
//   • nutq modeli (823 MB) — `model_yuklovchi.h`
//   • tarjima modeli (3,1 GB arxiv) — `tarjima_yuklovchi.h`
//
// `.part` fayl va Range — sekin yoki uzilib turadigan internetda shart:
// uzilgan yuklash keyingi urinishda oʻsha joydan davom etadi. macOS tomonida
// ham shu naqsh (`model_download.swift`, `tarjima_yuklovchi.swift`).
#pragma once

#include <functional>
#include <string>

namespace rubai {

struct YuklashNatijasi {
    bool ok = false;
    // Xato boʻlsa — oʻzbekcha xabar (foydalanuvchiga koʻrsatiladi).
    std::wstring xato;
    // Yuklab olingan jami bayt (davom ettirilgani bilan birga).
    long long bayt = 0;
};

// Faylni `qismYoli` ga yuklaydi (`.part` kengaytmasi CHAQIRUVCHIDA).
// Fayl bor boʻlsa — oʻsha joydan davom etadi.
//
// `taxminiyHajm` — server `Content-Length` bermasa koʻrsatkich uchun.
// `jarayon(olingan, jami)` va `bekor()` ISHCHI OQIMDAN chaqiriladi.
//
// Bloklaydi. Fon oqimidan chaqiring.
//
// `.part` allaqachon `taxminiyHajm` ga teng boʻlsa — tarmoqqa chiqilmaydi va
// darhol muvaffaqiyat qaytadi; server 416 (soʻralgan joy fayl oxiridan
// keyin) desa ham — muvaffaqiyat. Ikkalasida ham faylni chaqiruvchi
// tekshiradi (hajm, sha256). Ilgari toʻliq `.part` Range bilan soʻralar va
// har urinish «Server javob bermadi (416)» bilan tugardi (barqarorlik F2).
YuklashNatijasi faylYukla(const std::wstring& url, const std::wstring& qismYoli,
                          long long taxminiyHajm,
                          const std::function<void(long long, long long)>& jarayon,
                          const std::function<bool()>& bekor);

// Faylning SHA-256 i, kichik harfli hex (Windows CNG — `bcrypt`). 4 MB lik
// boʻlaklar bilan oʻqiydi. Oʻqib boʻlmasa — boʻsh satr.
std::string faylSha256(const std::wstring& yol);

// Xotiradagi baytlarning SHA-256 i (kichik harfli hex). Xatoda — boʻsh satr.
std::string baytlarSha256(const void* bayt, size_t uzunlik);

}  // namespace rubai
