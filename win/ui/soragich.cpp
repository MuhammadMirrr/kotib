// Kichik modal yordamchilar: matn soʻrash, fayl tanlash, bufer.
// Interfeys va izohlar — `soragich.h`.

#include "soragich.h"

#include "uslub.h"

#include <commdlg.h>

#include <iterator>

namespace rubai {

namespace {

constexpr wchar_t kSinf[] = L"KotibSoragich";

struct Holat {
    HWND tahrir = nullptr;
    std::wstring natija;
    bool tugadi = false;
    bool bekor = true;
};

LRESULT CALLBACK proc(HWND h, UINT xabar, WPARAM wp, LPARAM lp) {
    auto* s = reinterpret_cast<Holat*>(GetWindowLongPtrW(h, GWLP_USERDATA));

    switch (xabar) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) {
                if (s) {
                    s->bekor = (LOWORD(wp) == IDCANCEL);
                    if (!s->bekor) {
                        const int n = GetWindowTextLengthW(s->tahrir);
                        std::wstring b(static_cast<size_t>(n) + 1, L'\0');
                        GetWindowTextW(s->tahrir, b.data(), n + 1);
                        b.resize(static_cast<size_t>(n));
                        s->natija = b;
                    }
                    s->tugadi = true;
                }
                return 0;
            }
            break;

        case WM_CLOSE:
            if (s) {
                s->bekor = true;
                s->tugadi = true;
            }
            return 0;

        default: break;
    }
    return DefWindowProcW(h, xabar, wp, lp);
}

// Tizim shrifti — standart boshqaruv elementlari uchun. Busiz Windows
// 1990-yillardagi «System» shriftini beradi.
HFONT tizimShrifti(UINT dpi) {
    NONCLIENTMETRICSW m{};
    m.cbSize = sizeof(m);
    if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(m), &m, 0, dpi)) {
        return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    return CreateFontIndirectW(&m.lfMessageFont);
}

}  // namespace

