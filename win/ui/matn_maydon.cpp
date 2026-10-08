// Tahrirlanadigan matn maydoni — RichEdit ustida.
// Interfeys va izohlar — `matn_maydon.h`.

#include "matn_maydon.h"

#include <commctrl.h>
#include <windowsx.h>
#include <richedit.h>

#include <map>

namespace rubai {

namespace {

// RichEdit 4.1+ (`RICHEDIT50W`) shu kutubxonada. `LoadLibrary` bir marta —
// yuklanmasa oyna sinfi roʻyxatdan oʻtmaydi va CreateWindow nullptr qaytaradi.
bool richeditYuklandi() {
    static const bool bor = (LoadLibraryW(L"Msftedit.dll") != nullptr);
    return bor;
}

COLORREF d2dRang(uint32_t rgb) { return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }

// HWND → MatnMaydon. EN_CHANGE ota oynaga WM_COMMAND boʻlib keladi va u
// yerdan qaysi maydon oʻzgarganini faqat deskriptor boʻyicha topish mumkin.
std::map<HWND, MatnMaydon*>& royxat() {
    static std::map<HWND, MatnMaydon*> m;
    return m;
}

constexpr UINT_PTR kSubclassId = 1;

}  // namespace

// Subclass protsedurasi va MatnMaydon'ning yopiq maydonlari orasidagi koʻprik.
struct MatnMaydon::Ichki {
    static LRESULT CALLBACK proc(HWND h, UINT xabar, WPARAM wp, LPARAM lp, UINT_PTR,
                                 DWORD_PTR ref) {
        auto* m = reinterpret_cast<MatnMaydon*>(ref);

        switch (xabar) {
            // RichEdit'ning oʻz menyusi ingliz tilida va bizga kerakmagan
            // bandlar bilan keladi ("Insert Object…"). Oʻzimiznikini chizamiz.
            case WM_CONTEXTMENU: {
                POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                if (p.x == -1 && p.y == -1) {  // klaviaturadagi menyu tugmasi
                    RECT r{};
                    GetWindowRect(h, &r);
                    p = POINT{r.left + 20, r.top + 20};
                }
                if (m && m->onOngTugma) {
                    m->onOngTugma(p);
                    return 0;
                }
                break;
            }

            // Ctrl+A — RichEdit'ning eski versiyalarida yoʻq.
            case WM_KEYDOWN:
                if (wp == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    CHARRANGE hammasi{0, -1};
                    SendMessageW(h, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&hammasi));
                    return 0;
                }
                break;

            case WM_NCDESTROY:
                royxat().erase(h);
                RemoveWindowSubclass(h, proc, kSubclassId);
                break;

            default: break;
        }
        return DefSubclassProc(h, xabar, wp, lp);
    }
};

// ---- Qurish ----------------------------------------------------------------

MatnMaydon::~MatnMaydon() {
    if (oyna_) {
        royxat().erase(oyna_);
        DestroyWindow(oyna_);
        oyna_ = nullptr;
    }
    if (shrift_) {
        DeleteObject(shrift_);
        shrift_ = nullptr;
    }
}

bool MatnMaydon::qur(HWND ota, float shriftOlchami) {
    if (!richeditYuklandi()) return false;
    shriftOlchami_ = shriftOlchami;

    oyna_ = CreateWindowExW(
        0, MSFTEDIT_CLASS, L"",
        // ES_AUTOHSCROLL YOʻQ — busiz matn oyna kengligiga oʻraladi.
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL, 0, 0, 10, 10, ota,
        nullptr, reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(ota, GWLP_HINSTANCE)), nullptr);
    if (!oyna_) return false;

    royxat()[oyna_] = this;
    SetWindowSubclass(oyna_, &Ichki::proc, kSubclassId, reinterpret_cast<DWORD_PTR>(this));

    // 64 KB standart chegara — bir soatlik transkript undan katta boʻladi.
    SendMessageW(oyna_, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    SendMessageW(oyna_, EM_SETEVENTMASK, 0, ENM_CHANGE);
    SendMessageW(oyna_, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(d2dRang(U::oq)));

    // Qator oraligʻi 1.35 — dizayndagi qiymat (macOS'da `lineHeightMultiple`).
    // 5-qoida: dyLineSpacing/20 — qatorlarda. 27/20 = 1.35.
    PARAFORMAT2 pf{};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_LINESPACING;
    pf.bLineSpacingRule = 5;
    pf.dyLineSpacing = 27;
    SendMessageW(oyna_, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&pf));

    return true;
}

void MatnMaydon::shriftniQayta(float dpiNisbat) {
    if (oxirgiDpi_ == dpiNisbat) return;
    oxirgiDpi_ = dpiNisbat;

    HFONT eski = shrift_;
    // Manfiy balandlik — belgi balandligi (piksel), ichki oraliqsiz.
    shrift_ =
        CreateFontW(-static_cast<int>(shriftOlchami_ * dpiNisbat + 0.5f), 0, 0, 0, FW_NORMAL, FALSE,
                    FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(oyna_, WM_SETFONT, reinterpret_cast<WPARAM>(shrift_), TRUE);
    if (eski) DeleteObject(eski);

    // WM_SETFONT rangni tiklamaydi — uni alohida qoʻyamiz.
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = d2dRang(U::matn);
    SendMessageW(oyna_, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));
}

