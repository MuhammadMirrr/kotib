// «Yozish» tabi — macOS'dagi `diktovka_view.swift` egizagi.
// Interfeys va izohlar — `yozish_tab.h`.

#include "yozish_tab.h"

#include "soragich.h"
#include "uslub.h"
#include "../core/util.h"
#include "../core/vaqt_format.h"

#include <algorithm>
#include <ctime>

namespace rubai {

namespace {

// Dizayndagi oʻlchamlar (DIP) — `diktovka_view.swift` bilan bir xil.
constexpr float kChet = 20.0f;  // tab chetidan boʻshliq
constexpr float kKartaBalandligi = 108.0f;
constexpr float kBannerBalandligi = 52.0f;
constexpr float kQatorBalandligi = 60.0f;  // macOS'dagi `rowHeight`
constexpr float kQatorOraligi = 8.0f;
constexpr float kDoiraRadius = 30.0f;

// Kontekst menyusi identifikatorlari.
// «Nusxa» bosilgach tugma 1,5 soniyaga «Nusxa olindi» boʻlib turadi
// (macOS'da ham shunday: `diktovka_view.swift` → `nusxaBosildi`). Busiz
// bosishning HECH QANDAY belgisi yoʻq edi — matn buferga tushdimi yoki
// yoʻqmi, bilib boʻlmasdi.
constexpr UINT_PTR kNusxaTaymer = 0xC0B1;
constexpr UINT kNusxaMs = 1500;

constexpr UINT kMenyuNusxa = 1;
constexpr UINT kMenyuOchir = 2;
constexpr UINT kMenyuTozala = 3;

// Yangi qator belgilarini boʻshliqqa almashtiradi — qatorda bitta satr
// koʻrinadi va «\n» oʻrniga kvadratcha chiqmaydi. Uzunligini kesish kerak
// emas: chizish qatlami uni uch nuqta bilan oʻzi kesadi (`bitta` bayrogʻi).
std::wstring birQator(const std::wstring& s) {
    std::wstring t;
    t.reserve(s.size());
    for (wchar_t ch : s) t += (ch == L'\n' || ch == L'\r') ? L' ' : ch;
    return t;
}

}  // namespace

// ---- Yozish kartasi --------------------------------------------------------
// Alohida vidjet, chunki uning ichki holati (hover, yozish, taymer) va
// chizilishi boshqa hech qayerda takrorlanmaydi.
class YozishKartasi : public Vidjet {
public:
    bool yozilyapti = false;
    std::wstring hotkey = L"Ctrl+Alt+D";
    std::function<void()> bosilganda;
    float daraja = 0.0f;           // signal darajasi 0..1
    long long boshlanganVaqt = 0;  // Unix soniya

    void chiz(Chizgich& c) override;
    bool sichqonQoyildi(D2D1_POINT_2F p) override;
    LPCWSTR kursor() const override { return IDC_HAND; }

