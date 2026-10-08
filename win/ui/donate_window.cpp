// «Donat qilish» oynasi — karta raqamlari va nusxalash tugmalari.
// Interfeys va izohlar — `donate_window.h`.

#include <windows.h>

#include "donate_window.h"

#include <string>

#include "../core/util.h"

namespace rubai {

namespace {

constexpr wchar_t kClassName[] = L"KotibDonate";

// Sayt bilan bir xil iliq palitra.
const COLORREF kBg = RGB(250, 249, 245);
const COLORREF kInk = RGB(25, 25, 23);
const COLORREF kInkSoft = RGB(74, 72, 66);
const COLORREF kInkMute = RGB(99, 96, 90);
const COLORREF kLine = RGB(214, 209, 194);

struct Card {
    const wchar_t* brand;
    const wchar_t* pretty;  // ekranda koʻrinadigan, boʻshliqlar bilan
    const wchar_t* digits;  // clipboard'ga ketadigan, boʻshliqsiz
    COLORREF color;
};

// Karta raqamlari shu yerda, bitta joyda.
const Card kCards[] = {
    {L"HUMO", L"9860 1606 0855 5431", L"9860160608555431", RGB(20, 150, 200)},
    {L"VISA", L"4231 2000 9261 0830", L"4231200092610830", RGB(35, 45, 125)},
    {L"UZCARD", L"5614 6818 5541 0035", L"5614681855410035", RGB(30, 150, 100)},
};
constexpr int kCardCount = (int)(sizeof(kCards) / sizeof(kCards[0]));

constexpr int kIdCopyBase = 400;
constexpr int kIdClose = 410;
constexpr UINT kTimerReset = 1;

constexpr int kWinW = 430;
constexpr int kCardH = 84;

bool copyToClipboard(HWND owner, const wchar_t* text) {
    const size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!h) return false;
    void* p = GlobalLock(h);
    if (!p) {
        GlobalFree(h);
        return false;
    }
    memcpy(p, text, bytes);
    GlobalUnlock(h);

    if (!OpenClipboard(owner)) {
        GlobalFree(h);
        return false;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, h)) {
        CloseClipboard();
        GlobalFree(h);
        return false;
    }
    CloseClipboard();  // muvaffaqiyatdan keyin xotira tizimniki
    return true;
}

}  // namespace

struct DonateWindow::Impl {
    HWND hwnd = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontText = nullptr;
    HFONT fontCard = nullptr;
    HFONT fontLabel = nullptr;
    UINT dpi = 96;
    int copiedIndex = -1;  // qaysi tugmada "Nusxalandi" yozuvi turibdi

    int scale(int v) const { return MulDiv(v, (int)dpi, 96); }
    void buildFonts();
    void paint(HDC hdc, const RECT& rc);
};

namespace {

DonateWindow::Impl* implOf(HWND h) {
    return reinterpret_cast<DonateWindow::Impl*>(GetWindowLongPtrW(h, GWLP_USERDATA));
}

LRESULT CALLBACK donateProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* d = implOf(hwnd);

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (d) d->paint(hdc, rc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND: return 1;  // WM_PAINT oʻzi toʻldiradi, miltillash boʻlmasin

        case WM_COMMAND: {
            if (!d) break;
            const int id = LOWORD(wp);
            if (id == kIdClose) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            if (id >= kIdCopyBase && id < kIdCopyBase + kCardCount) {
                const int i = id - kIdCopyBase;
                if (copyToClipboard(hwnd, kCards[i].digits)) {
                    d->copiedIndex = i;
                    SetWindowTextW(GetDlgItem(hwnd, id), L"Nusxalandi \x2713");
                    SetTimer(hwnd, kTimerReset, 2000, nullptr);
                }
                return 0;
            }
            break;
        }

        case WM_TIMER:
            if (wp == kTimerReset && d) {
                KillTimer(hwnd, kTimerReset);
                if (d->copiedIndex >= 0) {
                    SetWindowTextW(GetDlgItem(hwnd, kIdCopyBase + d->copiedIndex), L"Nusxalash");
                    d->copiedIndex = -1;
                }
            }
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            auto hdc = (HDC)wp;
            SetBkMode(hdc, TRANSPARENT);
            static HBRUSH bg = CreateSolidBrush(kBg);
            return (LRESULT)bg;
        }

