// Matnni tarjimaga tayyorlash (gaplarga boʻlish) va natijani qayta yigʻish.
// Interfeys va izohlar — `matn_boluvchi.h`.

#include "matn_boluvchi.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>

namespace rubai {
namespace MatnBoluvchi {

namespace {

// ---- Windows ICU -----------------------------------------------------------
//
// Jumla chegaralarini ICU beradi — macOS'da `enumerateSubstrings(.bySentences)`
// ham xuddi shu kutubxonaning ustida ishlaydi, shuning uchun natijalar mos
// tushadi.
//
// Nega qoʻlda `.` sanamaymiz: nuqta jumla oxiri BOʻLMAGAN holatlar koʻp —
// `t.me/dr_azamoff`, `3.14`, `v1.0`, qisqartmalar. ICU bularni biladi va bu
// 202 tilga baravar ishlaydi (xitoy `。`, arab `؟`).
//
// Nega dinamik yuklash: Windows ICU'ning sarlavha fayllari Windows SDK bilan
// keladi, llvm-mingw'da ular yoʻq. Eksport nomlari esa VERSIYASIZ (`ubrk_open`),
// oddiy ICU'dagidek `ubrk_open_74` emas — shuning uchun `GetProcAddress`
// bilan olish xavfsiz.
//
// Windows 10 1809 da kutubxona `icuuc.dll`, 1903 dan boshlab `icu.dll`.
// Ikkalasi ham topilmasa zaxira qoida ishlaydi (`zaxiraJumlalar`).

using UChar = wchar_t;
using UErrorCode = int32_t;
constexpr int32_t kUbrkSentence = 3;
constexpr int32_t kUbrkDone = -1;

using PFN_ubrk_open = void*(__cdecl*)(int32_t, const char*, const UChar*, int32_t, UErrorCode*);
using PFN_ubrk_first = int32_t(__cdecl*)(void*);
using PFN_ubrk_next = int32_t(__cdecl*)(void*);
using PFN_ubrk_close = void(__cdecl*)(void*);

struct Icu {
    PFN_ubrk_open open = nullptr;
    PFN_ubrk_first first = nullptr;
    PFN_ubrk_next next = nullptr;
    PFN_ubrk_close close = nullptr;
    bool bor() const { return open && first && next && close; }
};

const Icu& icu() {
    static const Icu qiymat = [] {
        Icu i;
        HMODULE m = LoadLibraryW(L"icu.dll");
        if (!m) m = LoadLibraryW(L"icuuc.dll");
        if (!m) return i;
        i.open = reinterpret_cast<PFN_ubrk_open>(
            reinterpret_cast<void*>(GetProcAddress(m, "ubrk_open")));
        i.first = reinterpret_cast<PFN_ubrk_first>(
            reinterpret_cast<void*>(GetProcAddress(m, "ubrk_first")));
        i.next = reinterpret_cast<PFN_ubrk_next>(
            reinterpret_cast<void*>(GetProcAddress(m, "ubrk_next")));
        i.close = reinterpret_cast<PFN_ubrk_close>(
            reinterpret_cast<void*>(GetProcAddress(m, "ubrk_close")));
        return i;
    }();
    return qiymat;
}

// ---- Kichik yordamchilar ---------------------------------------------------

bool boshliqmi(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\f' || c == L'\v' ||
           c == 0x00A0;
}

std::wstring kes(const std::wstring& s) {
    size_t a = 0, z = s.size();
    while (a < z && boshliqmi(s[a])) ++a;
    while (z > a && boshliqmi(s[z - 1])) --z;
    return s.substr(a, z - a);
}

bool boshmi(const std::wstring& s) { return kes(s).empty(); }

bool harfmi(wchar_t c) {
    // `iswalpha` UCRT'da lokalga bogʻliq va ASCII'dan tashqarida ishonchsiz.
    // Bizga kerak boʻlgani — «bu belgi soʻz qismimi» degan qoʻpol savol.
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return true;
    if (c < 0x80) return false;
    // Lotin kengaytmalari, kirill, arab, ibroniy, hind, CJK va boshqalar.
    // Emoji va tinish belgilari bu oraliqlarga tushmaydi.
    return (c >= 0x00C0 && c <= 0x024F) || (c >= 0x0370 && c <= 0x1FFF) ||
           (c >= 0x2C00 && c <= 0x2DFF) || (c >= 0x2E80 && c <= 0xD7FF) ||
           (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD);
}

bool raqammi(wchar_t c) { return c >= L'0' && c <= L'9'; }

// Kichik harfmi. `towlower`/`towupper` UCRT'da lokalga bogʻliq va ASCII'dan
// tashqarida ishonchsiz, shuning uchun oraliqlar qoʻlda: lotin (oʻzbek) va
// kirill (rus). Boshqa yozuvlarda registr tushunchasi yoʻq yoki bizga kerak
// emas — bu faqat zaxira jumla boʻluvchisining bitta qoidasi uchun.
bool kichikHarfmi(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= 0x00DF && c <= 0x00FF) ||  // lotin-1 kichik harflari
           (c >= 0x0430 && c <= 0x044F) ||                              // kirill а–я
           c == 0x0451;                                                 // ё
}

// Emoji va boshqa belgi-simvollar. Model ularni bilmaydi.
//
// macOS'da bu `Unicode.Scalar.Properties.isEmoji`. Windows'da bunday xossa
// jadvali yoʻq, shuning uchun oraliqlar qoʻlda sanab chiqilgan — natija
// amalda bir xil, chunki harf/raqam/tinish belgisi bu oraliqlarga tushmaydi.
bool tarjimaQilinmaydi(char32_t u) {
    if (u == 0x00A9 || u == 0x00AE || u == 0x2122) return true;  // © ® ™
    if (u == 0x200D || u == 0xFE0F || u == 0x20E3) return true;  // ZWJ, VS16
    return (u >= 0x2190 && u <= 0x21FF) ||                       // strelkalar
           (u >= 0x2300 && u <= 0x23FF) ||                       // texnik belgilar (⌚ ⏰)
           (u >= 0x25A0 && u <= 0x25FF) ||                       // geometrik shakllar
           (u >= 0x2600 && u <= 0x27BF) ||                       // turli belgilar, dingbats
           (u >= 0x2B00 && u <= 0x2BFF) ||                       // ⬛ ⭐
           (u >= 0x1F000 && u <= 0x1FAFF);                       // emoji, bayroqlar, teri rangi
}

// UTF-16 juftligidan kod nuqtasini oladi. `i` surrogat juftlikda bir qadam
// oldinga suriladi.
char32_t kodNuqtasi(const std::wstring& s, size_t& i) {
    const wchar_t c = s[i];
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 &&
        s[i + 1] <= 0xDFFF) {
        const char32_t u = 0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10) +
                           (static_cast<char32_t>(s[i + 1]) - 0xDC00);
        ++i;
        return u;
    }
    return c;
}

