// Tarjima dvigatelining C++ qatlami.
//
// macOS'dagi `src/tarjimon.swift` va `src/tarjima_model.swift` ning
// ekvivalenti. `Engine` (whisper) bilan bir xil naqsh:
//   • jarayon-global bitta model, alohida oqimda ishlaydi;
//   • `band` bayrogʻi — ish ketayotganda idle taymer modelni boʻshatmaydi;
//   • 180 soniya ishlatilmasa model RAM'dan boʻshaydi.
//
// Nega boʻshatish muhim: ovoz modeli ~1 GB, tarjima modeli ~1,5 GB RAM oladi.
// Ikkalasi doim xotirada tursa 8 GB li mashina swap'ga tushadi.
//
// Bekor qilish JUMLALAR ORASIDA ishlaydi: CTranslate2 tarjima oʻrtasida
// toʻxtashni qoʻllamaydi. Bitta jumla bir soniyadan kam vaqt oladi, shuning
// uchun foydalanuvchi uchun bu sezilmaydi.
#pragma once

#include "tillar.h"

#include <cstdint>
#include <functional>
#include <string>

namespace rubai {

// ---- Model papkasi ---------------------------------------------------------
//
// Yarim yuklangan papka bilan ishga tushish CTranslate2 ichida tushunarsiz
// xato beradi, shuning uchun papka faqat BARCHA fayllar boʻlgandagina
// «tayyor» hisoblanadi. Fayllarning boʻsh emasligi ham tekshiriladi: uzilib
// qolgan arxiv ochish 0 baytli fayl qoldirishi mumkin.
namespace TarjimaModel {

// `%LOCALAPPDATA%\Kotib\tarjima-model`
std::wstring papka();

// `.tar.gz` arxivining aniq hajmi va ochilgandan keyingi hajmi.
// Ochilgan papka arxivdan KATTA — int8 ogʻirliklar deyarli siqilmaydi.
inline constexpr long long kTaxminiyBayt = 3114748195LL;
inline constexpr long long kOchilganBayt = 3366822263LL;
// Yuklash uchun kerakli boʻsh joy: oʻrtada ikkalasi ham diskda turadi.
inline constexpr long long kKerakliJoy = kTaxminiyBayt + kOchilganBayt;

bool tayyor();
bool tayyormi(const std::wstring& papka);

// Papka turgan diskda `kerak` bayt boʻsh joy bormi. Aniqlab boʻlmasa — true:
// noaniqlik tufayli ishlayotgan narsani toʻsmaymiz.
bool yetarliJoyBormi(const std::wstring& papka, long long kerak);

}  // namespace TarjimaModel

// ---- Tarjimon --------------------------------------------------------------

enum class TarjimaXatosi {
    Yoq,
    ModelYuklanmadi,
    BekorQilindi,
};

std::wstring tarjimaXatoXabari(TarjimaXatosi x);

class Tarjimon {
public:
    static Tarjimon& birgalik();

    bool yuklanganmi() const;

    // Modelni yuklaydi. Bir necha soniya oladi — UI oqimida chaqirilmasin.
    bool yukla();
    void bosat();

    void bekorQil();

    // Matnni tarjima qiladi. Ish fon oqimida ketadi va qayta chaqiruvlar
    // OʻSHA oqimdan keladi — UI'ga `PostMessage` bilan uzating.
    //
    // `jarayon(bajarilgan, jami)` — jumlalar boʻyicha.
    // `tugadi(matn, xato)` — xato `Yoq` boʻlsa `matn` tayyor natija.
    void tarjimaQil(const std::wstring& matn, const Til& manba, const Til& maqsad,
                    std::function<void(int, int)> jarayon,
                    std::function<void(std::wstring, TarjimaXatosi)> tugadi);

    bool bandmi() const;

private:
    Tarjimon() = default;
    ~Tarjimon() = default;
    Tarjimon(const Tarjimon&) = delete;
    Tarjimon& operator=(const Tarjimon&) = delete;
};

}  // namespace rubai