    // Yangi signal darajasi keldi — toʻlqin chizigʻini bir qadam suramiz.
    void darajaQoshi(float d) {
        daraja = d;
        tarix_.erase(tarix_.begin());
        tarix_.push_back(d);
    }
    bool fokusOladimi() const override { return true; }
    bool klavisha(WPARAM kod) override {
        if ((kod == VK_RETURN || kod == VK_SPACE) && bosilganda) {
            bosilganda();
            return true;
        }
        return false;
    }

private:
    void doiraChiz(Chizgich& c, float cx, float cy);
    void tolqinChiz(Chizgich& c, const D2D1_RECT_F& r);
    // Toʻlqin uchun oxirgi darajalar — chapdan oʻngga suriladi.
    std::vector<float> tarix_ = std::vector<float>(24, 0.05f);
};

void YozishKartasi::doiraChiz(Chizgich& c, float cx, float cy) {
    // Yozib olishda doira ortida ochroq halqa — macOS'dagi pulsatsiya
    // qatlamining tinch varianti. Animatsiya yoʻq: u har kadrda qayta
    // chizishni talab qiladi va zaif mashinada protsessorni behuda yeydi.
    if (yozilyapti) c.doira(cx, cy, kDoiraRadius + 6, U::yozishChet);

    c.doira(cx, cy, kDoiraRadius, yozilyapti ? U::qizil : U::kok);

    if (yozilyapti) {
        // Toʻxtatish belgisi — oq kvadrat.
        c.toldir(ramkaYasa(cx - 9, cy - 9, 18, 18), U::oq, 5.0f);
        return;
    }

    // Mikrofon shakli. Ataylab shrift belgisi emas: «Segoe MDL2 Assets»
    // Windows 10 da, «Segoe Fluent Icons» Windows 11 da — ikkalasida ham
    // belgi kodi boshqa. Geometriya hech qanday shriftga bogʻliq emas.
    c.toldir(ramkaYasa(cx - 5, cy - 13, 10, 17), U::oq, 5.0f);        // kapsula
    c.chegara(ramkaYasa(cx - 9, cy - 4, 18, 13), U::oq, 6.5f, 2.0f);  // yoy
    c.toldir(ramkaYasa(cx - 1, cy + 9, 2, 5), U::oq);                 // oyoq
}

void YozishKartasi::tolqinChiz(Chizgich& c, const D2D1_RECT_F& r) {
    const float markaz = (r.top + r.bottom) / 2;
    const float kenglik = 3.0f, oraliq = 2.0f;
    const int n = static_cast<int>(tarix_.size());

    for (int i = 0; i < n; ++i) {
        const float x = r.left + static_cast<float>(i) * (kenglik + oraliq);
        if (x + kenglik > r.right) break;
        const float h = std::max(3.0f, tarix_[static_cast<size_t>(i)] * 22.0f);
        c.toldir(ramkaYasa(x, markaz - h / 2, kenglik, h), U::qizil, 1.5f);
    }
}

void YozishKartasi::chiz(Chizgich& c) {
    if (!korinadi) return;

    const bool ustida = (joriyHolat() == Holat::Ustida || joriyHolat() == Holat::Bosilgan);

    uint32_t fon, chet;
    if (yozilyapti) {
        fon = U::yozishFon;
        chet = U::yozishChet;
    } else if (ustida) {
        fon = U::kartaHover;
        chet = U::kartaHoverChet;
    } else {
        fon = U::kartaFon;
        chet = U::kartaChet;
    }

    c.toldir(ramka, fon, U::radiusKarta);
    c.chegara(ramka, chet, U::radiusKarta);
    if (fokusda) {
        c.chegara(D2D1::RectF(ramka.left - 2, ramka.top - 2, ramka.right + 2, ramka.bottom + 2),
                  U::kok, U::radiusKarta + 2, 2.0f);
    }

    const float cy = (ramka.top + ramka.bottom) / 2;
    const float cx = ramka.left + 24 + kDoiraRadius;
    doiraChiz(c, cx, cy);

    // Matnlar
    const float matnX = cx + kDoiraRadius + 20;
    // Matnlar macOS'dagi bilan AYNAN bir xil (`diktovka_view.swift`).
    const std::wstring sarlavha = yozilyapti ? L"Eshityapman… gapiring" : L"Bosing va gapiring";
    const std::wstring izoh =
        yozilyapti ? L"Tugatgach yana bosing" : L"Matn siz turgan joyga oʻzi yoziladi";

    c.matn(sarlavha, ramkaYasa(matnX, cy - 24, 300, 26), U::matn, 20.0f, Ogirlik::Yarim);
    c.matn(izoh, ramkaYasa(matnX, cy + 3, 320, 20), yozilyapti ? U::yozishMatn2 : U::matn2, 14.0f);

    // Oʻng taraf
    if (yozilyapti) {
        const D2D1_RECT_F tolqinRamka = ramkaYasa(ramka.right - 210, cy - 14, 120, 28);
        tolqinChiz(c, tolqinRamka);

        const long long otgan =
            boshlanganVaqt > 0 ? static_cast<long long>(std::time(nullptr)) - boshlanganVaqt : 0;
        wchar_t taymer[16];
        swprintf(taymer, 16, L"%lld:%02lld", otgan / 60, otgan % 60);
        c.matn(taymer, ramkaYasa(ramka.right - 76, cy - 12, 56, 24), U::qizil, 17.0f,
               Ogirlik::Yarim, Hizalash::Ong);
    } else {
        // Tugmalar «chip» koʻrinishida: Ctrl · Alt · D
        std::vector<std::wstring> bolaklar;
        std::wstring joriy;
        for (wchar_t ch : hotkey) {
            if (ch == L'+') {
                if (!joriy.empty()) bolaklar.push_back(joriy);
                joriy.clear();
            } else
                joriy += ch;
        }
        if (!joriy.empty()) bolaklar.push_back(joriy);

        float x = ramka.right - 20;
        for (auto it = bolaklar.rbegin(); it != bolaklar.rend(); ++it) {
            const float w = std::max(28.0f, c.matnOlchami(*it, 13.0f).width + 16);
            x -= w;
            const D2D1_RECT_F chip = ramkaYasa(x, cy - 13, w, 26);
            c.toldir(chip, U::oq, U::radiusKichik);
            c.chegara(chip, U::tugmaChet, U::radiusKichik);
            c.matn(*it, chip, U::matn2, 13.0f, Ogirlik::Oddiy, Hizalash::Markaz, true);
            x -= 5;
        }
    }
}

bool YozishKartasi::sichqonQoyildi(D2D1_POINT_2F p) {
    const bool ediBosilgan = bosilgan_;
    Vidjet::sichqonQoyildi(p);
    if (ediBosilgan && ichidami(ramka, p) && bosilganda) bosilganda();
    return true;
}

// ---- YozishTab -------------------------------------------------------------

struct YozishTab::Ichki {
    std::shared_ptr<YozishKartasi> karta;
    std::shared_ptr<Royxat> royxat;
    std::shared_ptr<Tugma> bannerTugmasi;

