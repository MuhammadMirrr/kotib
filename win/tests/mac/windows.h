// macOS'dagi sinov quvuri uchun `windows.h` ning eng kichik oʻrnini bosuvchisi.
//
// Nega bor: Windows tomonining sof mantiq fayllari (`matn_format.cpp`,
// `matn_boluvchi.cpp`, `vaqt_format.cpp`) faqat bir necha Win32 chaqiruviga
// bogʻlangan. Ular uchun shu qalqonni yozib, testlarni **macOS'da darhol
// ishga tushirish** mumkin boʻladi — Windows mashinasi yoki VM kutmasdan.
//
// Bu RELIZGA kirmaydi va faqat `win/tests/mac/sinov.sh` ichida ishlatiladi.
// U yerda tekshirilmaydigan yagona narsa — ICU orqali jumlalarga boʻlish
// (bu yerda zaxira qoida ishlaydi) va UTF-16 surrogat juftliklari
// (macOS'da `wchar_t` 32-bitli). Ularni VM'dagi `kotib-testlar.exe` qamraydi.
#pragma once

#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <ctime>
#include <string>
#include <strings.h>

using HMODULE = void*;
using FARPROC = void*;
using BOOL = int;
using DWORD = unsigned int;
using UINT = unsigned int;
using WPARAM = unsigned long long;
using LPARAM = long long;

constexpr BOOL FALSE_ = 0;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

inline HMODULE LoadLibraryW(const wchar_t*) { return nullptr; }
inline FARPROC GetProcAddress(HMODULE, const char*) { return nullptr; }

// `CharUpperW` — bitta belgini bosh harfga oʻgiradi. macOS'da `towupper`
// bir xil ish qiladi (ikkalasi ham Unicode jadvaliga qaraydi).
inline void CharUpperW(wchar_t* s) {
    if (s) *s = static_cast<wchar_t>(towupper(*s));
}

// UCRT'dagi xavfsiz variantlar POSIX'da boshqa nom bilan keladi.
inline void localtime_s(std::tm* chiqish, const std::time_t* t) {
    if (chiqish && t) localtime_r(t, chiqish);
}

// `IsCharAlphaW` — belgi harfmi. macOS'da `iswalpha` shu ishni qiladi.
inline BOOL IsCharAlphaW(wchar_t c) { return iswalpha(c) ? 1 : 0; }
inline BOOL IsCharAlphaNumericW(wchar_t c) { return iswalnum(c) ? 1 : 0; }

// ---- Yoʻl va kodlash (faqat `tarjima_bridge.cpp` uchun) -------------------
//
// Windows koʻprigi model yoʻlini ANSI muammosidan qutqarish uchun qisqa (8.3)
// yoʻlga oʻgiradi. macOS'da bunday narsa yoʻq va kerak ham emas: bu yerda
// yoʻllar UTF-8 va toʻgʻridan-toʻgʻri ishlaydi. `GetShortPathNameW` nol
// qaytarganda koʻprik oʻzi UTF-8 yoʻlni ishlatadi — aynan shu kerak.
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

inline int MultiByteToWideChar(unsigned, unsigned long, const char* manba, int manbaN,
                               wchar_t* chiqish, int chiqishN) {
    std::string s =
        (manbaN < 0) ? std::string(manba) : std::string(manba, static_cast<size_t>(manbaN));
    // Nol bilan tugash Windows'dagidek: `manbaN < 0` boʻlsa u ham sanaladi.
    const int kerak = static_cast<int>(s.size()) + (manbaN < 0 ? 1 : 0);
    if (chiqishN == 0) return kerak;
    int i = 0;
    for (char c : s) {
        if (i >= chiqishN) return 0;
        chiqish[i++] = static_cast<unsigned char>(c);
    }
    if (manbaN < 0 && i < chiqishN) chiqish[i++] = 0;
    return i;
}

inline DWORD GetShortPathNameW(const wchar_t*, wchar_t*, DWORD) { return 0; }

inline int _wcsnicmp(const wchar_t* a, const wchar_t* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const wchar_t x = static_cast<wchar_t>(towlower(a[i]));
        const wchar_t y = static_cast<wchar_t>(towlower(b[i]));
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) break;
    }
    return 0;
}
