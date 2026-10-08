// Studiya amallari va ularning prompt'lari.
//
// macOS'dagi `src/amallar.swift` ning ekvivalenti. Prompt matnlari AYNAN bir
// xil: ular ilovaning javob sifatini belgilaydi va ikkala platformada bir xil
// natija berishi kerak. Prompt oʻzgarsa — ikkala faylda birga oʻzgaradi.
//
// Prompt'lar oʻzbek tilida — model javobni ham oʻzbekcha berishi uchun.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rubai {

struct Amal {
    std::string id;
    std::wstring nom;
    // true — natija butun matndan yigʻiladi (map-reduce kerak).
    // false — matnni oʻzgartiradi (boʻlaklar ketma-ket ulanadi).
    bool yiguvchi = false;
    std::wstring system;
};

// Tayyor amallar roʻyxati — «Matnni yaxshilash ⌄» menyusi shundan quriladi.
const std::vector<Amal>& amallar();

// Roʻyxatdan identifikatori boʻyicha topadi. Topilmasa nullptr.
const Amal* amalTop(const std::string& id);

// Erkin soʻrov — foydalanuvchi oʻz koʻrsatmasini yozadi.
Amal erkinAmal(const std::wstring& korsatma);

// ---- Ish -------------------------------------------------------------------
//
// Amalni fon oqimida bajaradi va natijani boʻlak-boʻlak qaytaradi.
//
// MUHIM: uchala qayta chaqiruv ham ISHCHI OQIMDA keladi, UI oqimida emas.
// Ularning ichida hech qanday oynaga tegmang — `PostMessage` bilan UI oqimiga
// uzating (`fayl_tab.cpp` shunday qiladi).
class AmalIshi {
public:
    AmalIshi();
    ~AmalIshi();
    AmalIshi(const AmalIshi&) = delete;
    AmalIshi& operator=(const AmalIshi&) = delete;

    // false qaytsa ish umuman boshlanmadi (`onXato` allaqachon chaqirilgan).
    bool bajar(const Amal& amal, const std::wstring& matn,
               std::function<void(const std::wstring&)> onDelta,
               std::function<void(const std::wstring&)> onTayyor,
               std::function<void(const std::wstring&)> onXato);

    // Ketayotgan oqimni toʻxtatadi. Foydalanuvchi «Bekor qilish» ni bosganda
    // xato emas, «Bekor qilindi.» xabari beriladi — macOS bilan bir xil.
    void bekorQil();

    bool ketyaptimi() const;

private:
    struct Ichki;
    std::shared_ptr<Ichki> ichki_;
};

}  // namespace rubai
