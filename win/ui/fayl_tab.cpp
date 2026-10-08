// «Fayl» tabi (Studiya) — macOS'dagi `studiya_view.swift` egizagi.
// Interfeys va izohlar — `fayl_tab.h`.

#include "fayl_tab.h"

#include "matn_maydon.h"
#include "soragich.h"
#include "uslub.h"
#include "vidjet.h"
#include "../core/amallar.h"
#include "../core/engine.h"
#include "../core/ish.h"
#include "../core/config.h"
#include "../core/llm.h"
#include "../core/tillar.h"
#include "../core/util.h"
#include "../core/vaqt_format.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace rubai {

namespace {

// Dizayndagi oʻlchamlar (DIP) — `studiya_view.swift` bilan bir xil.
constexpr float kChet = 28.0f;
constexpr float kZonaBalandligi = 236.0f;
constexpr float kQatorBalandligi = 60.0f;
constexpr float kQatorOraligi = 8.0f;
constexpr float kFooterBalandligi = 76.0f;
constexpr float kSarlavhaBalandligi = 52.0f;
constexpr float kProgressBalandligi = 48.0f;

// Menyu identifikatorlari. TrackPopupMenu TPM_RETURNCMD bilan shuni qaytaradi,
// shuning uchun WM_COMMAND yoʻnaltirish kerak emas.
constexpr UINT kMenyuErkin = 1000;
constexpr UINT kMenyuBoshqaTil = 1001;
constexpr UINT kMenyuChiroyli = 1002;
constexpr UINT kMenyuXom = 1003;
constexpr UINT kMenyuNusxa = 1004;
constexpr UINT kMenyuAmalBoshi = 1100;  // + amal indeksi
constexpr UINT kMenyuTilBoshi = 1200;   // + til indeksi

// Fayl nomidan kengaytmani olib tashlaydi — «Saqlash…» tavsiya nomi uchun.
std::wstring kengaytmasiz(const std::wstring& nom) {
    const size_t nuqta = nom.find_last_of(L'.');
    return (nuqta == std::wstring::npos) ? nom : nom.substr(0, nuqta);
}

// Menyuni berilgan tugmaning TEPASIDA ochadi: tugma oynaning pastki chetida
// turibdi va pastga ochilsa ekrandan chiqib ketardi (macOS'da ham shunday).
UINT menyuniOch(HWND ota, HMENU menyu, const D2D1_RECT_F& tugma, float k) {
    POINT p{static_cast<LONG>(tugma.left * k), static_cast<LONG>(tugma.top * k)};
    ClientToScreen(ota, &p);
    return TrackPopupMenu(menyu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN, p.x, p.y - 6, 0,
                          ota, nullptr);
}

}  // namespace

// ---- Tashlash zonasi -------------------------------------------------------
// Dizayndagi katta punktir ramka: hujjat ikonkasi, sarlavha va «Fayl tanlash».
// Butun zona bosiladi — foydalanuvchi tugmani izlab oʻtirmasin.
class TashlashZonasi : public Vidjet {
public:
    std::function<void()> bosilganda;
    bool sudralyapti = false;  // ustiga fayl sudrab kelingan

    void chiz(Chizgich& c) override;
    bool sichqonQoyildi(D2D1_POINT_2F p) override;
    LPCWSTR kursor() const override { return IDC_HAND; }
    bool fokusOladimi() const override { return true; }
    bool klavisha(WPARAM kod) override {
        if ((kod == VK_RETURN || kod == VK_SPACE) && bosilganda) {
            bosilganda();
            return true;
        }
        return false;
    }

private:
    void ikonkaChiz(Chizgich& c, float cx, float ust);
};

// 46×58 hujjat ikonkasi — ichida uchta matn chizigʻi (uchinchisi kaltaroq).
// Shrift belgisi EMAS: «Segoe MDL2 Assets» va «Segoe Fluent Icons» da belgi
// kodlari boshqa-boshqa, geometriya esa hech qanday shriftga bogʻliq emas.
void TashlashZonasi::ikonkaChiz(Chizgich& c, float cx, float ust) {
    const D2D1_RECT_F ramkaIkon = ramkaYasa(cx - 23, ust, 46, 58);
    c.chegara(ramkaIkon, U::matn4, 7.0f, 2.0f);

    float y = ust + 18;
    const float nisbatlar[3] = {1.0f, 1.0f, 0.6f};
    for (float n : nisbatlar) {
        c.toldir(ramkaYasa(ramkaIkon.left + 10, y, 26 * n, 2), 0xC4C4CA, 1.0f);
        y += 9;
    }
}

