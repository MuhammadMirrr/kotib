// Avto-yangilanish siyosati, URL ruxsat roʻyxati, base64 va manifest —
// `yangilanish_siyosat.h` ga qarang. Versiya taqqoslash — `versiya.cpp` da.
//
// macOS'dagi egizagi: `src/yangilanish_siyosat.swift`. Har qoida u yerdagi
// bilan bir xil boʻlishi SHART — umumiy jadval
// (`tests/umumiy/yangilanish_holatlari.def`) farqni darhol ushlaydi.
#include "yangilanish_siyosat.h"

#include "imzo.h"
#include "json.h"
#include "util.h"

#include <cmath>

namespace rubai {

namespace {

bool raqammi(char c) { return c >= '0' && c <= '9'; }
bool harfmi(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// JSON butun soni [quyi, yuqori] oraligʻida. Maydon yoʻq — `sukut` (u ham
// manfiy boʻlsa — majburiy maydon, xato). `true`/`false`, satr, kasr — son emas.
bool butunSon(const Json& j, long long sukut, long long quyi, long long yuqori, long long& natija) {
    if (!j.bormi()) {
        if (sukut < 0) return false;
        natija = sukut;
        return true;
    }
    if (j.tur() != Json::Tur::Son) return false;
    const double d = j.son();
    if (std::floor(d) != d || d < static_cast<double>(quyi) || d > static_cast<double>(yuqori))
        return false;
    natija = static_cast<long long>(d);
    return true;
}

bool satrMaydon(const Json& j, std::string& natija) {
    if (j.tur() != Json::Tur::Satr) return false;
    natija = toUtf8(j.satr());
    return true;
}

bool sha256Formatimi(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s)
        if (!raqammi(c) && !(c >= 'a' && c <= 'f')) return false;
    return true;
}

// ---- qisqaIzoh yordamchilari: Unicode skalyarlari, macOS bilan bir xil ----

// UTF-8 → kod nuqtalari. Buzuq bayt — U+FFFD (izoh JSON'dan keladi va u
// yaroqli UTF-8, bu faqat ehtiyot uchun).
std::u32string kodNuqtalari(std::string_view s) {
    std::u32string r;
    for (size_t i = 0; i < s.size();) {
        const auto b = static_cast<unsigned char>(s[i]);
        const int n = b < 0x80         ? 1
                      : (b >> 5) == 6  ? 2
                      : (b >> 4) == 14 ? 3
                      : (b >> 3) == 30 ? 4
                                       : 0;
        char32_t c = n == 1 ? b : n == 2 ? (b & 0x1F) : n == 3 ? (b & 0x0F) : (b & 0x07);
        bool ok = n > 0 && i + n <= s.size();
        for (int k = 1; ok && k < n; ++k) {
            const auto d = static_cast<unsigned char>(s[i + k]);
            ok = (d >> 6) == 2;
            c = (c << 6) | (d & 0x3F);
        }
        r += ok ? c : U'�';
        i += ok ? n : 1;
    }
    return r;
}

std::string utf8ga(std::u32string_view s) {
    std::string r;
    for (char32_t c : s) {
        if (c < 0x80) {
            r += static_cast<char>(c);
        } else if (c < 0x800) {
            r += static_cast<char>(0xC0 | (c >> 6));
            r += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            r += static_cast<char>(0xE0 | (c >> 12));
            r += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            r += static_cast<char>(0xF0 | (c >> 18));
            r += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            r += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return r;
}

// Swift `Character.isNewline` va `isWhitespace` (Unicode White_Space) toʻplamlari.
bool yangiQatormi(char32_t c) {
    return (c >= 0x0A && c <= 0x0D) || c == 0x85 || c == 0x2028 || c == 0x2029;
}
bool boshliqmi(char32_t c) {
    return yangiQatormi(c) || c == 0x09 || c == 0x20 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000;
}

bool hmi(char c) { return c == 'h' || c == 'H'; }
bool sarlavhaRaqamimi(char c) { return c >= '1' && c <= '6'; }

// macOS: `(?is)<h[1-6][^>]*>.*?</h[1-6]>` → "\n". Belgilar ASCII, shuning uchun
// UTF-8 baytlari ustida xavfsiz: koʻp baytli belgi ichida ASCII bayt boʻlmaydi.
std::string sarlavhalarsiz(std::string_view s) {
    std::string r;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '<' && i + 2 < s.size() && hmi(s[i + 1]) && sarlavhaRaqamimi(s[i + 2])) {
            const size_t ochiq = s.find('>', i + 3);
            size_t k = ochiq == std::string_view::npos ? s.size() : ochiq + 1;
            for (; k + 4 < s.size(); ++k) {
                if (s[k] == '<' && s[k + 1] == '/' && hmi(s[k + 2]) && sarlavhaRaqamimi(s[k + 3]) &&
                    s[k + 4] == '>')
                    break;
            }
            if (k + 4 < s.size()) {
                r += '\n';
                i = k + 5;
                continue;
            }
        }
        r += s[i++];
    }
    return r;
}

// macOS: `<[^>]+>` → "\n".
std::string teglarsiz(std::string_view s) {
    std::string r;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '<' && i + 1 < s.size() && s[i + 1] != '>') {
            const size_t k = s.find('>', i + 1);
            if (k != std::string_view::npos) {
                r += '\n';
                i = k + 1;
                continue;
            }
        }
        r += s[i++];
    }
    return r;
}

}  // namespace