void almashtir(std::wstring& s, const std::wstring& eski, const std::wstring& yangi) {
    if (eski.empty()) return;
    size_t joy = 0;
    while ((joy = s.find(eski, joy)) != std::wstring::npos) {
        s.replace(joy, eski.size(), yangi);
        joy += yangi.size();
    }
}

// ---- Zaxira jumla boʻluvchi ------------------------------------------------
//
// ICU topilmaganda ishlaydi. Qoida: jumla tugatuvchi belgidan KEYIN boʻsh joy
// (yoki matn oxiri) kelsa — chegara. Aynan shu shart `3.14`, `v1.0` va
// `t.me/dr_azamoff` ni butun qoldiradi.
std::vector<std::wstring> zaxiraJumlalar(const std::wstring& qator) {
    // Ikki guruh, va farq muhim.
    //
    // Lotin belgilaridan (`.`, `!`, `?`, `…`) keyin BOʻSH JOY talab qilinadi:
    // aynan shu shart `3.14`, `v1.0` va `t.me/dr_azamoff` ni butun qoldiradi.
    //
    // CJK, arab va hind belgilaridan keyin boʻsh joy talab QILINMAYDI —
    // bu yozuvlarda jumlalar odatda boʻshliqsiz ulanadi (`第一句。第二句。`)
    // va bu belgilar son yoki manzil ichida uchramaydi.
    static const std::wstring kLotin = L".!?…";
    static const std::wstring kBoshqa = L"。！？؟।";
    const std::wstring kTugatuvchilar = kLotin + kBoshqa;

    std::vector<std::wstring> natija;
    size_t boshi = 0;
    for (size_t i = 0; i < qator.size(); ++i) {
        if (kTugatuvchilar.find(qator[i]) == std::wstring::npos) continue;
        // Ketma-ket kelgan belgilarni (`?!`, `...`) bitta chegara deb olamiz.
        size_t j = i;
        while (j + 1 < qator.size() && kTugatuvchilar.find(qator[j + 1]) != std::wstring::npos) {
            ++j;
        }
        // Yopuvchi qavs va qoʻshtirnoq chegaraning ichida qoladi.
        while (j + 1 < qator.size() && (qator[j + 1] == L'"' || qator[j + 1] == L')' ||
                                        qator[j + 1] == L'»' || qator[j + 1] == L'”')) {
            ++j;
        }
        const bool boshliqShart = kLotin.find(qator[j]) != std::wstring::npos;
        if (boshliqShart && j + 1 < qator.size() && !boshliqmi(qator[j + 1])) {
            i = j;
            continue;
        }

        // Keyingi soʻz kichik harf bilan boshlansa — bu jumla oxiri emas.
        // ICU shu qoidani biladi va aynan shu holat sinovda farq bergan:
        // «E-e, anavi... nima edi... ha, hujjat.» — ICU buni bitta jumla
        // deb oʻqiydi, oddiy «nuqta + boʻshliq» qoidasi esa uchga boʻlardi.
        if (boshliqShart) {
            size_t keyingi = j + 1;
            while (keyingi < qator.size() && boshliqmi(qator[keyingi])) ++keyingi;
            if (keyingi < qator.size() && kichikHarfmi(qator[keyingi])) {
                i = j;
                continue;
            }
        }

        natija.push_back(qator.substr(boshi, j + 1 - boshi));
        boshi = j + 1;
        i = j;
    }
    if (boshi < qator.size()) natija.push_back(qator.substr(boshi));
    return natija;
}

