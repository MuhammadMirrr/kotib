// "Donat qilish" oynasi — karta raqamlari va nusxalash tugmalari.
//
// Ilova bepul. Bu oyna Sozlamalardagi tugma bosilganda ochiladi —
// oʻzi chaqirilmaydi, reklama koʻrsatmaydi, hech narsani bloklamaydi.
#pragma once

#include <windows.h>

namespace rubai {

class DonateWindow {
public:
    DonateWindow();
    ~DonateWindow();
    DonateWindow(const DonateWindow&) = delete;
    DonateWindow& operator=(const DonateWindow&) = delete;

    // Ochadi (allaqachon ochiq boʻlsa oldinga chiqaradi).
    // owner — modal his berish uchun ota oyna; nullptr boʻlishi mumkin.
    void show(HINSTANCE instance, HWND owner);

    struct Impl;

private:
    Impl* d;
};

}  // namespace rubai
