// Chizilgan boshqaruv elementlari (vidjetlar).
// Interfeys va izohlar — `vidjet.h`.

#include "vidjet.h"

#include <algorithm>

namespace rubai {

// ---- Vidjet ----------------------------------------------------------------

Holat Vidjet::joriyHolat() const {
    if (!yoqilgan) return Holat::Ochirilgan;
    if (bosilgan_) return Holat::Bosilgan;
    if (ustida_) return Holat::Ustida;
    return Holat::Oddiy;
}

bool Vidjet::sichqonUstida(bool ichida) {
    if (ustida_ == ichida) return false;
    ustida_ = ichida;
    return true;
}

bool Vidjet::sichqonBosildi(D2D1_POINT_2F) {
    if (bosilgan_) return false;
    bosilgan_ = true;
    return true;
}

bool Vidjet::sichqonQoyildi(D2D1_POINT_2F) {
    if (!bosilgan_) return false;
    bosilgan_ = false;
    return true;
}

bool Vidjet::sichqonSurildi(D2D1_POINT_2F) { return false; }
bool Vidjet::aylantirildi(float) { return false; }

// ---- Tugma -----------------------------------------------------------------

Tugma::Tugma(std::wstring n, Kor k) : nom(std::move(n)), kor(k) {}

void Tugma::chiz(Chizgich& c) {
    if (!korinadi) return;

    const Holat h = joriyHolat();
    const float r = U::radiusTugma;

    uint32_t fon = 0, matnRang = 0, chet = 0;
    bool chetBor = false;

    switch (kor) {
        case Kor::Asosiy:
            fon = (h == Holat::Bosilgan) ? U::kokBosilgan : U::kok;
            matnRang = U::oq;
            break;
        case Kor::Ikkilamchi:
            fon = (h == Holat::Bosilgan) ? U::yumshoqFon
                  : (h == Holat::Ustida) ? U::qatorHover
                                         : U::oq;
            matnRang = U::matn;
            chet = U::tugmaChet;
            chetBor = true;
            break;
        case Kor::Matnli:
            fon = (h == Holat::Ustida || h == Holat::Bosilgan) ? U::yumshoqFon : 0xFFFFFF;
            matnRang = U::kok;
            break;
    }

    // Oʻchirilgan tugma: fon oʻzgarmaydi, matn ochroq boʻladi. Dizayn shunday —
    // «yoʻq» tugmani butunlay yashirish emas, uni sust koʻrsatish kerak.
    if (h == Holat::Ochirilgan) matnRang = U::matn4;

    if (kor != Kor::Matnli || h != Holat::Oddiy) c.toldir(ramka, fon, r);
    if (chetBor) c.chegara(ramka, chet, r);

    // Klaviatura fokusi — koʻk halqa. Sichqoncha bilan ishlaganda koʻrinmaydi,
    // faqat Tab bilan kelinganda; buni chaqiruvchi `fokusda` orqali boshqaradi.
    if (fokusda)
        c.chegara(D2D1::RectF(ramka.left - 2, ramka.top - 2, ramka.right + 2, ramka.bottom + 2),
                  U::kok, r + 2, 2.0f);

    D2D1_RECT_F matnRamka = ramka;
    if (chevron) matnRamka.right -= 16;

    c.matn(nom, matnRamka, matnRang, shriftOlchami,
           kor == Kor::Asosiy ? Ogirlik::Yarim : Ogirlik::Oddiy, Hizalash::Markaz, true);

    if (chevron) {
        // ⌄ — kichik uchburchak oʻrniga oddiy ikki chiziq: DirectWrite'da
        // belgi shriftga bogʻliq boʻlib qolardi.
        const float x = ramka.right - 14, y = (ramka.top + ramka.bottom) / 2 - 1;
        c.chiziq(x, y, x + 4, y + 4, matnRang, 1.5f);
        c.chiziq(x + 4, y + 4, x + 8, y, matnRang, 1.5f);
    }
}

bool Tugma::sichqonQoyildi(D2D1_POINT_2F p) {
    const bool ediBosilgan = bosilgan_;
    Vidjet::sichqonQoyildi(p);
    // Faqat tugma ustida qoʻyib yuborilgandagina ishga tushadi — bosib turib
    // chetga surib qoʻyib yuborish bekor qilish demak, Windows'dagi odat shu.
    if (ediBosilgan && yoqilgan && ichidami(ramka, p) && bosilganda) bosilganda();
    return true;
}

bool Tugma::klavisha(WPARAM kod) {
    if ((kod == VK_RETURN || kod == VK_SPACE) && yoqilgan && bosilganda) {
        bosilganda();
        return true;
    }
    return false;
}

float Tugma::kerakliKenglik(Chizgich& c) const {
    const auto o =
        c.matnOlchami(nom, shriftOlchami, kor == Kor::Asosiy ? Ogirlik::Yarim : Ogirlik::Oddiy);
    return o.width + 32 + (chevron ? 16 : 0);
}

// ---- Tanlagich ---------------------------------------------------------------

Tanlagich::Tanlagich(std::vector<std::wstring> b) : bandlar(std::move(b)) {}

int Tanlagich::bandIndeksi(D2D1_POINT_2F p) const {
    if (bandlar.empty() || !ichidami(ramka, p)) return -1;
    const float w = (ramka.right - ramka.left) / static_cast<float>(bandlar.size());
    const int i = static_cast<int>((p.x - ramka.left) / w);
    return std::clamp(i, 0, static_cast<int>(bandlar.size()) - 1);
}

void Tanlagich::chiz(Chizgich& c) {
    if (!korinadi || bandlar.empty()) return;

    const float r = U::radiusKichik;
    c.toldir(ramka, U::yumshoqFon, r);

    const float w = (ramka.right - ramka.left) / static_cast<float>(bandlar.size());

    for (size_t i = 0; i < bandlar.size(); ++i) {
        const float x = ramka.left + w * static_cast<float>(i);
        const D2D1_RECT_F band = D2D1::RectF(x, ramka.top, x + w, ramka.bottom);
        const bool tanlangan_ = (static_cast<int>(i) == tanlangan);

        if (tanlangan_) {
            // Tanlangan band — ichkariga 2 DIP surilgan oq «tosh».
            c.toldir(D2D1::RectF(band.left + 2, band.top + 2, band.right - 2, band.bottom - 2),
                     U::oq, r - 1);
        } else if (static_cast<int>(i) == ustidagi_) {
            c.toldir(D2D1::RectF(band.left + 2, band.top + 2, band.right - 2, band.bottom - 2),
                     U::qatorHover, r - 1);
        }

        c.matn(bandlar[i], band, tanlangan_ ? U::matn : U::matn2, 13.0f,
               tanlangan_ ? Ogirlik::Yarim : Ogirlik::Oddiy, Hizalash::Markaz, true);
    }

    if (fokusda)
        c.chegara(D2D1::RectF(ramka.left - 2, ramka.top - 2, ramka.right + 2, ramka.bottom + 2),
                  U::kok, r + 2, 2.0f);
}

bool Tanlagich::sichqonQoyildi(D2D1_POINT_2F p) {
    Vidjet::sichqonQoyildi(p);
    const int i = bandIndeksi(p);
    if (i >= 0 && i != tanlangan) {
        tanlangan = i;
        if (ozgarganda) ozgarganda(i);
    }
    return true;
}

bool Tanlagich::klavisha(WPARAM kod) {
    if (kod != VK_LEFT && kod != VK_RIGHT) return false;
    const int yangi =
        std::clamp(tanlangan + (kod == VK_LEFT ? -1 : 1), 0, static_cast<int>(bandlar.size()) - 1);
    if (yangi == tanlangan) return false;
    tanlangan = yangi;
    if (ozgarganda) ozgarganda(yangi);
    return true;
}

// ---- Yozuv -----------------------------------------------------------------

Yozuv::Yozuv(std::wstring m, float o, uint32_t r, Ogirlik og)
    : matn(std::move(m)), olcham(o), rang(r), ogirlik(og) {}

void Yozuv::chiz(Chizgich& c) {
    if (!korinadi) return;
    c.matn(matn, ramka, rang, olcham, ogirlik, hizalash, vertikalMarkaz);
}

// ---- Roʻyxat ---------------------------------------------------------------

float Royxat::toliqBalandlik() const {
    if (!qatorlarSoni || !qatorBalandligi) return 0;
    float h = 0;
    const int n = qatorlarSoni();
    for (int i = 0; i < n; ++i) h += qatorBalandligi(i);
    return h;
}

int Royxat::qatorIndeksi(D2D1_POINT_2F p) const {
    if (!qatorlarSoni || !qatorBalandligi || !ichidami(ramka, p)) return -1;
    float y = ramka.top - surish_;
    const int n = qatorlarSoni();
    for (int i = 0; i < n; ++i) {
        const float h = qatorBalandligi(i);
        if (p.y >= y && p.y < y + h) return i;
        y += h;
    }
    return -1;
}

void Royxat::chiz(Chizgich& c) {
    if (!korinadi || !qatorlarSoni || !qatorBalandligi || !qatorChiz) return;

    c.kesishBoshla(ramka);

    float y = ramka.top - surish_;
    const int n = qatorlarSoni();
    for (int i = 0; i < n; ++i) {
        const float h = qatorBalandligi(i);
        // Virtualizatsiya: koʻrinmaydigan qatorlar chizilmaydi. 200 yozuvli
        // tarixda bu sezilarli farq qiladi.
        if (y + h >= ramka.top && y <= ramka.bottom) {
            qatorChiz(c, i, D2D1::RectF(ramka.left, y, ramka.right, y + h), i == ustidagi_);
        }
        y += h;
        if (y > ramka.bottom) break;
    }

    c.kesishTugat();

    // Surish koʻrsatkichi — kontent sigʻmaganda oʻng chetda ingichka chiziq.
    const float toliq = toliqBalandlik();
    const float korinadigan = ramka.bottom - ramka.top;
    if (toliq > korinadigan) {
        const float nisbat = korinadigan / toliq;
        const float uzunlik = korinadigan * nisbat;
        const float joy = (korinadigan - uzunlik) * (surish_ / (toliq - korinadigan));
        c.toldir(D2D1::RectF(ramka.right - 5, ramka.top + joy, ramka.right - 2,
                             ramka.top + joy + uzunlik),
                 U::matn4, 1.5f);
    }
}

bool Royxat::sichqonUstida(bool ichida) {
    Vidjet::sichqonUstida(ichida);
    if (!ichida && ustidagi_ != -1) {
        ustidagi_ = -1;
        return true;
    }
    return false;
}

bool Royxat::sichqonSurildi(D2D1_POINT_2F p) {
    const int i = qatorIndeksi(p);
    if (i == ustidagi_) return false;
    ustidagi_ = i;
    return true;
}

bool Royxat::sichqonBosildi(D2D1_POINT_2F p) { return Vidjet::sichqonBosildi(p); }

bool Royxat::sichqonQoyildi(D2D1_POINT_2F p) {
    const bool ediBosilgan = bosilgan_;
    Vidjet::sichqonQoyildi(p);
    if (ediBosilgan) {
        const int i = qatorIndeksi(p);
        if (i >= 0 && qatorBosildi) qatorBosildi(i);
    }
    return true;
}

bool Royxat::sichqonOngBosildi(D2D1_POINT_2F p) {
    const int i = qatorIndeksi(p);
    if (i >= 0 && qatorOngBosildi) qatorOngBosildi(i, p);
    return false;
}

bool Royxat::aylantirildi(float delta) {
    const float toliq = toliqBalandlik();
    const float korinadigan = ramka.bottom - ramka.top;
    if (toliq <= korinadigan) return false;

    const float eski = surish_;
    surish_ = std::clamp(surish_ - delta, 0.0f, toliq - korinadigan);
    return surish_ != eski;
}

void Royxat::yangilandi() {
    const float toliq = toliqBalandlik();
    const float korinadigan = ramka.bottom - ramka.top;
    surish_ = std::clamp(surish_, 0.0f, std::max(0.0f, toliq - korinadigan));
}

// ---- Ochirgich -------------------------------------------------------------

void Ochirgich::chiz(Chizgich& c) {
    if (!korinadi) return;

    const float h = ramka.bottom - ramka.top;
    const float w = h * 1.75f;  // Windows odatidagi nisbat
    const D2D1_RECT_F yol = D2D1::RectF(ramka.left, ramka.top, ramka.left + w, ramka.bottom);

    c.toldir(yol, yoqiq ? U::kok : U::yumshoqFon, h / 2);
    if (!yoqiq) c.chegara(yol, U::tugmaChet, h / 2);

    const float r = h / 2 - 3;
    const float cx = yoqiq ? yol.right - r - 3 : yol.left + r + 3;
    c.doira(cx, (yol.top + yol.bottom) / 2, r, yoqiq ? U::oq : U::matn2);

    if (fokusda)
        c.chegara(D2D1::RectF(yol.left - 2, yol.top - 2, yol.right + 2, yol.bottom + 2), U::kok,
                  h / 2 + 2, 2.0f);
}

bool Ochirgich::sichqonQoyildi(D2D1_POINT_2F p) {
    const bool ediBosilgan = bosilgan_;
    Vidjet::sichqonQoyildi(p);
    if (ediBosilgan && yoqilgan && ichidami(ramka, p)) {
        yoqiq = !yoqiq;
        if (ozgarganda) ozgarganda(yoqiq);
    }
    return true;
}

bool Ochirgich::klavisha(WPARAM kod) {
    if (kod != VK_SPACE && kod != VK_RETURN) return false;
    yoqiq = !yoqiq;
    if (ozgarganda) ozgarganda(yoqiq);
    return true;
}

// ---- Konteyner -------------------------------------------------------------

void Konteyner::qosh(std::shared_ptr<Vidjet> v) { vidjetlar_.push_back(std::move(v)); }

void Konteyner::tozala() {
    vidjetlar_.clear();
    ustidagi_ = bosilgan_ = fokusdagi_ = nullptr;
}

void Konteyner::chiz(Chizgich& c) {
    for (auto& v : vidjetlar_)
        if (v->korinadi) v->chiz(c);
}

Vidjet* Konteyner::topish(D2D1_POINT_2F p) const {
    // Teskari tartibda: keyin qoʻshilgani ustida turadi.
    for (auto it = vidjetlar_.rbegin(); it != vidjetlar_.rend(); ++it) {
        if ((*it)->korinadi && (*it)->yoqilgan && ichidami((*it)->ramka, p)) return it->get();
    }
    return nullptr;
}

bool Konteyner::sichqonHarakat(D2D1_POINT_2F p) {
    bool qaytaChiz = false;

    // Bosib turilgan vidjet bor boʻlsa, sichqoncha undan chiqib ketsa ham
    // hodisalar oʻshanga boradi — «bosib turib sudrash» shunday ishlaydi.
    if (bosilgan_) {
        qaytaChiz |= bosilgan_->sichqonSurildi(p);
        qaytaChiz |= bosilgan_->sichqonUstida(ichidami(bosilgan_->ramka, p));
        return qaytaChiz;
    }

    Vidjet* yangi = topish(p);
    if (yangi != ustidagi_) {
        if (ustidagi_) qaytaChiz |= ustidagi_->sichqonUstida(false);
        if (yangi) qaytaChiz |= yangi->sichqonUstida(true);
        ustidagi_ = yangi;
    }
    if (yangi) qaytaChiz |= yangi->sichqonSurildi(p);
    return qaytaChiz;
}

bool Konteyner::sichqonBosildi(D2D1_POINT_2F p) {
    Vidjet* v = topish(p);
    bool qaytaChiz = false;

    // Fokus sichqoncha bosilganda koʻchadi, lekin fokus halqasi faqat Tab
    // bilan kelinganda chiziladi — shuning uchun bu yerda `fokusda` yoqilmaydi.
    if (fokusdagi_ && fokusdagi_ != v) {
        fokusdagi_->fokusda = false;
        qaytaChiz = true;
    }
    fokusdagi_ = v;

    if (!v) return qaytaChiz;
    bosilgan_ = v;
    return v->sichqonBosildi(p) || qaytaChiz;
}

bool Konteyner::sichqonQoyildi(D2D1_POINT_2F p) {
    if (!bosilgan_) return false;
    Vidjet* v = bosilgan_;
    bosilgan_ = nullptr;
    return v->sichqonQoyildi(p);
}

bool Konteyner::sichqonOngBosildi(D2D1_POINT_2F p) {
    Vidjet* v = topish(p);
    return v ? v->sichqonOngBosildi(p) : false;
}

bool Konteyner::sichqonChiqdi() {
    if (!ustidagi_) return false;
    const bool q = ustidagi_->sichqonUstida(false);
    ustidagi_ = nullptr;
    return q;
}

bool Konteyner::aylantirildi(D2D1_POINT_2F p, float delta) {
    Vidjet* v = topish(p);
    return v ? v->aylantirildi(delta) : false;
}

bool Konteyner::klavisha(WPARAM kod) { return fokusdagi_ ? fokusdagi_->klavisha(kod) : false; }

bool Konteyner::tab(bool orqaga) {
    std::vector<Vidjet*> nomzodlar;
    for (auto& v : vidjetlar_) {
        if (v->korinadi && v->yoqilgan && v->fokusOladimi()) nomzodlar.push_back(v.get());
    }
    if (nomzodlar.empty()) return false;

    int joriy = -1;
    for (size_t i = 0; i < nomzodlar.size(); ++i) {
        if (nomzodlar[i] == fokusdagi_) {
            joriy = static_cast<int>(i);
            break;
        }
    }

    const int n = static_cast<int>(nomzodlar.size());
    const int keyingi =
        (joriy < 0) ? (orqaga ? n - 1 : 0) : ((joriy + (orqaga ? -1 : 1)) % n + n) % n;

    if (fokusdagi_) fokusdagi_->fokusda = false;
    fokusdagi_ = nomzodlar[keyingi];
    fokusdagi_->fokusda = true;
    return true;
}

LPCWSTR Konteyner::kursor(D2D1_POINT_2F p) const {
    Vidjet* v = topish(p);
    return v ? v->kursor() : nullptr;
}

}  // namespace rubai
