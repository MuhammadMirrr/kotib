// Kotib — asosiy oyna.
//
// macOS tomonidagi `src/asosiy_oyna.swift` (`AsosiyOyna`) ning ekvivalenti.
// Dizayn bir xil: tepada toolbar — oʻrtada segment boshqaruvi
// (Yozish | Fayl | Tarjima), oʻng chetda ⚙ tugmasi; ostida tanlangan tabning
// koʻrinishi. Yon panel YOʻQ — macOS'da u ataylab olib tashlangan, chunki
// ikki-uch rejim ustiga qoʻyilgan daraxt hech qanday maʼlumot bermasdi.
//
// Sozlamalar tab EMAS: ⚙ alohida modal oyna ochadi (macOS'da sheet).
//
// Tab koʻrinishlari birinchi ochilganda yaratiladi va saqlanadi — ketayotgan
// transkripsiya ishi yoki kiritilgan matn tab almashganda yoʻqolmasligi kerak.
#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rubai {

class Chizgich;
class Tab;
class YozishTab;
class FaylTab;
class TarjimaTab;

class AsosiyOyna {
public:
    AsosiyOyna();
    ~AsosiyOyna();
    AsosiyOyna(const AsosiyOyna&) = delete;
    AsosiyOyna& operator=(const AsosiyOyna&) = delete;

    bool qur(HINSTANCE instance);
    void korsat();
    void yashir();
    bool korinadimi() const;

    HWND deskriptor() const;

    // Diktovka holati oʻzgarganda chaqiriladi (dictate oqimidan).
    //
    // MUHIM: holat oxirgi marta nima berilgani bilan saqlanadi. Foydalanuvchi
    // oynani diktovka OʻRTASIDA ochsa, yangi yaratilgan tab darhol toʻgʻri
    // holatni koʻrsatishi kerak — keyingi oʻzgarishni kutib turmasligi.
    // macOS tomonida ham shu qoida (`AsosiyOyna.diktovkaHolati`).
    // Foydalanuvchi «Bosing va gapiring» kartasini bosdi.
    std::function<void()> onDiktovka;

    void diktovkaHolati(bool yozilyapti);

    // ⚙ tugmasi. Sozlamalar tab EMAS — alohida modal oyna (macOS'da sheet).
    std::function<void()> onSozlamalar;

    // «Fayl» tabidagi matnni «Tarjima» tabiga uzatish soʻrovi. `tilKodi`
    // boʻsh — foydalanuvchi tilni oʻsha tabda tanlaydi.
    std::function<void(const std::wstring& matn, const std::string& tilKodi)> onTarjima;

    YozishTab* yozishTabi();
    FaylTab* faylTabi();
    TarjimaTab* tarjimaTabi();

    // Diktovka tugagach tarixga qoʻshish uchun.
    void diktovkaTugadi(const std::wstring& matn);

    // Diktovka tarixini diskdan qayta oʻqib roʻyxatni yangilaydi.
    void tarixniYangila();

    // Yozib olish paytidagi signal darajasi (0…1) — toʻlqin chizigʻi uchun.
    void darajaniQoy(float daraja);

    // Kartadagi «Ctrl · Alt · D» chiplari.
    void hotkeyMatni(const std::wstring& matn);

    // Yangilanish haqidagi ingichka chiziq («Kotib 1.2.1 ga yangilandi — …») —
    // oyna chrome'ining bir qismi, uchala tabda ham koʻrinadi. Modal oyna
    // ataylab EMAS: foydalanuvchining ishini toʻxtatishga haqqi yoʻq (macOS'dagi
    // `BannerXabar` bilan bir xil). `tugma` boʻsh boʻlmasa — amal tugmasi.
    //
    // Foydalanuvchi ✕ ni bossa, AYNAN SHU `kalit` li banner boshqa koʻrsatilmaydi.
    // `kalit` boʻsh — ✕ yoʻq (majburiy yangilanish banneri yopilmaydi, S9).
    // `ikkinchi…` — chaproqdagi qoʻshimcha tugma («Saytdan yuklab olish»).
    void yangilanishBanneri(const std::wstring& matn, const std::wstring& kalit,
                            const std::wstring& tugma = L"", std::function<void()> amal = nullptr,
                            const std::wstring& ikkinchiTugma = L"",
                            std::function<void()> ikkinchiAmal = nullptr);
    void yangilanishBanneriniYashir();

    // Yozish tabidagi ogohlantirish banneri (mikrofon tanlanmagan va h.k.).
    void banner(const std::wstring& matn, const std::wstring& tugmaNomi = L"",
                std::function<void()> tugmaBosildi = nullptr);

    // Oyna protsedurasi (erkin funksiya) shu tuzilmaga murojaat qiladi,
    // shuning uchun eʼlon ochiq; taʼrifi .cpp faylda. `overlay.h` da ham
    // shunday qilingan.
    struct Ichki;

private:
    std::unique_ptr<Ichki> ichki_;
};

}  // namespace rubai
