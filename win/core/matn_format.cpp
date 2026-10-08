// Segmentlardan oʻqishga qulay matn — macOS'dagi `text_format.swift` egizagi.
// Interfeys va izohlar — `matn_format.h`.

#include "matn_format.h"

#include <windows.h>

#include <algorithm>

namespace rubai {

namespace {

// oʻ / gʻ uchun — MODIFIER LETTER TURNED COMMA
constexpr wchar_t kBurilganVergul = L'ʻ';
// tutuq belgisi uchun — MODIFIER LETTER APOSTROPHE
constexpr wchar_t kTutuq = L'ʼ';

// Whisper aralash chiqaradigan apostrof variantlari. Roʻyxat
// `text_format.swift` dagi bilan bir xil.
bool apostrofmi(wchar_t ch) {
    switch (ch) {
        case L'\'':  // ASCII
        case L'‘':   // '
        case L'’':   // '
        case L'`':   // `
        case L'´':   // ´
        case L'ʻ':   // ʻ
        case L'ʼ':   // ʼ
            return true;
        default: return false;
    }
}

bool harfmi(wchar_t ch) {
    // IsCharAlphaW Unicode'ni biladi — lotin, kirill va boshqalarni ham.
    // `iswalpha` C lokalidan bogʻliq va lotin boʻlmagan yozuvlarda yolgʻon
    // javob berishi mumkin.
    return IsCharAlphaW(ch) != FALSE;
}

// Bitta belgini bosh harfga oʻgiradi. CharUpperW Windows'ning Unicode
// jadvalidan foydalanadi.
wchar_t kattaHarf(wchar_t ch) {
    wchar_t bufer[2] = {ch, 0};
    CharUpperW(bufer);
    return bufer[0];
}

std::wstring kesish(const std::wstring& s) {
    size_t boshi = s.find_first_not_of(L" \t\r\n");
    if (boshi == std::wstring::npos) return {};
    size_t oxiri = s.find_last_not_of(L" \t\r\n");
    return s.substr(boshi, oxiri - boshi + 1);
}

// Ortiqcha boʻshliqlar va tinish belgisi oldidagi boʻshliq.
std::wstring boshliqlarniTozala(const std::wstring& s) {
    std::wstring t = s;

    // MUHIM: har bir belgi uchun BITTA oʻtish, takrorlanmaydi. macOS'da bu
    // `replacingOccurrences` bilan qilinadi va u ham bitta oʻtish qiladi.
    // Takrorlanadigan qilib yozilsa «a  ,» → «a,» chiqadi, macOS'da esa
    // «a ,» — yaʼni ikki platforma boshqa natija berardi.
    for (const wchar_t* belgi : {L" ,", L" .", L" !", L" ?", L" :", L" ;"}) {
        std::wstring chiqish;
        chiqish.reserve(t.size());
        size_t i = 0;
        while (i < t.size()) {
            if (t[i] == belgi[0] && i + 1 < t.size() && t[i + 1] == belgi[1]) {
                chiqish += belgi[1];  // boʻshliqni tashlab, belgini qoʻyamiz
                i += 2;
            } else {
                chiqish += t[i++];
            }
        }
        t = std::move(chiqish);
    }
    size_t p;
    while ((p = t.find(L"  ")) != std::wstring::npos) { t.erase(p, 1); }
    return kesish(t);
}

// Paragraf chegarasi qoidalari — macOS bilan bir xil qiymatlar.
constexpr double kQatiyPauza = 1.5;    // s — shartsiz yangi paragraf
constexpr double kYumshoqPauza = 0.8;  // s — paragraf yetarlicha uzun boʻlsa
constexpr size_t kMinParagraf = 200;   // belgi

}  // namespace

std::wstring apostrofniBirxillashtir(const std::wstring& s, Apostrof uslub) {
    std::wstring natija;
    natija.reserve(s.size());

    wchar_t oldingi = 0;
    for (wchar_t ch : s) {
        if (apostrofmi(ch)) {
            const bool harfdanKeyin =
                (oldingi == L'o' || oldingi == L'O' || oldingi == L'g' || oldingi == L'G');
            if (uslub == Apostrof::Oddiy) {
                natija += L'\'';
            } else {
                natija += harfdanKeyin ? kBurilganVergul : kTutuq;
            }
        } else {
            natija += ch;
        }
        oldingi = ch;
    }
    return natija;
}

std::wstring takrorniQisqartir(const std::wstring& s) {
    // Soʻzlar — faqat L' ' bilan ajratilgan, boʻsh boʻlaklarsiz (Swift'dagi
    // `split(separator: " ")` bilan bir xil).
    std::vector<std::wstring> soz;
    size_t b = 0;
    while (b < s.size()) {
        const size_t e = s.find(L' ', b);
        const size_t oxir = e == std::wstring::npos ? s.size() : e;
        if (oxir > b) soz.push_back(s.substr(b, oxir - b));
        b = oxir + 1;
    }

    auto tengmi = [&](size_t a, size_t c, size_t k) {
        for (size_t j = 0; j < k; j++) {
            if (soz[a + j] != soz[c + j]) return false;
        }
        return true;
    };

    std::vector<std::wstring> natija;
    natija.reserve(soz.size());
    bool qisqardi = false;
    size_t i = 0;
    while (i < soz.size()) {
        bool olindi = false;
        for (size_t k = 1; k <= kTakrorIboraMax && i + k * kTakrorMin <= soz.size(); k++) {
            size_t marta = 1;
            while (i + (marta + 1) * k <= soz.size() && tengmi(i + marta * k, i, k)) marta++;
            if (marta >= kTakrorMin) {
                for (size_t j = 0; j < k; j++) natija.push_back(soz[i + j]);
                i += marta * k;
                qisqardi = olindi = true;
                break;
            }
        }
        if (!olindi) natija.push_back(soz[i++]);
    }
    if (!qisqardi) return s;

    std::wstring chiqish;
    for (size_t j = 0; j < natija.size(); j++) {
        if (j > 0) chiqish += L' ';
        chiqish += natija[j];
    }
    return chiqish;
}

std::wstring matnniTayyorla(const std::wstring& xom, Apostrof apostrof) {
    return apostrofniBirxillashtir(takrorniQisqartir(xom), apostrof);
}

std::wstring jumlaBoshiniKattalashtir(const std::wstring& s) {
    std::wstring natija;
    natija.reserve(s.size());

    bool kutilyapti = true;  // keyingi harf bosh boʻlsinmi
    for (wchar_t ch : s) {
        if (kutilyapti && harfmi(ch)) {
            natija += kattaHarf(ch);
            kutilyapti = false;
        } else {
            natija += ch;
            if (ch == L'.' || ch == L'!' || ch == L'?' || ch == L'\n') { kutilyapti = true; }
        }
    }
    return natija;
}

std::wstring xomMatn(const std::vector<Segment>& segmentlar) {
    std::wstring natija;
    for (const auto& s : segmentlar) {
        const std::wstring m = kesish(s.matn);
        if (m.empty()) continue;
        if (!natija.empty()) natija += L' ';
        natija += m;
    }
    return natija;
}

std::wstring chiroyliMatn(const std::vector<Segment>& segmentlar, Apostrof apostrof) {
    std::vector<Segment> toza;
    for (const auto& s : segmentlar) {
        if (!kesish(s.matn).empty()) toza.push_back(s);
    }
    if (toza.empty()) return {};

    std::vector<std::wstring> paragraflar;
    std::wstring joriy;

    for (size_t i = 0; i < toza.size(); ++i) {
        const std::wstring matn = kesish(toza[i].matn);
        joriy = joriy.empty() ? matn : joriy + L" " + matn;

        if (i + 1 >= toza.size()) break;

        const double pauza = toza[i + 1].t0 - toza[i].t1;
        const bool bolinsin =
            pauza > kQatiyPauza || (pauza > kYumshoqPauza && joriy.size() >= kMinParagraf);
        if (bolinsin) {
            paragraflar.push_back(joriy);
            joriy.clear();
        }
    }
    if (!joriy.empty()) paragraflar.push_back(joriy);

    std::wstring birlashgan;
    for (size_t i = 0; i < paragraflar.size(); ++i) {
        if (i) birlashgan += L"\n\n";
        birlashgan += boshliqlarniTozala(paragraflar[i]);
    }

    return jumlaBoshiniKattalashtir(apostrofniBirxillashtir(birlashgan, apostrof));
}

}  // namespace rubai