// ---- Himoyalangan oraliqlar ------------------------------------------------
//
// macOS'da bu `NSDataDetector`. Windows'da bunday aniqlagich yoʻq va uni
// takrorlash mumkin emas, shuning uchun qoida ATAYLAB torroq: faqat
// SHUBHASIZ havolalar himoyalanadi —
//   • sxemali manzil (`https://…`)
//   • `www.` bilan boshlanadigan
//   • ichida `/` bor domen (`t.me/dr_azamoff` — asl muammo shu edi)
//   • email
//   • `@handle`, `#hashtag`
// Bu roʻyxatda `example.uz` kabi yalangʻoch domen YOʻQ: uni oddiy matndan
// («shu.bu» kabi terim xatosi) ajratib boʻlmaydi va notoʻgʻri himoya jumlani
// tarjimasiz qoldirardi — bu havolani buzishdan koʻra yomonroq.

bool manzilBelgisi(wchar_t c) {
    return harfmi(c) || raqammi(c) || c == L'.' || c == L'-' || c == L'_' || c == L'/' ||
           c == L':' || c == L'?' || c == L'=' || c == L'&' || c == L'%' || c == L'+' ||
           c == L'#' || c == L'~' || c == L'@' || c == L',' || c == L';' || c == L'!' ||
           c == L'(' || c == L')';
}

// Oxiridagi tinish belgilarini oraliqdan chiqaradi: «t.me/x.» dagi nuqta
// jumlaning oʻzinikidir.
size_t oxirniKes(const std::wstring& s, size_t boshi, size_t oxiri) {
    while (oxiri > boshi) {
        const wchar_t c = s[oxiri - 1];
        if (c == L'.' || c == L',' || c == L';' || c == L':' || c == L'!' || c == L'?' ||
            c == L')' || c == L'»' || c == L'”') {
            --oxiri;
        } else {
            break;
        }
    }
    return oxiri;
}

bool havolaMi(const std::wstring& soz) {
    if (soz.size() < 4) return false;

    auto boshlanadi = [&](const wchar_t* p) {
        const size_t n = wcslen(p);
        return soz.size() >= n && _wcsnicmp(soz.c_str(), p, n) == 0;
    };
    if (boshlanadi(L"http://") || boshlanadi(L"https://") || boshlanadi(L"ftp://") ||
        boshlanadi(L"www.")) {
        return true;
    }

    const size_t chiziq = soz.find(L'/');
    const size_t nuqta = soz.find(L'.');
    const size_t kuchuk = soz.find(L'@');

    // Email: `x@y.z`
    if (kuchuk != std::wstring::npos && kuchuk > 0 && nuqta != std::wstring::npos &&
        nuqta > kuchuk + 1 && nuqta + 2 < soz.size() && chiziq == std::wstring::npos) {
        return true;
    }

    // Ichida `/` bor domen: nuqta chiziqdan oldin turishi kerak.
    if (chiziq != std::wstring::npos && nuqta != std::wstring::npos && nuqta < chiziq &&
        nuqta > 0 && chiziq > nuqta + 1) {
        // Nuqtadan keyingi qism harf bilan boshlansin — `1.5/2` havola emas.
        return harfmi(soz[nuqta + 1]);
    }
    return false;
}