        case WM_CLOSE: ShowWindow(hwnd, SW_HIDE); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

// ------------------------------------------------------------------ chizish

void DonateWindow::Impl::buildFonts() {
    auto make = [&](int px, int weight) {
        return CreateFontW(-scale(px), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH,
                           L"Segoe UI");
    };
    fontTitle = make(19, FW_SEMIBOLD);
    fontText = make(13, FW_NORMAL);
    fontLabel = make(11, FW_BOLD);
    fontCard = CreateFontW(-scale(16), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH,
                           L"Consolas");
}

void DonateWindow::Impl::paint(HDC hdc, const RECT& rc) {
    HBRUSH bg = CreateSolidBrush(kBg);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    const int pad = scale(24);

    // Sarlavha
    SelectObject(hdc, fontTitle);
    SetTextColor(hdc, kInk);
    RECT t{pad, scale(20), rc.right - pad, scale(52)};
    DrawTextW(hdc, L"Ilova bepul", -1, &t, DT_LEFT | DT_TOP | DT_SINGLELINE);

    // Tushuntirish
    SelectObject(hdc, fontText);
    SetTextColor(hdc, kInkSoft);
    RECT s{pad, scale(52), rc.right - pad, scale(112)};
    DrawTextW(hdc,
              L"Reklama yoʻq, obuna yoʻq. Agar foydali boʻlsa va imkoningiz "
              L"boʻlsa, qoʻllab-quvvatlashingiz mumkin \x2014 majburiy emas.",
              -1, &s, DT_LEFT | DT_TOP | DT_WORDBREAK);

    // Kartalar
    int y = scale(120);
    for (int i = 0; i < kCardCount; i++) {
        const RECT box{pad, y, rc.right - pad, y + scale(kCardH) - scale(10)};

        HBRUSH cardBg = CreateSolidBrush(RGB(240, 238, 230));
        FillRect(hdc, &box, cardBg);
        DeleteObject(cardBg);

        HPEN pen = CreatePen(PS_SOLID, 1, kLine);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, box.left, box.top, box.right, box.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);

        // Rangli belgi
        HBRUSH chip = CreateSolidBrush(kCards[i].color);
        RECT c{box.left + scale(14), box.top + scale(14), box.left + scale(40),
               box.top + scale(31)};
        FillRect(hdc, &c, chip);
        DeleteObject(chip);

        // Brend nomi
        SelectObject(hdc, fontLabel);
        SetTextColor(hdc, kInkMute);
        RECT b{box.left + scale(48), box.top + scale(14), box.left + scale(180),
               box.top + scale(32)};
        DrawTextW(hdc, kCards[i].brand, -1, &b, DT_LEFT | DT_TOP | DT_SINGLELINE);

        // Raqam
        SelectObject(hdc, fontCard);
        SetTextColor(hdc, kInk);
        RECT n{box.left + scale(14), box.top + scale(38), box.right - scale(120),
               box.bottom - scale(6)};
        DrawTextW(hdc, kCards[i].pretty, -1, &n, DT_LEFT | DT_TOP | DT_SINGLELINE);

        y += scale(kCardH);
    }

    // Karta egasi
    SelectObject(hdc, fontText);
    SetTextColor(hdc, kInkMute);
    RECT h{pad, y + scale(6), rc.right - pad, y + scale(28)};
    DrawTextW(hdc, L"Karta egasi", -1, &h, DT_LEFT | DT_TOP | DT_SINGLELINE);

    SelectObject(hdc, fontTitle);
    SetTextColor(hdc, kInk);
    RECT hv{pad, y + scale(24), rc.right - pad, y + scale(50)};
    DrawTextW(hdc, L"Muhammad Mirkabilov", -1, &hv, DT_LEFT | DT_TOP | DT_SINGLELINE);
}

// ---------------------------------------------------------------------- API

DonateWindow::DonateWindow() : d(new Impl) {}

DonateWindow::~DonateWindow() {
    if (d->hwnd) DestroyWindow(d->hwnd);
    if (d->fontTitle) DeleteObject(d->fontTitle);
    if (d->fontText) DeleteObject(d->fontText);
    if (d->fontCard) DeleteObject(d->fontCard);
    if (d->fontLabel) DeleteObject(d->fontLabel);
    delete d;
}

void DonateWindow::show(HINSTANCE instance, HWND owner) {
    if (!d->hwnd) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = donateProc;
        wc.hInstance = instance;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
        RegisterClassExW(&wc);

        d->hwnd =
            CreateWindowExW(0, kClassName, L"Donat qilish", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                            CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, owner, nullptr, instance, d);
        if (!d->hwnd) {
            logWrite(L"XATO: donat oynasi yaratilmadi");
            return;
        }

        d->dpi = GetDpiForWindow(d->hwnd);
        d->buildFonts();

        const int height = 120 + kCardCount * kCardH + 96;
        RECT rc{0, 0, d->scale(kWinW), d->scale(height)};
        AdjustWindowRectExForDpi(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0, d->dpi);
        SetWindowPos(d->hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOMOVE | SWP_NOZORDER);

        // Nusxalash tugmalari — har karta oʻngida
        for (int i = 0; i < kCardCount; i++) {
            HWND b = CreateWindowExW(0, L"BUTTON", L"Nusxalash", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     d->scale(kWinW - 24 - 104), d->scale(120 + i * kCardH + 36),
                                     d->scale(104), d->scale(28), d->hwnd,
                                     (HMENU)(INT_PTR)(kIdCopyBase + i), instance, nullptr);
            SendMessageW(b, WM_SETFONT, (WPARAM)d->fontText, TRUE);
        }

        HWND close = CreateWindowExW(
            0, L"BUTTON", L"Yopish", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            d->scale(kWinW - 24 - 96), d->scale(height - 46), d->scale(96), d->scale(30), d->hwnd,
            (HMENU)(INT_PTR)kIdClose, instance, nullptr);
        SendMessageW(close, WM_SETFONT, (WPARAM)d->fontText, TRUE);
    }

    // Ota oyna markazida
    RECT wr, or_;
    GetWindowRect(d->hwnd, &wr);
    const int w = wr.right - wr.left, h = wr.bottom - wr.top;
    if (owner && GetWindowRect(owner, &or_)) {
        SetWindowPos(d->hwnd, HWND_TOP, or_.left + ((or_.right - or_.left) - w) / 2,
                     or_.top + ((or_.bottom - or_.top) - h) / 2, 0, 0, SWP_NOSIZE);
    }

    ShowWindow(d->hwnd, SW_SHOW);
    SetForegroundWindow(d->hwnd);
}

}  // namespace rubai