void TashlashZonasi::chiz(Chizgich& c) {
    if (!korinadi) return;

    const bool ustida = (joriyHolat() == Holat::Ustida || joriyHolat() == Holat::Bosilgan);

    if (sudralyapti) {
        // Fayl ustiga sudralganda — koʻk yorugʻlik va qalinroq ramka
        // (macOS'dagi `DropView.draw` bilan bir xil).
        c.toldir(ramka, U::kartaHover, 14.0f);
        c.chegara(ramka, U::kok, 14.0f, 3.0f);
    } else {
        c.toldir(ramka, ustida ? U::kartaHover : U::maydonFon, 14.0f);
        c.punktirChegara(ramka, ustida ? U::kartaHoverChet : U::punktir, 14.0f);
    }

    if (fokusda) {
        c.chegara(D2D1::RectF(ramka.left - 2, ramka.top - 2, ramka.right + 2, ramka.bottom + 2),
                  U::kok, 16.0f, 2.0f);
    }

    // Ikonka + sarlavha + tugma — vertikal markazda, 16 DIP oraliq bilan.
    const float cx = (ramka.left + ramka.right) / 2;
    const float cy = (ramka.top + ramka.bottom) / 2;
    const float jamiBalandlik = 58 + 16 + 26 + 16 + 46;
    float y = cy - jamiBalandlik / 2;

    ikonkaChiz(c, cx, y);
    y += 58 + 16;

    c.matn(L"Ovozli faylni shu yerga tashlang", D2D1::RectF(ramka.left, y, ramka.right, y + 28),
           U::matn, 20.0f, Ogirlik::Yarim, Hizalash::Markaz);
    y += 26 + 16;

    const D2D1_RECT_F tugma = ramkaYasa(cx - 70, y, 140, 46);
    c.toldir(tugma, bosilgan_ ? U::kokBosilgan : U::kok, U::radiusTugma);
    c.matn(L"Fayl tanlash", tugma, U::oq, 16.0f, Ogirlik::Yarim, Hizalash::Markaz, true);
}

bool TashlashZonasi::sichqonQoyildi(D2D1_POINT_2F p) {
    const bool ediBosilgan = bosilgan_;
    Vidjet::sichqonQoyildi(p);
    if (ediBosilgan && ichidami(ramka, p) && bosilganda) bosilganda();
    return true;
}

// ---- Ichki holat -----------------------------------------------------------

struct FaylTab::Ichki {
    HWND ota = nullptr;
    float dpi = 1.0f;
    D2D1_RECT_F hudud{};

    // 1-koʻrinish
    std::shared_ptr<TashlashZonasi> zona;
    std::shared_ptr<Royxat> royxat;
    std::vector<Hujjat> hujjatlar;

    // 2-koʻrinish
    MatnMaydon maydon;
    std::shared_ptr<Tugma> orqaga;
    std::shared_ptr<Tugma> nusxa;
    std::shared_ptr<Tugma> saqla;
    std::shared_ptr<Tugma> tarjima;
    std::shared_ptr<Tugma> yaxshilash;

    // Progress paneli (ikkala koʻrinish ustida)
    std::shared_ptr<Tugma> bekor;
    bool progressKorinadi = false;
    double progress = 0;
    bool progressAniqmas = false;  // LLM amali — foiz yoʻq
    std::wstring holatMatni;

    bool matnKorinishi = false;
    // Tabning OʻZI koʻrinyaptimi. Matn maydoni — bola oyna: u tabdan
    // mustaqil va uni «koʻrsat» desak, foydalanuvchi boshqa tabda turgan
    // boʻlsa ham ekranda paydo boʻladi. Transkripsiya tugaganda aynan shu
    // holat yuzaga keladi.
    bool tabKorinadi = false;
    bool joriyBor = false;
    Hujjat joriy;

    // Matn maydonida hozir nima turibdi.
    MatnTuri korinayotganTur = MatnTuri::Chiroyli;
    bool natijaKorinyapti = false;  // LLM natijasi — diskka saqlanmaydi

    std::unique_ptr<TranskripsiyaIshi> ish;
    AmalIshi amalIshi;
    bool bandmi = false;

    // Har bir LLM oqimini oʻzi boshlangan hujjatga bogʻlaydi: koʻrinish
    // almashganda token oshadi va eski oqimning deltalari yangi koʻrinishga
    // yozilmaydi (macOS'dagi `oqimToken` bilan bir xil).
    unsigned oqimToken = 0;

    // «Tarjima qilish» tugmasi natijani tashqariga uzatadi. Ichki tuzilma
    // FaylTab'ning ommaviy maydoniga toʻgʻridan-toʻgʻri tegmasin.
    std::function<void(const std::wstring&, const std::string&)> onTarjimaUzat;