    HWND ota = nullptr;
    std::wstring bannerMatni;
    std::vector<TarixYozuvi> yozuvlar;
    D2D1_RECT_F hudud{};
    // «Nusxa olindi» qaysi qatorda koʻrsatilyapti (-1 = hech qaysi).
    int nusxaQatori = -1;

    void nusxaBelgisi(int qator);

    float bannerBalandligi() const { return bannerMatni.empty() ? 0.0f : kBannerBalandligi + 12; }
};

// Taymer qayta chaqiruvi obyektga koʻrsatkich ololmaydi, shuning uchun
// faol tab shu yerda turadi. «Yozish» tabi ilovada BITTA — `AsosiyOyna`
// uni bir marta yaratadi va oxirigacha saqlaydi.
static YozishTab::Ichki* g_nusxaKutmoqda = nullptr;

static void CALLBACK nusxaTaymeri(HWND h, UINT, UINT_PTR id, DWORD) {
    KillTimer(h, id);
    if (!g_nusxaKutmoqda) return;
    g_nusxaKutmoqda->nusxaQatori = -1;
    InvalidateRect(h, nullptr, FALSE);
    g_nusxaKutmoqda = nullptr;
}

void YozishTab::Ichki::nusxaBelgisi(int qator) {
    if (!ota) return;
    nusxaQatori = qator;
    g_nusxaKutmoqda = this;
    SetTimer(ota, kNusxaTaymer, kNusxaMs, nusxaTaymeri);
    InvalidateRect(ota, nullptr, FALSE);
}

YozishTab::YozishTab() : ichki_(std::make_unique<Ichki>()) {
    ichki_->karta = std::make_shared<YozishKartasi>();
    ichki_->karta->bosilganda = [this] {
        if (onDiktovka) onDiktovka();
    };
    vidjetlar.qosh(ichki_->karta);

    ichki_->bannerTugmasi = std::make_shared<Tugma>(L"", Tugma::Kor::Ikkilamchi);
    ichki_->bannerTugmasi->korinadi = false;
    ichki_->bannerTugmasi->shriftOlchami = 13.0f;
    vidjetlar.qosh(ichki_->bannerTugmasi);

    auto r = std::make_shared<Royxat>();
    r->qatorlarSoni = [this] { return static_cast<int>(ichki_->yozuvlar.size()); };
    r->qatorBalandligi = [](int) { return kQatorBalandligi + kQatorOraligi; };
    // Qator dizayni macOS'dagi bilan bir xil: matn (16), ostida nisbiy vaqt
    // (13), oʻngda «Nusxa» tugmasi. Tugma ATAYLAB chizilgan koʻrinish —
    // butun qator bosilsa ham nusxa olinadi, tugma esa nima boʻlishini
    // koʻrsatib turadi.
    r->qatorChiz = [this](Chizgich& c, int i, const D2D1_RECT_F& qr, bool ustida) {
        const auto& y = ichki_->yozuvlar[static_cast<size_t>(i)];
        const D2D1_RECT_F ich = D2D1::RectF(qr.left, qr.top, qr.right, qr.bottom - kQatorOraligi);

        c.toldir(ich, ustida ? U::qatorHover : U::oq, U::radiusQator);
        c.chegara(ich, U::qatorChiziq, U::radiusQator);

        // Nusxa olingan qatorda tugma kengayadi — «Nusxa olindi» sigʻsin.
        const bool olindi = (i == ichki_->nusxaQatori);
        const float tugmaKengligi = olindi ? 118.0f : 80.0f;
        const D2D1_RECT_F tugma = ramkaYasa(ich.right - 15 - tugmaKengligi,
                                            (ich.top + ich.bottom) / 2 - 15, tugmaKengligi, 30);
        c.toldir(tugma, U::oq, U::radiusTugma);
        c.chegara(tugma, U::tugmaChet, U::radiusTugma);
        c.matn(olindi ? L"Nusxa olindi" : L"Nusxa", tugma, U::matn, 14.0f, Ogirlik::Oddiy,
               Hizalash::Markaz, true);

        const float matnKengligi = tugma.left - 14 - (ich.left + 15);
        c.matn(birQator(y.matn), ramkaYasa(ich.left + 15, ich.top + 13, matnKengligi, 22), U::matn,
               16.0f, Ogirlik::Oddiy, Hizalash::Chap, false, true);
        c.matn(VaqtFormat::nisbiy(y.vaqt), ramkaYasa(ich.left + 15, ich.top + 38, matnKengligi, 18),
               U::matn3, 13.0f);
    };
    r->qatorBosildi = [this](int i) {
        if (onNusxa && i >= 0 && i < static_cast<int>(ichki_->yozuvlar.size())) {
            onNusxa(ichki_->yozuvlar[static_cast<size_t>(i)].matn);
            ichki_->nusxaBelgisi(i);
        }
    };
    // Kontekst menyusi — dizaynda koʻrinmaydigan, lekin zarur amallar
    // (macOS'dagi `tarixMenyusi` ning aynan oʻzi).
    r->qatorOngBosildi = [this](int i, D2D1_POINT_2F p) {
        if (i < 0 || i >= static_cast<int>(ichki_->yozuvlar.size())) return;
        if (!ichki_->ota) return;

        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, kMenyuNusxa, L"Nusxa olish");
        AppendMenuW(m, MF_STRING, kMenyuOchir, L"Oʻchirish");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, kMenyuTozala, L"Tarixni tozalash…");