void MatnMaydon::joylashtir(const D2D1_RECT_F& ramka, float dpiNisbat) {
    if (!oyna_) return;
    shriftniQayta(dpiNisbat);

    const int x = static_cast<int>(ramka.left * dpiNisbat);
    const int y = static_cast<int>(ramka.top * dpiNisbat);
    const int w = static_cast<int>((ramka.right - ramka.left) * dpiNisbat);
    const int h = static_cast<int>((ramka.bottom - ramka.top) * dpiNisbat);
    SetWindowPos(oyna_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

    // Ichki boʻshliq — macOS'dagi `textContainerInset` (28×24).
    RECT ich{static_cast<LONG>(28 * dpiNisbat), static_cast<LONG>(24 * dpiNisbat),
             w - static_cast<LONG>(28 * dpiNisbat), h - static_cast<LONG>(24 * dpiNisbat)};
    SendMessageW(oyna_, EM_SETRECT, 0, reinterpret_cast<LPARAM>(&ich));
}

void MatnMaydon::korsat(bool v) {
    if (oyna_) ShowWindow(oyna_, v ? SW_SHOW : SW_HIDE);
}

bool MatnMaydon::korinadimi() const { return oyna_ && IsWindowVisible(oyna_); }

void MatnMaydon::faqatOqish(bool v) {
    if (oyna_) SendMessageW(oyna_, EM_SETREADONLY, v ? TRUE : FALSE, 0);
}

void MatnMaydon::fokusOl() {
    if (oyna_) SetFocus(oyna_);
}

// ---- Matn ------------------------------------------------------------------

void MatnMaydon::matnQoy(const std::wstring& s) {
    if (!oyna_) return;
    ozimizYozyapmiz_ = true;
    SetWindowTextW(oyna_, s.c_str());
    // Bekor qilish tarixini tozalaymiz: yangi hujjat ochilganda Ctrl+Z
    // eskisining matnini qaytarib qoʻymasligi kerak.
    SendMessageW(oyna_, EM_EMPTYUNDOBUFFER, 0, 0);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = d2dRang(U::matn);
    SendMessageW(oyna_, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));
    ozimizYozyapmiz_ = false;
}

void MatnMaydon::qoshib(const std::wstring& s) {
    if (!oyna_ || s.empty()) return;
    ozimizYozyapmiz_ = true;
    // Kursorni oxiriga qoʻyib qoʻshamiz — butun matnni qayta yozmaymiz.
    const CHARRANGE oxir{-1, -1};
    SendMessageW(oyna_, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&oxir));
    SendMessageW(oyna_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(s.c_str()));
    SendMessageW(oyna_, EM_SCROLLCARET, 0, 0);
    ozimizYozyapmiz_ = false;
}

size_t MatnMaydon::uzunlik() const {
    if (!oyna_) return 0;
    GETTEXTLENGTHEX gl{};
    gl.flags = GTL_NUMCHARS;
    gl.codepage = 1200;
    const LRESULT n = SendMessageW(oyna_, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&gl), 0);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

std::wstring MatnMaydon::matn() const {
    if (!oyna_) return {};

    GETTEXTLENGTHEX gl{};
    gl.flags = GTL_NUMCHARS;
    gl.codepage = 1200;  // UTF-16
    const LRESULT uzunlik =
        SendMessageW(oyna_, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&gl), 0);
    if (uzunlik <= 0) return {};

    std::wstring bufer(static_cast<size_t>(uzunlik) + 1, L'\0');
    GETTEXTEX gt{};
    gt.cb = static_cast<DWORD>((uzunlik + 1) * sizeof(wchar_t));
    gt.flags = GT_DEFAULT;
    gt.codepage = 1200;
    SendMessageW(oyna_, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&gt),
                 reinterpret_cast<LPARAM>(bufer.data()));
    bufer.resize(static_cast<size_t>(uzunlik));

    // RichEdit qator chegarasini `\r` bilan beradi. Diskka va macOS bilan
    // umumiy mantiqqa doim `\n` ketadi — aks holda saqlangan matnda
    // koʻrinmas belgilar toʻplanib qolardi.
    std::wstring toza;
    toza.reserve(bufer.size());
    for (size_t i = 0; i < bufer.size(); ++i) {
        if (bufer[i] == L'\r') {
            if (i + 1 < bufer.size() && bufer[i + 1] == L'\n') ++i;
            toza += L'\n';
        } else {
            toza += bufer[i];
        }
    }
    return toza;
}

// ---- Ota oynadan keladigan xabar -------------------------------------------

void matnMaydonXabari(WPARAM wp, LPARAM lp) {
    if (HIWORD(wp) != EN_CHANGE) return;
    auto it = royxat().find(reinterpret_cast<HWND>(lp));
    if (it == royxat().end()) return;
    MatnMaydon* m = it->second;
    if (m->ozimizYozyapmiz_) return;
    if (m->onOzgardi) m->onOzgardi();
}

}  // namespace rubai
