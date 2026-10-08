// Asosiy oyna — tablar, toolbar va bannerlar.
// Interfeys va izohlar — `asosiy_oyna.h`.

#include "asosiy_oyna.h"

#include "tab.h"
#include "fayl_tab.h"
#include "tarjima_tab.h"
#include "matn_maydon.h"
#include "yozish_tab.h"
#include "uslub.h"
#include "vidjet.h"
#include "../core/config.h"
#include "../core/tarix.h"
#include "../core/util.h"
#include "soragich.h"

#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <iterator>

namespace rubai {

namespace {

constexpr wchar_t kSinfNomi[] = L"KotibAsosiyOyna";
constexpr wchar_t kSarlavha[] = L"Kotib";

// Dizayndagi oʻlchamlar (DIP).
constexpr float kToolbarBalandligi = 52.0f;
constexpr float kYangiBannerBalandligi = 34.0f;
constexpr int kBoshlangichKenglik = 720;
constexpr int kBoshlangichBalandlik = 560;
constexpr int kEngKichikKenglik = 560;
constexpr int kEngKichikBalandlik = 420;

}  // namespace

// ---- Sozlamalar tugmasi ----------------------------------------------------
//
// ⚙ belgisi ATAYLAB ishlatilmaydi: DirectWrite uni «Segoe UI Emoji» dan olib
// rangli chizadi va toolbar'da koʻk-sariq emoji paydo boʻladi. Geometriya
// esa hech qanday shriftga bogʻliq emas va rangni biz beramiz — macOS'dagi
// `gearshape` ham `U.matn` rangida turadi.
class SozlamaTugmasi : public Vidjet {
public:
    std::function<void()> bosilganda;

    void chiz(Chizgich& c) override {
        if (!korinadi) return;

        const Holat h = joriyHolat();
        if (h == Holat::Ustida || h == Holat::Bosilgan) {
            c.toldir(ramka, U::yumshoqFon, U::radiusKichik);
        }
        if (fokusda) {
            c.chegara(D2D1::RectF(ramka.left - 2, ramka.top - 2, ramka.right + 2, ramka.bottom + 2),
                      U::kok, U::radiusKichik + 2, 2.0f);
        }

        const float cx = (ramka.left + ramka.right) / 2;
        const float cy = (ramka.top + ramka.bottom) / 2;
        const uint32_t rang = (h == Holat::Bosilgan) ? U::matn2 : U::matn;

        // Sakkizta tish — har biri 45 gradusga burilgan.
        for (int i = 0; i < 8; ++i) {
            c.aylantir(i * 45.0f, cx, cy);
            c.toldir(ramkaYasa(cx - 1.6f, cy - 9.0f, 3.2f, 3.6f), rang, 1.0f);
            c.aylantirishniBekor();
        }
        // Halqa va oʻrtadagi teshik.
        c.chegara(ramkaYasa(cx - 5.4f, cy - 5.4f, 10.8f, 10.8f), rang, 5.4f, 2.0f);
        c.doira(cx, cy, 1.7f, rang);
    }

    bool sichqonQoyildi(D2D1_POINT_2F p) override {
        const bool ediBosilgan = bosilgan_;
        Vidjet::sichqonQoyildi(p);
        if (ediBosilgan && ichidami(ramka, p) && bosilganda) bosilganda();
        return true;
    }

    LPCWSTR kursor() const override { return IDC_HAND; }
    bool fokusOladimi() const override { return true; }
    bool klavisha(WPARAM kod) override {
        if ((kod == VK_RETURN || kod == VK_SPACE) && bosilganda) {
            bosilganda();
            return true;
        }
        return false;
    }
};

// ---- Ichki holat -----------------------------------------------------------

struct AsosiyOyna::Ichki {
    HWND oyna = nullptr;
    HINSTANCE instance = nullptr;
    Chizgich chizgich;

    // Toolbar vidjetlari — tab almashganda oʻzgarmaydi, shuning uchun alohida
    // konteynerda turadi.
    Konteyner toolbar;
    std::shared_ptr<Tanlagich> segment;
    std::shared_ptr<SozlamaTugmasi> sozlamalarTugmasi;