bool handleMi(const std::wstring& soz) {
    if (soz.size() < 3) return false;
    if (soz[0] != L'@' && soz[0] != L'#') return false;
    for (size_t i = 1; i < soz.size(); ++i) {
        if (!harfmi(soz[i]) && !raqammi(soz[i]) && soz[i] != L'_') return false;
    }
    return true;
}

}  // namespace

bool icuBormi() { return icu().bor(); }

// ---- Jumlalarga boʻlish ----------------------------------------------------

std::vector<std::wstring> jumlalargaBol(const std::wstring& qator) {
    std::vector<std::wstring> xom;

    const Icu& i = icu();
    if (i.bor()) {
        UErrorCode xato = 0;
        void* it =
            i.open(kUbrkSentence, "uz", qator.c_str(), static_cast<int32_t>(qator.size()), &xato);
        if (it && xato <= 0) {
            int32_t oldingi = i.first(it);
            for (int32_t joy = i.next(it); joy != kUbrkDone; joy = i.next(it)) {
                if (joy > oldingi) {
                    xom.push_back(qator.substr(static_cast<size_t>(oldingi),
                                               static_cast<size_t>(joy - oldingi)));
                }
                oldingi = joy;
            }
            i.close(it);
        } else if (it) {
            i.close(it);
        }
    }

    if (xom.empty()) xom = zaxiraJumlalar(qator);

    std::vector<std::wstring> natija;
    for (const auto& s : xom) {
        const std::wstring t = kes(s);
        if (!t.empty()) natija.push_back(t);
    }
    return natija;
}

std::vector<std::pair<size_t, size_t>> himoyalanganlar(const std::wstring& jumla) {
    std::vector<std::pair<size_t, size_t>> natija;

    size_t i = 0;
    while (i < jumla.size()) {
        if (boshliqmi(jumla[i])) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < jumla.size() && !boshliqmi(jumla[j]) && manzilBelgisi(jumla[j])) ++j;
        if (j == i) {
            ++i;
            continue;
        }

        const size_t oxiri = oxirniKes(jumla, i, j);
        if (oxiri > i) {
            const std::wstring soz = jumla.substr(i, oxiri - i);
            if (havolaMi(soz) || handleMi(soz)) natija.emplace_back(i, oxiri);
        }
        i = j;
    }
    return natija;
}

// ---- Boʻlak yasash ---------------------------------------------------------

namespace {

Bolak bolakYasa(const std::wstring& jumla) {
    std::wstring toza, qoshimcha;
    for (size_t i = 0; i < jumla.size(); ++i) {
        const size_t boshi = i;
        const char32_t u = kodNuqtasi(jumla, i);
        if (u == 0x2014 || u == 0x2013) {  // — –
            toza += L'-';
        } else if (tarjimaQilinmaydi(u)) {
            qoshimcha.append(jumla, boshi, i - boshi + 1);
        } else {
            toza.append(jumla, boshi, i - boshi + 1);
        }
    }

    // Belgi olib tashlangan joyda ikki boʻshliq qolishi mumkin.
    while (toza.find(L"  ") != std::wstring::npos) almashtir(toza, L"  ", L" ");
    const std::wstring kesilgan = kes(toza);

    // Harf ham, raqam ham yoʻq — tarjima qiladigan narsa qolmadi.
    bool bormi = false;
    for (wchar_t c : kesilgan) {
        if (harfmi(c) || raqammi(c)) {
            bormi = true;
            break;
        }
    }
    if (!bormi) return Bolak{Bolak::Tur::Xom, jumla, L""};

    const std::wstring q = qoshimcha.empty() ? L"" : L" " + kes(qoshimcha);
    return Bolak{Bolak::Tur::Jumla, kesilgan, q};
}

// Harfsiz boʻlak (emoji, belgi) oʻzidan oldingi jumlaga qoʻshiladi —
// tarjimadan keyin oʻz joyiga qaytadi. Havola bundan MUSTASNO: u alohida
// xom boʻlib turishi kerak, aks holda «Manba: havola» → «havola Manba:»
// tartibi buzilardi.
void qoshib(std::vector<Bolak>& natija, const Bolak& bolak) {
    if (bolak.tur == Bolak::Tur::Xom && himoyalanganlar(bolak.matn).empty() && !natija.empty() &&
        natija.back().tur == Bolak::Tur::Jumla) {
        natija.back().qoshimcha += L" " + kes(bolak.matn);
        return;
    }
    natija.push_back(bolak);
}

// Jumlani himoyalangan oraliqlar boʻyicha kesadi.
std::vector<Bolak> himoyaBoyichaAjrat(const std::wstring& jumla) {
    const auto oraliqlar = himoyalanganlar(jumla);
    if (oraliqlar.empty()) return {bolakYasa(jumla)};

    std::vector<Bolak> natija;
    size_t joriy = 0;
    for (const auto& r : oraliqlar) {
        const std::wstring oldi = jumla.substr(joriy, r.first - joriy);
        if (!boshmi(oldi)) natija.push_back(bolakYasa(oldi));
        natija.push_back(Bolak{Bolak::Tur::Xom, jumla.substr(r.first, r.second - r.first), L""});
        joriy = r.second;
    }
    const std::wstring qoldiq = jumla.substr(joriy);
    if (!boshmi(qoldiq)) natija.push_back(bolakYasa(qoldiq));
    return natija;
}

std::vector<Bolak> qatorniBol(const std::wstring& qator) {
    if (boshmi(qator)) return {};
    std::vector<Bolak> natija;
    for (const auto& jumla : jumlalargaBol(qator)) {
        for (const auto& b : himoyaBoyichaAjrat(jumla)) qoshib(natija, b);
    }
    return natija;
}

}  // namespace

