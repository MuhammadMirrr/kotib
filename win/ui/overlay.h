// Suzuvchi holat oynasi (HUD).
//
// macOS versiyasidagi `Overlay` ning ekvivalenti (overlay.swift).
// Eng muhim xususiyati: fokusni OʻGʻIRLAMAYDI — foydalanuvchi yozayotgan
// maydon faol qolishi kerak, aks holda matn notoʻgʻri joyga tushadi.
#pragma once

#include <string>

namespace rubai {

enum class OverlayIcon {
    Recording,  // 🔴 yozilmoqda
    Working,    // ⏳ matnga oʻgirilmoqda
    Done,       // ✅ tayyor
    Warning,    // ⚠️ ogohlantirish
};

class Overlay {
public:
    Overlay();
    ~Overlay();
    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;

    bool create(HINSTANCE instance);

    // Matnli holat koʻrsatadi. autoHideMs > 0 boʻlsa shu vaqtdan keyin
    // oʻzi yashiriladi.
    void show(OverlayIcon icon, const std::wstring& text, int autoHideMs = 0);

    void hide();

    // Oyna protsedurasi (erkin funksiya) shu tuzilmaga murojaat qiladi,
    // shuning uchun eʼlon ochiq; taʼrifi .cpp faylda.
    struct Impl;

private:
    Impl* d;
};

}  // namespace rubai
