// Tab koʻrinishining umumiy interfeysi.
//
// macOS'da bu rolni `NSViewController` bajaradi (`DiktovkaVC`, `StudiyaVC`,
// `TarjimaVC`). Windows'da alohida oyna sinfi shart emas: uchala tab ham bitta
// asosiy oynaning ichida chiziladi, chunki ular hech qachon bir vaqtda
// koʻrinmaydi va har biriga alohida HWND berish faqat DPI, fokus va chizish
// muammolarini koʻpaytirardi.
#pragma once

#include "vidjet.h"

namespace rubai {

class Tab {
public:
    virtual ~Tab() = default;

    // Vidjetlarni berilgan hududga joylashtiradi. Oyna oʻlchami oʻzgarganda
    // qayta chaqiriladi.
    virtual void joylashtir(const D2D1_RECT_F& hudud) = 0;

    // Vidjetlardan tashqari chiziladigan narsalar (fon, ajratgichlar, boʻsh
    // holat matni). Vidjetlarning oʻzini konteyner chizadi.
    virtual void chiz(Chizgich& /*c*/) {}

    // Tab koʻrinadigan boʻlganda — maʼlumotni yangilash uchun.
    virtual void faollashdi() {}

    // Tab koʻringan/yashiringanda. Faqat bola oynasi bor tablar uchun kerak:
    // Direct2D vidjetlari chizilmasa oʻz-oʻzidan yoʻqoladi, HWND esa yoʻq —
    // uni ochiq qoldirsak, boshqa tab ustida osilib qolardi.
    virtual void korinishOzgardi(bool /*korinadi*/) {}

    Konteyner vidjetlar;
};

}  // namespace rubai