    void qaytaChiz() {
        if (ota) InvalidateRect(ota, nullptr, FALSE);
    }

    void matnniKorsat(bool v);
    void kutubxonaniYangila();
    void hujjatniOch(const Hujjat& h);
    void royxatgaQayt();
    void matnniOqi(MatnTuri tur);
    void tugmalarniYangila();
    void progressniKorsat(bool v);
    void ishTugadi();

    void yaxshilashMenyusi();
    void tarjimaMenyusi();
    void matnMenyusi(POINT ekran);
    void amalniBajar(const Amal& amal);
    void eksport();
    void nusxaOl();
};

// ---- Koʻrinish almashuvi ---------------------------------------------------

void FaylTab::Ichki::matnniKorsat(bool v) {
    matnKorinishi = v;

    zona->korinadi = !v;
    royxat->korinadi = !v;
    orqaga->korinadi = v;
    nusxa->korinadi = v;
    saqla->korinadi = v;
    tarjima->korinadi = v;
    // «Matnni yaxshilash» LLM sozlanmagan boʻlsa umuman koʻrinmaydi.
    yaxshilash->korinadi = v && LLMSozlama::sozlanganmi();
    maydon.korsat(v && tabKorinadi);
}

void FaylTab::Ichki::kutubxonaniYangila() {
    hujjatlar = HujjatOmbori::royxat();
    royxat->yangilandi();
}

void FaylTab::Ichki::hujjatniOch(const Hujjat& h) {
    ++oqimToken;
    joriy = h;
    joriyBor = true;
    natijaKorinyapti = false;
    korinayotganTur = MatnTuri::Chiroyli;
    matnniOqi(MatnTuri::Chiroyli);
    matnniKorsat(true);
    tugmalarniYangila();
}

void FaylTab::Ichki::royxatgaQayt() {
    // Oqim ketayotgan boʻlsa toʻxtatmaymiz — u oʻz hujjatiga saqlanadi.
    matnniKorsat(false);
    kutubxonaniYangila();
}

void FaylTab::Ichki::matnniOqi(MatnTuri tur) {
    if (!joriyBor) return;
    korinayotganTur = tur;
    natijaKorinyapti = false;
    maydon.matnQoy(HujjatOmbori::matnOqi(joriy.id, tur));
}

void FaylTab::Ichki::tugmalarniYangila() {
    const bool bor = joriyBor && !bandmi;
    nusxa->yoqilgan = bor;
    saqla->yoqilgan = bor;
    tarjima->yoqilgan = bor;
    yaxshilash->yoqilgan = bor;
    if (matnKorinishi) yaxshilash->korinadi = LLMSozlama::sozlanganmi();
}

void FaylTab::Ichki::progressniKorsat(bool v) {
    progressKorinadi = v;
    bekor->korinadi = v;
}

void FaylTab::Ichki::ishTugadi() {
    bandmi = false;
    progressAniqmas = false;
    progressniKorsat(false);
    tugmalarniYangila();
}

// ---- Menyular --------------------------------------------------------------

void FaylTab::Ichki::yaxshilashMenyusi() {
    HMENU m = CreatePopupMenu();
    const auto& royxatAmallar = amallar();
    for (size_t i = 0; i < royxatAmallar.size(); ++i) {
        AppendMenuW(m, MF_STRING, kMenyuAmalBoshi + static_cast<UINT>(i),
                    royxatAmallar[i].nom.c_str());
    }
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kMenyuErkin, L"Oʻz soʻrovim…");

    const UINT tanlov = menyuniOch(ota, m, yaxshilash->ramka, dpi);
    DestroyMenu(m);

    if (tanlov == 0) return;
    if (tanlov == kMenyuErkin) {
        const std::wstring korsatma = matnSora(ota, L"Oʻz soʻrovingiz",
                                               L"Matn ustida nima qilish kerakligini yozing.\n"
                                               L"Masalan: inglizchaga tarjima qil");
        if (korsatma.empty()) return;
        amalniBajar(erkinAmal(korsatma));
        return;
    }
    const size_t i = tanlov - kMenyuAmalBoshi;
    if (i < royxatAmallar.size()) amalniBajar(royxatAmallar[i]);
}