    // Yangilanish banneri.
    std::shared_ptr<Tugma> yangiAmal;
    std::shared_ptr<Tugma> yangiIkkinchi;
    std::shared_ptr<Tugma> yangiYopish;
    std::wstring yangiMatn, yangiKalit;
    std::function<void()> yangiAmalBosildi, yangiIkkinchiBosildi;
    float yangiMatnOngi = 0;  // matn shu x gacha (tugmalar chapi)
    bool yangiBannerKorinadi = false;

    float bannerBalandligi() const { return yangiBannerKorinadi ? kYangiBannerBalandligi : 0.0f; }

    std::array<std::unique_ptr<Tab>, 3> tablar{};
    int joriyTab = 0;

    // Oxirgi berilgan diktovka holati. Tab keyin yaratilsa ham shu qiymatni
    // darhol oladi — sarlavhadagi izohga qarang.
    bool yozilyapti = false;

    bool sichqonKuzatilyapti = false;

    float dpiNisbat() const { return static_cast<float>(GetDpiForWindow(oyna)) / 96.0f; }

    // Piksel koordinatasini DIP ga oʻgiradi. Direct2D DIP'da ishlaydi,
    // Windows hodisalari esa pikselda keladi.
    D2D1_POINT_2F nuqta(LPARAM lp) const {
        const float k = dpiNisbat();
        return D2D1::Point2F(static_cast<float>(GET_X_LPARAM(lp)) / k,
                             static_cast<float>(GET_Y_LPARAM(lp)) / k);
    }

    D2D1_RECT_F tabHududi() const {
        const auto o = chizgich.olcham();
        return D2D1::RectF(0, bannerBalandligi() + kToolbarBalandligi, o.width, o.height);
    }

    Tab* tab() {
        return (joriyTab >= 0 && joriyTab < static_cast<int>(tablar.size()))
                   ? tablar[static_cast<size_t>(joriyTab)].get()
                   : nullptr;
    }

    FaylTab* faylTab() { return static_cast<FaylTab*>(tablar[1].get()); }
    TarjimaTab* tarjimaTab() { return static_cast<TarjimaTab*>(tablar[2].get()); }

    void qaytaChiz() {
        if (oyna) InvalidateRect(oyna, nullptr, FALSE);
    }

