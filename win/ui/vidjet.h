// Kotib — chizilgan boshqaruv elementlari (vidjetlar).
//
// macOS tomonida bularning oʻrnini AppKit bajaradi (`NSButton`, `NSTableView`,
// `NSSegmentedControl`). Windows'ning standart boshqaruv elementlari dizaynga
// mos kelmaydi — ular tizim mavzusida chiziladi va yumaloq burchak, oʻz rangi
// yoki 15pt shriftni qabul qilmaydi. Shu sababli ular bu yerda Direct2D bilan
// oʻzimiz chizamiz.
//
// ISTISNO — tahrirlanadigan matn maydonlari. Ular RichEdit boshqaruvi ustida
// quriladi (`matn_maydon.h`), chunki matn tanlash, bekor qilish (Ctrl+Z),
// klaviatura tillari (IME) va ekran oʻqiruvchilar bilan ishlash — oʻzi bir
// yillik ish, Windows'da esa u tayyor va tabiiy holda mavjud.
//
// Model: vidjetlar daraxti emas, bitta konteyner + tekis roʻyxat. Ilovada
// chuqur ichma-ich joylashuv yoʻq, daraxt esa kodni ikki barobar
// murakkablashtirardi.
#pragma once

#include "uslub.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rubai {

// Vidjetning holati — chizishda ranglarni tanlash uchun.
enum class Holat { Oddiy, Ustida, Bosilgan, Ochirilgan };

class Vidjet {
public:
    virtual ~Vidjet() = default;

    D2D1_RECT_F ramka{};
    bool korinadi = true;
    bool yoqilgan = true;

    virtual void chiz(Chizgich& c) = 0;

    // Hodisalar. Qaytariladigan qiymat: «qayta chizish kerak».
    virtual bool sichqonUstida(bool ichida);
    virtual bool sichqonBosildi(D2D1_POINT_2F p);
    virtual bool sichqonQoyildi(D2D1_POINT_2F p);
    virtual bool sichqonSurildi(D2D1_POINT_2F p);
    // Oʻng tugma qoʻyib yuborildi — kontekst menyusi uchun.
    virtual bool sichqonOngBosildi(D2D1_POINT_2F /*p*/) { return false; }
    virtual bool aylantirildi(float delta);

    // Sichqoncha ustida turganda koʻrsatkich shakli. nullptr = standart.
    virtual LPCWSTR kursor() const { return nullptr; }

    // Klaviatura fokusini oladimi (Tab bilan aylanishda qatnashadimi).
    virtual bool fokusOladimi() const { return false; }
    virtual bool klavisha(WPARAM /*kod*/) { return false; }

    bool fokusda = false;

protected:
    Holat holat_ = Holat::Oddiy;
    bool ustida_ = false;
    bool bosilgan_ = false;
    Holat joriyHolat() const;
};

// ---- Tugma -----------------------------------------------------------------
// Uch koʻrinish. Dizayndagi uchtasining aynan oʻzi:
//   Asosiy      — koʻk toʻldirilgan («Yozishni boshlash»)
//   Ikkilamchi  — oq, chegarali («Nusxa olish»)
//   Matnli      — fonsiz, faqat matn («Bekor qilish»)
class Tugma : public Vidjet {
public:
    enum class Kor { Asosiy, Ikkilamchi, Matnli };

    Tugma(std::wstring nom, Kor kor = Kor::Asosiy);

    std::wstring nom;
    Kor kor;
    float shriftOlchami = 15.0f;
    // Oʻng chetdagi ⌄ belgisi — «Matnni yaxshilash ⌄» kabi menyu ochadigan
    // tugmalar uchun.
    bool chevron = false;
    std::function<void()> bosilganda;

    void chiz(Chizgich& c) override;
    bool sichqonQoyildi(D2D1_POINT_2F p) override;
    LPCWSTR kursor() const override { return IDC_HAND; }
    bool fokusOladimi() const override { return true; }
    bool klavisha(WPARAM kod) override;

    // Matnga qarab kerakli kenglikni hisoblaydi (joylashtirish uchun).
    float kerakliKenglik(Chizgich& c) const;
};

// ---- Tanlagich (segment boshqaruvi) ----------------------------------------
// Toolbar oʻrtasidagi «Yozish | Fayl | Tarjima».
//
// Nomi ataylab `Segment` EMAS: `rubai::Segment` allaqachon band —
// `matn_format.h` da u whisper qaytargan transkript boʻlagini bildiradi.
class Tanlagich : public Vidjet {
public:
    explicit Tanlagich(std::vector<std::wstring> bandlar);