void FaylTab::Ichki::tarjimaMenyusi() {
    // Tepada «Tarjima» tabida oxirgi ishlatilgan til, keyin eng koʻp
    // soʻraladigan uchtasi, oxirida toʻliq tanlagichga oʻtish. 202 tani
    // menyuga solish oʻrniga «Tarjima» tabidagi qidiruvli roʻyxat
    // ishlatiladi — u allaqachon oʻsha yerda (macOS bilan bir xil qaror).
    //
    // Nomlar `tillar.cpp` dan olinadi, qoʻlda yozilmaydi: aks holda menyuda
    // «Rus tili», roʻyxatda «Ruscha» turib, ikkitasi boshqa narsadek
    // koʻrinardi.
    std::vector<std::string> kodlar;
    auto qosh = [&](const std::string& kod) {
        // Oʻzbekchaga tarjima qilishning maʼnosi yoʻq — manba oʻzbekcha.
        if (kod.empty() || kod == Tillar::kUz) return;
        if (std::find(kodlar.begin(), kodlar.end(), kod) != kodlar.end()) return;
        if (!Tillar::top(kod)) return;
        kodlar.push_back(kod);
    };

    qosh(loadSettings().tarjimaMaqsad);
    qosh(Tillar::kRu);
    qosh(Tillar::kEn);
    qosh("tur_Latn");

    HMENU m = CreatePopupMenu();
    for (size_t i = 0; i < kodlar.size(); ++i) {
        const Til* t = Tillar::top(kodlar[i]);
        AppendMenuW(m, MF_STRING, kMenyuTilBoshi + static_cast<UINT>(i), t->nom.c_str());
    }
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kMenyuBoshqaTil, L"Boshqa til…");

    const UINT tanlov = menyuniOch(ota, m, tarjima->ramka, dpi);
    DestroyMenu(m);
    if (tanlov == 0) return;

    const std::wstring matn = maydon.matn();
    if (matn.find_first_not_of(L" \t\r\n") == std::wstring::npos) return;

    std::string kod;
    if (tanlov != kMenyuBoshqaTil) {
        const size_t i = tanlov - kMenyuTilBoshi;
        if (i < kodlar.size()) kod = kodlar[i];
    }
    if (onTarjimaUzat) onTarjimaUzat(matn, kod);
}

void FaylTab::Ichki::matnMenyusi(POINT ekran) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m,
                MF_STRING |
                    (!natijaKorinyapti && korinayotganTur == MatnTuri::Chiroyli ? MF_CHECKED
                                                                                : MF_UNCHECKED),
                kMenyuChiroyli, L"Tayyor matn");
    AppendMenuW(m,
                MF_STRING | (!natijaKorinyapti && korinayotganTur == MatnTuri::Xom ? MF_CHECKED
                                                                                   : MF_UNCHECKED),
                kMenyuXom, L"Asl (tahrirsiz) matn");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kMenyuNusxa, L"Nusxa olish");

    const UINT tanlov =
        TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, ekran.x, ekran.y, 0, ota, nullptr);
    DestroyMenu(m);

    switch (tanlov) {
        case kMenyuChiroyli:
            if (joriyBor) {
                ++oqimToken;
                matnniOqi(MatnTuri::Chiroyli);
            }
            break;
        case kMenyuXom:
            if (joriyBor) {
                ++oqimToken;
                matnniOqi(MatnTuri::Xom);
            }
            break;
        case kMenyuNusxa: nusxaOl(); break;
        default: break;
    }
}

// ---- LLM amali -------------------------------------------------------------

void FaylTab::Ichki::amalniBajar(const Amal& amal) {
    if (bandmi || !joriyBor) return;

    const std::wstring manba = HujjatOmbori::matnOqi(joriy.id, MatnTuri::Chiroyli);
    bandmi = true;

    // Bu oqimni AYNAN shu hujjatga bogʻlaymiz: koʻrinish almashsa token
    // oshadi va kechikkan delta yangi hujjat ustiga yozilmaydi.
    ++oqimToken;
    const unsigned token = oqimToken;
    const std::wstring docId = joriy.id;
    const std::string amalId = amal.id;

    tugmalarniYangila();
    progressAniqmas = true;
    progress = 0;
    holatMatni = amal.nom + L"…";
    progressniKorsat(true);

    // Natija shu maydonga oqib tushadi.
    natijaKorinyapti = true;
    maydon.matnQoy(L"");

    HWND oyna = ota;
    amalIshi.bajar(
        amal, manba,
        [oyna, token](const std::wstring& d) {
            PostMessageW(oyna, FaylXabar::kDelta, token,
                         reinterpret_cast<LPARAM>(new std::wstring(d)));
        },
        [oyna, token, docId, amalId](const std::wstring& natija) {
            // Natija OʻZ hujjatiga saqlanadi — foydalanuvchi boshqasiga
            // oʻtib ketgan boʻlsa ham (tokenga qaramasdan).
            HujjatOmbori::natijaSaqla(docId, toWide(amalId), natija);
            PostMessageW(oyna, FaylXabar::kAmalTayyor, token,
                         reinterpret_cast<LPARAM>(new std::wstring(natija)));
        },
        [oyna, token](const std::wstring& xato) {
            PostMessageW(oyna, FaylXabar::kAmalXato, token,
                         reinterpret_cast<LPARAM>(new std::wstring(xato)));
        });

    qaytaChiz();
}