        const float k = static_cast<float>(GetDpiForWindow(ichki_->ota)) / 96.0f;
        POINT ekran{static_cast<LONG>(p.x * k), static_cast<LONG>(p.y * k)};
        ClientToScreen(ichki_->ota, &ekran);
        const UINT tanlov = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, ekran.x, ekran.y, 0,
                                           ichki_->ota, nullptr);
        DestroyMenu(m);

        const auto& y = ichki_->yozuvlar[static_cast<size_t>(i)];
        switch (tanlov) {
            case kMenyuNusxa:
                if (onNusxa) {
                    onNusxa(y.matn);
                    ichki_->nusxaBelgisi(i);
                }
                break;
            case kMenyuOchir:
                if (onOchir) onOchir(y.id);
                break;
            case kMenyuTozala: {
                const int javob = MessageBoxW(
                    ichki_->ota, L"Barcha yozuvlar oʻchiriladi. Bu amalni qaytarib boʻlmaydi.",
                    L"Tarixni tozalash", MB_OKCANCEL | MB_ICONWARNING);
                if (javob == IDOK && onTozala) onTozala();
                break;
            }
            default: break;
        }
    };
    ichki_->royxat = r;
    vidjetlar.qosh(r);
}

YozishTab::~YozishTab() = default;

void YozishTab::joylashtir(const D2D1_RECT_F& hudud) {
    ichki_->hudud = hudud;

    float y = hudud.top + kChet;
    const float kenglik = hudud.right - hudud.left - kChet * 2;

    if (!ichki_->bannerMatni.empty()) {
        // Banner tugmasi bannerning oʻng chetida.
        ichki_->bannerTugmasi->ramka = ramkaYasa(hudud.right - kChet - 110, y + 10, 110, 32);
        y += ichki_->bannerBalandligi();
    }

    ichki_->karta->ramka = ramkaYasa(hudud.left + kChet, y, kenglik, kKartaBalandligi);
    y += kKartaBalandligi + 24;

    // «Oxirgi yozuvlar» sarlavhasi uchun joy — u `chiz` da chiziladi.
    y += 26;

    ichki_->royxat->ramka =
        D2D1::RectF(hudud.left + kChet, y, hudud.right - kChet, hudud.bottom - kChet);
    ichki_->royxat->yangilandi();
}

