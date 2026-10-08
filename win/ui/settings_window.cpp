// Sozlamalar oynasi: qurilish, joylashuv va surish, mikrofon roʻyxati,
// saqlash (`sozlamaOynasidan`) va ochiq API. LLM boʻlimi —
// `settings_window_llm.cpp`; ichki eʼlonlar — `settings_window_ichki.h`.

#include "settings_window_ichki.h"

namespace rubai {

namespace {

// HOTKEY boshqaruvi HOTKEYF_* bayroqlarini beradi, RegisterHotKey esa
// MOD_* kutadi — ular boshqacha qiymatlar.
UINT hotkeyFlagsToMod(BYTE flags) {
    UINT m = 0;
    if (flags & HOTKEYF_CONTROL) m |= MOD_CONTROL;
    if (flags & HOTKEYF_ALT) m |= MOD_ALT;
    if (flags & HOTKEYF_SHIFT) m |= MOD_SHIFT;
    return m;
}

BYTE modToHotkeyFlags(UINT mod) {
    BYTE f = 0;
    if (mod & MOD_CONTROL) f |= HOTKEYF_CONTROL;
    if (mod & MOD_ALT) f |= HOTKEYF_ALT;
    if (mod & MOD_SHIFT) f |= HOTKEYF_SHIFT;
    return f;
}

// Tugma nomini virtual key code'dan olamiz — sozlamalar faylida
// va menyuda koʻrsatish uchun.
std::wstring keyLabel(UINT vk) {
    const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    wchar_t name[64] = {};
    if (scan && GetKeyNameTextW((LONG)(scan << 16), name, 64) > 0) return name;
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    return L"Key" + std::to_wstring(vk);
}

// Oynani ishonchli tarzda oldinga chiqaradi.
//
// SetForegroundWindow yolgʻiz oʻzi YETARLI EMAS: Windows fonda ishlayotgan
// jarayonga faol oynani almashtirishni taqiqlaydi (foreground lock).
// Amalda tekshirildi — sozlamalar oynasi yaratilar, lekin boshqa ilova
// ostida koʻrinmay qolar edi.
//
// Yechim: faol oynaning kirish oqimiga vaqtincha ulanamiz — shunda tizim
// bizni "foydalanuvchi bilan ishlayotgan" deb hisoblaydi.
void forceForeground(HWND hwnd) {
    const HWND fg = GetForegroundWindow();
    const DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    const DWORD myThread = GetCurrentThreadId();
    const bool attach = fgThread != 0 && fgThread != myThread;

    if (attach) AttachThreadInput(fgThread, myThread, TRUE);

    ShowWindow(hwnd, SW_SHOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);

    if (attach) AttachThreadInput(fgThread, myThread, FALSE);

    // Baribir ochilmasa — vazifalar panelida miltillatib eʼtibor tortamiz.
    if (GetForegroundWindow() != hwnd) {
        FLASHWINFO fi{sizeof(fi), hwnd, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0};
        FlashWindowEx(&fi);
    }
}

SettingsWindow::Impl* implOf(HWND h) {
    return reinterpret_cast<SettingsWindow::Impl*>(GetWindowLongPtrW(h, GWLP_USERDATA));
}

LRESULT CALLBACK settingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* d = implOf(hwnd);