// ---- Eksport va nusxa ------------------------------------------------------

void FaylTab::Ichki::eksport() {
    if (!joriyBor) return;
    const std::wstring yol = saqlashSora(ota, kengaytmasiz(joriy.manbaNomi) + L".txt");
    if (yol.empty()) return;

    const std::string utf8 = toUtf8(maydon.matn());
    HANDLE f = CreateFileW(yol.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        ogohlantir(ota, L"Xato", L"Faylni saqlab boʻlmadi.");
        return;
    }
    DWORD yozildi = 0;
    WriteFile(f, utf8.data(), static_cast<DWORD>(utf8.size()), &yozildi, nullptr);
    CloseHandle(f);
}

void FaylTab::Ichki::nusxaOl() { buferGaQoy(ota, maydon.matn()); }

// ---- FaylTab ---------------------------------------------------------------

FaylTab::FaylTab(HWND ota) : ichki_(std::make_unique<Ichki>()) {
    ichki_->ota = ota;

    // 1-koʻrinish
    ichki_->zona = std::make_shared<TashlashZonasi>();
    ichki_->zona->bosilganda = [this] {
        const std::wstring yol = faylSora(ichki_->ota);
        if (!yol.empty()) faylniQabulQil(yol);
    };
    vidjetlar.qosh(ichki_->zona);

    auto r = std::make_shared<Royxat>();
    r->qatorlarSoni = [this] { return static_cast<int>(ichki_->hujjatlar.size()); };
    r->qatorBalandligi = [](int) { return kQatorBalandligi + kQatorOraligi; };
    r->qatorChiz = [this](Chizgich& c, int i, const D2D1_RECT_F& qr, bool ustida) {
        const auto& h = ichki_->hujjatlar[static_cast<size_t>(i)];
        const D2D1_RECT_F ich = D2D1::RectF(qr.left, qr.top, qr.right, qr.bottom - kQatorOraligi);

        c.toldir(ich, ustida ? U::qatorHover : U::panel, 10.0f);

        c.matn(h.manbaNomi, ramkaYasa(ich.left + 16, ich.top + 12, ich.right - ich.left - 50, 22),
               U::matn, 16.0f, Ogirlik::Oddiy, Hizalash::Chap, false, true);
        c.matn(VaqtFormat::davomiylik(h.davomiylik) + L" · " + VaqtFormat::kun(h.yaratilgan),
               ramkaYasa(ich.left + 16, ich.top + 34, ich.right - ich.left - 50, 20), U::matn2,
               14.0f);
        c.matn(L"›", ramkaYasa(ich.right - 30, ich.top, 20, ich.bottom - ich.top), U::matn4, 20.0f,
               Ogirlik::Oddiy, Hizalash::Markaz, true);
    };
    r->qatorBosildi = [this](int i) {
        if (i >= 0 && i < static_cast<int>(ichki_->hujjatlar.size())) {
            ichki_->hujjatniOch(ichki_->hujjatlar[static_cast<size_t>(i)]);
        }
    };
    ichki_->royxat = r;
    vidjetlar.qosh(r);

    // 2-koʻrinish
    ichki_->orqaga = std::make_shared<Tugma>(L"‹ Audio", Tugma::Kor::Matnli);
    ichki_->orqaga->shriftOlchami = 16.0f;
    ichki_->orqaga->bosilganda = [this] { ichki_->royxatgaQayt(); };
    vidjetlar.qosh(ichki_->orqaga);

    ichki_->nusxa = std::make_shared<Tugma>(L"Nusxa olish", Tugma::Kor::Asosiy);
    ichki_->nusxa->shriftOlchami = 16.0f;
    ichki_->nusxa->bosilganda = [this] { ichki_->nusxaOl(); };
    vidjetlar.qosh(ichki_->nusxa);

    ichki_->saqla = std::make_shared<Tugma>(L"Saqlash…", Tugma::Kor::Ikkilamchi);
    ichki_->saqla->shriftOlchami = 16.0f;
    ichki_->saqla->bosilganda = [this] { ichki_->eksport(); };
    vidjetlar.qosh(ichki_->saqla);

    ichki_->tarjima = std::make_shared<Tugma>(L"Tarjima qilish", Tugma::Kor::Ikkilamchi);
    ichki_->tarjima->shriftOlchami = 16.0f;
    ichki_->tarjima->chevron = true;
    ichki_->tarjima->bosilganda = [this] { ichki_->tarjimaMenyusi(); };
    vidjetlar.qosh(ichki_->tarjima);

    ichki_->yaxshilash = std::make_shared<Tugma>(L"Matnni yaxshilash", Tugma::Kor::Ikkilamchi);
    ichki_->yaxshilash->shriftOlchami = 16.0f;
    ichki_->yaxshilash->chevron = true;
    ichki_->yaxshilash->bosilganda = [this] { ichki_->yaxshilashMenyusi(); };
    vidjetlar.qosh(ichki_->yaxshilash);

    ichki_->bekor = std::make_shared<Tugma>(L"Bekor qilish", Tugma::Kor::Ikkilamchi);
    ichki_->bekor->shriftOlchami = 13.0f;
    ichki_->bekor->bosilganda = [this] {
        if (ichki_->ish)
            ichki_->ish->bekorQil();
        else
            ichki_->amalIshi.bekorQil();
        ichki_->holatMatni = L"Bekor qilinmoqda…";
        ichki_->qaytaChiz();
    };
    vidjetlar.qosh(ichki_->bekor);

    ichki_->maydon.qur(ota, 17.0f);
    ichki_->maydon.onOzgardi = [this] { matnOzgardi(); };
    ichki_->maydon.onOngTugma = [this](POINT p) { ichki_->matnMenyusi(p); };

    ichki_->onTarjimaUzat = [this](const std::wstring& m, const std::string& kod) {
        if (onTarjima) onTarjima(m, kod);
    };

    ichki_->matnniKorsat(false);
    ichki_->progressniKorsat(false);
    ichki_->kutubxonaniYangila();
}

