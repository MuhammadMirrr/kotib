// Avto-yangilanish (Windows) — oʻz yangilovchimiz (spec «avto-yangilanish», D3).
//
// macOS'dagi egizagi — `src/yangilovchi.swift` (u yerda yuklash va almashtirish
// Sparkle'da). Bu yerda hammasi oʻzimizniki, lekin QARORLAR bir xil va sof
// funksiyalarda (`yangilanish_siyosat.h`, ikki platforma bitta jadvaldan
// sinaladi):
//
//   tekshiruv  ishga tushgandan 60 s keyin, keyin har soatda «24 soat oʻtdimi»
//              (`uygonishdaTekshirish`), uyqudan uygʻonganda ham; vaqt belgisi
//              faqat MUVAFFAQIYATLI javobdan keyin saqlanadi; xatoda
//              15 daq → 1 soat → 4 soat (`qaytaUrinishKechikishi`).
//   manifest   https://stat.mirqobilov.com/v1/yangilanish/win.json — avval
//              Ed25519 imzosi (ochiq kalit .exe ichida), keyin maydonlar;
//              qaror — `siyosatQarori` (versiya, majburiy, bosqichli tarqatish).
//   yuklash    %LOCALAPPDATA%\Kotib\yangilanish\ ga, `.part` bilan davom
//              ettirib (`yuklovchi.h`); keyin hajm → sha256 → Ed25519. Xato
//              boʻlsa fayl oʻchiriladi va qayta urinish jadvali ishlaydi.
//   oʻrnatish  boʻsh paytda (`ornatishMumkinmi`; majburiyda — faqat yozuv
//              tugashini kutib): oʻrnatuvchi `/VERYSILENT … /KOTIBYANGILASH`
//              bilan ishga tushadi va Kotib oʻzi chiqadi. Inno fayllarni
//              almashtiradi va Kotib'ni qayta ochadi (S7).
//
// Tekshirilgan fayl oʻrnatilguncha YOZISHGA VA OʻCHIRISHGA QULFLANGAN dastak
// bilan ochiq turadi: tekshiruv bilan ishga tushirish orasida (soatlab boʻsh
// paytni kutish) uni hech kim almashtira olmaydi.
//
// Hammasi UI oqimida, tarmoq va disk ishi — fon oqimida; natija oynaga
// `xabarId` bilan qaytadi va `xabar()` ga uzatiladi.
#pragma once

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "yangilanish_siyosat.h"

namespace rubai {

// Yangilovchining taymerlari — ilova oynasidagi boshqa taymerlar bilan
// toʻqnashmasin (`app.h` dagilar 1–9).
constexpr UINT_PTR kYangilovchiTekshiruvTaymer = 20;
constexpr UINT_PTR kYangilovchiQaytaTaymer = 21;
constexpr UINT_PTR kYangilovchiBoshPaytTaymer = 22;

class Yangilovchi {
public:
    // Ilova holati — UI oqimida soʻraladi.
    std::function<bool()> bandmi;              // yozuv, fayl ishi, tarjima, model yuklash
    std::function<bool()> yozilyaptimi;        // faqat diktovka (majburiy yangilanish uchun)
    std::function<long long()> oxirgiFaollik;  // monoton soniya, -1 — hali yoʻq
    // Foydalanuvchiga xabar (tray). Fon tekshiruvi jim — faqat foydalanuvchi
    // oʻzi soʻraganda yoki muhim holatda chaqiriladi.
    std::function<void(const std::wstring& sarlavha, const std::wstring& matn)> bildir;
    // Qayta ochilgandan keyingi «Kotib X ga yangilandi» — oyna banneri.
    std::function<void(const std::wstring& matn, const std::wstring& kalit)> yangilandi;
    // Oʻrnatuvchi ishga tushdi — ilova darhol yopilsin.
    std::function<void()> chiqish;
    // Holat oʻzgardi (Sozlamalar qatori, tray menyusi yangilansin).
    std::function<void()> ozgardi;

    // `oyna` — taymerlar va fon natijalari shu oynaga keladi.
    void boshla(HWND oyna, UINT xabarId);
    // WM_TIMER: yangilovchiniki boʻlsa true.
    bool taymer(UINT_PTR id);
    // WM_POWERBROADCAST / PBT_APMRESUMEAUTOMATIC.
    void uygondi();
    // `xabarId` xabari (fon oqimidan).
    void xabar(WPARAM wp, LPARAM lp);
    // Sozlamalar / tray: «Hozir tekshirish». Natija `bildir` bilan aytiladi.
    void hozirTekshir();
    // Sozlamalar / tray: tayyor yangilanishni hozir oʻrnatish (yozuv
    // ketayotgan boʻlsa — kutadi va aytadi).
    void hozirOrnat();
    // Ilova yopilmoqda: fon ishlari toʻxtaydi.
    void toxtat();

