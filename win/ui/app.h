// Kotib — Windows ilovasining asosiy klassi (`App`) va umumiy doimiylar.
//
// Ilova ikki faylda: `app.cpp` — oyna, tray, hotkey, sozlamalar, ishga
// tushish/yopilish; `app_diktovka.cpp` — diktovka oqimi (yozish → whisper →
// kiritish) va saqlanmagan ovoz (A2). macOS'dagi egizagi — `ilova.swift` va
// `ilova_saqlanmagan.swift`.
#pragma once

#include <windows.h>
#include <shellapi.h>

#include <ctime>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "../core/audio_capture.h"
#include "../core/config.h"
#include "../core/engine.h"
#include "../core/ish.h"
#include "../core/matn_format.h"
#include "../core/samples.h"
#include "../core/saqlanmagan.h"
#include "../core/util.h"
#include "../core/model_yuklovchi.h"
#include "../core/tarix.h"
#include "../core/statistika.h"
#include "../core/yangilovchi.h"
#include "asosiy_oyna.h"
#include "autostart.h"
#include "inserter.h"
#include "overlay.h"
#include "settings_window.h"
#include "soragich.h"

namespace rubai {

// ---------------------------------------------------------------- doimiylar

constexpr wchar_t kWindowClass[] = L"KotibHiddenWindow";
constexpr wchar_t kMutexName[] = L"Global\\KotibSingleInstance";
constexpr wchar_t kAppName[] = L"Kotib";

constexpr UINT WM_RUBAI_TRAY = WM_APP + 1;
constexpr UINT WM_RUBAI_RESULT = WM_APP + 2;
// Avto-yangilovchining fon natijalari (`yangilovchi.h`): wp — hodisa, lp — maʼlumot.
constexpr UINT WM_RUBAI_YANGILANISH = WM_APP + 3;
// Nutq modelini yuklab olish (`model_yuklovchi.h`). Fon oqimidan keladi.
constexpr UINT WM_RUBAI_MODEL = WM_APP + 4;         // wp: olingan MB, lp: jami MB
constexpr UINT WM_RUBAI_MODEL_TUGADI = WM_APP + 5;  // wp: ok, lp: new wstring*
// Saqlangan ovozni qayta oʻgirish natijasi (`pendingQayta_`). Ishchi oqimdan.
constexpr UINT WM_RUBAI_QAYTA = WM_APP + 6;

constexpr int kHotkeyId = 1;

enum MenuId : UINT {
    kMenuDictate = 100,
    kMenuSettings = 101,
    kMenuLog = 102,
    kMenuQuit = 103,
    kMenuOyna = 104,
    kMenuQayta = 105,        // «Saqlangan ovozni matnga oʻgirish (N)»
    kMenuLitsenziya = 106,   // «Litsenziyalar…» — exe yonidagi Litsenziyalar.txt
    kMenuYangilanish = 107,  // «Yangilanishni tekshirish» / «… oʻrnatish va qayta ochish»
};

// Yozib olish paytida toʻlqin chizigʻini yangilab turadigan taymer.
// 20 kadr/soniya — koʻz uchun yetarli, protsessor uchun sezilmaydi.
constexpr UINT_PTR kDarajaTaymer = 1;
constexpr UINT kDarajaOraligi = 50;

// Eʼtibordan chetda qolgan yozuvni cheklaydi: yozish shuncha davom etsa
// oʻzi toʻxtaydi. Diktovka uchun 10 daqiqa juda koʻp (odatdagi yozuvlar bir
// daqiqagacha), lekin macOS'da 2026-09-04 da bitta yozuv 32 daqiqa davom
// etib, 7952 belgilik keraksiz matn qoʻyib yuborgan. Chegara ikkala
// platformada bir xil.
constexpr UINT_PTR kMaxYozishTaymer = 2;
constexpr UINT kMaxYozishMs = 600 * 1000;

// Muvaffaqiyatli diktovka yoki model yuklanganidan shuncha keyin saqlangan
// ovozlar avtomatik sinaladi (macOS bilan bir xil, 3 s).
constexpr UINT_PTR kQaytaTaymer = 3;
constexpr UINT kQaytaKutishMs = 3000;

// Ikki marta bosilib ketishdan himoya. Hotkey `MOD_NOREPEAT` bilan
// roʻyxatdan oʻtgan, lekin u faqat klavishani BOSIB TURISHNI toʻsadi —
// tez ikki marta bosish baribir ikkita hodisa beradi va yozuv ochilib
// darhol yopilardi. macOS'da bu `toggleOynasi`.
constexpr ULONGLONG kToggleOynasiMs = 400;

// ------------------------------------------------------------------ ilova

class App {
public:
    bool init(HINSTANCE instance);
    void run();
    void shutdown();

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(HWND, UINT, WPARAM, LPARAM);