FaylTab::~FaylTab() = default;

void FaylTab::dpiNisbat(float k) { ichki_->dpi = k; }

void FaylTab::korinishOzgardi(bool korinadi) {
    // Matn maydoni bola oyna — tab yashirilganda u oʻz-oʻzidan yoʻqolmaydi.
    ichki_->tabKorinadi = korinadi;
    ichki_->maydon.korsat(korinadi && ichki_->matnKorinishi);
}

void FaylTab::faollashdi() {
    ichki_->kutubxonaniYangila();
    // LLM sozlamasi Sozlamalar oynasida oʻzgargan boʻlishi mumkin.
    ichki_->tugmalarniYangila();
}

// ---- Joylashtirish ---------------------------------------------------------

void FaylTab::joylashtir(const D2D1_RECT_F& hudud) {
    auto& i = *ichki_;
    i.hudud = hudud;

    const float pastki = hudud.bottom - (i.progressKorinadi ? kProgressBalandligi : 0.0f);

    // ---- Roʻyxat koʻrinishi ----
    i.zona->ramka = ramkaYasa(hudud.left + kChet, hudud.top + kChet,
                              hudud.right - hudud.left - kChet * 2, kZonaBalandligi);

    const float royxatUsti = i.zona->ramka.bottom + 22 + 20 + 10;
    i.royxat->ramka = D2D1::RectF(hudud.left + kChet, royxatUsti, hudud.right - kChet, pastki - 24);
    i.royxat->yangilandi();

    // ---- Matn koʻrinishi ----
    i.orqaga->ramka = ramkaYasa(hudud.left + 16, hudud.top + 11, 76, 30);

    const float footerUsti = pastki - kFooterBalandligi;
    i.maydon.joylashtir(
        D2D1::RectF(hudud.left, hudud.top + kSarlavhaBalandligi, hudud.right, footerUsti), i.dpi);

    const float tugmaY = footerUsti + (kFooterBalandligi - 44) / 2;
    float x = hudud.left + 20;
    i.nusxa->ramka = ramkaYasa(x, tugmaY, 130, 44);
    x += 130 + 10;
    i.saqla->ramka = ramkaYasa(x, tugmaY, 110, 44);
    x += 110 + 10;
    i.tarjima->ramka = ramkaYasa(x, tugmaY, 160, 44);

    i.yaxshilash->ramka = ramkaYasa(hudud.right - 20 - 190, tugmaY, 190, 44);

    // ---- Progress paneli ----
    i.bekor->ramka =
        ramkaYasa(hudud.right - 20 - 110, pastki + (kProgressBalandligi - 30) / 2, 110, 30);
}

// ---- Chizish ---------------------------------------------------------------

