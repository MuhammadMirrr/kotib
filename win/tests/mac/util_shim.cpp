// macOS sinov quvuri uchun `core/util.cpp` ning eng kichik oʻrnini bosuvchisi.
//
// Sof mantiq fayllari (`json.cpp`) faqat matn kodlash funksiyalariga tegadi,
// asl `util.cpp` esa Win32 API'ga toʻla. Shu ikkitasi shu yerda yozilgan.
//
// macOS'da `wchar_t` — 32-bitli UTF-32, Windows'da 16-bitli UTF-16. Bu
// yerdagi oʻgirish shu farqni hisobga oladi.
#include "../../core/util.h"

namespace rubai {

std::string toUtf8(const std::wstring& w) {
    std::string b;
    b.reserve(w.size());
    for (size_t i = 0; i < w.size(); ++i) {
        unsigned long u = static_cast<unsigned long>(w[i]);
        // Windows'da (16-bitli wchar_t) surrogat juftligi kelishi mumkin.
        if (sizeof(wchar_t) == 2 && u >= 0xD800 && u <= 0xDBFF && i + 1 < w.size()) {
            const unsigned long past = static_cast<unsigned long>(w[i + 1]);
            if (past >= 0xDC00 && past <= 0xDFFF) {
                u = 0x10000 + ((u - 0xD800) << 10) + (past - 0xDC00);
                ++i;
            }
        }
        if (u < 0x80) {
            b += static_cast<char>(u);
        } else if (u < 0x800) {
            b += static_cast<char>(0xC0 | (u >> 6));
            b += static_cast<char>(0x80 | (u & 0x3F));
        } else if (u < 0x10000) {
            b += static_cast<char>(0xE0 | (u >> 12));
            b += static_cast<char>(0x80 | ((u >> 6) & 0x3F));
            b += static_cast<char>(0x80 | (u & 0x3F));
        } else {
            b += static_cast<char>(0xF0 | (u >> 18));
            b += static_cast<char>(0x80 | ((u >> 12) & 0x3F));
            b += static_cast<char>(0x80 | ((u >> 6) & 0x3F));
            b += static_cast<char>(0x80 | (u & 0x3F));
        }
    }
    return b;
}

std::wstring toWide(const std::string& s) {
    std::wstring w;
    w.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned long u = 0;
        int qoshimcha = 0;
        if (c < 0x80) {
            u = c;
            qoshimcha = 0;
        } else if (c < 0xE0) {
            u = c & 0x1F;
            qoshimcha = 1;
        } else if (c < 0xF0) {
            u = c & 0x0F;
            qoshimcha = 2;
        } else {
            u = c & 0x07;
            qoshimcha = 3;
        }
        ++i;
        for (int k = 0; k < qoshimcha && i < s.size(); ++k, ++i) {
            u = (u << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        }
        if (sizeof(wchar_t) == 2 && u > 0xFFFF) {
            u -= 0x10000;
            w += static_cast<wchar_t>(0xD800 + (u >> 10));
            w += static_cast<wchar_t>(0xDC00 + (u & 0x3FF));
        } else {
            w += static_cast<wchar_t>(u);
        }
    }
    return w;
}

void logWrite(const std::wstring&) {}

}  // namespace rubai
