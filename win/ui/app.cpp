// Kotib — Windows ilovasi: yashirin oyna va xabarlar, tray, hotkey,
// sozlamalarni qoʻllash, ishga tushish va yopilish, `wWinMain`.
//
// Klass va doimiylar — `app.h`; diktovka oqimi va saqlanmagan ovoz —
// `app_diktovka.cpp`. macOS'dagi egizagi — `ilova.swift`.
//
// Oqim: Ctrl+Alt+D -> AudioCapture -> Ctrl+Alt+D -> Engine -> Inserter

#include "app.h"

#if KOTIB_TARJIMA
#include "../core/tarjimon.h"
#endif

namespace rubai {
namespace {

// Explorer qayta ishga tushganda tray ikonasi yoʻqoladi va uni qayta
// qoʻshish kerak. Windows buni shu xabar bilan bildiradi.
UINT g_taskbarCreated = 0;

// Ikkinchi nusxa ishlab turgan nusxaga "Sozlamalarni koʻrsat" deyish uchun.
// RegisterWindowMessage tizim boʻylab yagona raqam beradi, shuning uchun
// ikkala jarayon ham bir xil qiymatni oladi.
UINT g_showSettings = 0;

}  // namespace

// ------------------------------------------------------------------- oyna

bool App::createWindow(HINSTANCE instance) {
    instance_ = instance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) {
        logWrite(L"XATO: oyna klassi roʻyxatdan oʻtmadi");
        return false;
    }

    // Koʻrinmas oyna: faqat xabarlarni qabul qilish uchun (hotkey, tray).
    // HWND_MESSAGE ishlatmaymiz — bunday oynalar tray xabarlarini olmaydi.
    hwnd_ =
        CreateWindowExW(0, kWindowClass, kAppName, 0, 0, 0, 0, 0, nullptr, nullptr, instance, this);
    if (!hwnd_) {
        logWrite(L"XATO: oyna yaratilmadi");
        return false;
    }
    return true;
}