    switch (msg) {
        case WM_COMMAND:
            if (!d) break;
            switch (LOWORD(wp)) {
                case kIdSave:
                    d->save();
                    ShowWindow(hwnd, SW_HIDE);
                    return 0;
                case IDCANCEL:  // Esc — `IsDialogMessageW` shuni yuboradi
                case kIdCancel: ShowWindow(hwnd, SW_HIDE); return 0;
                case kIdReset:
                    SendMessageW(d->ctrl(kIdHotkey), HKM_SETHOTKEY,
                                 MAKEWORD('D', HOTKEYF_CONTROL | HOTKEYF_ALT), 0);
                    return 0;
                case kIdMic:
                    if (HIWORD(wp) == CBN_SELCHANGE) d->updateMicWarning();
                    return 0;
                case kIdDonate: d->donate.show(d->instance, hwnd); return 0;
                case kIdLog:
                    ShellExecuteW(nullptr, L"open", logPath().c_str(), nullptr, nullptr,
                                  SW_SHOWNORMAL);
                    return 0;
                case kIdYangilanishTugma:
                    // Bu amal, sozlama emas — «Saqlash» ni kutmaydi.
                    if (d->yangilanishBosildi) d->yangilanishBosildi();
                    d->yangilanishQatori();
                    return 0;
                case kIdQoshimcha:
                    d->qoshimchaOchiq = !d->qoshimchaOchiq;
                    d->joylashuvniYangila();
                    return 0;
                case kIdProvayder:
                    if (HIWORD(wp) == CBN_SELCHANGE) d->provayderOzgardi();
                    return 0;
                case kIdModelYangila: d->modellarniOlish(); return 0;
                case kIdTekshir: d->ulanishniTekshir(); return 0;
            }
            break;

        // Fon oqimidan kelgan natijalar.
        case WM_LLM_MODELLAR: {
            std::unique_ptr<std::vector<std::wstring>> r(
                reinterpret_cast<std::vector<std::wstring>*>(lp));
            if (!d) return 0;
            d->llmSoravKetyapti = false;
            if (r->empty()) {
                d->maydonQoy(kIdModelHolati,
                             L"Bu provayder model roʻyxatini bermaydi — nomni qoʻlda yozing");
            } else {
                d->modellarniKorsat(*r);
                d->maydonQoy(kIdModelHolati, std::to_wstring(r->size()) + L" ta model");
            }
            return 0;
        }

        case WM_LLM_HOLAT: {
            std::unique_ptr<std::wstring> m(reinterpret_cast<std::wstring*>(lp));
            if (!d) return 0;
            d->llmSoravKetyapti = false;
            d->maydonQoy(static_cast<int>(wp), *m);
            return 0;
        }

        // Donat tugmasini oʻzimiz chizamiz — Windows'ning standart tugmasi
        // rang qabul qilmaydi, bu esa ajralib turishi kerak.
        case WM_DRAWITEM: {
            auto* di = (DRAWITEMSTRUCT*)lp;
            if (!d || di->CtlID != kIdDonate) break;

            const bool pressed = (di->itemState & ODS_SELECTED) != 0;
            const bool focused = (di->itemState & ODS_FOCUS) != 0;

            HBRUSH b = CreateSolidBrush(pressed ? kDonateBgHover : kDonateBg);
            FillRect(di->hDC, &di->rcItem, b);
            DeleteObject(b);

            if (focused) {
                RECT f = di->rcItem;
                InflateRect(&f, -3, -3);
                DrawFocusRect(di->hDC, &f);
            }

            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, RGB(255, 255, 255));
            HFONT old = (HFONT)SelectObject(di->hDC, d->font);
            wchar_t text[64] = {};
            GetWindowTextW(di->hwndItem, text, 64);
            DrawTextW(di->hDC, text, -1, &di->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(di->hDC, old);
            return TRUE;
        }

        case WM_VSCROLL: {
            if (!d) break;
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int yangi = d->surilish;
            switch (LOWORD(wp)) {
                case SB_LINEUP: yangi -= 24; break;
                case SB_LINEDOWN: yangi += 24; break;
                case SB_PAGEUP: yangi -= (int)si.nPage; break;
                case SB_PAGEDOWN: yangi += (int)si.nPage; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: yangi = si.nTrackPos; break;
                case SB_TOP: yangi = 0; break;
                case SB_BOTTOM: yangi = si.nMax; break;
                default: return 0;
            }
            d->sur(yangi);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (!d) break;
            const int qadam = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            d->sur(d->surilish - qadam * 60);
            return 0;
        }

        // `IsDialogMessageW` Enter bosilganda default tugmani shu xabar
        // orqali soʻraydi. Javob bermasak u IDOK (1) yuboradi va biz uni
        // bilmaymiz — Enter hech narsa qilmasdi.
        case DM_GETDEFID: return MAKELONG(kIdSave, DC_HASDEFID);

        case WM_CLOSE: ShowWindow(hwnd, SW_HIDE); return 0;

        case WM_CTLCOLORSTATIC: {
            // Yorliqlar va belgilash katakchalari oyna foni bilan bir xil
            // koʻrinishi kerak — aks holda ular kulrang tasma boʻlib chiqadi.
            auto hdc = (HDC)wp;
            SetBkMode(hdc, TRANSPARENT);
            if (d && (HWND)lp == d->ctrl(kIdMicWarning)) {
                SetTextColor(hdc, RGB(176, 92, 0));  // ogohlantirish — toʻq sariq
            }
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

// -------------------------------------------------------------- qurilish

void SettingsWindow::Impl::build() {
    auto S = [&](int v) { return scale(v); };

    auto add = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h,
                   int id) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w),
                                 S(h), hwnd, (HMENU)(INT_PTR)id, instance, nullptr);
        joylar.push_back({c, x, y, w, h});
        if (c) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        return c;
    };