    bool faolmi() const { return faol_; }
    bool tayyormi() const { return tayyor_.has_value(); }
    std::wstring tayyorVersiya() const;

    // ---- Majburiy yangilanish (S9) ----
    // Diskdagi talabdan: Hech | Ornat (muhlat ichida) | Blokla (diktovka toʻxtaydi).
    Qaror majburiyHolat() const;
    bool bloklanganmi() const { return majburiyHolat() == Qaror::Blokla; }
    // Oyna banneri: `bor` false — banner yoʻq. `sayt` — «Saytdan yuklab olish»
    // zaxira tugmasi ham koʻrsatilsin (faqat blokda).
    struct MajburiyKorinish {
        bool bor = false;
        std::wstring matn;
        std::wstring tugma;  // boʻsh — tugmasiz (yuklanmoqda)
        bool sayt = false;   // faqat yuklash xatosidan keyin (blok holatida)
    };
    MajburiyKorinish majburiyKorinish() const;
    // Banner tugmasi: tayyor boʻlsa — oʻrnatish, aks holda — qayta urinish.
    void majburiyTugma();

    // Sozlamalardagi «Yangilanishlar» qatori.
    struct Korinish {
        std::wstring matn;
        std::wstring tugma;
        bool tugmaFaol = true;
    };
    Korinish korinish() const;
    void tugmaBosildi();

    ~Yangilovchi();

private:
    struct Tayyor {
        std::string versiya;
        std::string izoh;
        std::wstring yol;
        HANDLE qulf = INVALID_HANDLE_VALUE;  // yozish/oʻchirishga qulf
        bool majburiy = false;
        long long kutishBoshlandi = 0;  // monoton soniya
    };

    void yangilanganBolsaXabarBer();
    void eskiOrnatuvchilarniOchir();
    void kerakBolsaTekshir(const wchar_t* sabab);
    void tekshir(const wchar_t* sabab);
    void muvaffaqiyat(const std::optional<Manifest>& m);
    // Qaysi bosqich yiqildi: qayta urinish jadvali har biri uchun alohida —
    // manifest muvaffaqiyati yuklash/oʻrnatish xatolarini nolga tushirmasin
    // (aks holda har ~15 daqiqada 40 MB qayta yuklanardi).
    enum class Bosqich { Tekshiruv, Yuklash };
    void xato(const std::wstring& sabab, Bosqich bosqich);
    void yukla(const Manifest& m, const std::string& arx, bool majburiy);
    // `faylniOchir` false — tekshirilgan fayl diskda qoladi (oʻrnatuvchi ishga
    // tushmadi: keyingi urinish uni qayta yuklamasdan qayta tekshiradi).
    void tayyorniBekorQil(const wchar_t* sabab, bool faylniOchir = true);
    void boshPaytniTekshir();
    void ornat();
    std::wstring papka() const;

    HWND oyna_ = nullptr;
    UINT xabarId_ = 0;
    bool faol_ = false;
    std::array<uint8_t, 32> kalit_{};
    std::shared_ptr<std::atomic<bool>> toxta_ = std::make_shared<std::atomic<bool>>(false);

    bool tekshiruvKetyapti_ = false;
    bool yuklashKetyapti_ = false;
    bool foydalanuvchiSoradi_ = false;
    int tekshiruvXato_ = 0;  // ketma-ket manifest xatolari
    int yuklashXato_ = 0;    // `xatoVersiya_` uchun ketma-ket yuklash/oʻrnatish xatolari
    std::string xatoVersiya_;
    bool qaytaKutilyapti_ = false;  // qayta urinish taymeri kutyapti
    // Qayta urinishlar tugagan vaqt (unix): keyingi tekshiruv undan 24 soat keyin.
    long long tugaganUrinish_ = 0;
    std::wstring oxirgiXato_;    // majburiy banner uchun; muvaffaqiyatda tozalanadi
    std::string yuklanayotgan_;  // yuklanayotgan versiya
    bool yuklanayotganMajburiy_ = false;
    std::string yuklanayotganIzoh_;
    long long olingan_ = 0, jami_ = 0;
    std::optional<Tayyor> tayyor_;
};

}  // namespace rubai