// ---- Jadval --------------------------------------------------------------------

long long qaytaUrinishKechikishi(int urinish) {
    switch (urinish) {
        case 0: return 15 * 60;
        case 1: return 60 * 60;
        case 2: return 4 * 3600;
        default: return -1;
    }
}

bool uygonishdaTekshirish(long long oxirgiMuvaffaqiyat, long long hozir) {
    if (oxirgiMuvaffaqiyat < 0 || hozir < oxirgiMuvaffaqiyat) return true;
    return hozir - oxirgiMuvaffaqiyat >= 24 * 3600;
}

bool ornatishMumkinmi(bool band, long long oxirgiFaollik, long long kutishBoshlandi,
                      long long hozir) {
    if (band) return false;
    if (oxirgiFaollik < 0) return true;
    const long long kerak = hozir - kutishBoshlandi >= 24 * 3600 ? 60 : 120;
    return hozir - oxirgiFaollik >= kerak;
}

std::string qisqaIzoh(std::string_view xom) {
    const std::u32string matn = kodNuqtalari(teglarsiz(sarlavhalarsiz(xom)));
    std::u32string natija;
    size_t i = 0;
    while (i < matn.size()) {
        // Bitta qator: keyingi yangi qatorgacha (boʻsh qatorlar tashlanadi).
        size_t oxiri = i;
        while (oxiri < matn.size() && !yangiQatormi(matn[oxiri])) ++oxiri;
        std::u32string t = matn.substr(i, oxiri - i);
        i = oxiri + 1;

        size_t b = 0, e = t.size();
        while (b < e && boshliqmi(t[b])) ++b;
        while (e > b && boshliqmi(t[e - 1])) --e;
        t = t.substr(b, e - b);
        if (t.empty() || t[0] == U'#') continue;
        if (t.size() >= 2 && (t[0] == U'-' || t[0] == U'*') && t[1] == U' ') t.erase(0, 2);

        std::u32string y;
        for (size_t k = 0; k < t.size(); ++k) {
            if (t[k] == U'*' && k + 1 < t.size() && t[k + 1] == U'*') {
                ++k;
                continue;
            }
            y += t[k];
        }

        std::u32string bir;
        for (size_t k = 0; k < y.size();) {
            while (k < y.size() && boshliqmi(y[k])) ++k;
            if (k == y.size()) break;
            if (!bir.empty()) bir += U' ';
            while (k < y.size() && !boshliqmi(y[k])) bir += y[k++];
        }
        if (bir.empty()) continue;
        if (!natija.empty()) natija += U"; ";
        natija += bir;
    }
    if (natija.size() > 140) natija = natija.substr(0, 139) + U"…";
    return utf8ga(natija);
}