    void joylashtir();
    void chiz();
    void tabniOzgartir(int i);
    LRESULT hodisa(UINT xabar, WPARAM wp, LPARAM lp);
};

void AsosiyOyna::Ichki::joylashtir() {
    const auto o = chizgich.olcham();
    if (o.width <= 0) return;

    // Tanlagich toolbar'ning aynan oʻrtasida — dizaynda shunday. Kengligi
    // matnga qarab emas, qatʼiy: bandlar bir xil kenglikda boʻlishi kerak.
    constexpr float kSegmentKengligi = 260.0f;
    constexpr float kSegmentBalandligi = 30.0f;
    const float ust = bannerBalandligi();

    segment->ramka = ramkaYasa((o.width - kSegmentKengligi) / 2,
                               ust + (kToolbarBalandligi - kSegmentBalandligi) / 2,
                               kSegmentKengligi, kSegmentBalandligi);

    sozlamalarTugmasi->ramka = ramkaYasa(o.width - 46, ust + (kToolbarBalandligi - 30) / 2, 30, 30);

    if (yangiBannerKorinadi) {
        // Oʻngdan chapga: ✕ (boʻlsa), asosiy tugma, ikkinchi tugma; matn qolgan joyda.
        float x = o.width - 14;
        if (yangiYopish->korinadi) {
            yangiYopish->ramka = ramkaYasa(x - 22, (kYangiBannerBalandligi - 22) / 2, 22, 22);
            x -= 22 + 10;
        }
        for (Tugma* t : {yangiAmal.get(), yangiIkkinchi.get()}) {
            if (!t->korinadi) continue;
            const float w = t->kerakliKenglik(chizgich);
            t->ramka = ramkaYasa(x - w, (kYangiBannerBalandligi - 24) / 2, w, 24);
            x -= w + 8;
        }
        yangiMatnOngi = x - 4;
    }

    // Matn maydoni bola oyna va u pikselda joylashadi — tab oʻlchamni
    // hisoblashdan OLDIN joriy masshtabni bilishi kerak.
    if (auto* f = faylTab()) f->dpiNisbat(dpiNisbat());
    if (auto* t = tarjimaTab()) t->dpiNisbat(dpiNisbat());
    if (auto* t = tab()) t->joylashtir(tabHududi());
}

void AsosiyOyna::Ichki::chiz() {
    chizgich.chizishBoshlandi();

    const auto o = chizgich.olcham();

    // Yangilanish banneri
    const float ust = bannerBalandligi();
    if (yangiBannerKorinadi) {
        chizgich.toldir(D2D1::RectF(0, 0, o.width, kYangiBannerBalandligi), U::bannerFon);
        chizgich.chiziq(0, kYangiBannerBalandligi, o.width, kYangiBannerBalandligi, U::bannerChet);
        const float matnKengligi = std::max(40.0f, yangiMatnOngi - 20);
        chizgich.matn(yangiMatn, ramkaYasa(20, 0, matnKengligi, kYangiBannerBalandligi),
                      U::bannerMatn, 13.0f, Ogirlik::Oddiy, Hizalash::Chap, true);
    }

    // Toolbar
    chizgich.toldir(D2D1::RectF(0, ust, o.width, ust + kToolbarBalandligi), U::panel);
    chizgich.chiziq(0, ust + kToolbarBalandligi, o.width, ust + kToolbarBalandligi, U::ajratgich);
    toolbar.chiz(chizgich);

    // Tab hududi
    chizgich.toldir(tabHududi(), U::oq);
    if (auto* t = tab()) {
        t->chiz(chizgich);
        t->vidjetlar.chiz(chizgich);
    }

    if (!chizgich.chizishTugadi()) {
        // Resurslar yoʻqolgan — keyingi WM_PAINT ularni qayta yaratadi.
        qaytaChiz();
    }
}

void AsosiyOyna::Ichki::tabniOzgartir(int i) {
    if (i == joriyTab) return;
    if (auto* eski = tab()) eski->korinishOzgardi(false);
    joriyTab = i;
    if (auto* t = tab()) {
        t->joylashtir(tabHududi());
        t->faollashdi();
        t->korinishOzgardi(true);
    }
    qaytaChiz();
}

LRESULT AsosiyOyna::Ichki::hodisa(UINT xabar, WPARAM wp, LPARAM lp) {
    // Fon oqimlaridan («Fayl» tabining transkripsiya ishi va LLM oqimi)
    // kelgan xabarlar. Ular oynaga faqat shu yoʻl bilan tegadi.
    if (xabar >= FaylXabar::kBirinchi && xabar <= FaylXabar::kOxirgi) {
        if (auto* f = faylTab()) f->xabarKeldi(xabar, wp, lp);
        return 0;
    }
    if (xabar >= TarjimaXabar::kBirinchi && xabar <= TarjimaXabar::kOxirgi) {
        if (auto* t = tarjimaTab()) t->xabarKeldi(xabar, wp, lp);
        return 0;
    }

    switch (xabar) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(oyna, &ps);
            chiz();
            EndPaint(oyna, &ps);
            return 0;
        }

        // Fon Direct2D bilan toʻliq chiziladi. Windows'ning oʻzi tozalashiga
        // ruxsat berilsa, oʻlchamni oʻzgartirganda oyna miltillaydi.
        case WM_ERASEBKGND: return 1;

        case WM_SIZE:
            chizgich.olchamOzgardi(LOWORD(lp), HIWORD(lp));
            joylashtir();
            qaytaChiz();
            return 0;

        case WM_GETMINMAXINFO: {
            const float k = oyna ? dpiNisbat() : 1.0f;
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = static_cast<LONG>(kEngKichikKenglik * k);
            mmi->ptMinTrackSize.y = static_cast<LONG>(kEngKichikBalandlik * k);
            return 0;
        }