// ---- Ommaviy interfeys -----------------------------------------------------

Qatorlar bol(const std::wstring& matn) {
    if (boshmi(matn)) return {};

    Qatorlar qatorlar;
    size_t boshi = 0;
    for (;;) {
        const size_t joy = matn.find(L'\n', boshi);
        std::wstring qator =
            matn.substr(boshi, joy == std::wstring::npos ? std::wstring::npos : joy - boshi);
        // Windows qator oxiri `\r\n` — `\r` matnga qoʻshilib ketmasin.
        if (!qator.empty() && qator.back() == L'\r') qator.pop_back();
        qatorlar.push_back(qatorniBol(qator));
        if (joy == std::wstring::npos) break;
        boshi = joy + 1;
    }

    // Oxiridagi boʻsh qatorlar natijaga hech narsa qoʻshmaydi.
    while (!qatorlar.empty() && qatorlar.back().empty()) qatorlar.pop_back();
    return qatorlar;
}

std::vector<std::wstring> jumlalar(const Qatorlar& qatorlar) {
    std::vector<std::wstring> natija;
    for (const auto& qator : qatorlar) {
        for (const auto& b : qator) {
            if (b.tur == Bolak::Tur::Jumla) natija.push_back(b.matn);
        }
    }
    return natija;
}

size_t jumlalarSoni(const Qatorlar& qatorlar) { return jumlalar(qatorlar).size(); }

std::wstring yig(const Qatorlar& qatorlar, const std::vector<std::wstring>& tarjimalar) {
    size_t i = 0;
    std::wstring chiqish;
    for (size_t q = 0; q < qatorlar.size(); ++q) {
        if (q > 0) chiqish += L'\n';
        std::wstring qator;
        for (const auto& b : qatorlar[q]) {
            if (!qator.empty()) qator += L' ';
            if (b.tur == Bolak::Tur::Xom) {
                qator += b.matn;
            } else {
                qator += (i < tarjimalar.size() ? tozala(tarjimalar[i]) : b.matn);
                ++i;
                qator += b.qoshimcha;
            }
        }
        chiqish += qator;
    }
    return chiqish;
}

std::wstring tozala(const std::wstring& s) {
    std::wstring t = s;
    almashtir(t, L"<unk>", L"");
    for (const wchar_t* b : {L",", L".", L":", L";", L"!", L"?", L")", L"»", L"”"}) {
        almashtir(t, std::wstring(L" ") + b, b);
    }
    for (const wchar_t* b : {L"(", L"«", L"“"}) { almashtir(t, std::wstring(b) + L" ", b); }
    almashtir(t, L" - ", L" — ");
    while (t.find(L"  ") != std::wstring::npos) almashtir(t, L"  ", L" ");
    return kes(t);
}

}  // namespace MatnBoluvchi
}  // namespace rubai