// ---- Siyosat -----------------------------------------------------------------

Qaror siyosatQarori(std::string_view joriy, std::string_view versiya, std::string_view minVersiya,
                    int muhlatSoat, int foiz, int chelak, long long korilgan, long long hozir) {
    const bool majburiyBor = !minVersiya.empty();
    // Ilova bajara olmaydigan shart (min > versiya) bilan uni abadiy
    // bloklab qoʻymaslik uchun bunday manifest yaroqsiz hisoblanadi.
    if (majburiyBor && versiyaTaqqosla(minVersiya, versiya) > 0) return Qaror::Hech;

    if (majburiyBor && versiyaTaqqosla(joriy, minVersiya) < 0) {
        // Soat orqaga surilgan boʻlsa oʻtgan vaqt manfiy chiqardi — nol deb
        // olamiz: shubhada bloklamaslik kerak.
        long long otgan = 0;
        if (korilgan >= 0 && hozir > korilgan) otgan = hozir - korilgan;
        return otgan >= static_cast<long long>(muhlatSoat) * 3600 ? Qaror::Blokla : Qaror::Ornat;
    }

    if (versiyaTaqqosla(versiya, joriy) > 0) return chelak < foiz ? Qaror::Yukla : Qaror::Hech;
    return Qaror::Hech;
}

Qaror majburiyQaror(std::string_view joriy, std::string_view min, int muhlatSoat,
                    long long korilgan, long long hozir) {
    if (min.empty() || versiyaTaqqosla(joriy, min) >= 0) return Qaror::Hech;
    return siyosatQarori(joriy, min, min, muhlatSoat, 0, 0, korilgan, hozir);
}

// ---- URL ---------------------------------------------------------------------

bool urlRuxsatmi(std::string_view url, UrlTuri tur) {
    const std::string_view prefiks =
        tur == UrlTuri::Fayl ? "https://cdn.mirqobilov.com/" : "https://stat.mirqobilov.com/";
    if (url.size() <= prefiks.size() || url.size() > 2048) return false;
    if (url.substr(0, prefiks.size()) != prefiks) return false;
    const std::string_view yol = url.substr(prefiks.size());
    for (char c : yol) {
        if (!raqammi(c) && !harfmi(c) && c != '.' && c != '_' && c != '~' && c != '/' && c != '-')
            return false;
    }
    size_t boshi = 0;
    while (true) {
        const size_t i = yol.find('/', boshi);
        const std::string_view s =
            yol.substr(boshi, i == std::string_view::npos ? std::string_view::npos : i - boshi);
        if (s.empty() || s == "." || s == "..") return false;
        if (i == std::string_view::npos) return true;
        boshi = i + 1;
    }
}

// ---- Base64 ------------------------------------------------------------------

bool base64Och(std::string_view k, std::vector<uint8_t>& chiqish) {
    chiqish.clear();
    if (k.size() % 4 != 0) return false;
    auto qiymat = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    for (size_t i = 0; i < k.size(); i += 4) {
        const bool oxirgi = i + 4 == k.size();
        // '=' faqat oxirgi toʻrtlikning oxirida: "xx==" yoki "xxx=".
        const int toldirish = oxirgi ? (k[i + 3] == '=') + (k[i + 2] == '=' && k[i + 3] == '=') : 0;
        int v[4];
        for (int j = 0; j < 4; ++j) {
            if (j >= 4 - toldirish) {
                v[j] = 0;
                continue;
            }
            v[j] = qiymat(k[i + j]);
            if (v[j] < 0) return false;
        }
        const uint32_t n = (uint32_t(v[0]) << 18) | (uint32_t(v[1]) << 12) | (uint32_t(v[2]) << 6) |
                           uint32_t(v[3]);
        chiqish.push_back(uint8_t(n >> 16));
        if (toldirish < 2) chiqish.push_back(uint8_t(n >> 8));
        if (toldirish < 1) chiqish.push_back(uint8_t(n));
    }
    return true;
}

