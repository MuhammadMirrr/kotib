// Matnni faol oynaga kiritish.
//
// macOS versiyasidagi `Inserter` ning ekvivalenti (kirituvchi.swift).
// MUHIM FARQ: macOS'da bu Accessibility ruxsatini talab qiladi,
// Windows'da hech qanday ruxsat kerak emas.
#pragma once

#include <string>

#include "../core/config.h"

namespace rubai {

struct InsertResult {
    bool ok = false;
    std::wstring error;  // muvaffaqiyatsizlikda — oʻzbekcha sabab
};

// Matnni faol maydonga kiritadi.
//
// Paste rejimi: clipboard + Ctrl+V (tez, uzun matnlar uchun).
// Type rejimi:  belgima-belgi Unicode kiritish (paste'ni bloklaydigan
//               ilovalar — baʼzi bank va oʻyin oynalari uchun).
InsertResult insertText(const std::wstring& text, InsertMode mode);

// Faol oyna bizdan yuqori yaxlitlik darajasida (odatda administrator)
// ishlayaptimi.
//
// Windows'da past darajadagi jarayon yuqorisiga tugma yubora olmaydi
// (UIPI) — va SendInput buni XATO bilan ham bildirmaydi, matn jimgina
// yoʻqoladi. Shuning uchun bu KIRITISHDAN OLDIN tekshiriladi (E5).
bool foregroundWindowIsElevated();

// Matnni clipboard'ga qoʻyadi va shu yerda qoldiradi (foydalanuvchi oʻzi
// Ctrl+V bosadi). Diktovka transkripti kabi Windows clipboard tarixi va
// bulutiga chiqmaydi.
bool matnniClipboardgaQoy(const std::wstring& text);

}  // namespace rubai
