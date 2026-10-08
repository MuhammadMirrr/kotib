// Kotib — dizayn tokenlari va Direct2D chizish yordamchilari.
//
// Bu fayl macOS tomonidagi `src/kotib_uslub.swift` ning aynan ekvivalenti.
// Ikkalasining manbai bitta: Claude Design loyihasidagi "Kotib - yangi.dc.html".
// Rang yoki oʻlcham kerak boʻlsa — avval shu yerga qoʻshiladi, view faylida emas.
// Ikkala platformadagi qiymatlar bir xil boʻlishi SHART: ular bir ilovaning
// ikki tanasi, ikki xil ilova emas.
//
// Nega Direct2D va nega GDI emas: dizaynda yumaloq burchaklar, yarim shaffof
// chegaralar va katta shriftlar bor. GDI ularni antialiasingsiz chizadi va
// natija oʻtgan asr koʻrinishida boʻladi. Direct2D + DirectWrite — Windows'ning
// oʻz zamonaviy chizish quvuri, tashqi kutubxona emas.
//
// Ilova YORUGʻ rejaga qotirilgan, xuddi macOS'dagidek: dizayn faqat yorugʻ
// variantda chizilgan va ranglar brendning oʻzi. Windows'ning tungi rejimiga
// ergashish dizaynni buzadi.
#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace rubai {

// ---- COM uchun kichik aqlli koʻrsatkich ------------------------------------
// mingw'da `winrt::com_ptr` yoʻq (u Windows SDK bilan keladi), `ATL::CComPtr`
// ham. Kerak boʻlgani shu bir necha satr.
template <typename T> class Com {
public:
    Com() = default;
    ~Com() { bosat(); }
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    Com(Com&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }

    T** yozish() {
        bosat();
        return &p_;
    }  // chiqish parametri sifatida
    T* get() const { return p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }
    void bosat() {
        if (p_) {
            p_->Release();
            p_ = nullptr;
        }
    }

private:
    T* p_ = nullptr;
};

// ---- Dizayn tokenlari ------------------------------------------------------
// Ranglar 0xRRGGBB. kotib_uslub.swift bilan bir xil tartibda turadi, shunda
// ikkalasini yonma-yon solishtirish oson boʻladi.
namespace U {

inline constexpr uint32_t oq = 0xFFFFFF;
inline constexpr uint32_t panel = 0xF7F7F9;        // toolbar / yumshoq fon
inline constexpr uint32_t ajratgich = 0xECECF0;    // ichki ajratuvchi chiziqlar
inline constexpr uint32_t qatorChiziq = 0xEDEEF1;  // roʻyxat qatori chegarasi
inline constexpr uint32_t qatorHover = 0xF7F8FA;

inline constexpr uint32_t kok = 0x0A6CFF;  // asosiy amal rangi
inline constexpr uint32_t kokBosilgan = 0x0850C0;
inline constexpr uint32_t qizil = 0xE5484D;   // yozib olish holati
inline constexpr uint32_t yashil = 0x1F7A4D;  // "ruxsat berilgan" belgisi

inline constexpr uint32_t matn = 0x1D1D1F;
inline constexpr uint32_t matn2 = 0x8A8A90;  // ikkilamchi
inline constexpr uint32_t matn3 = 0x9A9AA0;  // vaqt, uchlamchi
inline constexpr uint32_t matn4 = 0xB0B0B6;  // eng och (chevron, ikonka)

inline constexpr uint32_t kartaFon = 0xF5F7FA;  // "Bosing va gapiring" kartasi
inline constexpr uint32_t kartaChet = 0xE3E6EC;
inline constexpr uint32_t kartaHover = 0xEFF4FF;
inline constexpr uint32_t kartaHoverChet = 0xC7DBFF;

inline constexpr uint32_t yozishFon = 0xFFF3F3;  // yozib olinayotgandagi karta
inline constexpr uint32_t yozishChet = 0xF3CCCE;
inline constexpr uint32_t yozishMatn2 = 0x8A6A6C;

inline constexpr uint32_t bannerFon = 0xFFF6E0;  // ruxsat banneri
inline constexpr uint32_t bannerChet = 0xF0D79A;
inline constexpr uint32_t bannerMatn = 0x5C4708;

inline constexpr uint32_t tugmaChet = 0xD6D6DB;   // ikkilamchi tugma chegarasi
inline constexpr uint32_t maydonFon = 0xFAFAFB;   // drop zona / footer foni
inline constexpr uint32_t punktir = 0xC9C9D0;     // drop zona punktiri
inline constexpr uint32_t yumshoqFon = 0xF0F0F3;  // hotkey "chip" foni

// Oʻlchamlar — DIP birligida (96 dpi da 1 DIP = 1 piksel). Direct2D render
// target'ga ekran DPI'si berilgani uchun bu qiymatlar hamma masshtabda
// oʻz-oʻzidan toʻgʻri chiqadi va kodda hech qayerda DPI koʻpaytmasi kerak emas.
inline constexpr float radiusKarta = 16.0f;
inline constexpr float radiusQator = 11.0f;
inline constexpr float radiusTugma = 9.0f;
inline constexpr float radiusKichik = 7.0f;

}  // namespace U

// ---- Shrift ----------------------------------------------------------------
// Dizayn "IBM Plex Sans" ni soʻraydi, macOS'da tizim shrifti (SF Pro)
// ishlatiladi. Windows'da ekvivalenti — Segoe UI Variable (Windows 11) yoki
// Segoe UI (Windows 10). DirectWrite birinchisini topmasa ikkinchisiga oʻzi
// tushadi, shuning uchun alohida tekshiruv kerak emas.
enum class Ogirlik { Oddiy, Yarim, Qalin };