    bool createWindow(HINSTANCE instance);
    bool addTrayIcon();
    void removeTrayIcon();
    void updateTrayTip(const std::wstring& text);
    void showMenu();

    bool registerHotkey();
    void toggleDictation();
    void startRecording();
    void stopAndTranscribe();

    void onResult(const TranscribeResult& r);
    void notify(const std::wstring& title, const std::wstring& text, DWORD icon);

    // Saqlanmagan ovoz (barqarorlik A2) — macOS'dagi `ilova_saqlanmagan.swift` bilan bir xil.
    std::wstring saqlanmaganPapka() const;
    void ovozniSaqla(std::vector<float> ovoz, const std::wstring& sabab);
    void saqlanganlarniOgir(bool qolda);
    void qaytaKeyingi();
    void onQaytaNatija(const TranscribeResult& r);
    void qaytaTugadi();

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW tray_{};
    bool trayAdded_ = false;

    void applySettings(const Settings& s);
    void oynaniOch();
    void mikrofonBanneri();
    void modelniYukla();

    Settings settings_;
    AudioCapture capture_;
    Overlay overlay_;
    SettingsWindow settingsWindow_;
    AsosiyOyna asosiy_;
    ModelYuklovchi modelYuklovchi_;
    bool recording_ = false;
    bool busy_ = false;     // transkripsiya ketmoqda
    bool yopildi_ = false;  // `shutdown()` bajarilgan
    // Oynada «Nutq modeli topilmadi» banneri turibdi. Model fayli tashqaridan
    // qaytsa (qoʻlda nusxalangan, antivirus karantinidan chiqqan) banner oʻzi
    // yangilanmaydi — keyingi yozuv boshlanganda qayta hisoblanadi.
    bool modelBanneri_ = false;

    // Diktovka uzunligi — tarixda saqlanadi. `GetTickCount64` (ms): «Juda
    // qisqa» ni soniya aniqligidagi `time()` bilan ajratib boʻlmasdi.
    ULONGLONG yozishBoshlandi_ = 0;
    double yozishDavomiyligi_ = 0;
    // Transkripsiyaga berilgan diktovkaning XOM ovozi: xato boʻlsa WAV'ga
    // yoziladi (A2), muvaffaqiyatda tashlanadi.
    std::vector<float> kutilayotganOvoz_;

    // Saqlangan ovozni qayta oʻgirish holati — faqat asosiy oqimda.
    bool qaytaIshlanyapti_ = false;
    bool qaytaQolda_ = false;
    std::vector<std::wstring> qaytaNavbat_;
    size_t qaytaIndeks_ = 0;
    size_t qaytaNamunaSoni_ = 0;
    std::vector<std::wstring> qaytaMatnlar_;
    std::wstring qaytaXato_;
    // Avtomatik urinish har fayl uchun bir marta (shu ishga tushish davomida).
    std::set<std::wstring> avtoUrinilgan_;
    std::unique_ptr<TranscribeResult> pendingQayta_;
    ULONGLONG oxirgiToggle_ = 0;
    // Model yuklashda banner faqat megabayt qiymati oʻzgarganda yangilanadi:
    // yuklovchi har 256 KB da xabar yuboradi va har xabarda butun oynani
    // qayta joylashtirish ortiqcha.
    long long oxirgiModelMb_ = -1;

    // Natija ishchi oqimdan keladi; UI faqat asosiy oqimda oʻzgaradi,
    // shuning uchun natijani shu yerga qoʻyib, oynaga xabar yuboramiz.
    std::unique_ptr<TranscribeResult> pending_;

    // Avto-yangilanish (S8). Boʻsh paytni shu klassdagi holatdan soʻraydi.
    Yangilovchi yangilovchi_;
    void yangilovchiniUla();
    // Majburiy yangilanish banneri (S9): holat oʻzgarganda qayta chiziladi.
    void majburiyBanneri();
    bool majburiyBannerKorinadi_ = false;
    // Oxirgi diktovka faolligi (yozuv boshlangan/tugagan, natija) — monoton
    // soniya, -1: hali yoʻq. Yangilanish undan ≥ 2 daqiqa keyin oʻrnatiladi.
    long long oxirgiFaollik_ = -1;
    void faollikBoldi() { oxirgiFaollik_ = static_cast<long long>(GetTickCount64() / 1000); }
};

}  // namespace rubai