        // Foydalanuvchi oynani boshqa masshtabdagi ekranga sudraganda.
        // Windows tavsiya qilgan yangi ramkani qabul qilamiz va render
        // target'ning DPI'sini yangilaymiz — shundan keyin barcha DIP
        // koordinatalari yana toʻgʻri chiqadi.
        case WM_DPICHANGED: {
            const auto* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(oyna, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            chizgich.dpiOzgardi(HIWORD(wp));
            joylashtir();
            qaytaChiz();
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!sichqonKuzatilyapti) {
                // WM_MOUSELEAVE oʻz-oʻzidan kelmaydi — uni soʻrash kerak.
                // Busiz sichqoncha oynadan chiqib ketganda tugma «ustida»
                // holatida qotib qoladi.
                TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, oyna, 0};
                TrackMouseEvent(&t);
                sichqonKuzatilyapti = true;
            }
            const auto p = nuqta(lp);
            bool q = toolbar.sichqonHarakat(p);
            if (auto* t = tab()) q |= t->vidjetlar.sichqonHarakat(p);
            if (q) qaytaChiz();
            return 0;
        }

        case WM_MOUSELEAVE: {
            sichqonKuzatilyapti = false;
            bool q = toolbar.sichqonChiqdi();
            if (auto* t = tab()) q |= t->vidjetlar.sichqonChiqdi();
            if (q) qaytaChiz();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetFocus(oyna);
            SetCapture(oyna);
            const auto p = nuqta(lp);
            bool q = toolbar.sichqonBosildi(p);
            if (auto* t = tab()) q |= t->vidjetlar.sichqonBosildi(p);
            if (q) qaytaChiz();
            return 0;
        }

        case WM_LBUTTONUP: {
            ReleaseCapture();
            const auto p = nuqta(lp);
            bool q = toolbar.sichqonQoyildi(p);
            if (auto* t = tab()) q |= t->vidjetlar.sichqonQoyildi(p);
            if (q) qaytaChiz();
            return 0;
        }

        // Oʻng tugma — roʻyxatlardagi kontekst menyusi uchun.
        case WM_RBUTTONUP: {
            const auto p = nuqta(lp);
            if (auto* t = tab()) {
                if (t->vidjetlar.sichqonOngBosildi(p)) qaytaChiz();
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            // Gʻildirak koordinatalari EKRAN boʻyicha keladi, boshqa
            // sichqoncha xabarlaridan farqli.
            POINT ekran{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(oyna, &ekran);
            const float k = dpiNisbat();
            const auto p = D2D1::Point2F(ekran.x / k, ekran.y / k);
            const float delta =
                static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA * 48.0f;
            if (auto* t = tab()) {
                if (t->vidjetlar.aylantirildi(p, delta)) qaytaChiz();
            }
            return 0;
        }

        case WM_SETCURSOR: {
            if (LOWORD(lp) != HTCLIENT) break;
            POINT c{};
            GetCursorPos(&c);
            ScreenToClient(oyna, &c);
            const float k = dpiNisbat();
            const auto p = D2D1::Point2F(c.x / k, c.y / k);
            LPCWSTR kur = toolbar.kursor(p);
            if (!kur)
                if (auto* t = tab()) kur = t->vidjetlar.kursor(p);
            SetCursor(LoadCursorW(nullptr, kur ? kur : IDC_ARROW));
            return TRUE;
        }

        case WM_KEYDOWN: {
            if (wp == VK_TAB) {
                const bool orqaga = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool q = false;
                if (auto* t = tab()) q = t->vidjetlar.tab(orqaga);
                if (!q) q = toolbar.tab(orqaga);
                if (q) qaytaChiz();
                return 0;
            }
            if (wp == VK_ESCAPE) {
                ShowWindow(oyna, SW_HIDE);
                return 0;
            }

            // Ctrl+1/2/3 — tab almashtirish (macOS'da ⌘1/⌘2/⌘3).
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wp >= '1' && wp <= '3') {
                const int i = static_cast<int>(wp - '1');
                segment->tanlangan = i;
                tabniOzgartir(i);
                return 0;
            }

            bool q = toolbar.klavisha(wp);
            if (auto* t = tab()) q |= t->vidjetlar.klavisha(wp);
            if (q) qaytaChiz();
            return 0;
        }

        // Matn maydonining (RichEdit) oʻzgarishi otaga notifikatsiya boʻlib
        // keladi. `matnMaydonXabari` uni tegishli maydonga ulaydi.
        case WM_COMMAND:
            matnMaydonXabari(wp, lp);
            if (auto* t = tarjimaTab()) t->buyruq(wp, lp);
            return 0;

        // Fayl sudrab tashlandi. macOS'da bu `DropView` (studiya_view.swift).
        // Faqat BIRINCHI fayl olinadi: bir vaqtda bitta ish ketadi.
        case WM_DROPFILES: {
            auto tashlash = reinterpret_cast<HDROP>(wp);
            wchar_t yol[MAX_PATH * 4] = {0};
            if (DragQueryFileW(tashlash, 0, yol, static_cast<UINT>(std::size(yol)))) {
                segment->tanlangan = 1;
                tabniOzgartir(1);
                if (auto* f = faylTab()) f->faylniQabulQil(yol);
            }
            DragFinish(tashlash);
            return 0;
        }

        // Oyna yopilmaydi — yashiriladi. Ilova tray'da yashaydi va diktovka
        // tugmasi oyna yopiq turganda ham ishlashi kerak.
        case WM_CLOSE: ShowWindow(oyna, SW_HIDE); return 0;

        default: break;
    }
    return DefWindowProcW(oyna, xabar, wp, lp);
}