void FaylTab::chiz(Chizgich& c) {
    auto& i = *ichki_;

    if (!i.matnKorinishi) {
        c.matn(L"Oxirgi fayllar",
               ramkaYasa(i.hudud.left + kChet, i.royxat->ramka.top - 30, 260, 22), U::matn2, 13.0f,
               Ogirlik::Yarim);

        if (i.hujjatlar.empty()) {
            c.matn(L"Hali birorta fayl oʻgirilmagan",
                   D2D1::RectF(i.royxat->ramka.left, i.royxat->ramka.top + 32,
                               i.royxat->ramka.right, i.royxat->ramka.top + 62),
                   U::matn3, 15.0f, Ogirlik::Oddiy, Hizalash::Markaz);
        }
    } else {
        // Sarlavha: «‹ Audio» tugmasi (vidjet) va fayl nomi.
        c.matn(i.joriyBor ? i.joriy.manbaNomi : L"",
               ramkaYasa(i.orqaga->ramka.right + 12, i.hudud.top + 14,
                         i.hudud.right - i.orqaga->ramka.right - 32, 24),
               U::matn, 16.0f, Ogirlik::Yarim);

        const float y = i.hudud.top + kSarlavhaBalandligi;
        c.chiziq(i.hudud.left, y, i.hudud.right, y, U::ajratgich);

        const float pastki = i.hudud.bottom - (i.progressKorinadi ? kProgressBalandligi : 0.0f);
        const float footerUsti = pastki - kFooterBalandligi;
        c.toldir(
            D2D1::RectF(i.hudud.left, footerUsti, i.hudud.right, footerUsti + kFooterBalandligi),
            U::maydonFon);
        c.chiziq(i.hudud.left, footerUsti, i.hudud.right, footerUsti, U::ajratgich);
    }

    // Progress paneli — ikkala koʻrinish ustida.
    if (i.progressKorinadi) {
        const float ust = i.hudud.bottom - kProgressBalandligi;
        c.toldir(D2D1::RectF(i.hudud.left, ust, i.hudud.right, i.hudud.bottom), U::panel);
        c.chiziq(i.hudud.left, ust, i.hudud.right, ust, U::ajratgich);

        const D2D1_RECT_F bar =
            ramkaYasa(i.hudud.left + 20, ust + kProgressBalandligi / 2 - 4, 260, 8);
        c.toldir(bar, U::yumshoqFon, 4.0f);
        if (!i.progressAniqmas) {
            const float w =
                (bar.right - bar.left) * static_cast<float>(std::clamp(i.progress, 0.0, 1.0));
            if (w > 0) c.toldir(ramkaYasa(bar.left, bar.top, w, 8), U::kok, 4.0f);
        } else {
            // Aniqmas holat: animatsiya oʻrniga toʻliq ochroq chiziq. Har
            // kadrda qayta chizish zaif mashinada protsessorni behuda yeydi.
            c.toldir(bar, U::kartaHoverChet, 4.0f);
        }

        c.matn(i.holatMatni, ramkaYasa(bar.right + 14, ust, 320, kProgressBalandligi), U::matn2,
               13.0f, Ogirlik::Oddiy, Hizalash::Chap, true);
    }
}

// ---- Fayl qabul qilish -----------------------------------------------------

void FaylTab::faylniQabulQil(const std::wstring& yol) {
    auto& i = *ichki_;

    if (Engine::findModel().empty()) {
        ogohlantir(i.ota, L"Model topilmadi",
                   L"Nutq modeli («ggml-rubaistt.bin») topilmadi.\n\n"
                   L"«Yozish» boʻlimidagi «Yuklab olish» tugmasini bosing — "
                   L"model bir marta yuklab olinadi (785 MB).");
        return;
    }
    if (TranskripsiyaIshi::ishlayapti()) {
        ogohlantir(i.ota, L"Ish ketmoqda", L"Boshqa fayl ustida ish ketmoqda. Tugashini kuting.");
        return;
    }
    // Diktovka yozilayotgan yoki matnga oʻgirilayotgan boʻlsa — fayl kutadi (D1).
    if (DiktovkaBand::faol()) {
        ogohlantir(i.ota, L"Diktovka ketmoqda",
                   L"Diktovka tugashini kuting, keyin faylni qayta tashlang.");
        return;
    }
    // LLM oqimi joriy hujjatga bogʻlangan. Shu payt yangi fayl qabul qilinsa,
    // oqim tugagach uning natijasi yangi hujjatning matniga qoʻshilib ketardi.
    if (i.bandmi) {
        ogohlantir(i.ota, L"Amal ketmoqda",
                   L"Joriy matn ustida amal tugashini kuting, keyin yangi fayl tashlang.");
        return;
    }

    i.progress = 0;
    i.progressAniqmas = false;
    i.holatMatni = L"Tayyorlanmoqda…";
    i.progressniKorsat(true);
    joylashtir(i.hudud);

    HWND oyna = i.ota;
    i.ish = std::make_unique<TranskripsiyaIshi>(yol);
    i.ish->onProgress = [oyna](double p, std::wstring s) {
        PostMessageW(oyna, FaylXabar::kProgress, static_cast<WPARAM>(p * 1000),
                     reinterpret_cast<LPARAM>(new std::wstring(std::move(s))));
    };
    i.ish->onTayyor = [oyna](Hujjat h) {
        PostMessageW(oyna, FaylXabar::kTayyor, 0,
                     reinterpret_cast<LPARAM>(new Hujjat(std::move(h))));
    };
    i.ish->onXato = [oyna](std::wstring m) {
        PostMessageW(oyna, FaylXabar::kXato, 0,
                     reinterpret_cast<LPARAM>(new std::wstring(std::move(m))));
    };
    i.ish->boshla();
    i.qaytaChiz();
}