enum class Hizalash { Chap, Markaz, Ong };

// ---- Chizgich --------------------------------------------------------------
// Bitta oynaning Direct2D resurslari va chizish amallari. Har bir oyna
// (asosiy oyna, sozlamalar, overlay) oʻz Chizgich'ini tutadi.
//
// Resurslar ikki guruhga boʻlinadi va bu bejiz emas: `IDWriteFactory` va
// `ID2D1Factory` qurilmaga bogʻliq emas va bir marta yaratiladi; render target
// va undan tugʻilgan moyqalamlar esa qurilmaga bogʻliq — videokarta drayveri
// yangilansa yoki ekran oʻzgarsa ular yaroqsiz boʻladi va QAYTA yaratilishi
// kerak. `chizishTugadi` D2DERR_RECREATE_TARGET ni aynan shu uchun kuzatadi.
class Chizgich {
public:
    Chizgich() = default;
    ~Chizgich();
    Chizgich(const Chizgich&) = delete;
    Chizgich& operator=(const Chizgich&) = delete;

    // Oyna bilan bogʻlaydi. Bir marta chaqiriladi.
    bool qur(HWND oyna);

    // Oyna oʻlchami oʻzgarganda.
    void olchamOzgardi(UINT kenglik, UINT balandlik);

    // DPI oʻzgarganda (foydalanuvchi oynani boshqa ekranga sudraganda).
    void dpiOzgardi(UINT dpi);

    // Chizish sikli. `chizishTugadi` false qaytarsa — resurslar yoʻqolgan,
    // chaqiruvchi oynani qayta chizishga qoʻyishi kerak.
    void chizishBoshlandi();
    bool chizishTugadi();

    // Oyna ichki oʻlchami (DIP).
    D2D1_SIZE_F olcham() const;

    // ---- Asosiy amallar ----
    void toldir(const D2D1_RECT_F& r, uint32_t rang, float radius = 0.0f);
    void chegara(const D2D1_RECT_F& r, uint32_t rang, float radius = 0.0f, float qalinlik = 1.0f);
    void chiziq(float x1, float y1, float x2, float y2, uint32_t rang, float qalinlik = 1.0f);
    void doira(float x, float y, float radius, uint32_t rang);

    // Punktir chegara — Studiya tabidagi "faylni tashlang" zonasi uchun.
    void punktirChegara(const D2D1_RECT_F& r, uint32_t rang, float radius, float qalinlik = 1.5f);

    // ---- Matn ----
    // `bitta` — matn bitta qatorga sigʻdiriladi va sigʻmagani uch nuqta
    // bilan kesiladi. Roʻyxat qatorlari uchun: uzun fayl nomi yoki uzun
    // diktovka matni qator chegarasidan chiqib ketmasligi kerak.
    void matn(const std::wstring& s, const D2D1_RECT_F& r, uint32_t rang, float olcham,
              Ogirlik ogirlik = Ogirlik::Oddiy, Hizalash hizalash = Hizalash::Chap,
              bool vertikalMarkaz = false, bool bitta = false);

    // Matnning oʻlchamini hisoblaydi — joylashtirish (layout) uchun.
    // `maksKenglik` berilsa matn shu kenglikda oʻraladi.
    D2D1_SIZE_F matnOlchami(const std::wstring& s, float olcham, Ogirlik ogirlik = Ogirlik::Oddiy,
                            float maksKenglik = 1e6f);

    // ---- Aylantirish ----
    // Berilgan nuqta atrofida keyingi chizishlarni buradi. Ikonkalar uchun
    // (masalan ⚙ ning tishlari) — boshqa hech qayerda kerak emas.
    // `aylantirishniBekor` ni CHAQIRISH SHART, aks holda butun oyna qiyshiq
    // chiziladi.
    void aylantir(float gradus, float cx, float cy);
    void aylantirishniBekor();

    // ---- Kesish (scroll qiluvchi hududlar uchun) ----
    void kesishBoshla(const D2D1_RECT_F& r);
    void kesishTugat();

    ID2D1HwndRenderTarget* target() const { return rt_.get(); }

private:
    ID2D1SolidColorBrush* moyqalam(uint32_t rang);
    IDWriteTextFormat* shakl(float olcham, Ogirlik ogirlik, Hizalash hizalash, bool bitta = false);
    bool qurilmaResurslari();
    void resurslarniTashla();

    HWND oyna_ = nullptr;
    Com<ID2D1Factory> d2d_;
    Com<IDWriteFactory> dw_;
    Com<ID2D1HwndRenderTarget> rt_;
    Com<ID2D1StrokeStyle> punktirUslub_;

    // Moyqalam va shrift shakllari keshi. Ular render target bilan birga
    // yoʻq qilinadi, shuning uchun kesh ham shu paytda tozalanadi.
    std::unordered_map<uint32_t, ID2D1SolidColorBrush*> moyqalamlar_;
    std::unordered_map<uint64_t, IDWriteTextFormat*> shakllar_;
};

// Yordamchi: DIP toʻrtburchagi yasash. Nomi `ramka` EMAS — har bir vidjetning
// `ramka` maydoni bor va u bu funksiyani toʻsib qoʻyardi.
inline D2D1_RECT_F ramkaYasa(float x, float y, float kenglik, float balandlik) {
    return D2D1::RectF(x, y, x + kenglik, y + balandlik);
}

inline bool ichidami(const D2D1_RECT_F& r, D2D1_POINT_2F p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

}  // namespace rubai