LRESULT CALLBACK App::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->handle(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_taskbarCreated && g_taskbarCreated != 0) {
        trayAdded_ = false;
        addTrayIcon();
        return 0;
    }

    // Foydalanuvchi ilovani qaytadan ochdi (yorliq yoki Start menyusi orqali).
    // Ikkinchi nusxa ishga tushmaydi — oʻrniga shu xabarni yuboradi.
    if (msg == g_showSettings && g_showSettings != 0) {
        oynaniOch();
        return 0;
    }

    switch (msg) {
        case WM_HOTKEY:
            if ((int)wp == kHotkeyId) toggleDictation();
            return 0;

        case WM_RUBAI_TRAY:
            // Chap tugma — asosiy oyna. macOS'da menyu satridagi ikonka ham
            // shu ishni qiladi; sozlamalar oynadagi ⚙ dan ochiladi.
            if (LOWORD(lp) == WM_LBUTTONUP) {
                oynaniOch();
            } else if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) {
                showMenu();
            }
            return 0;

        // Yozib olish davomida signal darajasini oynaga uzatamiz.
        case WM_TIMER:
            if (yangilovchi_.taymer(wp)) return 0;
            if (wp == kDarajaTaymer) {
                if (recording_ && asosiy_.korinadimi()) { asosiy_.darajaniQoy(capture_.daraja()); }
                return 0;
            }
            if (wp == kMaxYozishTaymer) {
                if (recording_) {
                    logWrite(L"yozish 10 daqiqadan oshdi — oʻzi toʻxtatildi");
                    stopAndTranscribe();
                }
                return 0;
            }
            if (wp == kQaytaTaymer) {
                KillTimer(hwnd_, kQaytaTaymer);
                // Foydalanuvchi shu payt yozayotgan yoki fayl ishi ketayotgan
                // boʻlsa — kutamiz: navbat bitta, qayta urinish uning
                // diktovkasini kechiktirmasin.
                if (!recording_ && !busy_ && !TranskripsiyaIshi::ishlayapti()) {
                    saqlanganlarniOgir(false);
                }
                return 0;
            }
            break;

        case WM_RUBAI_MODEL: {
            if (static_cast<long long>(wp) == oxirgiModelMb_) return 0;
            oxirgiModelMb_ = static_cast<long long>(wp);
            wchar_t matn[96];
            swprintf(matn, 96, L"Model yuklanmoqda…  %llu / %llu MB",
                     static_cast<unsigned long long>(wp), static_cast<unsigned long long>(lp));
            asosiy_.banner(matn, L"Bekor qilish", [this] { modelYuklovchi_.bekorQil(); });
            return 0;
        }

        case WM_RUBAI_MODEL_TUGADI: {
            std::unique_ptr<std::wstring> xato(reinterpret_cast<std::wstring*>(lp));
            if (wp == 1) {
                // Bannerni shunchaki tozalamaymiz: model masalasi hal
                // boʻldi, lekin mikrofon ogohlantirishi qolgan boʻlishi
                // mumkin. Ilgari u keyingi ochilishgacha koʻrinmasdi.
                mikrofonBanneri();
                notify(L"Kotib", L"Nutq modeli yuklandi — diktovka tayyor.", NIIF_INFO);
                SetTimer(hwnd_, kQaytaTaymer, kQaytaKutishMs, nullptr);
            } else {
                asosiy_.banner(L"Model yuklanmadi: " + *xato, L"Qayta urinish",
                               [this] { modelniYukla(); });
            }
            return 0;
        }

        case WM_RUBAI_RESULT:
            if (pending_) {
                onResult(*pending_);
                pending_.reset();
            }
            return 0;

        case WM_RUBAI_QAYTA:
            if (pendingQayta_) {
                const std::unique_ptr<TranscribeResult> r = std::move(pendingQayta_);
                onQaytaNatija(*r);
            }
            return 0;

        // Avto-yangilovchining fon natijasi (tekshiruv, yuklash jarayoni).
        case WM_RUBAI_YANGILANISH: yangilovchi_.xabar(wp, lp); return 0;

        // Uyqudan uygʻonish: oxirgi muvaffaqiyatli tekshiruvdan 24 soat
        // oʻtgan boʻlsa — tekshiruv (macOS'dagi `didWakeNotification`).
        case WM_POWERBROADCAST:
            if (wp == PBT_APMRESUMEAUTOMATIC) yangilovchi_.uygondi();
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case kMenuDictate: toggleDictation(); return 0;
                case kMenuQayta: saqlanganlarniOgir(true); return 0;
                case kMenuLog:
                    ShellExecuteW(nullptr, L"open", logPath().c_str(), nullptr, nullptr,
                                  SW_SHOWNORMAL);
                    return 0;
                case kMenuLitsenziya: {
                    // `scripts/litsenziyalar.sh` yasaydi, oʻrnatuvchi `{app}` ga qoʻyadi.
                    const std::wstring f = exeDir() + L"\\Litsenziyalar.txt";
                    ShellExecuteW(nullptr, L"open", f.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
                case kMenuOyna: oynaniOch(); return 0;
                case kMenuYangilanish: yangilovchi_.tugmaBosildi(); return 0;
                case kMenuSettings:
                    settingsWindow_.show(instance_, settings_, asosiy_.deskriptor());
                    return 0;
                case kMenuQuit: DestroyWindow(hwnd); return 0;
            }
            return 0;

        case WM_ENDSESSION:
            // wParam FALSE — oʻchirish/chiqish BEKOR qilindi: Kotib ishlashda
            // davom etadi (E4). Ilgari bu ham yopilish deb olinardi va
            // foydalanuvchi bekor qilgan oʻchirishdan keyin ilovasiz qolardi.
            if (!wp) {
                logWrite(L"seans tugashi bekor qilindi — ishlashda davom etamiz");
                return 0;
            }
            // Seans haqiqatan tugayapti. Shu xabar qaytgach Windows jarayonni
            // istalgan payt toʻxtatadi va WM_QUIT gacha yetib borilmaydi —
            // tozalash SHU YERDA, sinxron.
            logWrite(L"seans tugayapti — chiqilmoqda");
            shutdown();
            return 0;

        case WM_DESTROY:
            removeTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------------- tray

bool App::addTrayIcon() {
    if (trayAdded_) return true;

    tray_ = {};
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = hwnd_;
    tray_.uID = 1;
    tray_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray_.uCallbackMessage = WM_RUBAI_TRAY;
    tray_.hIcon = (HICON)LoadImageW(instance_, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                    LR_DEFAULTCOLOR);
    if (!tray_.hIcon) tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(tray_.szTip, kAppName, _TRUNCATE);

    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
    if (trayAdded_) {
        tray_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &tray_);
    } else {
        logWrite(L"XATO: tray ikonasi qoʻshilmadi");
    }
    return trayAdded_;
}

