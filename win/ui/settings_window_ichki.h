// Sozlamalar oynasining ichki qismi — `settings_window.cpp` va
// `settings_window_llm.cpp` uchun umumiy: boshqaruv ID'lari, oʻlchamlar,
// yordamchilar va `SettingsWindow::Impl`. Tashqariga ochiq interfeys —
// `settings_window.h`.
#pragma once

#include <windows.h>

#include "settings_window.h"

#include <commctrl.h>

#include <algorithm>
#include <ctime>
#include <memory>
#include <shellapi.h>
#include <string>
#include <thread>
#include <vector>

#include "../core/audio_capture.h"
#include "../core/llm.h"
#include "../core/util.h"
#include "autostart.h"
#include "donate_window.h"

namespace rubai {

namespace {

constexpr wchar_t kClassName[] = L"KotibSettings";

enum CtrlId : int {
    kIdHotkey = 200,
    kIdMic,
    kIdMicWarning,
    kIdInsertMode,
    kIdAutoStart,
    kIdUseGpu,
    kIdStatistika,
    kIdApostrof,
    kIdSave,
    kIdCancel,
    kIdReset,
    kIdDonate,
    kIdLog,
    // Sunʼiy intellekt boʻlimi
    kIdProvayder,
    kIdKalit,
    kIdBaseURL,
    kIdModel,
    kIdModelYangila,
    kIdModelHolati,
    kIdTekshir,
    kIdTekshirNatija,
    // «Qoʻshimcha sozlamalar» ostidagi hamma narsa. Yorliqlarga ham id
    // kerak: ular ham yashiriladi.
    kIdQoshimcha,
    kIdApostrofIzoh,
    kIdStatistikaIzoh,
    kIdAiSarlavha,
    kIdAiIzoh,
    kIdModelYozuv,
    // Yangilanishlar qatori (doim koʻrinadi)
    kIdYangilanishHolati,
    kIdYangilanishTugma,
    kIdDiagnostika,
    kIdBirinchiQoshimcha = kIdApostrof,  // roʻyxat chegaralari (pastga qarang)
};

// Fon oqimidan (model roʻyxati, ulanish tekshiruvi) natija qaytadi.
constexpr UINT WM_LLM_MODELLAR = WM_APP + 40;  // lp: new std::vector<std::wstring>*
constexpr UINT WM_LLM_HOLAT = WM_APP + 41;     // wp: maydon id, lp: new std::wstring*

// Oyna oʻlchamlari 96 DPI da; DPI boʻyicha masshtablanadi.
//
// Ikkita balandlik: qoʻshimcha sozlamalar yopiq va ochiq. macOS'da ham
// shunday (`sozlamalar_view.swift` → `qoshimchaniAlmashtir`).
//
// Nega yigʻiladi: toʻliq roʻyxat 700 DIP boʻlib ketadi va 150% masshtabdagi
// 768 piksellik noutbukda oynaning pastki qismi ekrandan chiqib ketardi —
// «Saqlash» tugmasi koʻrinmay qolardi.
constexpr int kWinW = 460;
constexpr int kWinH = 472;       // yopiq
constexpr int kWinHToliq = 834;  // ochiq

// Mikrofon ogohlantirishi uchun ajratilgan joy. Ogohlantirish YOʻQ boʻlsa
// (oddiy mikrofon — eng koʻp uchraydigan holat) pastdagi hamma narsa shu
// balandlikka koʻtariladi, aks holda oynada 52 DIP boʻsh joy qolib ketardi.
// macOS'da bu oʻz-oʻzidan chiqadi: u yerda `NSStackView` yashiringan
// qatorni yigʻib qoʻyadi.
constexpr int kMicOgohH = 52;
// Statistika izohi uchun LOYIHADAGI balandlik. Haqiqiysi ish paytida
// oʻlchanadi: shrift kengligi DPI va tizim sozlamalariga qarab oʻzgaradi va
// matn goh ikki, goh uch qatorga tushadi. Qatʼiy raqam bilan oxirgi qator
// kesilib qolardi.
constexpr int kStatIzohH = 44;
constexpr int kBolimBoshi = 218;  // ogohlantirishdan keyingi birinchi qator
constexpr int kAiBoshi = 518;     // statistika izohidan keyingi birinchi qator

// Qoʻshimcha boʻlimdagi boshqaruvlar — birga yashiriladi/koʻrsatiladi.
constexpr int kQoshimchaIdlar[] = {
    kIdApostrof,     kIdStatistika,  kIdStatistikaIzoh, kIdAiSarlavha,    kIdAiIzoh,
    kIdProvayder,    kIdKalit,       kIdBaseURL,        kIdModelYozuv,    kIdModel,
    kIdModelYangila, kIdModelHolati, kIdTekshir,        kIdTekshirNatija, kIdLog,
    kIdDiagnostika,
};

// Donat tugmasi ranglari — sayt palitrasidan.
const COLORREF kDonateBg = RGB(169, 78, 46);
const COLORREF kDonateBgHover = RGB(142, 63, 36);

}  // namespace

struct SettingsWindow::Impl {
    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;
    HFONT font = nullptr;
    UINT dpi = 96;

    Settings settings;
    std::vector<MicDevice> mics;
    std::function<void(const Settings&)> onSaved;
    std::function<Yangilovchi::Korinish()> yangilanishHolati;
    std::function<void()> yangilanishBosildi;
    void yangilanishQatori();
    DonateWindow donate;

    int scale(int v) const { return MulDiv(v, (int)dpi, 96); }

    void build();
    void fillMics();
    void updateMicWarning();
    void save();
    HWND ctrl(int id) const { return GetDlgItem(hwnd, id); }

    // ---- Sunʼiy intellekt boʻlimi ----
    void llmniYukla();
    void llmniSaqla();
    void provayderOzgardi();
    void xosQatorniYangila();
    void modellarniKorsat(const std::vector<std::wstring>& royxat);
    void modellarniOlish();
    void ulanishniTekshir();

    std::wstring maydonMatni(int id) const;
    void maydonQoy(int id, const std::wstring& s);

    // Fon oqimidagi soʻrov hali ketyaptimi — ikki marta bosishdan himoya.
    bool llmSoravKetyapti = false;

    // Qoʻshimcha sozlamalar ochiqmi.
    bool qoshimchaOchiq = false;
    void joylashuvniYangila();

    // Har bir boshqaruvning LOYIHA koordinatalari (DIP). Joylashuv
    // oʻzgarganda (ogohlantirish yigʻilishi, boʻlim ochilishi) hammasi shu
    // roʻyxatdan qayta hisoblanadi — ekrandagi joriy oʻrindan emas.
    struct Joy {
        HWND h;
        int x, y, w, balandlik;
    };
    std::vector<Joy> joylar;
    int ogohBalandligi = kMicOgohH;  // 0 yoki kMicOgohH

    // Matn shu kenglikda necha DIP joy egallaydi (oʻralishni hisobga oladi).
    int matnBalandligi(HWND c, int kenglikDip) const;

    // Vertikal surish. Kontent 716 DIP gacha choʻziladi va 768 piksellik
    // (yoki masshtablangan) ekranga sigʻmaydi — macOS tomonida sozlamalar
    // `NSScrollView` ichida turadi, bu yerda ham shunday boʻlishi kerak,
    // aks holda «Saqlash» tugmasi ekrandan chiqib ketadi.
    int surilish = 0;           // joriy siljish, piksel
    int kontentBalandligi = 0;  // toʻliq kontent balandligi, piksel
    void surishniSozla(int mijozBalandligi);
    void sur(int yangi);
};

}  // namespace rubai
