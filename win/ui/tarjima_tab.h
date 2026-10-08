// «Tarjima» tabi — macOS'dagi `src/tarjima_view.swift` (`TarjimaVC`) ning
// ekvivalenti.
//
// Dizayn: yuqorida ikkita til maydoni va ⇄ tugmasi; ostida manba katagi,
// natija katagi; pastda jarayon koʻrsatkichi, «Nusxa olish» va «Tarjima».
//
// Til maydonlari — tahrirlanadigan `COMBOBOX`, chunki model 202 tilni biladi
// va oddiy ochiluvchi roʻyxatda ularni topib boʻlmaydi. Foydalanuvchi til
// nomini yoza boshlaydi, qolgani oʻzi toʻldiriladi (macOS'da `NSComboBox`
// `completes = true` bilan aynan shu ishni qiladi).
//
// Model 3,4 GB va ilova ichida KELMAYDI. Shuning uchun tab ikki holatda
// boʻladi: model bor (ish holati) yoki yoʻq (banner). Holat har safar tab
// koʻrsatilganda qayta tekshiriladi — foydalanuvchi modelni boshqa tabda
// turganda yuklab olgan boʻlishi mumkin.
#pragma once

#include "tab.h"
#include "../core/tillar.h"

#include <functional>
#include <memory>
#include <string>

namespace rubai {

// Fon oqimidan UI oqimiga uzatiladigan xabarlar.
namespace TarjimaXabar {
inline constexpr UINT kJarayon = WM_APP + 30;  // wp: bajarilgan, lp: jami
inline constexpr UINT kTayyor = WM_APP + 31;   // lp: new wstring*
inline constexpr UINT kXato = WM_APP + 32;     // lp: new wstring*
// Model yuklab olish (`tarjima_yuklovchi.h`).
inline constexpr UINT kYuklash = WM_APP + 33;        // wp: olingan MB, lp: jami MB
inline constexpr UINT kYuklashTugadi = WM_APP + 34;  // wp: ok, lp: new wstring*
inline constexpr UINT kBirinchi = kJarayon;
inline constexpr UINT kOxirgi = kYuklashTugadi;
}  // namespace TarjimaXabar

class TarjimaTab : public Tab {
public:
    explicit TarjimaTab(HWND ota);
    ~TarjimaTab() override;

    void joylashtir(const D2D1_RECT_F& hudud) override;
    void chiz(Chizgich& c) override;
    void faollashdi() override;
    void korinishOzgardi(bool korinadi) override;

    void dpiNisbat(float k);

    // «Fayl» tabidan matn keladi. `tilKodi` boʻsh boʻlsa tarjima
    // BOSHLANMAYDI — foydalanuvchi tilni oʻzi tanlaydi.
    void matnniQabulQil(const std::wstring& matn, const std::string& tilKodi);

    void xabarKeldi(UINT xabar, WPARAM wp, LPARAM lp);
    void matnOzgardi();

    // Ota oynaning WM_COMMAND'idan: til tanlovi oʻzgarishi.
    void buyruq(WPARAM wp, LPARAM lp);

private:
    struct Ichki;
    std::unique_ptr<Ichki> ichki_;
};

}  // namespace rubai