// ---- Oyna protsedurasi -----------------------------------------------------

namespace {

LRESULT CALLBACK oynaProc(HWND h, UINT xabar, WPARAM wp, LPARAM lp) {
    auto* ichki = reinterpret_cast<AsosiyOyna::Ichki*>(GetWindowLongPtrW(h, GWLP_USERDATA));

    if (xabar == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        ichki = static_cast<AsosiyOyna::Ichki*>(cs->lpCreateParams);
        ichki->oyna = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ichki));
    }

    if (!ichki) return DefWindowProcW(h, xabar, wp, lp);
    return ichki->hodisa(xabar, wp, lp);
}

}  // namespace

// ---- AsosiyOyna ------------------------------------------------------------

AsosiyOyna::AsosiyOyna() : ichki_(std::make_unique<Ichki>()) {}
AsosiyOyna::~AsosiyOyna() = default;

bool AsosiyOyna::qur(HINSTANCE instance) {
    ichki_->instance = instance;

    WNDCLASSEXW sinf{};
    sinf.cbSize = sizeof(sinf);
    // CS_HREDRAW/CS_VREDRAW yoʻq: oʻlchamni oʻzgartirganda butun oynani qayta
    // chizishni oʻzimiz WM_SIZE da boshqaramiz.
    sinf.lpfnWndProc = oynaProc;
    sinf.hInstance = instance;
    sinf.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    sinf.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    sinf.lpszClassName = kSinfNomi;
    RegisterClassExW(&sinf);

    // Oyna oʻlchami DIP'da berilgan — ekran masshtabiga koʻpaytiramiz.
    const UINT dpi = GetDpiForSystem();
    const float k = static_cast<float>(dpi) / 96.0f;

    ichki_->oyna = CreateWindowExW(
        0, kSinfNomi, kSarlavha, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
        CW_USEDEFAULT, static_cast<int>(kBoshlangichKenglik * k),
        static_cast<int>(kBoshlangichBalandlik * k), nullptr, nullptr, instance, ichki_.get());

    if (!ichki_->oyna) return false;

    // Ish maydoniga sigʻdiramiz va markazga qoʻyamiz.
    //
    // 720×560 DIP koʻpchilik ekranga sigʻadi, lekin 100% masshtabdagi
    // 1024×600 netbukda yoki 150% dagi 1366×768 da yoʻq: oyna pastki
    // qismi vazifalar panelining ostiga tushib ketardi va Studiya
    // tabidagi «Nusxa olish / Saqlash» tugmalari koʻrinmasdi.
    {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(MonitorFromWindow(ichki_->oyna, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            RECT r{};
            GetWindowRect(ichki_->oyna, &r);
            const int ishW = mi.rcWork.right - mi.rcWork.left;
            const int ishH = mi.rcWork.bottom - mi.rcWork.top;
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            if (w > ishW) w = ishW;
            if (h > ishH) h = ishH;
            SetWindowPos(ichki_->oyna, nullptr, mi.rcWork.left + (ishW - w) / 2,
                         mi.rcWork.top + (ishH - h) / 2, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    if (!ichki_->chizgich.qur(ichki_->oyna)) {
        logWrite(L"Direct2D ishga tushmadi — oyna chizilmaydi");
        return false;
    }

    // Toolbar
    ichki_->segment =
        std::make_shared<Tanlagich>(std::vector<std::wstring>{L"Yozish", L"Fayl", L"Tarjima"});
    ichki_->segment->ozgarganda = [this](int i) { ichki_->tabniOzgartir(i); };
    ichki_->toolbar.qosh(ichki_->segment);

    ichki_->sozlamalarTugmasi = std::make_shared<SozlamaTugmasi>();
    ichki_->sozlamalarTugmasi->bosilganda = [this] {
        if (onSozlamalar) onSozlamalar();
    };
    ichki_->toolbar.qosh(ichki_->sozlamalarTugmasi);

    // 1.1.0 dagi «Yuklab olish» (serverdan kelgan URL'ni `ShellExecuteW` bilan
    // ochardi) olib tashlandi: yangilanishni endi yangilovchi oʻzi oʻrnatadi.
    ichki_->yangiAmal = std::make_shared<Tugma>(L"", Tugma::Kor::Ikkilamchi);
    ichki_->yangiAmal->shriftOlchami = 12.0f;
    ichki_->yangiAmal->korinadi = false;
    ichki_->yangiAmal->bosilganda = [this] {
        if (ichki_->yangiAmalBosildi) ichki_->yangiAmalBosildi();
    };
    ichki_->toolbar.qosh(ichki_->yangiAmal);

    ichki_->yangiIkkinchi = std::make_shared<Tugma>(L"", Tugma::Kor::Matnli);
    ichki_->yangiIkkinchi->shriftOlchami = 12.0f;
    ichki_->yangiIkkinchi->korinadi = false;
    ichki_->yangiIkkinchi->bosilganda = [this] {
        if (ichki_->yangiIkkinchiBosildi) ichki_->yangiIkkinchiBosildi();
    };
    ichki_->toolbar.qosh(ichki_->yangiIkkinchi);

    ichki_->yangiYopish = std::make_shared<Tugma>(L"✕", Tugma::Kor::Matnli);
    ichki_->yangiYopish->shriftOlchami = 13.0f;
    ichki_->yangiYopish->korinadi = false;
    ichki_->yangiYopish->bosilganda = [this] {
        Settings s = loadSettings();
        s.yangilanishYopildi = ichki_->yangiKalit;
        saveSettings(s);
        ichki_->yangiBannerKorinadi = false;
        ichki_->yangiAmal->korinadi = false;
        ichki_->yangiIkkinchi->korinadi = false;
        ichki_->yangiYopish->korinadi = false;
        ichki_->joylashtir();
        ichki_->qaytaChiz();
    };
    ichki_->toolbar.qosh(ichki_->yangiYopish);

    // Tablar. «Tarjima» hali qoʻshilmagan — segment unga oʻtsa boʻsh sahifa
    // koʻrsatiladi.
    auto yozish = std::make_unique<YozishTab>();
    yozish->onDiktovka = [this] {
        if (onDiktovka) onDiktovka();
    };
    yozish->onNusxa = [this](const std::wstring& m) { buferGaQoy(ichki_->oyna, m); };
    yozish->onOchir = [this](const std::wstring& id) {
        DiktovkaTarixi::birgalik().ochir(id);
        tarixniYangila();
    };
    yozish->onTozala = [this] {
        DiktovkaTarixi::birgalik().tozala();
        tarixniYangila();
    };
    yozish->otaOyna(ichki_->oyna);
    yozish->hotkeyMatni(loadSettings().hotkeyDisplay());
    ichki_->tablar[0] = std::move(yozish);

    auto fayl = std::make_unique<FaylTab>(ichki_->oyna);
    fayl->onTarjima = [this](const std::wstring& m, const std::string& kod) {
        // «Fayl» tabidan matn «Tarjima» tabiga oʻtadi va tab almashadi.
        ichki_->segment->tanlangan = 2;
        ichki_->tabniOzgartir(2);
        if (auto* t = ichki_->tarjimaTab()) t->matnniQabulQil(m, kod);
        if (onTarjima) onTarjima(m, kod);
    };
    ichki_->tablar[1] = std::move(fayl);

    ichki_->tablar[2] = std::make_unique<TarjimaTab>(ichki_->oyna);

    // Sudrab tashlashni butun oyna qabul qiladi — foydalanuvchi «Fayl»
    // tabiga oʻtishi shart emas, tashlagan zahoti oʻzi oʻtadi.
    DragAcceptFiles(ichki_->oyna, TRUE);

    ichki_->joylashtir();
    return true;
}

YozishTab* AsosiyOyna::yozishTabi() { return static_cast<YozishTab*>(ichki_->tablar[0].get()); }

void AsosiyOyna::yangilanishBanneri(const std::wstring& matn, const std::wstring& kalit,
                                    const std::wstring& tugma, std::function<void()> amal,
                                    const std::wstring& ikkinchiTugma,
                                    std::function<void()> ikkinchiAmal) {
    // Foydalanuvchi shu bannerni allaqachon yopgan boʻlsa — koʻrsatmaymiz.
    if (!kalit.empty() && loadSettings().yangilanishYopildi == kalit) return;

    ichki_->yangiMatn = matn;
    ichki_->yangiKalit = kalit;
    ichki_->yangiAmalBosildi = std::move(amal);
    ichki_->yangiIkkinchiBosildi = std::move(ikkinchiAmal);
    ichki_->yangiAmal->nom = tugma;
    ichki_->yangiIkkinchi->nom = ikkinchiTugma;
    ichki_->yangiBannerKorinadi = true;
    ichki_->yangiAmal->korinadi = !tugma.empty();
    ichki_->yangiIkkinchi->korinadi = !ikkinchiTugma.empty();
    // Kalitsiz banner (majburiy yangilanish) yopilmaydi — u holat oʻzgarganda
    // `yangilanishBanneriniYashir` bilan yoʻqoladi.
    ichki_->yangiYopish->korinadi = !kalit.empty();
    ichki_->joylashtir();
    ichki_->qaytaChiz();
}

void AsosiyOyna::yangilanishBanneriniYashir() {
    if (!ichki_->yangiBannerKorinadi) return;
    ichki_->yangiBannerKorinadi = false;
    ichki_->yangiAmal->korinadi = false;
    ichki_->yangiIkkinchi->korinadi = false;
    ichki_->yangiYopish->korinadi = false;
    ichki_->joylashtir();
    ichki_->qaytaChiz();
}

FaylTab* AsosiyOyna::faylTabi() { return ichki_->faylTab(); }

TarjimaTab* AsosiyOyna::tarjimaTabi() { return ichki_->tarjimaTab(); }

void AsosiyOyna::korsat() {
    ShowWindow(ichki_->oyna, SW_SHOW);
    SetForegroundWindow(ichki_->oyna);
    SetFocus(ichki_->oyna);
}

void AsosiyOyna::yashir() { ShowWindow(ichki_->oyna, SW_HIDE); }

bool AsosiyOyna::korinadimi() const {
    // `IsWindowVisible` YIGʻILGAN oyna uchun ham TRUE qaytaradi. Busiz
    // oyna yigʻilganda diktovkaning suzuvchi koʻrsatkichi chiqmasdi va
    // foydalanuvchi hech qanday belgi koʻrmasdi — holbuki ilova aynan
    // shu holatda ishlatiladi (boshqa ilovaga yozdirish uchun).
    return ichki_->oyna && IsWindowVisible(ichki_->oyna) && !IsIconic(ichki_->oyna);
}

HWND AsosiyOyna::deskriptor() const { return ichki_->oyna; }

void AsosiyOyna::diktovkaHolati(bool yozilyapti) {
    ichki_->yozilyapti = yozilyapti;
    if (auto* t = yozishTabi()) t->holatniQoy(yozilyapti);
    ichki_->qaytaChiz();
}

void AsosiyOyna::diktovkaTugadi(const std::wstring&) { tarixniYangila(); }

void AsosiyOyna::tarixniYangila() {
    auto* t = yozishTabi();
    if (!t) return;

    std::vector<TarixYozuvi> royxat;
    for (const auto& y : DiktovkaTarixi::birgalik().oqi()) {
        royxat.push_back({y.id, y.matn, y.sana});
    }
    t->tarixniQoy(std::move(royxat));
    ichki_->qaytaChiz();
}

void AsosiyOyna::darajaniQoy(float daraja) {
    if (auto* t = yozishTabi()) t->darajaniQoy(daraja);
    ichki_->qaytaChiz();
}

void AsosiyOyna::hotkeyMatni(const std::wstring& matn) {
    if (auto* t = yozishTabi()) t->hotkeyMatni(matn);
    ichki_->qaytaChiz();
}

void AsosiyOyna::banner(const std::wstring& matn, const std::wstring& tugmaNomi,
                        std::function<void()> tugmaBosildi) {
    if (auto* t = yozishTabi()) t->banner(matn, tugmaNomi, std::move(tugmaBosildi));
    ichki_->joylashtir();
    ichki_->qaytaChiz();
}

}  // namespace rubai