    // --- Diktovka tugmasi
    add(L"STATIC", L"Diktovka tugmasi", 0, 24, 20, 200, 18, -1);
    add(HOTKEY_CLASSW, L"", 0, 24, 42, 200, 26, kIdHotkey);
    add(L"BUTTON", L"Standart (Ctrl+Alt+D)", BS_PUSHBUTTON, 236, 42, 190, 26, kIdReset);
    add(L"STATIC", L"Kamida bitta modifikator (Ctrl, Alt, Shift) kerak.", 0, 24, 72, 400, 20, -1);

    // --- Mikrofon
    add(L"STATIC", L"Mikrofon", 0, 24, 106, 200, 18, -1);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 24, 128, 402, 200, kIdMic);
    add(L"STATIC", L"", SS_LEFT, 24, 160, 402, 52, kIdMicWarning);

    // --- Matn kiritish usuli
    add(L"STATIC", L"Matn kiritish usuli", 0, 24, 218, 200, 18, -1);
    HWND mode =
        add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 24, 240, 402, 120, kIdInsertMode);
    SendMessageW(mode, CB_ADDSTRING, 0,
                 (LPARAM)L"Tez (clipboard orqali qoʻyish) — tavsiya etiladi");
    SendMessageW(mode, CB_ADDSTRING, 0,
                 (LPARAM)L"Sekin (belgima-belgi yozish) — qoʻyish ishlamasa");

    // --- Bayroqlar
    add(L"BUTTON", L"Kompyuter yonganda ishga tushsin", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 278, 402,
        22, kIdAutoStart);
    add(L"BUTTON", L"Videokartadan foydalanish (tezroq)", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 304,
        402, 22, kIdUseGpu);

    // --- Yangilanishlar (S8). Holat va tugma `yangilovchi.h` dan keladi:
    // «Kotib 1.2.0 · oxirgi tekshiruv: …» / «… yuklanmoqda 45 %» / «… tayyor».
    // Matn ikki qatorga sigʻadi — sana va versiya uzun boʻlishi mumkin.
    add(L"STATIC", L"", SS_LEFT, 24, 336, 270, 34, kIdYangilanishHolati);
    add(L"BUTTON", L"Hozir tekshirish", BS_PUSHBUTTON | WS_TABSTOP, 306, 336, 120, 26,
        kIdYangilanishTugma);

    // --- «Qoʻshimcha sozlamalar» ochilishi
    //
    // Ostidagi hamma narsa sukut boʻyicha yashirin: toʻliq roʻyxat 700 DIP
    // boʻlib ketadi va kichik ekranga sigʻmaydi. macOS'da ham shu naqsh.
    // Uchburchak (▾/▴) — macOS'dagi ⌄/⌃ oʻrniga. Segoe UI'da U+2303/2304
    // yoʻq va Windows ularni boshqa shriftdan oladi: natijada ingliz
    // klaviaturasidagi «^» kabi ingichka belgi chiqadi.
    add(L"BUTTON", L"Qoʻshimcha sozlamalar  ▾", BS_PUSHBUTTON | BS_FLAT | WS_TABSTOP, 24, 378, 200,
        26, kIdQoshimcha);

    // Oddiy ASCII apostrof. Sukut boʻyicha oʻchiq — oʻzbek lotin meʼyori
    // ʻ va ʼ ni talab qiladi (macOS'da `Prefs.apostrof`).
    add(L"BUTTON", L"Oddiy apostrof (') ishlatish", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 416, 402, 22,
        kIdApostrof);

    // Anonim statistika — OSHKORALIK yozuvi (toggle YOʻQ, doim yoqiq).
    // Yigʻish yashirin emas: bu yerda nima yuborilishi ochiq aytiladi va
    // maxfiylik siyosatiga havola beriladi. Nima yuborilishi
    // `core/statistika.h` da yozilgan.
    add(L"STATIC", L"Anonim ishlash statistikasi", 0, 24, 442, 402, 22, kIdStatistika);
    add(L"STATIC",
        L"Ilovani yaxshilash uchun anonim maʼlumot yuboriladi: ilova va tizim "
        L"versiyasi, qurilma turi (protsessor, xotira, videokarta), hamda har "
        L"transkripsiya/tarjima uchun tezlik va davomiylik. Ovoz, matn va nima "
        L"yozganingiz HECH QACHON yuborilmaydi. Batafsil: uzb.mirqobilov.com/maxfiylik",
        0, 42, 464, 384, 44, kIdStatistikaIzoh);

    // --- Sunʼiy intellekt (ixtiyoriy)
    //
    // Ilova hech qanday kalit bilan kelmaydi va busiz toʻliq ishlaydi.
    // Kalit foydalanuvchining oʻzi kiritadi va Credential Manager'da
    // saqlanadi (`llm.h`), logga hech qachon tushmaydi.
    add(L"STATIC", L"Sunʼiy intellekt (ixtiyoriy)", 0, 24, 518, 300, 18, kIdAiSarlavha);
    add(L"STATIC", L"Kalit qoʻshsangiz, matnni tozalash va xulosa qilish qoʻshiladi.", 0, 24, 538,
        402, 20, kIdAiIzoh);

    HWND prov = add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 24, 560, 180, 240,
                    kIdProvayder);
    SendMessageW(prov, CB_ADDSTRING, 0, (LPARAM)L"— tanlanmagan —");
    for (const auto& pv : provayderlar()) {
        SendMessageW(prov, CB_ADDSTRING, 0, (LPARAM)pv.nom.c_str());
    }

    // Kalit — yulduzcha bilan. ES_PASSWORD yelkadan qarab oʻqishga qarshi;
    // kalitning oʻzi baribir Credential Manager'da shifrlanadi.
    // Maydonlarda yorliq yoʻq, shuning uchun ishora matni (cue banner)
    // qoʻyiladi — macOS'da bu `placeholderString`.
    add(L"EDIT", L"", ES_AUTOHSCROLL | ES_PASSWORD | WS_BORDER | WS_TABSTOP, 212, 560, 214, 24,
        kIdKalit);

    // Base URL faqat «Boshqa (custom)» provayderda kerak — qolganlarida
    // manzil presetdan keladi va uni koʻrsatish faqat chalgʻitadi.
    add(L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 24, 590, 402, 24, kIdBaseURL);

    // Model — tahrirlanadigan roʻyxat (CBS_DROPDOWN), oddiy tanlagich emas:
    //  1. OpenRouter 400 ga yaqin model qaytaradi;
    //  2. provayderda `/models` boʻlmasa nom qoʻlda yoziladi — boshi berk
    //     koʻcha hech qachon boʻlmaydi.
    add(L"STATIC", L"Model", 0, 24, 626, 60, 18, kIdModelYozuv);
    add(L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, 84, 622, 230,
        260, kIdModel);
    add(L"BUTTON", L"Yangilash", BS_PUSHBUTTON | WS_TABSTOP, 322, 622, 104, 26, kIdModelYangila);
    add(L"STATIC", L"", 0, 24, 654, 402, 18, kIdModelHolati);

    add(L"BUTTON", L"Ulanishni tekshirish", BS_PUSHBUTTON | WS_TABSTOP, 24, 676, 160, 26,
        kIdTekshir);
    add(L"STATIC", L"", 0, 192, 680, 234, 18, kIdTekshirNatija);

    // Diagnostika rejimi (G1): logda matn faqat shu yoqiq boʻlsa, 24 soatgacha.
    add(L"BUTTON", L"Diagnostika rejimi (24 soat): diktovka matni ham logga yoziladi",
        BS_AUTOCHECKBOX | WS_TABSTOP, 24, 710, 402, 22, kIdDiagnostika);
    add(L"BUTTON", L"Log faylini ochish…", BS_PUSHBUTTON | WS_TABSTOP, 24, 738, 150, 30, kIdLog);

    // --- Donat tugmasi
    //
    // Ataylab chapda, Saqlash/Bekor qilishdan uzoqda — tasodifan bosilmasin.
    // Rangi bilan ajralib turadi (BS_OWNERDRAW orqali oʻzimiz chizamiz),
    // lekin oʻlchami kichik va hech narsani toʻsmaydi.
    // Donat va pastki tugmalar — ular oʻrni qoʻshimcha boʻlim ochilganda
    // pastga suriladi (`joylashuvniYangila`).
    add(L"BUTTON", L"\x2665  Donat qilish", BS_OWNERDRAW | WS_TABSTOP, 24, 0, 132, 30, kIdDonate);
    add(L"BUTTON", L"Saqlash", BS_DEFPUSHBUTTON | WS_TABSTOP, 236, 0, 92, 30, kIdSave);
    add(L"BUTTON", L"Bekor qilish", BS_PUSHBUTTON | WS_TABSTOP, 334, 0, 92, 30, kIdCancel);

    // Ishora matnlari (macOS'dagi `placeholderString`).
    SendMessageW(ctrl(kIdKalit), EM_SETCUEBANNER, TRUE, (LPARAM)L"API kalit");
    SendMessageW(ctrl(kIdBaseURL), EM_SETCUEBANNER, TRUE, (LPARAM)L"Base URL");

    joylashuvniYangila();
}

