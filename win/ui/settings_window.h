// Sozlamalar oynasi.
//
// macOS versiyasidagi `SozlamalarVC` ning ekvivalenti (sozlamalar_view.swift),
// lekin mikrofon tanlash qoʻshilgan — Windows'da bu majburiy, chunki
// standart qurilma "Stereo Mix" boʻlib qolsa ilova ovoz oʻrniga
// kompyuter ovozini yozadi.
#pragma once

#include <windows.h>

#include <functional>

#include "../core/config.h"
#include "../core/yangilovchi.h"

namespace rubai {

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    // Sozlamalar saqlanganda chaqiriladi (hotkey qayta roʻyxatdan
    // oʻtkazilishi va h.k. uchun).
    void setOnSaved(std::function<void(const Settings&)> cb);

    // «Yangilanishlar» qatori: holat matni va tugma (`Yangilovchi::korinish`),
    // tugma bosilganda — `bosildi`. Holat oʻzgarganda `yangilanishniYangila`.
    void setYangilanish(std::function<Yangilovchi::Korinish()> holat,
                        std::function<void()> bosildi);
    void yangilanishniYangila();

    // Oynani ochadi (allaqachon ochiq boʻlsa oldinga chiqaradi).
    //
    // `ega` — asosiy oyna. Egasi boʻlgan oyna undan HECH QACHON orqaga
    // tushmaydi: ilgari sozlamalar ochiq turib asosiy oynaga bosilsa,
    // u ortida yoʻqolib qolardi va foydalanuvchi uni topa olmasdi.
    void show(HINSTANCE instance, const Settings& current, HWND ega = nullptr);

    // Asosiy xabar sikli uchun: oyna dialog EMAS, shuning uchun Tab,
    // strelkalar, Enter va Esc'ni `IsDialogMessageW` bajarishi kerak.
    // Oyna hali yaratilmagan boʻlsa nullptr.
    HWND oyna() const;

    struct Impl;

private:
    Impl* d;
};

}  // namespace rubai
