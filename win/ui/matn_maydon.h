// Tahrirlanadigan matn maydoni — RichEdit boshqaruvi ustida.
//
// Bu ilovadagi YAGONA tizim boshqaruvi. Qolgan hamma narsa Direct2D bilan
// oʻzimiz chiziladi (`vidjet.h` ga qarang), lekin matn tahriri — boshqa gap:
// tanlash, sudrash, Ctrl+Z, klaviatura tillari (IME), oʻng tugma menyusi,
// ekran oʻqiruvchi — bularni qaytadan yozish bir yillik ish, Windows'da esa
// ular tayyor va tabiiy holda mavjud.
//
// macOS tomonida bu `NSTextView` (`studiya_view.swift`, `tarjima_view.swift`).
//
// MUHIM: bola oyna Direct2D chizmasi ustida turadi. Ota oynada
// **`WS_CLIPCHILDREN` boʻlishi SHART** — busiz D2D har chizishda RichEdit'ni
// bosib ketadi va maydon miltillaydi.
#pragma once

#include "uslub.h"

#include <functional>
#include <string>

namespace rubai {

class MatnMaydon {
public:
    MatnMaydon() = default;
    ~MatnMaydon();
    MatnMaydon(const MatnMaydon&) = delete;
    MatnMaydon& operator=(const MatnMaydon&) = delete;

    // `shriftOlchami` — DIP'da (dizayndagi qiymat). DPI ga koʻpaytirishni
    // `joylashtir` oʻzi qiladi.
    bool qur(HWND ota, float shriftOlchami = 17.0f);

    // Ramka DIP'da, `dpiNisbat` — ekran masshtabi. Bola oyna pikselda
    // joylashadi, shuning uchun oʻgirish shu yerda.
    void joylashtir(const D2D1_RECT_F& ramka, float dpiNisbat);

    void korsat(bool v);
    bool korinadimi() const;

    void matnQoy(const std::wstring& s);
    std::wstring matn() const;

    // Faqat uzunlik. `matn()` butun matnni nusxalaydi — har kadrda yoki har
    // tugma bosilganda chaqirilsa, uzun transkriptda bu sezilarli.
    size_t uzunlik() const;

    // Oqim uchun: matnni oxiriga qoʻshadi va koʻrinishni pastga suradi.
    // `matnQoy` bilan har delta'da butun hujjatni qayta yozish O(n²) boʻlardi.
    void qoshib(const std::wstring& s);

    void faqatOqish(bool v);
    void fokusOl();

    HWND deskriptor() const { return oyna_; }

    // Matn oʻzgarganda (foydalanuvchi tahriri VA dasturiy oʻzgarish).
    std::function<void()> onOzgardi;

    // Oʻng tugma bosilganda — kontekst menyusini chaqiruvchi quradi.
    // Nuqta EKRAN koordinatasida.
    std::function<void(POINT)> onOngTugma;

    // Ichki foydalanish uchun: RichEdit subclass protsedurasi shularga tegadi.
    struct Ichki;

private:
    void shriftniQayta(float dpiNisbat);

    HWND oyna_ = nullptr;
    HFONT shrift_ = nullptr;
    float shriftOlchami_ = 17.0f;
    float oxirgiDpi_ = 0.0f;
    // `matnQoy` ichida EN_CHANGE keladi — uni oʻz oʻzgarishimiz deb
    // belgilab qoʻyamiz, aks holda hujjat oʻzini oʻziga qayta saqlardi.
    bool ozimizYozyapmiz_ = false;

    friend struct Ichki;
    friend void matnMaydonXabari(WPARAM wp, LPARAM lp);
};

// Ota oynaning `WM_COMMAND` ishlovchisidan chaqiriladi: RichEdit oʻzgarishi
// (`EN_CHANGE`) otaga notifikatsiya boʻlib keladi va uni tegishli maydonga
// faqat deskriptor boʻyicha ulash mumkin.
void matnMaydonXabari(WPARAM wp, LPARAM lp);

}  // namespace rubai