std::wstring matnSora(HWND ota, const std::wstring& sarlavha, const std::wstring& izoh,
                      const std::wstring& tavsiya) {
    static bool royxatdan = false;
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(ota, GWLP_HINSTANCE));
    if (!royxatdan) {
        WNDCLASSEXW sinf{};
        sinf.cbSize = sizeof(sinf);
        sinf.lpfnWndProc = proc;
        sinf.hInstance = instance;
        sinf.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        sinf.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        sinf.lpszClassName = kSinf;
        RegisterClassExW(&sinf);
        royxatdan = true;
    }

    const UINT dpi = GetDpiForWindow(ota);
    const float k = static_cast<float>(dpi) / 96.0f;
    auto px = [k](int dip) { return static_cast<int>(dip * k); };

    Holat holat;

    RECT otaR{};
    GetWindowRect(ota, &otaR);
    const int w = px(420), hgt = px(170);
    const int x = otaR.left + ((otaR.right - otaR.left) - w) / 2;
    const int y = otaR.top + ((otaR.bottom - otaR.top) - hgt) / 3;

    HWND oyna = CreateWindowExW(WS_EX_DLGMODALFRAME, kSinf, sarlavha.c_str(),
                                WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, w, hgt, ota, nullptr,
                                instance, &holat);
    if (!oyna) return {};

    HFONT shrift = tizimShrifti(dpi);

    RECT ich{};
    GetClientRect(oyna, &ich);
    const int kenglik = ich.right - ich.left;

    HWND yozuv = CreateWindowExW(0, L"STATIC", izoh.c_str(), WS_CHILD | WS_VISIBLE, px(16), px(14),
                                 kenglik - px(32), px(36), oyna, nullptr, instance, nullptr);
    holat.tahrir =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", tavsiya.c_str(),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, px(16), px(54),
                        kenglik - px(32), px(26), oyna, nullptr, instance, nullptr);
    HWND ok = CreateWindowExW(0, L"BUTTON", L"Bajarish",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                              kenglik - px(200), px(94), px(90), px(28), oyna,
                              reinterpret_cast<HMENU>(IDOK), instance, nullptr);
    HWND bekor = CreateWindowExW(0, L"BUTTON", L"Bekor", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 kenglik - px(104), px(94), px(90), px(28), oyna,
                                 reinterpret_cast<HMENU>(IDCANCEL), instance, nullptr);

    for (HWND c : {yozuv, holat.tahrir, ok, bekor}) {
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(shrift), TRUE);
    }

    EnableWindow(ota, FALSE);
    ShowWindow(oyna, SW_SHOW);
    SetFocus(holat.tahrir);

    // Modal sikl. `IsDialogMessage` Tab va Enter'ni oʻzi boshqaradi —
    // busiz oynada klaviatura umuman ishlamasdi.
    MSG msg;
    while (!holat.tugadi) {
        const BOOL bor = GetMessageW(&msg, nullptr, 0, 0);
        if (bor <= 0) {
            // Ilova yopilmoqda. `WM_QUIT` ni asosiy siklga qaytaramiz —
            // aks holda u shu yerda yutilib, ilova yopilmay qolardi.
            PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (IsDialogMessageW(oyna, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(ota, TRUE);
    DestroyWindow(oyna);
    SetActiveWindow(ota);
    DeleteObject(shrift);

    return holat.bekor ? std::wstring() : holat.natija;
}

// ---- Fayl dialoglari -------------------------------------------------------

std::wstring faylSora(HWND ota) {
    wchar_t bufer[MAX_PATH * 4] = {0};

    // Faqat Media Foundation ochadigan kengaytmalar. `mkv`, `webm`, `ogg`,
    // `opus` va `amr` ATAYLAB yoʻq — `media_decode.cpp` ularni ocholmaydi va
    // roʻyxatda turishi foydalanuvchini tanlab, keyin xato koʻrishga
    // majburlardi. «Barcha fayllar» filtri baribir qoldi.
    static const wchar_t kFiltr[] = L"Ovoz va video\0*.mp3;*.wav;*.m4a;*.aac;*.flac;*.wma;"
                                    L"*.mp4;*.mov;*.avi;*.m4v;*.wmv;*.3gp\0"
                                    L"Barcha fayllar\0*.*\0\0";

    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = ota;
    o.lpstrFilter = kFiltr;
    o.lpstrFile = bufer;
    o.nMaxFile = static_cast<DWORD>(std::size(bufer));
    o.lpstrTitle = L"Ovozli yoki videofayl tanlang";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;

    return GetOpenFileNameW(&o) ? std::wstring(bufer) : std::wstring();
}

std::wstring saqlashSora(HWND ota, const std::wstring& tavsiyaNom) {
    wchar_t bufer[MAX_PATH * 4] = {0};
    wcsncpy_s(bufer, tavsiyaNom.c_str(), _TRUNCATE);

    static const wchar_t kFiltr[] = L"Matn fayli\0*.txt\0Barcha fayllar\0*.*\0\0";

    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = ota;
    o.lpstrFilter = kFiltr;
    o.lpstrFile = bufer;
    o.nMaxFile = static_cast<DWORD>(std::size(bufer));
    o.lpstrTitle = L"Matnni saqlash";
    o.lpstrDefExt = L"txt";
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;

    return GetSaveFileNameW(&o) ? std::wstring(bufer) : std::wstring();
}

// ---- Bufer va xabar --------------------------------------------------------

bool buferGaQoy(HWND ota, const std::wstring& matn) {
    if (!OpenClipboard(ota)) return false;
    EmptyClipboard();

    const size_t bayt = (matn.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bayt);
    if (!h) {
        CloseClipboard();
        return false;
    }

    void* p = GlobalLock(h);
    if (!p) {
        GlobalFree(h);
        CloseClipboard();
        return false;
    }
    memcpy(p, matn.c_str(), bayt);
    GlobalUnlock(h);

    const bool ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
    // Muvaffaqiyatda xotira egaligi buferga oʻtadi — uni biz boʻshatmaymiz.
    if (!ok) GlobalFree(h);
    CloseClipboard();
    return ok;
}

void ogohlantir(HWND ota, const std::wstring& sarlavha, const std::wstring& matn) {
    MessageBoxW(ota, matn.c_str(), sarlavha.c_str(), MB_OK | MB_ICONINFORMATION);
}

}  // namespace rubai
