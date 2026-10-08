// «Fayl» tabi (Studiya) — macOS'dagi `src/studiya_view.swift` (`StudiyaVC`)
// ning ekvivalenti.
//
// Ikki koʻrinish, bir vaqtda faqat bittasi:
//   1. Roʻyxat — katta tashlash zonasi va «Oxirgi fayllar»
//   2. Matn   — «‹ Audio» sarlavhasi, transkript va pastdagi amallar
//
// Dizayndan ikkita ATAYLAB chetlanish (macOS bilan bir xil):
//   • LLM amallari bitta «Matnni yaxshilash ⌄» menyusiga yigʻilgan.
//   • Tayyor/xom matn almashtirgichi matn maydonining oʻng tugma menyusida —
//     LLM sozlanmagan foydalanuvchi ham xom matnga yeta olishi kerak.
//
// «Tarjima qilish ⌄» ALOHIDA tugma: yaxshilash API kalitisiz butunlay
// yashiriladi, tarjima esa oflayn ishlaydi va kalit talab qilmaydi.
#pragma once

#include "tab.h"
#include "../core/hujjat.h"

#include <functional>
#include <memory>
#include <string>

namespace rubai {

// Ishchi oqimlardan UI oqimiga uzatiladigan xabarlar. Transkripsiya ishi ham,
// LLM oqimi ham fon oqimida ketadi va ularning qayta chaqiruvlari oynaga
// TOʻGʻRIDAN-TOʻGʻRI tegmasligi kerak.
namespace FaylXabar {
inline constexpr UINT kProgress = WM_APP + 20;    // wp: foiz, lp: new wstring*
inline constexpr UINT kTayyor = WM_APP + 21;      // lp: new Hujjat*
inline constexpr UINT kXato = WM_APP + 22;        // lp: new wstring*
inline constexpr UINT kDelta = WM_APP + 23;       // wp: token, lp: new wstring*
inline constexpr UINT kAmalTayyor = WM_APP + 24;  // wp: token, lp: new wstring*
inline constexpr UINT kAmalXato = WM_APP + 25;    // wp: token, lp: new wstring*
inline constexpr UINT kBirinchi = kProgress;
inline constexpr UINT kOxirgi = kAmalXato;
}  // namespace FaylXabar

class FaylTab : public Tab {
public:
    // `ota` — asosiy oyna: matn maydoni uning bolasi boʻladi va fon
    // oqimlaridan xabarlar shu yerga yuboriladi.
    explicit FaylTab(HWND ota);
    ~FaylTab() override;

    void joylashtir(const D2D1_RECT_F& hudud) override;
    void chiz(Chizgich& c) override;
    void faollashdi() override;

    // Tab koʻrinmay qolganda matn maydonini yashirish uchun.
    void korinishOzgardi(bool korinadi) override;

    // Ekran masshtabi oʻzgarganda (matn maydoni pikselda joylashadi).
    void dpiNisbat(float k);

    // Sudrab tashlangan yoki «Fayl tanlash» bilan tanlangan fayl.
    void faylniQabulQil(const std::wstring& yol);

    // Transkriptni «Tarjima» tabiga uzatadi. `tilKodi` boʻsh — foydalanuvchi
    // tilni oʻsha tabda oʻzi tanlaydi.
    std::function<void(const std::wstring& matn, const std::string& tilKodi)> onTarjima;

    // Fon oqimidan kelgan xabar. Asosiy oyna `FaylXabar` oraligʻidagi
    // hammasini shu yerga uzatadi.
    void xabarKeldi(UINT xabar, WPARAM wp, LPARAM lp);

    // Matn maydoni EN_CHANGE bergani — asosiy oyna WM_COMMAND'dan uzatadi.
    void matnOzgardi();

private:
    struct Ichki;
    std::unique_ptr<Ichki> ichki_;
};

}  // namespace rubai