void YozishTab::chiz(Chizgich& c) {
    // Banner
    if (!ichki_->bannerMatni.empty()) {
        const D2D1_RECT_F b =
            ramkaYasa(ichki_->hudud.left + kChet, ichki_->hudud.top + kChet,
                      ichki_->hudud.right - ichki_->hudud.left - kChet * 2, kBannerBalandligi);
        c.toldir(b, U::bannerFon, U::radiusTugma);
        c.chegara(b, U::bannerChet, U::radiusTugma);
        c.matn(ichki_->bannerMatni,
               ramkaYasa(b.left + 16, b.top, b.right - b.left - 140, kBannerBalandligi),
               U::bannerMatn, 13.0f, Ogirlik::Oddiy, Hizalash::Chap, true);
    }

    // Roʻyxat sarlavhasi
    const float sarlavhaY = ichki_->royxat->ramka.top - 24;
    c.matn(L"Oxirgi yozuvlar", ramkaYasa(ichki_->hudud.left + kChet, sarlavhaY, 220, 20), U::matn2,
           12.0f, Ogirlik::Yarim);

    // Boʻsh holat
    if (ichki_->yozuvlar.empty()) {
        const auto& r = ichki_->royxat->ramka;
        c.matn(L"Hali hech narsa yozilmagan", D2D1::RectF(r.left, r.top + 30, r.right, r.top + 60),
               U::matn3, 15.0f, Ogirlik::Oddiy, Hizalash::Markaz);
    }
}

void YozishTab::holatniQoy(bool yozilyapti) {
    if (ichki_->karta->yozilyapti == yozilyapti) return;
    ichki_->karta->yozilyapti = yozilyapti;
    ichki_->karta->boshlanganVaqt = yozilyapti ? static_cast<long long>(std::time(nullptr)) : 0;
}

void YozishTab::hotkeyMatni(const std::wstring& matn) { ichki_->karta->hotkey = matn; }

void YozishTab::banner(const std::wstring& matn, const std::wstring& tugmaNomi,
                       std::function<void()> tugmaBosildi) {
    ichki_->bannerMatni = matn;
    ichki_->bannerTugmasi->korinadi = !matn.empty() && !tugmaNomi.empty();
    ichki_->bannerTugmasi->nom = tugmaNomi;
    ichki_->bannerTugmasi->bosilganda = std::move(tugmaBosildi);
    joylashtir(ichki_->hudud);
}

void YozishTab::tarixniQoy(std::vector<TarixYozuvi> yozuvlar) {
    ichki_->yozuvlar = std::move(yozuvlar);
    ichki_->royxat->yangilandi();
}

void YozishTab::otaOyna(HWND h) { ichki_->ota = h; }

void YozishTab::darajaniQoy(float daraja) {
    ichki_->karta->darajaQoshi(std::clamp(daraja, 0.0f, 1.0f));
}

}  // namespace rubai
