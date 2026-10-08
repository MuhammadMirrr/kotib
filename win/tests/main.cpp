// Windows tomonining test yugurtirgichi.
//
// macOS'dagi `src/test.sh` + `tests/main.swift` ning ekvivalenti: XCTest yoʻq,
// paket menejeri yoʻq — oddiy bajariladigan fayl va bir nechta funksiya.
//
// Nima uchun bu bor: `matn_format.cpp` va shunga oʻxshash sof mantiq
// macOS bilan BIR XIL natija berishi shart. Bu shartni faqat test ushlab
// turadi — ikkala tarafni qoʻlda solishtirish uzoq va ishonchsiz.
//
// Ishga tushirish:
//   • Windows'da yoki UTM'dagi VM'da — `kotib-testlar.exe` (hamma testlar);
//   • macOS'da — `win/tests/mac/sinov.sh` (sof mantiq testlarining qismi,
//     Win32 API'ga bogʻlanmagani). Ikkinchisi kundalik ish uchun: Windows
//     mashinasini kutmasdan darhol javob beradi.
#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace kotib_test {

int otganlar = 0;
int xatolar = 0;

// Oxirgi boshlangan tekshiruv nomi. Test yiqilib qolsa (istisno, segfault)
// qaysi joyda ekanini shu aytadi — aks holda faqat «Abort trap» koʻrinadi
// va sababni topish uchun bisect qilishga toʻgʻri keladi.
std::wstring oxirgiNom;

// Konsolga UTF-8 chiqaradi. Windows konsoli sukut boʻyicha kod sahifada
// ishlaydi va oʻzbekcha matn kvadratchalarga aylanadi.
void chiqar(const std::wstring& s) {
#ifdef _WIN32
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return;
    std::string b(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, b.data(), n, nullptr, nullptr);
    fputs(b.c_str(), stdout);
#else
    // macOS: `wchar_t` — 32-bitli UTF-32, surrogat juftlik yoʻq.
    std::string b;
    for (wchar_t c : s) {
        const unsigned long u = static_cast<unsigned long>(c);
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
    fputs(b.c_str(), stdout);
#endif
    // Test yiqilib qolsa oxirgi qatorlar buferda yoʻqolmasin.
    fflush(stdout);
}

void tekshir(const std::wstring& nom, bool shart) {
    oxirgiNom = nom;
    if (shart) {
        ++otganlar;
    } else {
        ++xatolar;
        chiqar(L"  ✗ " + nom + L"\n");
    }
}

void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan) {
    oxirgiNom = nom;
    if (olingan == kutilgan) {
        ++otganlar;
    } else {
        ++xatolar;
        chiqar(L"  ✗ " + nom + L"\n    olingan:  «" + olingan + L"»\n    kutilgan: «" + kutilgan +
               L"»\n");
    }
}

}  // namespace kotib_test

// Har bir test fayli oʻz funksiyasini shu yerga qoʻshadi.
void matnFormatTestlari();
void vaqtFormatTestlari();
void tarjimaTestlari();
void jsonTestlari();
void llmTestlari();
void yangilanishTestlari();
void saqlanmaganTestlari();
void sozlamaTestlari();
void nutqTestlari();

#ifdef _WIN32
int wmain() {
    // Konsol chiqishini UTF-8 ga oʻtkazamiz.
    SetConsoleOutputCP(CP_UTF8);
#else
int main() {
#endif
    std::set_terminate([] {
        kotib_test::chiqar(L"\n✗ TEST YIQILDI. Oxirgi tekshiruv: «" + kotib_test::oxirgiNom +
                           L"»\n");
        std::abort();
    });

    struct {
        const wchar_t* nom;
        void (*f)();
    } testlar[] = {
        {L"matn_format", matnFormatTestlari},
        {L"vaqt_format", vaqtFormatTestlari},
        {L"tarjima", tarjimaTestlari},
        {L"json", jsonTestlari},
        {L"llm", llmTestlari},
        {L"yangilanish", yangilanishTestlari},
        {L"saqlanmagan", saqlanmaganTestlari},
        {L"sozlama", sozlamaTestlari},
        {L"nutq", nutqTestlari},
    };

    for (const auto& t : testlar) {
        kotib_test::chiqar(std::wstring(L"• ") + t.nom + L"\n");
        t.f();
    }

    kotib_test::chiqar(L"\n");
    if (kotib_test::xatolar == 0) {
        kotib_test::chiqar(L"✓ " + std::to_wstring(kotib_test::otganlar) +
                           L" ta tekshiruv oʻtdi\n");
        return 0;
    }
    kotib_test::chiqar(L"✗ " + std::to_wstring(kotib_test::xatolar) + L" ta xato, " +
                       std::to_wstring(kotib_test::otganlar) + L" ta oʻtdi\n");
    return 1;
}
