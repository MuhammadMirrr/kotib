// Bitta faylni matnga oʻgirish ishi: dekodlash → whisper → saqlash.
//
// macOS'dagi `src/transcribe_job.swift` (`TranskripsiyaIshi`) ning
// ekvivalenti. Boʻlaklash chegaralari, progress hisobi va xato xabarlari
// aynan bir xil — foydalanuvchi ikkala platformada bir xil xatti-harakat
// koʻrishi kerak.
//
// Progress: har bir boʻlak butun ish barining OʻZ ulushiga ega — shu ulush
// ichida dekodlash 0–10%ni, transkripsiya 10–100%ni egallaydi. Shu tarzda
// koʻp boʻlakli ishda ham bar bir tekis, orqaga qaytmasdan oʻsadi.
#pragma once

#include "hujjat.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace rubai {

// Diktovka holati — Fayl ishi boshlanishidan oldin tekshiriladi (barqarorlik
// D1, macOS'dagi `DiktovkaBand` bilan bir xil). Ilgari Windows'da diktovka va
// fayl ishi bir-birini bloklamasdi: navbat bitta, shuning uchun diktovka fayl
// ortida daqiqalab kutib, matni foydalanuvchi allaqachon oʻtib ketgan boshqa
// oynaga tushardi. Faqat asosiy (UI) oqimda.
struct DiktovkaBand {
    // Mikrofon ochiq (yozilyapti).
    inline static bool mikrofon = false;
    // Whisper'ga berilgan, natijasi hali kelmagan diktovkalar.
    inline static int transkripsiya = 0;
    static bool faol() { return mikrofon || transkripsiya > 0; }
};

class TranskripsiyaIshi {
public:
    // Bir vaqtda faqat BITTA ish. Diktovka (Ctrl+Alt+D) ham shu bayroqni
    // tekshiradi — model bitta va uni ikki joydan chaqirib boʻlmaydi.
    static bool ishlayapti();

    explicit TranskripsiyaIshi(std::wstring yol);
    ~TranskripsiyaIshi();

    // 0…1 va holat matni. Boshqa oqimdan chaqiriladi — UI'ga uzatishda
    // xabar (PostMessage) ishlating.
    std::function<void(double, std::wstring)> onProgress;
    std::function<void(Hujjat)> onTayyor;
    std::function<void(std::wstring)> onXato;

    // Ishni fon oqimida boshlaydi. Ish oʻzini oʻzi tirik saqlaydi.
    void boshla();

    // Bekor qilish soʻrovi. Ish boʻlaklar orasida toʻxtaydi.
    void bekorQil();

private:
    struct Ichki;
    std::shared_ptr<Ichki> ichki_;
};

}  // namespace rubai