// Qoʻshimcha boʻlimni koʻrsatadi/yashiradi, pastki tugmalarni joyiga qoʻyadi
// va oyna balandligini moslaydi.
int SettingsWindow::Impl::matnBalandligi(HWND c, int kenglikDip) const {
    if (!c) return 0;
    wchar_t bufer[1024] = {};
    GetWindowTextW(c, bufer, static_cast<int>(std::size(bufer)));
    if (!bufer[0]) return 0;

    HDC dc = GetDC(c);
    if (!dc) return 0;
    HFONT eski = static_cast<HFONT>(SelectObject(dc, font));
    RECT r{0, 0, scale(kenglikDip), 0};
    DrawTextW(dc, bufer, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, eski);
    ReleaseDC(c, dc);

    const float k = static_cast<float>(dpi) / 96.0f;
    return static_cast<int>((r.bottom - r.top) / k) + 2;  // pastdan biroz joy
}

void SettingsWindow::Impl::joylashuvniYangila() {
    for (int id : kQoshimchaIdlar) {
        if (HWND c = ctrl(id)) ShowWindow(c, qoshimchaOchiq ? SW_SHOW : SW_HIDE);
    }
    // «Boshqa (custom)» boʻlmasa Base URL baribir yashirin qoladi.
    xosQatorniYangila();

    SetWindowTextW(ctrl(kIdQoshimcha),
                   qoshimchaOchiq ? L"Qoʻshimcha sozlamalar  ▴" : L"Qoʻshimcha sozlamalar  ▾");

    // Boʻlim ochilib-yopilganda kontent boshiga qaytamiz: quyidagi
    // joylashuv surilmagan koordinatalarda hisoblanadi.
    sur(0);

    // Mikrofon ogohlantirishi boʻsh boʻlsa — pastdagi hamma narsa
    // koʻtariladi.
    const int siljish = ogohBalandligi - kMicOgohH;

    // Statistika izohi haqiqatda necha qator boʻlishini oʻlchaymiz va
    // undan pastdagilarni shunga qarab suramiz.
    const int izohH = qoshimchaOchiq
                          ? std::max(kStatIzohH, matnBalandligi(ctrl(kIdStatistikaIzoh), 384))
                          : kStatIzohH;
    const int izohSiljishi = izohH - kStatIzohH;

    const int toliqBalandlik = (qoshimchaOchiq ? kWinHToliq : kWinH) + siljish + izohSiljishi;
    const int pastQator = toliqBalandlik - 42;

    for (const Joy& j : joylar) {
        if (!j.h) continue;
        int y = j.y;
        if (j.h == ctrl(kIdDonate) || j.h == ctrl(kIdSave) || j.h == ctrl(kIdCancel)) {
            y = pastQator;
        } else {
            if (j.y >= kBolimBoshi) y += siljish;
            if (j.y >= kAiBoshi) y += izohSiljishi;
        }
        SetWindowPos(j.h, nullptr, scale(j.x), scale(y), 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // Izohning oʻzini ham kerakli balandlikka kengaytiramiz.
    if (HWND izoh = ctrl(kIdStatistikaIzoh)) {
        SetWindowPos(izoh, nullptr, 0, 0, scale(384), scale(izohH),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    kontentBalandligi = scale(toliqBalandlik);

    RECT rc{0, 0, scale(kWinW), kontentBalandligi};
    AdjustWindowRectExForDpi(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0, dpi);
    const int ramkaBalandligi = (rc.bottom - rc.top) - kontentBalandligi;
    int kenglik = rc.right - rc.left;
    int mijoz = kontentBalandligi;

    // Ekranga sigʻmasa — oynani ish maydoni balandligiga qisqartiramiz va
    // kontentni suramiz. Ilgari oyna shunchaki tepaga surilardi va pastki
    // qismi (Saqlash/Bekor) koʻrinmay qolardi: 800×600 VM'da ham,
    // 1366×768 noutbukda ham.
    UINT bayroqlar = SWP_NOMOVE | SWP_NOZORDER;
    int x = 0, y = 0;
    RECT joriy{};
    GetWindowRect(hwnd, &joriy);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        const int ish = mi.rcWork.bottom - mi.rcWork.top;
        if (mijoz + ramkaBalandligi > ish) {
            mijoz = ish - ramkaBalandligi;
            // Surish paneli mijoz kengligini yeydi — oynani shuncha
            // kengaytiramiz, aks holda oʻng chekka kesiladi.
            kenglik += GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
        }
        const int balandlik = mijoz + ramkaBalandligi;
        if (joriy.top + balandlik > mi.rcWork.bottom) {
            x = joriy.left;
            y = (balandlik >= ish) ? mi.rcWork.top : mi.rcWork.bottom - balandlik;
            bayroqlar = SWP_NOZORDER;
        }
    }

    SetWindowPos(hwnd, nullptr, x, y, kenglik, mijoz + ramkaBalandligi, bayroqlar);
    surishniSozla(mijoz);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void SettingsWindow::Impl::surishniSozla(int mijozBalandligi) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = kontentBalandligi > 0 ? kontentBalandligi - 1 : 0;
    si.nPage = mijozBalandligi > 0 ? static_cast<UINT>(mijozBalandligi) : 1;
    si.nPos = surilish;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    // Diapazon qisqargan boʻlsa siljishni qaytadan qisamiz.
    sur(surilish);
}

void SettingsWindow::Impl::sur(int yangi) {
    RECT mijoz{};
    GetClientRect(hwnd, &mijoz);
    const int eng = kontentBalandligi - (mijoz.bottom - mijoz.top);
    if (yangi < 0) yangi = 0;
    if (yangi > eng) yangi = eng > 0 ? eng : 0;
    if (yangi == surilish) return;

    const int dy = surilish - yangi;
    surilish = yangi;
    ScrollWindowEx(hwnd, 0, dy, nullptr, nullptr, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    SetScrollPos(hwnd, SB_VERT, surilish, TRUE);
    UpdateWindow(hwnd);
}

void SettingsWindow::Impl::fillMics() {
    HWND combo = ctrl(kIdMic);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    mics = listMicrophones();

    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"(tizim standarti)");
    int select = 0;

    for (size_t i = 0; i < mics.size(); i++) {
        std::wstring label = mics[i].name;
        switch (mics[i].kind) {
            case MicKind::Loopback: label += L"   [mikrofon emas!]"; break;
            case MicKind::Bluetooth: label += L"   [sifat past]"; break;
            case MicKind::Virtual: label += L"   [virtual]"; break;
            default: break;
        }
        if (mics[i].isDefault) label += L"   (standart)";
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
        if (!settings.micDeviceId.empty() && mics[i].id == settings.micDeviceId) {
            select = (int)i + 1;
        }
    }

    SendMessageW(combo, CB_SETCURSEL, select, 0);
    updateMicWarning();
}

void SettingsWindow::Impl::updateMicWarning() {
    const int sel = (int)SendMessageW(ctrl(kIdMic), CB_GETCURSEL, 0, 0);
    std::wstring text;

    if (sel > 0 && (size_t)(sel - 1) < mics.size()) {
        text = mics[sel - 1].warning();
    } else if (sel == 0) {
        // Standart qurilma xavfli boʻlishi mumkin — tekshiramiz.
        for (const auto& m : mics) {
            if (m.isDefault && m.kind != MicKind::Normal) {
                text = L"Tizim standarti: " + m.name + L"\n" + m.warning();
                break;
            }
        }
    }
    SetWindowTextW(ctrl(kIdMicWarning), text.c_str());

    const int yangiBalandlik = text.empty() ? 0 : kMicOgohH;
    if (yangiBalandlik != ogohBalandligi) {
        ogohBalandligi = yangiBalandlik;
        joylashuvniYangila();
    }
}

void SettingsWindow::Impl::save() {
    // --- Hotkey
    const WORD hk = (WORD)SendMessageW(ctrl(kIdHotkey), HKM_GETHOTKEY, 0, 0);
    const BYTE vk = LOBYTE(hk);
    const BYTE flags = HIBYTE(hk);
    const UINT mods = hotkeyFlagsToMod(flags);

    if (vk != 0 && mods != 0) {
        settings.vkCode = vk;
        settings.modifiers = mods;
        settings.hotkeyLabel = keyLabel(vk);
    } else {
        MessageBoxW(hwnd,
                    L"Tugma saqlanmadi: kamida bitta modifikator\n"
                    L"(Ctrl, Alt yoki Shift) va bitta tugma kerak.\n\n"
                    L"Oldingi tugma oʻzgarishsiz qoldi.",
                    L"Sozlamalar", MB_OK | MB_ICONWARNING);
    }

    // --- Mikrofon
    const int sel = (int)SendMessageW(ctrl(kIdMic), CB_GETCURSEL, 0, 0);
    if (sel > 0 && (size_t)(sel - 1) < mics.size()) {
        settings.micDeviceId = mics[sel - 1].id;
        settings.micDeviceName = mics[sel - 1].name;
    } else {
        settings.micDeviceId.clear();
        settings.micDeviceName.clear();
    }

    // --- Qolgan sozlamalar
    settings.insertMode = SendMessageW(ctrl(kIdInsertMode), CB_GETCURSEL, 0, 0) == 1
                              ? InsertMode::Type
                              : InsertMode::Paste;

    const bool autoStart = SendMessageW(ctrl(kIdAutoStart), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.autoStart = autoStart;
    // Statistika doim yoqiq — oʻchirish tugmasi yoʻq (oshkoralik yozuvi bilan).
    settings.useGpu = SendMessageW(ctrl(kIdUseGpu), BM_GETCHECK, 0, 0) == BST_CHECKED;
    settings.oddiyApostrof = SendMessageW(ctrl(kIdApostrof), BM_GETCHECK, 0, 0) == BST_CHECKED;
    // Yoqilgan rejim qayta saqlansa muddat uzaymaydi — faqat yangi yoqilganda 24 soat.
    const long long hozir = static_cast<long long>(std::time(nullptr));
    const bool diagnostika = SendMessageW(ctrl(kIdDiagnostika), BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!diagnostika) {
        settings.diagnostikaTugash = 0;
    } else if (!diagnostikaFaolmi(settings.diagnostikaTugash, hozir)) {
        settings.diagnostikaTugash = hozir + kDiagnostikaMuddati;
    }

    llmniSaqla();

    setAutoStart(autoStart);

    // Fayl QAYTA oʻqiladi va faqat shu oyna tahrirlaydigan maydonlar ustiga
    // yoziladi (E1): oyna ochiq turganda statistika `ornatmaId` ni yasagan
    // yoki yangilanish tekshiruvi vaqtni yozgan boʻlishi mumkin.
    settings = sozlamaOynasidan(loadSettings(), settings);
    if (!saveSettings(settings)) {
        MessageBoxW(hwnd, L"Sozlamalar saqlanmadi. Diskda joy bormi?", L"Sozlamalar",
                    MB_OK | MB_ICONERROR);
        return;
    }
    logWrite(L"sozlamalar saqlandi: " + settings.hotkeyDisplay());
    if (onSaved) onSaved(settings);
}

// ------------------------------------------------------------------- API

SettingsWindow::SettingsWindow() : d(new Impl) {}

SettingsWindow::~SettingsWindow() {
    if (d->hwnd) DestroyWindow(d->hwnd);
    if (d->font) DeleteObject(d->font);
    delete d;
}

HWND SettingsWindow::oyna() const { return d ? d->hwnd : nullptr; }

void SettingsWindow::setOnSaved(std::function<void(const Settings&)> cb) {
    d->onSaved = std::move(cb);
}

void SettingsWindow::setYangilanish(std::function<Yangilovchi::Korinish()> holat,
                                    std::function<void()> bosildi) {
    d->yangilanishHolati = std::move(holat);
    d->yangilanishBosildi = std::move(bosildi);
}

void SettingsWindow::yangilanishniYangila() {
    if (d->hwnd && IsWindowVisible(d->hwnd)) d->yangilanishQatori();
}

void SettingsWindow::Impl::yangilanishQatori() {
    HWND matn = ctrl(kIdYangilanishHolati), tugma = ctrl(kIdYangilanishTugma);
    if (!matn || !tugma) return;
    if (!yangilanishHolati) {
        ShowWindow(matn, SW_HIDE);
        ShowWindow(tugma, SW_HIDE);
        return;
    }
    const Yangilovchi::Korinish k = yangilanishHolati();
    SetWindowTextW(matn, k.matn.c_str());
    SetWindowTextW(tugma, k.tugma.c_str());
    EnableWindow(tugma, k.tugmaFaol);
}

void SettingsWindow::show(HINSTANCE instance, const Settings& current, HWND ega) {
    d->settings = current;

    if (!d->hwnd) {
        d->instance = instance;

        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_HOTKEY_CLASS | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&icc);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = settingsProc;
        wc.hInstance = instance;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        RegisterClassExW(&wc);

        d->hwnd =
            CreateWindowExW(0, kClassName, L"Kotib — Sozlamalar",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL, CW_USEDEFAULT,
                            CW_USEDEFAULT, 100, 100, ega, nullptr, instance, d);
        if (!d->hwnd) {
            logWrite(L"XATO: sozlamalar oynasi yaratilmadi");
            return;
        }

        d->dpi = GetDpiForWindow(d->hwnd);
        d->font = CreateFontW(-d->scale(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        // Mijoz maydoni aynan kerakli oʻlchamda boʻlishi uchun ramka
        // qalinligini hisobga olamiz.
        RECT rc{0, 0, d->scale(kWinW), d->scale(kWinH)};
        AdjustWindowRectExForDpi(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0, d->dpi);
        SetWindowPos(d->hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOMOVE | SWP_NOZORDER);

        d->build();
    }

    // Joriy qiymatlarni qoʻyamiz
    SendMessageW(d->ctrl(kIdHotkey), HKM_SETHOTKEY,
                 MAKEWORD(current.vkCode, modToHotkeyFlags(current.modifiers)), 0);
    SendMessageW(d->ctrl(kIdInsertMode), CB_SETCURSEL,
                 current.insertMode == InsertMode::Type ? 1 : 0, 0);
    SendMessageW(d->ctrl(kIdAutoStart), BM_SETCHECK,
                 autoStartEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    // kIdStatistika endi statik yozuv — belgilash yoʻq.
    SendMessageW(d->ctrl(kIdUseGpu), BM_SETCHECK, current.useGpu ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(d->ctrl(kIdApostrof), BM_SETCHECK,
                 current.oddiyApostrof ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(
        d->ctrl(kIdDiagnostika), BM_SETCHECK,
        diagnostikaFaolmi(current.diagnostikaTugash, static_cast<long long>(std::time(nullptr)))
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);
    d->llmniYukla();
    d->yangilanishQatori();

    // Qurilmalar roʻyxatini har safar yangilaymiz — foydalanuvchi oyna
    // ochiq boʻlmagan paytda mikrofon ulagan boʻlishi mumkin.
    d->fillMics();

    // Oynani kursor turgan monitorda markazlashtiramiz
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    RECT wr;
    GetWindowRect(d->hwnd, &wr);
    const int w = wr.right - wr.left, h = wr.bottom - wr.top;
    SetWindowPos(d->hwnd, HWND_TOP, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2,
                 mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2, 0, 0, SWP_NOSIZE);

    forceForeground(d->hwnd);
}

}  // namespace rubai
