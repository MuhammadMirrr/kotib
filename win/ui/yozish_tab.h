// «Yozish» tabi — macOS'dagi `src/diktovka_view.swift` (`DiktovkaVC`) ning
// ekvivalenti.
//
// Uch qism: ogohlantirish banneri, katta «Bosing va gapiring» kartasi va
// oxirgi yozuvlar roʻyxati.
//
// MUHIM (macOS bilan bir xil qoida): bu koʻrinish diktovka mantigʻini
// BOSHQARMAYDI. U faqat «toggle» signalini uzatadi va berilgan holatni
// koʻrsatadi. Matn baribir fokusdagi ilovaga Ctrl+V bilan tushadi; tarix —
// qoʻshimcha qulaylik, asosiy yoʻl emas.
//
// Karta ikkala holatni — kutish va yozib olish — BITTA vidjet ichida tutadi.
// macOS'da ham shunday qilingan: ikki alohida koʻrinish boʻlsa, ular
// almashganda bir-biriga nisbatan sakrab ketadi.
#pragma once

#include "tab.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rubai {

// Tarixdagi bitta yozuv. macOS'dagi `DiktovkaYozuvi` ning ekvivalenti.
struct TarixYozuvi {
    std::wstring id;
    std::wstring matn;
    long long vaqt = 0;  // Unix soniya
};

class YozishKartasi;

class YozishTab : public Tab {
public:
    YozishTab();
    ~YozishTab() override;

    // Foydalanuvchi kartani bosdi — diktovkani boshlash/toʻxtatish.
    std::function<void()> onDiktovka;

    // Tarix qatoridan «nusxa olish».
    std::function<void(const std::wstring&)> onNusxa;

    // Kontekst menyusi amallari — macOS'dagi tarix menyusining aynan oʻzi.
    // Dizaynda koʻrinmaydi, lekin ularsiz yozuvni oʻchirib boʻlmaydi.
    std::function<void(const std::wstring& id)> onOchir;
    std::function<void()> onTozala;

    // Menyu chiqarish uchun ota oyna deskriptori kerak.
    void otaOyna(HWND h);

    void joylashtir(const D2D1_RECT_F& hudud) override;
    void chiz(Chizgich& c) override;

    void holatniQoy(bool yozilyapti);
    void hotkeyMatni(const std::wstring& matn);

    // Ogohlantirish banneri (mikrofon tanlanmagan, «Stereo Mix» va hokazo).
    // Boʻsh matn — bannerni yashiradi.
    void banner(const std::wstring& matn, const std::wstring& tugmaNomi = L"",
                std::function<void()> tugmaBosildi = nullptr);

    void tarixniQoy(std::vector<TarixYozuvi> yozuvlar);

    // Yozib olish paytida toʻlqin chizigʻi uchun signal darajasi (0..1).
    void darajaniQoy(float daraja);

    // Taymer qayta chaqiruvi shu turga koʻrsatkich saqlaydi
    // (`yozish_tab.cpp`), shuning uchun nomi ochiq turadi.
    struct Ichki;

private:
    std::unique_ptr<Ichki> ichki_;
};

}  // namespace rubai
