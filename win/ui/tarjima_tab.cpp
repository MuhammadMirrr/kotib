// «Tarjima» tabi — macOS'dagi `tarjima_view.swift` egizagi.
// Interfeys va izohlar — `tarjima_tab.h`.

#include "tarjima_tab.h"

#include "matn_maydon.h"
#include "soragich.h"
#include "uslub.h"
#include "vidjet.h"
#include "../core/config.h"
#include "../core/tarjima_yuklovchi.h"
#include "../core/tarjimon.h"
#include "../core/util.h"

#include <algorithm>
#include <map>
#include <vector>

namespace rubai {

namespace {

// Dizayndagi oʻlchamlar (DIP) — `tarjima_view.swift` bilan bir xil.
constexpr float kChet = 24.0f;
constexpr float kTilBalandligi = 30.0f;
constexpr float kTilKengligi = 200.0f;
constexpr float kBannerBalandligi = 72.0f;
constexpr float kBarBalandligi = 6.0f;
constexpr float kTugmaBalandligi = 38.0f;

// Til tanlagichlarining boshqaruv identifikatorlari.
constexpr int kIdManbaTil = 700;
constexpr int kIdMaqsadTil = 701;

}  // namespace

// ---- Til tanlagich ---------------------------------------------------------
//
// Tahrirlanadigan COMBOBOX ustidagi yupqa qatlam: 202 ta nom roʻyxatda,
// yozilgan harflar boʻyicha avtomatik toʻldirish.
class TilTanlagich {
public:
    bool qur(HWND ota, int id);
    void joylashtir(const D2D1_RECT_F& r, float k);
    void korsat(bool v);
    void yoqilgan(bool v);

    void tilniQoy(const Til& t);
    const Til& til() const { return joriy_; }

    // COMBOBOX'ning tahrir qismidagi oʻzgarish — avtomatik toʻldirish.
    void ozgardi();
    // Roʻyxatdan tanlov.
    void tanlandi();

    HWND deskriptor() const { return oyna_; }

private:
    std::wstring matn() const;

    HWND oyna_ = nullptr;
    HFONT shrift_ = nullptr;
    float oxirgiDpi_ = 0;
    Til joriy_;
    bool ozimizYozyapmiz_ = false;
};

bool TilTanlagich::qur(HWND ota, int id) {
    oyna_ = CreateWindowExW(
        0, L"COMBOBOX", L"", WS_CHILD | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL, 0,
        0, 10, 240, ota, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(ota, GWLP_HINSTANCE)), nullptr);
    if (!oyna_) return false;

    for (const auto& t : Tillar::hammasi()) {
        SendMessageW(oyna_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.nom.c_str()));
    }
    // Roʻyxatda bir vaqtda koʻrinadigan bandlar soni — macOS'dagi 12 bilan
    // bir xil. Makro mingw sarlavhalarida yoʻq, qiymati Windows SDK'dan.
    constexpr UINT kCbSetMinVisible = 0x1701;
    SendMessageW(oyna_, kCbSetMinVisible, 12, 0);
    return true;
}