// ---- Manifest ----------------------------------------------------------------

bool manifestniAjrat(const std::string& bayt, std::string_view platforma, Manifest& m) {
    m = Manifest{};
    const Json o = Json::ajrat(bayt);
    if (o.tur() != Json::Tur::Obyekt) return false;

    std::string p;
    if (!satrMaydon(o["platforma"], p) || p != platforma) return false;
    m.platforma = p;
    if (!satrMaydon(o["versiya"], m.versiya) || !versiyaFormatimi(m.versiya)) return false;

    if (o["min_versiya"].bormi()) {
        if (!satrMaydon(o["min_versiya"], m.minVersiya) || !versiyaFormatimi(m.minVersiya) ||
            versiyaTaqqosla(m.minVersiya, m.versiya) > 0) {
            return false;
        }
    }

    long long n = 0;
    if (!butunSon(o["majburiy_muhlat_soat"], 72, 0, 8760, n)) return false;
    m.muhlatSoat = static_cast<int>(n);
    if (!butunSon(o["tarqatish_foiz"], 100, 0, 100, n)) return false;
    m.foiz = static_cast<int>(n);

    if (o["izoh"].bormi() && !satrMaydon(o["izoh"], m.izoh)) return false;

    // macOS'da fayllar boʻlmaydi (yuklashni Sparkle appcast orqali qiladi) —
    // boʻlsa ham eʼtiborsiz qoldiriladi.
    if (platforma == "win") {
        const Json& f = o["fayllar"];
        if (f.tur() != Json::Tur::Obyekt || f.hajmi() == 0) return false;
        for (const std::string& arx : f.kalitlar()) {
            if (arx != "x64" && arx != "arm64") return false;
            const Json& d = f[arx];
            if (d.tur() != Json::Tur::Obyekt) return false;
            YangilanishFayli y;
            std::string imzo64;
            if (!satrMaydon(d["url"], y.url) || !urlRuxsatmi(y.url, UrlTuri::Fayl)) return false;
            if (!butunSon(d["hajm"], -1, 1, 1000000000000LL, y.hajm)) return false;
            if (!satrMaydon(d["sha256"], y.sha256) || !sha256Formatimi(y.sha256)) return false;
            if (!satrMaydon(d["imzo"], imzo64) || !base64Och(imzo64, y.imzo) || y.imzo.size() != 64)
                return false;
            m.fayllar[arx] = std::move(y);
        }
    }
    return true;
}

bool javobniTekshir(const std::string& javob, const uint8_t ochiqKalit[32],
                    std::string_view platforma, Manifest& natija) {
    natija = Manifest{};
    const Json o = Json::ajrat(javob);
    std::string m64, s64;
    if (o.tur() != Json::Tur::Obyekt || !satrMaydon(o["m"], m64) || !satrMaydon(o["s"], s64))
        return false;
    std::vector<uint8_t> m, s;
    if (!base64Och(m64, m) || !base64Och(s64, s)) return false;
    if (!ed25519Tekshir(ochiqKalit, 32, m.data(), m.size(), s.data(), s.size())) return false;
    return manifestniAjrat(std::string(m.begin(), m.end()), platforma, natija);
}

// ---- Yuklangan fayl -----------------------------------------------------------

FaylXatosi faylniTekshir(const uint8_t* bayt, size_t uzunlik, std::string_view sha256,
                         const YangilanishFayli& kutilgan, const uint8_t ochiqKalit[32]) {
    if (kutilgan.hajm <= 0 || static_cast<unsigned long long>(kutilgan.hajm) != uzunlik)
        return FaylXatosi::Hajm;
    if (sha256.size() != 64 || sha256 != kutilgan.sha256) return FaylXatosi::Sha256;
    if (!ed25519Tekshir(ochiqKalit, 32, bayt, uzunlik, kutilgan.imzo.data(), kutilgan.imzo.size()))
        return FaylXatosi::Imzo;
    return FaylXatosi::Yoq;
}

}  // namespace rubai