    std::vector<std::wstring> bandlar;
    int tanlangan = 0;
    std::function<void(int)> ozgarganda;

    void chiz(Chizgich& c) override;
    bool sichqonQoyildi(D2D1_POINT_2F p) override;
    LPCWSTR kursor() const override { return IDC_HAND; }
    bool fokusOladimi() const override { return true; }
    bool klavisha(WPARAM kod) override;

private:
    int bandIndeksi(D2D1_POINT_2F p) const;
    int ustidagi_ = -1;
};

// ---- Yozuv (label) ---------------------------------------------------------
class Yozuv : public Vidjet {
public:
    explicit Yozuv(std::wstring matn, float olcham = 13.0f, uint32_t rang = U::matn,
                   Ogirlik ogirlik = Ogirlik::Oddiy);

    std::wstring matn;
    float olcham;
    uint32_t rang;
    Ogirlik ogirlik;
    Hizalash hizalash = Hizalash::Chap;
    bool vertikalMarkaz = false;

    void chiz(Chizgich& c) override;
};

// ---- Roʻyxat ---------------------------------------------------------------
// Diktovka tarixi, hujjatlar roʻyxati, tillar roʻyxati — hammasi shu.
// Qatorlar talab boʻyicha chiziladi (virtualizatsiya): 200 ta yozuvli tarixda
// ham faqat koʻrinadigan qatorlar chiziladi.
class Royxat : public Vidjet {
public:
    std::function<int()> qatorlarSoni;
    std::function<float(int)> qatorBalandligi;
    std::function<void(Chizgich&, int, const D2D1_RECT_F&, bool /*ustida*/)> qatorChiz;
    std::function<void(int)> qatorBosildi;
    std::function<void(int, D2D1_POINT_2F)> qatorOngBosildi;

    void chiz(Chizgich& c) override;
    bool sichqonUstida(bool ichida) override;
    bool sichqonSurildi(D2D1_POINT_2F p) override;
    bool sichqonQoyildi(D2D1_POINT_2F p) override;
    bool sichqonBosildi(D2D1_POINT_2F p) override;
    bool sichqonOngBosildi(D2D1_POINT_2F p) override;
    bool aylantirildi(float delta) override;

    // Roʻyxat oʻzgarganda chaqiriladi — surish chegarasi qayta hisoblanadi.
    void yangilandi();

private:
    int qatorIndeksi(D2D1_POINT_2F p) const;
    float toliqBalandlik() const;
    float surish_ = 0.0f;
    int ustidagi_ = -1;
};

// ---- Ochirgich (toggle) ----------------------------------------------------
class Ochirgich : public Vidjet {
public:
    bool yoqiq = false;
    std::function<void(bool)> ozgarganda;

    void chiz(Chizgich& c) override;
    bool sichqonQoyildi(D2D1_POINT_2F p) override;
    LPCWSTR kursor() const override { return IDC_HAND; }
    bool fokusOladimi() const override { return true; }
    bool klavisha(WPARAM kod) override;
};

// ---- Konteyner -------------------------------------------------------------
// Bitta ekran (tab yoki oyna) vidjetlarini tutadi va hodisalarni tarqatadi.
// Sichqoncha holatini (qaysi vidjet ustida, qaysi biri bosilgan) shu kuzatadi:
// har bir vidjet buni oʻzi qilsa, kod takrorlanardi va «bosib turib chetga
// chiqish» holati doim buzilardi.
class Konteyner {
public:
    void qosh(std::shared_ptr<Vidjet> v);
    void tozala();

    void chiz(Chizgich& c);

    // Qaytaradi: qayta chizish kerakmi.
    bool sichqonHarakat(D2D1_POINT_2F p);
    bool sichqonBosildi(D2D1_POINT_2F p);
    bool sichqonQoyildi(D2D1_POINT_2F p);
    bool sichqonOngBosildi(D2D1_POINT_2F p);
    bool sichqonChiqdi();
    bool aylantirildi(D2D1_POINT_2F p, float delta);
    bool klavisha(WPARAM kod);
    bool tab(bool orqaga);

    LPCWSTR kursor(D2D1_POINT_2F p) const;

    const std::vector<std::shared_ptr<Vidjet>>& vidjetlar() const { return vidjetlar_; }

private:
    Vidjet* topish(D2D1_POINT_2F p) const;
    std::vector<std::shared_ptr<Vidjet>> vidjetlar_;
    Vidjet* ustidagi_ = nullptr;
    Vidjet* bosilgan_ = nullptr;
    Vidjet* fokusdagi_ = nullptr;
};

}  // namespace rubai