void TilTanlagich::joylashtir(const D2D1_RECT_F& r, float k) {
    if (!oyna_) return;
    if (oxirgiDpi_ != k) {
        oxirgiDpi_ = k;
        HFONT eski = shrift_;
        shrift_ = CreateFontW(-static_cast<int>(14 * k + 0.5f), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        SendMessageW(oyna_, WM_SETFONT, reinterpret_cast<WPARAM>(shrift_), TRUE);
        if (eski) DeleteObject(eski);
    }
    // COMBOBOX'ning yopiq balandligi band balandligidan kelib chiqadi, oyna
    // balandligidan emas — oyna balandligi ochilgan roʻyxat hududini
    // bildiradi. Yonidagi ⇄ tugmasi bilan tenglashishi uchun band
    // balandligini oʻzimiz qoʻyamiz (`-1` — tahrir maydonining balandligi).
    const int qatorBalandligi = static_cast<int>((r.bottom - r.top) * k) - 8;
    SendMessageW(oyna_, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1),
                 static_cast<LPARAM>(qatorBalandligi));

    SetWindowPos(oyna_, nullptr, static_cast<int>(r.left * k), static_cast<int>(r.top * k),
                 static_cast<int>((r.right - r.left) * k), static_cast<int>(240 * k),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void TilTanlagich::korsat(bool v) {
    if (oyna_) ShowWindow(oyna_, v ? SW_SHOW : SW_HIDE);
}
void TilTanlagich::yoqilgan(bool v) {
    if (oyna_) EnableWindow(oyna_, v ? TRUE : FALSE);
}

std::wstring TilTanlagich::matn() const {
    if (!oyna_) return {};
    const int n = GetWindowTextLengthW(oyna_);
    if (n <= 0) return {};
    std::wstring b(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(oyna_, b.data(), n + 1);
    b.resize(static_cast<size_t>(n));
    return b;
}

void TilTanlagich::tilniQoy(const Til& t) {
    joriy_ = t;
    if (!oyna_) return;
    ozimizYozyapmiz_ = true;
    const LRESULT i = SendMessageW(oyna_, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
                                   reinterpret_cast<LPARAM>(t.nom.c_str()));
    if (i >= 0) SendMessageW(oyna_, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
    SetWindowTextW(oyna_, t.nom.c_str());
    // `CB_SETCURSEL` matnni BUTUNLAY tanlangan holda qoldiradi: tab ochilishi
    // bilan ikkala til koʻk fonda turardi, goʻyo foydalanuvchi ularni
    // belgilagandek. Kursorni oxiriga qoʻyib tanlovni olib tashlaymiz.
    const int uzunlik = static_cast<int>(t.nom.size());
    SendMessageW(oyna_, CB_SETEDITSEL, 0, MAKELPARAM(uzunlik, uzunlik));
    ozimizYozyapmiz_ = false;
}

void TilTanlagich::tanlandi() {
    const LRESULT i = SendMessageW(oyna_, CB_GETCURSEL, 0, 0);
    if (i < 0) return;
    const auto& hammasi = Tillar::hammasi();
    if (static_cast<size_t>(i) < hammasi.size()) joriy_ = hammasi[static_cast<size_t>(i)];
}

void TilTanlagich::ozgardi() {
    if (ozimizYozyapmiz_) return;

    const std::wstring yozilgan = matn();
    if (yozilgan.empty()) return;

    // Avtomatik toʻldirish: yozilgan boshlanish boʻyicha bandni topamiz va
    // qolgan qismini tanlangan holda qoʻyamiz. Foydalanuvchi yozishda davom
    // etsa u ustiga yoziladi, Enter bossa tanlov qoladi.
    const LRESULT i = SendMessageW(oyna_, CB_FINDSTRING, static_cast<WPARAM>(-1),
                                   reinterpret_cast<LPARAM>(yozilgan.c_str()));
    if (i < 0) return;

    const auto& hammasi = Tillar::hammasi();
    if (static_cast<size_t>(i) >= hammasi.size()) return;
    const std::wstring toliq = hammasi[static_cast<size_t>(i)].nom;
    if (toliq.size() <= yozilgan.size()) {
        joriy_ = hammasi[static_cast<size_t>(i)];
        return;
    }

    ozimizYozyapmiz_ = true;
    SetWindowTextW(oyna_, toliq.c_str());
    SendMessageW(oyna_, CB_SETEDITSEL, 0, MAKELPARAM(static_cast<int>(yozilgan.size()), -1));
    ozimizYozyapmiz_ = false;
    joriy_ = hammasi[static_cast<size_t>(i)];
}

// ---- Ichki holat -----------------------------------------------------------

struct TarjimaTab::Ichki {
    HWND ota = nullptr;
    float dpi = 1.0f;
    D2D1_RECT_F hudud{};

    TilTanlagich manbaTil, maqsadTil;
    MatnMaydon manba, natija;

    std::shared_ptr<Tugma> almashtir;
    std::shared_ptr<Tugma> nusxa;
    std::shared_ptr<Tugma> tarjima;
    std::shared_ptr<Tugma> bannerTugma;

    TarjimaYuklovchi yuklovchi;
    bool yuklanyapti = false;
    long long yuklOlingan = 0, yuklJami = 0;
    std::wstring yuklXatosi;

    bool modelBor = false;
    bool ishHolati = false;
    int bajarilgan = 0, jami = 0;
    std::wstring jarayonMatni;

    void qaytaChiz() {
        if (ota) InvalidateRect(ota, nullptr, FALSE);
    }

    float bannerBalandligi() const { return modelBor ? 0.0f : kBannerBalandligi + 16; }

    void modelHolatiniYangila();
    void kiritishHolatiniYangila();
    void tilniSaqla();
    void tillarniAlmashtir();
    void tarjimaniBoshla();
    void ishTugadi();
    void yuklashniBoshla();
};

void TarjimaTab::Ichki::modelHolatiniYangila() {
    modelBor = TarjimaModel::tayyor();
    manbaTil.yoqilgan(modelBor);
    maqsadTil.yoqilgan(modelBor);
    manba.faqatOqish(!modelBor);
    bannerTugma->korinadi = !modelBor;
    kiritishHolatiniYangila();
}

void TarjimaTab::Ichki::yuklashniBoshla() {
    if (yuklanyapti) {
        yuklovchi.bekorQil();
        return;
    }
    yuklanyapti = true;
    yuklOlingan = yuklJami = 0;
    yuklXatosi.clear();
    bannerTugma->nom = L"Bekor qilish";
    qaytaChiz();

    HWND oyna = ota;
    yuklovchi.boshla(
        [oyna](long long olingan, long long jami) {
            // Baytlar `WPARAM` ga sigʻmasligi mumkin emas, lekin megabaytda
            // uzatish xabar navbatini ham yengillashtiradi.
            PostMessageW(oyna, TarjimaXabar::kYuklash, static_cast<WPARAM>(olingan / (1024 * 1024)),
                         static_cast<LPARAM>(jami / (1024 * 1024)));
        },
        [oyna](bool ok, std::wstring xato) {
            PostMessageW(oyna, TarjimaXabar::kYuklashTugadi, ok ? 1 : 0,
                         reinterpret_cast<LPARAM>(new std::wstring(std::move(xato))));
        });
}

void TarjimaTab::Ichki::kiritishHolatiniYangila() {
    // Har tugma bosilganda chaqiriladi — butun matnni nusxalamaymiz.
    // Faqat boʻshliqdan iborat matn uchun uzunlik yetarli emas, lekin u
    // holatda ham tarjima qilishga arziydigan narsa yoʻq va `tarjimaniBoshla`
    // buni oxirgi marta oʻzi tekshiradi.
    if (!ishHolati) tarjima->yoqilgan = manba.uzunlik() > 0 && modelBor;
    nusxa->yoqilgan = natija.uzunlik() > 0;
}

void TarjimaTab::Ichki::tilniSaqla() {
    Settings s = loadSettings();
    s.tarjimaManba = manbaTil.til().nllb;
    s.tarjimaMaqsad = maqsadTil.til().nllb;
    saveSettings(s);
}

void TarjimaTab::Ichki::tillarniAlmashtir() {
    const Til eskiManba = manbaTil.til();
    const Til eskiMaqsad = maqsadTil.til();
    manbaTil.tilniQoy(eskiMaqsad);
    maqsadTil.tilniQoy(eskiManba);

    // Kataklardagi matn ham almashadi — foydalanuvchi odatda tarjimani
    // koʻrib, teskari yoʻnalishda davom ettirmoqchi boʻladi.
    const std::wstring n = natija.matn();
    if (!n.empty()) {
        const std::wstring m = manba.matn();
        manba.matnQoy(n);
        natija.matnQoy(m);
    }
    tilniSaqla();
    kiritishHolatiniYangila();
    qaytaChiz();
}

void TarjimaTab::Ichki::ishTugadi() {
    ishHolati = false;
    tarjima->nom = L"Tarjima";
    bajarilgan = jami = 0;
    jarayonMatni.clear();
    kiritishHolatiniYangila();
    qaytaChiz();
}

void TarjimaTab::Ichki::tarjimaniBoshla() {
    if (ishHolati) {
        Tarjimon::birgalik().bekorQil();
        jarayonMatni = L"Bekor qilinmoqda…";
        qaytaChiz();
        return;
    }

    const std::wstring matn = manba.matn();
    if (matn.find_first_not_of(L" \t\r\n") == std::wstring::npos) return;

    ishHolati = true;
    tarjima->nom = L"Bekor qilish";
    bajarilgan = 0;
    jami = 0;
    jarayonMatni = L"Tayyorlanmoqda…";
    natija.matnQoy(L"");
    qaytaChiz();

    HWND oyna = ota;
    Tarjimon::birgalik().tarjimaQil(
        matn, manbaTil.til(), maqsadTil.til(),
        [oyna](int bajarilgan_, int jami_) {
            PostMessageW(oyna, TarjimaXabar::kJarayon, static_cast<WPARAM>(bajarilgan_),
                         static_cast<LPARAM>(jami_));
        },
        [oyna](std::wstring natija_, TarjimaXatosi xato) {
            if (xato == TarjimaXatosi::Yoq) {
                PostMessageW(oyna, TarjimaXabar::kTayyor, 0,
                             reinterpret_cast<LPARAM>(new std::wstring(std::move(natija_))));
            } else {
                PostMessageW(oyna, TarjimaXabar::kXato, 0,
                             reinterpret_cast<LPARAM>(new std::wstring(tarjimaXatoXabari(xato))));
            }
        });
}

// ---- TarjimaTab ------------------------------------------------------------

TarjimaTab::TarjimaTab(HWND ota) : ichki_(std::make_unique<Ichki>()) {
    ichki_->ota = ota;

    ichki_->manbaTil.qur(ota, kIdManbaTil);
    ichki_->maqsadTil.qur(ota, kIdMaqsadTil);

    const Settings s = loadSettings();
    const Til* m = Tillar::top(s.tarjimaManba.empty() ? Tillar::kUz : s.tarjimaManba);
    if (!m) m = Tillar::top(Tillar::kUz);
    const Til* q = Tillar::top(s.tarjimaMaqsad.empty() ? Tillar::kRu : s.tarjimaMaqsad);
    if (!q) q = Tillar::top(Tillar::kRu);
    if (m) ichki_->manbaTil.tilniQoy(*m);
    if (q) ichki_->maqsadTil.tilniQoy(*q);

    ichki_->manba.qur(ota, 15.0f);
    ichki_->natija.qur(ota, 15.0f);
    ichki_->natija.faqatOqish(true);
    ichki_->manba.onOzgardi = [this] { matnOzgardi(); };

    ichki_->almashtir = std::make_shared<Tugma>(L"⇄", Tugma::Kor::Ikkilamchi);
    ichki_->almashtir->shriftOlchami = 16.0f;
    ichki_->almashtir->bosilganda = [this] { ichki_->tillarniAlmashtir(); };
    vidjetlar.qosh(ichki_->almashtir);

    ichki_->nusxa = std::make_shared<Tugma>(L"Nusxa olish", Tugma::Kor::Ikkilamchi);
    ichki_->nusxa->shriftOlchami = 14.0f;
    ichki_->nusxa->bosilganda = [this] { buferGaQoy(ichki_->ota, ichki_->natija.matn()); };
    vidjetlar.qosh(ichki_->nusxa);

    ichki_->tarjima = std::make_shared<Tugma>(L"Tarjima", Tugma::Kor::Asosiy);
    ichki_->tarjima->shriftOlchami = 15.0f;
    ichki_->tarjima->bosilganda = [this] { ichki_->tarjimaniBoshla(); };
    vidjetlar.qosh(ichki_->tarjima);

    ichki_->bannerTugma = std::make_shared<Tugma>(L"Yuklab olish", Tugma::Kor::Asosiy);
    ichki_->bannerTugma->shriftOlchami = 14.0f;
    ichki_->bannerTugma->bosilganda = [this] { ichki_->yuklashniBoshla(); };
    vidjetlar.qosh(ichki_->bannerTugma);

    ichki_->modelHolatiniYangila();
    korinishOzgardi(false);
}

TarjimaTab::~TarjimaTab() = default;

void TarjimaTab::dpiNisbat(float k) { ichki_->dpi = k; }

void TarjimaTab::korinishOzgardi(bool korinadi) {
    ichki_->manbaTil.korsat(korinadi);
    ichki_->maqsadTil.korsat(korinadi);
    ichki_->manba.korsat(korinadi);
    ichki_->natija.korsat(korinadi);
}

void TarjimaTab::faollashdi() {
    // Model boshqa tabda turganda yuklab olingan boʻlishi mumkin.
    ichki_->modelHolatiniYangila();
    ichki_->qaytaChiz();
}

void TarjimaTab::matnOzgardi() {
    ichki_->kiritishHolatiniYangila();
    ichki_->qaytaChiz();
}

void TarjimaTab::buyruq(WPARAM wp, LPARAM lp) {
    const int id = LOWORD(wp);
    const int hodisa = HIWORD(wp);
    if (id != kIdManbaTil && id != kIdMaqsadTil) return;
    TilTanlagich& t = (id == kIdManbaTil) ? ichki_->manbaTil : ichki_->maqsadTil;
    if (reinterpret_cast<HWND>(lp) != t.deskriptor()) return;

    if (hodisa == CBN_SELCHANGE) {
        t.tanlandi();
        ichki_->tilniSaqla();
    } else if (hodisa == CBN_EDITCHANGE) {
        t.ozgardi();
    } else if (hodisa == CBN_KILLFOCUS) {
        ichki_->tilniSaqla();
    }
}

// ---- Joylashtirish ---------------------------------------------------------

void TarjimaTab::joylashtir(const D2D1_RECT_F& hudud) {
    auto& i = *ichki_;
    i.hudud = hudud;

    const float chap = hudud.left + kChet;
    const float ong = hudud.right - kChet;
    float y = hudud.top + kChet + i.bannerBalandligi();

    if (!i.modelBor) {
        i.bannerTugma->ramka = ramkaYasa(ong - 140, hudud.top + kChet + 20, 140, 32);
    }

    // Tillar qatori
    i.manbaTil.joylashtir(ramkaYasa(chap, y, kTilKengligi, kTilBalandligi), i.dpi);
    i.almashtir->ramka = ramkaYasa(chap + kTilKengligi + 10, y, 44, kTilBalandligi);
    i.maqsadTil.joylashtir(ramkaYasa(chap + kTilKengligi + 64, y, kTilKengligi, kTilBalandligi),
                           i.dpi);
    y += kTilBalandligi + 14;

    // Pastdagi qator: jarayon bari va tugmalar.
    const float tugmalarY = hudud.bottom - kChet - kTugmaBalandligi;
    const float barY = tugmalarY - 12 - kBarBalandligi;

    i.tarjima->ramka = ramkaYasa(ong - 120, tugmalarY, 120, kTugmaBalandligi);
    i.nusxa->ramka = ramkaYasa(ong - 120 - 10 - 130, tugmalarY, 130, kTugmaBalandligi);

    // Ikki katak teng balandlikda — dizaynda shunday.
    const float katakJami = barY - 12 - y;
    const float katak = std::max(60.0f, (katakJami - 14) / 2);
    i.manba.joylashtir(D2D1::RectF(chap, y, ong, y + katak), i.dpi);
    i.natija.joylashtir(D2D1::RectF(chap, y + katak + 14, ong, y + katak * 2 + 14), i.dpi);
}

// ---- Chizish ---------------------------------------------------------------

void TarjimaTab::chiz(Chizgich& c) {
    auto& i = *ichki_;

    // Model banneri
    if (!i.modelBor) {
        const D2D1_RECT_F b =
            ramkaYasa(i.hudud.left + kChet, i.hudud.top + kChet,
                      i.hudud.right - i.hudud.left - kChet * 2, kBannerBalandligi);
        c.toldir(b, U::bannerFon, U::radiusTugma);
        c.chegara(b, U::bannerChet, U::radiusTugma);
        if (i.yuklanyapti) {
            c.matn(L"Model yuklab olinmoqda…",
                   ramkaYasa(b.left + 16, b.top + 12, b.right - b.left - 180, 22), U::bannerMatn,
                   15.0f, Ogirlik::Yarim);

            const D2D1_RECT_F bar =
                ramkaYasa(b.left + 16, b.top + 40, b.right - b.left - 180, kBarBalandligi);
            c.toldir(bar, U::oq, kBarBalandligi / 2);
            if (i.yuklJami > 0) {
                const float w =
                    (bar.right - bar.left) *
                    std::clamp(static_cast<float>(i.yuklOlingan) / i.yuklJami, 0.0f, 1.0f);
                if (w > 0)
                    c.toldir(ramkaYasa(bar.left, bar.top, w, kBarBalandligi), U::kok,
                             kBarBalandligi / 2);
            }
            wchar_t izoh[96];
            swprintf(izoh, 96, L"%lld / %lld MB", i.yuklOlingan, i.yuklJami);
            c.matn(izoh, ramkaYasa(bar.left, b.top + 50, bar.right - bar.left, 18), U::bannerMatn,
                   12.0f);
        } else {
            c.matn(i.yuklXatosi.empty() ? L"Tarjima modeli yuklanmagan" : L"Yuklab boʻlmadi",
                   ramkaYasa(b.left + 16, b.top + 14, b.right - b.left - 180, 22), U::bannerMatn,
                   15.0f, Ogirlik::Yarim);
            c.matn(i.yuklXatosi.empty()
                       ? L"Oflayn tarjima uchun 3,1 GB arxiv yuklab olinadi. Bir marta."
                       : i.yuklXatosi,
                   ramkaYasa(b.left + 16, b.top + 38, b.right - b.left - 180, 20), U::bannerMatn,
                   13.0f);
        }
    }

    // Kataklar ostidagi ramkalar — RichEdit oʻzi chegara chizmaydi.
    for (const MatnMaydon* m : {&i.manba, &i.natija}) {
        RECT r{};
        if (!m->deskriptor() || !GetWindowRect(m->deskriptor(), &r)) continue;
        POINT ust{r.left, r.top}, past{r.right, r.bottom};
        ScreenToClient(i.ota, &ust);
        ScreenToClient(i.ota, &past);
        const D2D1_RECT_F ramka = D2D1::RectF(ust.x / i.dpi - 1, ust.y / i.dpi - 1,
                                              past.x / i.dpi + 1, past.y / i.dpi + 1);
        c.chegara(ramka, U::tugmaChet, U::radiusTugma);
    }

    // Manba boʻsh boʻlsa — joy-yozuv. RichEdit'da placeholder yoʻq.
    // Bu yerda `matn()` EMAS, `uzunlik()`: chizish har kadrda ketadi.
    if (i.modelBor && i.manba.uzunlik() == 0) {
        RECT r{};
        if (i.manba.deskriptor() && GetWindowRect(i.manba.deskriptor(), &r)) {
            POINT p{r.left, r.top};
            ScreenToClient(i.ota, &p);
            c.matn(L"Matnni shu yerga yozing…",
                   ramkaYasa(p.x / i.dpi + 28, p.y / i.dpi + 24, 260, 22), U::matn3, 15.0f);
        }
    }

    // Jarayon bari va matni
    const float tugmalarY = i.hudud.bottom - kChet - kTugmaBalandligi;
    const float barY = tugmalarY - 12 - kBarBalandligi;
    if (i.ishHolati) {
        const D2D1_RECT_F bar = ramkaYasa(i.hudud.left + kChet, barY,
                                          i.hudud.right - i.hudud.left - kChet * 2, kBarBalandligi);
        c.toldir(bar, U::yumshoqFon, kBarBalandligi / 2);
        if (i.jami > 0) {
            const float w = (bar.right - bar.left) *
                            std::clamp(static_cast<float>(i.bajarilgan) / i.jami, 0.0f, 1.0f);
            if (w > 0)
                c.toldir(ramkaYasa(bar.left, bar.top, w, kBarBalandligi), U::kok,
                         kBarBalandligi / 2);
        }
    }
    if (!i.jarayonMatni.empty()) {
        c.matn(i.jarayonMatni, ramkaYasa(i.hudud.left + kChet, tugmalarY, 320, kTugmaBalandligi),
               U::matn2, 13.0f, Ogirlik::Oddiy, Hizalash::Chap, true);
    }
}

// ---- Tashqi kirish ---------------------------------------------------------

void TarjimaTab::matnniQabulQil(const std::wstring& matn, const std::string& tilKodi) {
    auto& i = *ichki_;

    // `Tarjimon` bir vaqtda bitta ishni koʻtaradi. Ish ketayotgan boʻlsa uni
    // bekor qilamiz va YANGISINI BOSHLAMAYMIZ — eski ishning natijasi
    // keyinroq kelib yangisining holatini buzardi (macOS bilan bir xil).
    const bool bandEdi = i.ishHolati;
    if (bandEdi) Tarjimon::birgalik().bekorQil();

    i.manba.matnQoy(matn);
    i.natija.matnQoy(L"");

    // Manba tili doim oʻzbekcha: transkript whisper'ning oʻzbek modelidan keladi.
    if (const Til* uz = Tillar::top(Tillar::kUz)) i.manbaTil.tilniQoy(*uz);
    if (!tilKodi.empty()) {
        if (const Til* t = Tillar::top(tilKodi)) i.maqsadTil.tilniQoy(*t);
    }
    i.tilniSaqla();
    i.modelHolatiniYangila();
    i.qaytaChiz();

    if (!bandEdi && !tilKodi.empty() && i.modelBor) i.tarjimaniBoshla();
}

void TarjimaTab::xabarKeldi(UINT xabar, WPARAM wp, LPARAM lp) {
    auto& i = *ichki_;

    switch (xabar) {
        case TarjimaXabar::kJarayon: {
            i.bajarilgan = static_cast<int>(wp);
            i.jami = static_cast<int>(lp);
            i.jarayonMatni =
                std::to_wstring(i.bajarilgan) + L" / " + std::to_wstring(i.jami) + L" jumla";
            i.qaytaChiz();
            return;
        }
        case TarjimaXabar::kTayyor: {
            std::unique_ptr<std::wstring> m(reinterpret_cast<std::wstring*>(lp));
            i.natija.matnQoy(*m);
            i.ishTugadi();
            return;
        }
        case TarjimaXabar::kXato: {
            std::unique_ptr<std::wstring> m(reinterpret_cast<std::wstring*>(lp));
            i.ishTugadi();
            ogohlantir(i.ota, L"Tarjima", *m);
            return;
        }
        case TarjimaXabar::kYuklash: {
            // Yuklovchi har 256 KB da xabar yuboradi; megabayt qiymati
            // oʻzgarmagan boʻlsa qayta chizishga hojat yoʻq.
            if (static_cast<long long>(wp) == i.yuklOlingan) return;
            i.yuklOlingan = static_cast<long long>(wp);
            i.yuklJami = static_cast<long long>(lp);
            i.qaytaChiz();
            return;
        }
        case TarjimaXabar::kYuklashTugadi: {
            std::unique_ptr<std::wstring> m(reinterpret_cast<std::wstring*>(lp));
            i.yuklanyapti = false;
            i.bannerTugma->nom = L"Yuklab olish";
            i.yuklXatosi = (wp == 1) ? std::wstring() : *m;
            i.modelHolatiniYangila();
            joylashtir(i.hudud);
            i.qaytaChiz();
            return;
        }
        default: return;
    }
}

}  // namespace rubai