void App::removeTrayIcon() {
    if (!trayAdded_) return;
    Shell_NotifyIconW(NIM_DELETE, &tray_);
    trayAdded_ = false;
}

void App::updateTrayTip(const std::wstring& text) {
    if (!trayAdded_) return;
    tray_.uFlags = NIF_TIP;
    wcsncpy_s(tray_.szTip, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &tray_);
}

void App::notify(const std::wstring& title, const std::wstring& text, DWORD icon) {
    if (!trayAdded_) return;
    NOTIFYICONDATAW n = tray_;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = icon;
    wcsncpy_s(n.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

void App::showMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    const std::wstring hk = settings_.hotkeyDisplay();
    AppendMenuW(
        menu, MF_STRING | (busy_ ? MF_GRAYED : 0), kMenuDictate,
        (recording_ ? L"Yozishni toʻxtatish  (" + hk + L")" : L"Diktovka  (" + hk + L")").c_str());
    // Matnga oʻgirib boʻlmagan ovoz bor boʻlsagina (A2).
    const size_t saqlangan = saqlanmagan::royxat(saqlanmaganPapka()).size();
    if (saqlangan > 0) {
        AppendMenuW(
            menu, MF_STRING | (qaytaIshlanyapti_ ? MF_GRAYED : 0), kMenuQayta,
            (L"Saqlangan ovozni matnga oʻgirish (" + std::to_wstring(saqlangan) + L")").c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuOyna, L"Oynani ochish");
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"Sozlamalar…");
    if (yangilovchi_.faolmi()) {
        AppendMenuW(menu, MF_STRING, kMenuYangilanish,
                    yangilovchi_.tayyormi() ? (L"Kotib " + yangilovchi_.tayyorVersiya() +
                                               L" ni oʻrnatish va qayta ochish")
                                                  .c_str()
                                            : L"Yangilanishni tekshirish");
    }
    AppendMenuW(menu, MF_STRING, kMenuLog, L"Log faylini ochish…");
    AppendMenuW(menu, MF_STRING, kMenuLitsenziya, L"Litsenziyalar…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, L"Chiqish");

    POINT pt;
    GetCursorPos(&pt);
    // Menyu tashqarisiga bosilganda yopilishi uchun oyna faol boʻlishi kerak.
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

// ---------------------------------------------------------------- hotkey

bool App::registerHotkey() {
    const UINT mods = settings_.modifiers | MOD_NOREPEAT;
    if (RegisterHotKey(hwnd_, kHotkeyId, mods, settings_.vkCode)) {
        logWrite(L"hotkey roʻyxatdan oʻtdi: " + settings_.hotkeyDisplay());
        return true;
    }

    logWrite(L"XATO: hotkey band — " + settings_.hotkeyDisplay());
    MessageBoxW(nullptr,
                (L"Diktovka tugmasi (" + settings_.hotkeyDisplay() +
                 L") boshqa dastur tomonidan band qilingan.\n\n"
                 L"Sozlamalar faylidan boshqa tugma tanlang:\n" +
                 settingsPath())
                    .c_str(),
                kAppName, MB_OK | MB_ICONWARNING);
    return false;
}

// ------------------------------------------------------------ asosiy oyna

void App::oynaniOch() {
    asosiy_.tarixniYangila();
    asosiy_.hotkeyMatni(settings_.hotkeyDisplay());
    mikrofonBanneri();
    asosiy_.korsat();
}

// Yozish tabidagi ogohlantirish banneri.
//
// macOS'da bu yerda Accessibility ruxsati banneri turadi; Windows'da unday
// ruxsat kerak emas, lekin ikkita boshqa muammo bor va ular jimgina
// «ilova ishlamayapti» boʻlib koʻrinadi:
//   1. Nutq modeli yoʻq — har diktovkada xato chiqadi;
//   2. Mikrofon «Stereo Mix» — ovoz oʻrniga kompyuter ovozi yoziladi.
// Model muhimroq, shuning uchun u birinchi tekshiriladi.
void App::mikrofonBanneri() {
    modelBanneri_ = false;
    if (Engine::findModel().empty()) {
        if (modelYuklovchi_.ketyaptimi()) return;  // matnni yuklash yangilaydi
        modelBanneri_ = true;
        asosiy_.banner(L"Nutq modeli topilmadi — diktovka ishlamaydi.", L"Yuklab olish",
                       [this] { modelniYukla(); });
        return;
    }
    if (settings_.micDeviceId.empty()) {
        // Qurilma tanlanmagan — Windows standartini oladi. Standart oddiy
        // mikrofon boʻlsa hech qanday muammo yoʻq va bezovta qilmaymiz;
        // ogohlantirish faqat standart XAVFLI boʻlganda chiqadi («Stereo
        // Mix» kabi qurilmalar ovoz oʻrniga kompyuter ovozini yozadi).
        // Sozlamalar oynasidagi tekshiruv ham aynan shunday
        // (`settings_window.cpp` → `updateMicWarning`).
        const auto royxat = listMicrophones();
        if (royxat.empty()) {
            asosiy_.banner(L"Mikrofon topilmadi — diktovka ishlamaydi.", L"Sozlamalar", [this] {
                settingsWindow_.show(instance_, settings_, asosiy_.deskriptor());
            });
            return;
        }
        for (const auto& m : royxat) {
            if (!m.isDefault || m.kind == MicKind::Normal) continue;
            asosiy_.banner(
                L"Tizim standarti: " + m.name + L" — " + m.warning(), L"Tanlash",
                [this] { settingsWindow_.show(instance_, settings_, asosiy_.deskriptor()); });
            return;
        }
        asosiy_.banner(L"");
        return;
    }
    for (const auto& m : listMicrophones()) {
        if (m.id != settings_.micDeviceId) continue;
        const std::wstring ogoh = m.warning();
        if (!ogoh.empty()) {
            asosiy_.banner(ogoh, L"Almashtirish", [this] {
                settingsWindow_.show(instance_, settings_, asosiy_.deskriptor());
            });
        } else {
            asosiy_.banner(L"");
        }
        return;
    }
    // Saqlangan qurilma endi yoʻq (chiqarib olindi yoki oʻchirildi).
    asosiy_.banner(L"Tanlangan mikrofon topilmadi.", L"Tanlash",
                   [this] { settingsWindow_.show(instance_, settings_, asosiy_.deskriptor()); });
}

// Nutq modelini ilova ichida yuklab olish.
//
// macOS'da bu alohida oyna (`model_download.swift`); Windows'da esa banner
// ichida ketadi — u allaqachon oʻsha yerda turibdi va yangi oyna ochish
// foydalanuvchiga qoʻshimcha qadam boʻlardi.
void App::modelniYukla() {
    if (modelYuklovchi_.ketyaptimi()) {
        modelYuklovchi_.bekorQil();
        return;
    }
    oxirgiModelMb_ = -1;
    asosiy_.banner(L"Model yuklanmoqda…", L"Bekor qilish", [this] { modelYuklovchi_.bekorQil(); });

    HWND oyna = hwnd_;
    modelYuklovchi_.boshla(
        [oyna](long long olingan, long long jami) {
            PostMessageW(oyna, WM_RUBAI_MODEL, static_cast<WPARAM>(olingan / (1024 * 1024)),
                         static_cast<LPARAM>(jami / (1024 * 1024)));
        },
        [oyna](bool ok, std::wstring xato) {
            PostMessageW(oyna, WM_RUBAI_MODEL_TUGADI, ok ? 1 : 0,
                         reinterpret_cast<LPARAM>(new std::wstring(std::move(xato))));
        });
}

// -------------------------------------------------- sozlamalarni qoʻllash

void App::applySettings(const Settings& s) {
    const bool hotkeyChanged =
        (s.vkCode != settings_.vkCode) || (s.modifiers != settings_.modifiers);
    const bool gpuChanged = (s.useGpu != settings_.useGpu);

    settings_ = s;

    if (hotkeyChanged) {
        UnregisterHotKey(hwnd_, kHotkeyId);
        registerHotkey();
    }

    Engine& engine = Engine::instance();
    engine.setIdleUnloadSeconds(settings_.idleUnloadSeconds);

    if (gpuChanged) {
        // GPU rejimi oʻzgardi — modelni qayta yuklash kerak, chunki
        // backend yuklash paytida tanlanadi.
        engine.setUseGpu(settings_.useGpu);
        engine.unload();
        engine.preload();
    }

    asosiy_.hotkeyMatni(settings_.hotkeyDisplay());
    mikrofonBanneri();
}

// ------------------------------------------------------------------- init

bool App::init(HINSTANCE instance) {
    logInit();
    settings_ = loadSettings();

    // Ishga tushish qatori (A6): muammo xabarida «qaysi versiya, qaysi
    // Windows, qaysi model» degan savol qolmasin.
    {
        const std::wstring model = Engine::findModel();
        std::wstring hajm;
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (!model.empty() && GetFileAttributesExW(model.c_str(), GetFileExInfoStandard, &a)) {
            const unsigned long long b =
                (static_cast<unsigned long long>(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
            hajm = L" (" + std::to_wstring(b) + L" bayt)";
        }
        const size_t saqlangan = saqlanmagan::royxat(saqlanmaganPapka()).size();
        logWrite(
            L"--- ilova ishga tushdi: " + ishgaTushishMuhiti() + L", model: " +
            (model.empty() ? std::wstring(L"yoʻq") : model + hajm) + L", GPU: " +
            (settings_.useGpu ? L"yoqiq" : L"oʻchiq") +
            (saqlangan ? L", saqlanmagan ovoz: " + std::to_wstring(saqlangan) : std::wstring()) +
            L" ---");
        saqlanmagan::tozala(saqlanmaganPapka(), saqlanmagan::hozirMs());
    }

    if (!createWindow(instance)) return false;
    overlay_.create(instance);
    addTrayIcon();
    registerHotkey();

    settingsWindow_.setOnSaved([this](const Settings& s) { applySettings(s); });

    // Asosiy oyna. Ilova tray'da yashaydi va oyna faqat soʻralganda
    // koʻrinadi — macOS'dagi LSUIElement xatti-harakatining ekvivalenti.
    if (!asosiy_.qur(instance)) { logWrite(L"XATO: asosiy oyna qurilmadi — faqat tray rejimi"); }
    asosiy_.onDiktovka = [this] { toggleDictation(); };
    asosiy_.onSozlamalar = [this] {
        settingsWindow_.show(instance_, settings_, asosiy_.deskriptor());
    };
    asosiy_.hotkeyMatni(settings_.hotkeyDisplay());
    asosiy_.tarixniYangila();

    // Eskirgan avtostart yozuvini tuzatamiz (nom yoki yoʻl oʻzgargan
    // boʻlsa). Sozlamalar oynasi ochilishini kutib boʻlmaydi: koʻpchilik
    // foydalanuvchi uni umuman ochmaydi.
    avtostartniTuzat();

    // Birinchi ishga tushish: avtostartni yoqamiz va sozlamalarni
    // koʻrsatamiz, foydalanuvchi mikrofonini tanlab olsin.
    if (!settings_.didOnboard) {
        settings_.didOnboard = true;
        setAutoStart(settings_.autoStart);
        saveSettings(settings_);
        PostMessageW(hwnd_, WM_COMMAND, kMenuSettings, 0);
    }

    // Xavfsiz rejim (E6): oldingi ishga tushishda GPU yuklanayotgan (yoki
    // isitilayotgan) paytda jarayon yiqilgan — belgi qolib ketgan. Sabab
    // deyarli har doim drayver: CPU'ga oʻtamiz va buni saqlaymiz, aks holda
    // login'dagi avtostart har safar yiqilaverardi.
    if (Engine::gpuOldinYiqilgan()) {
        Engine::gpuBelgisiniOchir();
        if (settings_.useGpu) {
            logWrite(L"XATO: oldingi ishga tushishda GPU yuklanayotganda ilova yiqilgan — CPU "
                     L"rejimiga oʻtildi");
            settings_.useGpu = false;
            Settings s = loadSettings();
            s.useGpu = false;
            saveSettings(s);
            notify(L"Kotib — protsessor rejimi",
                   L"Oldingi safar videokarta drayveri Kotib'ni yopib yubordi.\n"
                   L"Ovoz tanish endi protsessorda ishlaydi. Sozlamalarda\n"
                   L"«Videokartadan foydalanish» ni qayta yoqish mumkin.",
                   NIIF_WARNING);
        }
    }

    Engine& engine = Engine::instance();
    engine.setUseGpu(settings_.useGpu);
    engine.setIdleUnloadSeconds(settings_.idleUnloadSeconds);

    // Modelni fonda oldindan yuklaymiz va GPU quvurini isitamiz — birinchi
    // diktovka darhol ishlashi uchun.
    engine.preload();

    updateTrayTip(kAppName);

    // Statistika va yangilanish — alohida (spec: biri yiqilsa ikkinchisi
    // ishlayveradi). Ikkalasi ham fon oqimida, tarmoq yoʻq boʻlsa jim.
    pingYubor();
    yangilovchiniUla();

    return true;
}

// Avto-yangilovchi ilova holatini shu funksiyalar orqali soʻraydi —
// `yangilovchi.h` ga qarang. Hammasi UI oqimida.
void App::yangilovchiniUla() {
    // Boʻsh payt: yozuv, diktovka natijasi, fayl ishi, saqlangan ovozni
    // oʻgirish, model yuklash va tarjima — hech biri ketmayotgan boʻlsin.
    yangilovchi_.bandmi = [this] {
        bool tarjima = false;
#if KOTIB_TARJIMA
        tarjima = Tarjimon::birgalik().bandmi();
#endif
        return DiktovkaBand::faol() || busy_ || TranskripsiyaIshi::ishlayapti() ||
               qaytaIshlanyapti_ || modelYuklovchi_.ketyaptimi() || tarjima;
    };
    // Majburiy yangilanish faqat diktovkani kutadi (spec «Majburiy yangilanish»).
    yangilovchi_.yozilyaptimi = [this] { return DiktovkaBand::faol() || busy_; };
    yangilovchi_.oxirgiFaollik = [this] { return oxirgiFaollik_; };
    yangilovchi_.bildir = [this](const std::wstring& s, const std::wstring& m) {
        notify(s, m, NIIF_INFO);
    };
    yangilovchi_.yangilandi = [this](const std::wstring& m, const std::wstring& kalit) {
        asosiy_.yangilanishBanneri(m, kalit);
    };
    // Oʻrnatuvchi ishga tushdi: odatdagi yopilish (shutdown) — Inno fayllarni
    // almashtirib, Kotib'ni qayta ochadi.
    yangilovchi_.chiqish = [this] { DestroyWindow(hwnd_); };
    yangilovchi_.ozgardi = [this] {
        settingsWindow_.yangilanishniYangila();
        majburiyBanneri();
    };
    settingsWindow_.setYangilanish([this] { return yangilovchi_.korinish(); },
                                   [this] { yangilovchi_.tugmaBosildi(); });
    yangilovchi_.boshla(hwnd_, WM_RUBAI_YANGILANISH);
}

// Majburiy yangilanish (S9): muhlat ichida — «Muhim yangilanish …» (yopilmaydi),
// muhlat tugagach — «Yangilanish majburiy — diktovka toʻxtatildi»; zaxira
// «Saytdan yuklab olish» faqat avtomatik yuklash xatosidan keyin. Dizayndagi alohida karta oʻrniga oyna banneri:
// koʻrinish soddalashdi, imkoniyat (holat, qayta urinish, sayt) qoldi.
void App::majburiyBanneri() {
    const Yangilovchi::MajburiyKorinish k = yangilovchi_.majburiyKorinish();
    if (!k.bor) {
        if (majburiyBannerKorinadi_) asosiy_.yangilanishBanneriniYashir();
        majburiyBannerKorinadi_ = false;
        return;
    }
    majburiyBannerKorinadi_ = true;
    // Sayt manzili QATTIQ yozilgan — serverdan kelgan URL ochilmaydi (1.1.0
    // dagi xato qaytmasin).
    asosiy_.yangilanishBanneri(
        k.matn, L"", k.tugma, [this] { yangilovchi_.majburiyTugma(); },
        k.sayt ? L"Saytdan yuklab olish" : L"",
        [] {
            ShellExecuteW(nullptr, L"open", L"https://uzb.mirqobilov.com", nullptr, nullptr,
                          SW_SHOWNORMAL);
        });
}

void App::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // Sozlamalar oynasi tizim boshqaruvlaridan iborat, lekin dialog
        // EMAS — shuning uchun Tab, strelkalar, Enter va Esc'ni oʻzi
        // boshqara olmaydi. `IsDialogMessageW` shuni beradi. Busiz oynani
        // faqat sichqoncha bilan boshqarish mumkin edi: klaviatura bilan
        // ishlaydiganlar va ekran oʻqiruvchidan foydalanuvchilar uchun u
        // umuman ochilmaydigan eshik boʻlardi.
        //
        // Asosiy oynaga BERILMAYDI: uning boshqaruvlari Direct2D bilan
        // chiziladi va yagona haqiqiy bola oyna — RichEdit. U yerda
        // `IsDialogMessageW` Tab'ni yeb qoʻyardi.
        HWND sozlamalar = settingsWindow_.oyna();
        if (sozlamalar && IsWindowVisible(sozlamalar) && IsDialogMessageW(sozlamalar, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void App::shutdown() {
    // Ikki joydan chaqiriladi: WM_ENDSESSION va xabar halqasidan keyin.
    if (yopildi_) return;
    yopildi_ = true;
    yangilovchi_.toxtat();
    if (recording_) capture_.stop();
    UnregisterHotKey(hwnd_, kHotkeyId);
    removeTrayIcon();
    Engine::instance().shutdown();
    logWrite(L"--- ilova yopildi ---");
}

}  // namespace rubai

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    using namespace rubai;

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_showSettings = RegisterWindowMessageW(L"KotibShowSettings");

    // Ikkinchi nusxa ishga tushmasin — ikkita ilova bitta hotkey'ni
    // ushlab, kutilmagan xatti-harakat beradi.
    //
    // Lekin foydalanuvchi yorliqni bosgan boʻlsa, u nimadir boʻlishini
    // kutadi. "Allaqachon ishlayapti" degan xabar boshi berk koʻcha —
    // buning oʻrniga ishlab turgan nusxaning Sozlamalar oynasini ochamiz.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kWindowClass, nullptr);
        if (existing) {
            PostMessageW(existing, g_showSettings, 0, 0);
        } else {
            // Oyna topilmadi (ilova yopilish jarayonida boʻlishi mumkin).
            MessageBoxW(nullptr,
                        L"Kotib allaqachon ishlayapti.\n"
                        L"Sozlamalar uchun vazifalar panelidagi 🎙 ikonkasini bosing.",
                        kAppName, MB_OK | MB_ICONINFORMATION);
        }
        CloseHandle(mutex);
        return 0;
    }

    // Yuqori DPI monitorlarda oynalar xira koʻrinmasligi uchun.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    App app;
    if (!app.init(instance)) {
        MessageBoxW(nullptr, L"Ilova ishga tushmadi. Log faylini tekshiring.", kAppName,
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    app.run();
    app.shutdown();

    if (mutex) CloseHandle(mutex);
    return 0;
}