// ---- Fon oqimidan kelgan xabarlar ------------------------------------------

void FaylTab::xabarKeldi(UINT xabar, WPARAM wp, LPARAM lp) {
    auto& i = *ichki_;

    switch (xabar) {
        case FaylXabar::kProgress: {
            std::unique_ptr<std::wstring> s(reinterpret_cast<std::wstring*>(lp));
            i.progress = static_cast<double>(wp) / 1000.0;
            wchar_t bufer[160];
            swprintf(bufer, 160, L"%ls  %d%%", s->c_str(), static_cast<int>(i.progress * 100));
            i.holatMatni = bufer;
            i.qaytaChiz();
            return;
        }

        case FaylXabar::kTayyor: {
            std::unique_ptr<Hujjat> h(reinterpret_cast<Hujjat*>(lp));
            i.ish.reset();
            i.progressniKorsat(false);
            i.kutubxonaniYangila();
            i.hujjatniOch(*h);
            joylashtir(i.hudud);
            // Kalit bor boʻlsa transkript avtomatik tozalanadi (macOS bilan
            // bir xil): foydalanuvchi hech narsa bosmasdan tayyor matn oladi.
            if (LLMSozlama::sozlanganmi()) {
                if (const Amal* a = amalTop("tozalash")) i.amalniBajar(*a);
            }
            i.qaytaChiz();
            return;
        }

        case FaylXabar::kXato: {
            std::unique_ptr<std::wstring> m(reinterpret_cast<std::wstring*>(lp));
            i.ish.reset();
            i.progressniKorsat(false);
            joylashtir(i.hudud);
            i.qaytaChiz();
            ogohlantir(i.ota, L"Xato", *m);
            return;
        }

        case FaylXabar::kDelta: {
            std::unique_ptr<std::wstring> d(reinterpret_cast<std::wstring*>(lp));
            if (static_cast<unsigned>(wp) != i.oqimToken) return;
            i.maydon.qoshib(*d);
            return;
        }

        case FaylXabar::kAmalTayyor: {
            std::unique_ptr<std::wstring> s(reinterpret_cast<std::wstring*>(lp));
            i.ishTugadi();
            joylashtir(i.hudud);
            i.qaytaChiz();
            return;
        }

        case FaylXabar::kAmalXato: {
            std::unique_ptr<std::wstring> m(reinterpret_cast<std::wstring*>(lp));
            i.ishTugadi();
            joylashtir(i.hudud);
            i.qaytaChiz();
            if (static_cast<unsigned>(wp) != i.oqimToken) return;
            // Amal boshida matn maydoni boʻshatilgan edi. Amal yiqilsa uni SHU
            // HOLDA qoldirib boʻlmaydi: transkript diskda turibdi, lekin
            // foydalanuvchi boʻsh ekranni koʻradi va matnini yoʻqotdim deb
            // oʻylaydi. Tayyor matnni qaytaramiz, keyin xatoni aytamiz.
            i.matnniOqi(MatnTuri::Chiroyli);
            ogohlantir(i.ota, L"Xato", *m);
            return;
        }

        default: return;
    }
}

void FaylTab::matnOzgardi() {
    auto& i = *ichki_;
    // Tahrirlar avtomatik saqlanadi — faqat matn turlarida, LLM natijalarida
    // emas (ular oʻz faylida `natijalar\` papkasida turadi).
    if (!i.joriyBor || i.natijaKorinyapti) return;
    HujjatOmbori::matnSaqla(i.joriy.id, i.korinayotganTur, i.maydon.matn());
}

}  // namespace rubai
