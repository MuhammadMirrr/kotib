// Avto-yangilanish mantigʻi testlari — `versiya.cpp`, `yangilanish_siyosat.cpp`
// va `imzo.cpp`.
//
// Holatlarning oʻzi bu yerda EMAS: ular `tests/umumiy/yangilanish_holatlari.def`
// da. Bu fayl jadvalni X-makro usulida `#include` qiladi — u kotib-testlar.exe
// ichiga kompilyatsiya vaqtida kiradi va VM'da alohida fayl kerak boʻlmaydi.
// macOS testi (`tests/test_yangilanish.swift`) xuddi shu jadvalni oʻqiydi.
#include "../core/imzo.h"
#include "../core/util.h"
#include "../core/yangilanish_siyosat.h"
#include "../third_party/monocypher/monocypher-ed25519.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan);
}  // namespace kotib_test

using kotib_test::tekshir;
using kotib_test::tengmi;
using namespace rubai;

namespace {

// Jadvaldagi identifikatorlar.
enum Kutilgan { HECH, YUKLA, ORNAT, BLOKLA };
enum Tur { FAYL_URL, MANIFEST_URL };
enum Rejim { TOGRI, BUZUQ_IMZO, BUZUQ_XABAR, BOSHQA_KALIT };

std::wstring w(const std::string& s) { return toWide(s); }

std::vector<uint8_t> hexdan(const char* s) {
    std::vector<uint8_t> v;
    for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
        unsigned x = 0;
        std::sscanf(s + i, "%2x", &x);
        v.push_back(static_cast<uint8_t>(x));
    }
    return v;
}

std::string hexga(const std::vector<uint8_t>& v) {
    std::string s;
    char b[3];
    for (uint8_t c : v) {
        std::snprintf(b, sizeof b, "%02x", c);
        s += b;
    }
    return s;
}

std::string base64Yoz(const std::vector<uint8_t>& d) {
    static const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string s;
    for (size_t i = 0; i < d.size(); i += 3) {
        const uint32_t n = (uint32_t(d[i]) << 16) |
                           (i + 1 < d.size() ? uint32_t(d[i + 1]) << 8 : 0) |
                           (i + 2 < d.size() ? uint32_t(d[i + 2]) : 0);
        s += a[(n >> 18) & 63];
        s += a[(n >> 12) & 63];
        s += i + 1 < d.size() ? a[(n >> 6) & 63] : '=';
        s += i + 2 < d.size() ? a[n & 63] : '=';
    }
    return s;
}

const char* qarorNomi(Qaror q) {
    switch (q) {
        case Qaror::Hech: return "hech";
        case Qaror::Yukla: return "yukla";
        case Qaror::Ornat: return "ornat";
        case Qaror::Blokla: return "blokla";
    }
    return "?";
}

const char* kutilganNomi(Kutilgan k) {
    switch (k) {
        case HECH: return "hech";
        case YUKLA: return "yukla";
        case ORNAT: return "ornat";
        case BLOKLA: return "blokla";
    }
    return "?";
}

// Tahlil natijasining matnli xulosasi — jadvaldagi kutilgan qiymat shu
// shaklda yozilgan (macOS testi ham aynan shunday xulosa yasaydi).
std::string xulosa(const Manifest& m) {
    std::string s = "v=" + m.versiya + ";min=" + m.minVersiya +
                    ";muhlat=" + std::to_string(m.muhlatSoat) + ";foiz=" + std::to_string(m.foiz) +
                    ";izoh=" + m.izoh;
    for (const auto& [arx, f] : m.fayllar) {  // std::map — alifbo tartibida
        s += ";" + arx + "=" + f.url + "|" + std::to_string(f.hajm) + "|" + f.sha256 + "|imzo" +
             std::to_string(f.imzo.size());
    }
    return s;
}

// ---- Jadval (X-makro) --------------------------------------------------------

struct VersiyaHolat {
    const char* a;
    const char* b;
    int k;
};
const VersiyaHolat kVersiya[] = {
#define VERSIYA(a, b, k) {a, b, k},
#define FORMAT(...)
#define SIYOSAT(...)
#define URL(...)
#define BASE64(...)
#define ED25519(...)
#define SINOV_KALITI(...)
#define BOSHQA_KALIT(...)
#define MANIFEST(...)
#define JAVOB_XOM(...)
#define QAYTA(...)
#define UYGONISH(...)
#define BOSH_PAYT(...)
#define IZOH(...)
#define MAJBURIY(...)
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef VERSIYA
#define VERSIYA(...)
};

struct FormatHolat {
    const char* v;
    bool k;
};
const FormatHolat kFormat[] = {
#undef FORMAT
#define FORMAT(v, k) {v, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef FORMAT
#define FORMAT(...)
};

struct SiyosatHolat {
    const char* nom;
    const char* joriy;
    const char* versiya;
    const char* min;
    int muhlat;
    int foiz;
    int chelak;
    long long korilgan;
    long long hozir;
    Kutilgan k;
};
const SiyosatHolat kSiyosat[] = {
#undef SIYOSAT
#define SIYOSAT(n, j, v, m, mu, f, c, ko, h, k) {n, j, v, m, mu, f, c, ko, h, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef SIYOSAT
#define SIYOSAT(...)
};

struct UrlHolat {
    const char* url;
    Tur tur;
    bool k;
};
const UrlHolat kUrl[] = {
#undef URL
#define URL(u, t, k) {u, t, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef URL
#define URL(...)
};

struct Base64Holat {
    const char* nom;
    const char* kirish;
    const char* k;
};
const Base64Holat kBase64[] = {
#undef BASE64
#define BASE64(n, i, k) {n, i, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef BASE64
#define BASE64(...)
};

struct EdHolat {
    const char* nom;
    const char* kalit;
    const char* xabar;
    const char* imzo;
    bool k;
};
const EdHolat kEd[] = {
#undef ED25519
#define ED25519(n, kl, x, i, k) {n, kl, x, i, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef ED25519
#define ED25519(...)
};

struct Kalitlar {
    const char* urug;
    const char* ochiq;
    const char* boshqa;
};
const Kalitlar kKalit = {
#undef SINOV_KALITI
#undef BOSHQA_KALIT
#define SINOV_KALITI(u, o) u, o,
#define BOSHQA_KALIT(b)    b
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef SINOV_KALITI
#undef BOSHQA_KALIT
#define SINOV_KALITI(...)
#define BOSHQA_KALIT(...)
};

struct ManifestHolat {
    const char* nom;
    const char* platforma;
    const char* json;
    Rejim rejim;
    const char* k;
};
const ManifestHolat kManifest[] = {
#undef MANIFEST
#define MANIFEST(n, p, j, r, k) {n, p, j, r, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef MANIFEST
#define MANIFEST(...)
};

struct XomHolat {
    const char* nom;
    const char* javob;
    const char* k;
};
const XomHolat kXom[] = {
#undef JAVOB_XOM
#define JAVOB_XOM(n, j, k) {n, j, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef JAVOB_XOM
#define JAVOB_XOM(...)
};

struct QaytaHolat {
    int urinish;
    long long k;
};
const QaytaHolat kQayta[] = {
#undef QAYTA
#define QAYTA(u, k) {u, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef QAYTA
#define QAYTA(...)
};

struct UygonishHolat {
    const char* nom;
    long long oxirgi;
    long long hozir;
    bool k;
};
const UygonishHolat kUygonish[] = {
#undef UYGONISH
#define UYGONISH(n, o, h, k) {n, o, h, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef UYGONISH
#define UYGONISH(...)
};

struct BoshPaytHolat {
    const char* nom;
    bool band;
    long long faollik;
    long long kutish;
    long long hozir;
    bool k;
};
const BoshPaytHolat kBoshPayt[] = {
#undef BOSH_PAYT
#define BOSH_PAYT(n, b, f, ku, h, k) {n, b, f, ku, h, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef BOSH_PAYT
#define BOSH_PAYT(...)
};

struct IzohHolat {
    const char* nom;
    const char* xom;
    const char* k;
};
const IzohHolat kIzoh[] = {
#undef IZOH
#define IZOH(n, x, k) {n, x, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
#undef IZOH
#define IZOH(...)
};

struct MajburiyHolat {
    const char* nom;
    const char* joriy;
    const char* min;
    int muhlat;
    long long korilgan;
    long long hozir;
    Kutilgan k;
};
const MajburiyHolat kMajburiy[] = {
#undef MAJBURIY
#define MAJBURIY(n, j, m, mu, ko, h, k) {n, j, m, mu, ko, h, k},
#include "../../tests/umumiy/yangilanish_holatlari.def"
};

#undef VERSIYA
#undef FORMAT
#undef SIYOSAT
#undef URL
#undef BASE64
#undef ED25519
#undef MANIFEST
#undef JAVOB_XOM
#undef QAYTA
#undef UYGONISH
#undef BOSH_PAYT
#undef IZOH
#undef MAJBURIY

// Server javobini sinov kaliti bilan yasaydi (Monocypher — deterministik
// RFC 8032 imzo; macOS testi CryptoKit'ning tasodifiy imzosini ishlatadi —
// ikkisi ham haqiqiy Ed25519, tekshiruvchi uchun farqi yoʻq).
std::string imzoliJavob(const std::string& manifest, Rejim rejim) {
    std::vector<uint8_t> urug = hexdan(kKalit.urug);
    uint8_t maxfiy[64], ochiq[32], imzo[64];
    crypto_ed25519_key_pair(maxfiy, ochiq, urug.data());  // urugni oʻchirib yuboradi
    std::vector<uint8_t> m(manifest.begin(), manifest.end());
    crypto_ed25519_sign(imzo, maxfiy, m.data(), m.size());
    if (rejim == BUZUQ_IMZO) imzo[0] ^= 1;
    if (rejim == BUZUQ_XABAR) m.push_back(' ');
    return "{\"m\":\"" + base64Yoz(m) + "\",\"s\":\"" + base64Yoz({imzo, imzo + 64}) + "\"}";
}

}  // namespace

void yangilanishTestlari() {
    for (const auto& h : kVersiya) {
        tengmi(L"taqqosla(" + w(h.a) + L", " + w(h.b) + L")",
               std::to_wstring(versiyaTaqqosla(h.a, h.b)), std::to_wstring(h.k));
    }
    for (const auto& h : kFormat) {
        tekshir(L"format «" + w(h.v) + L"»", versiyaFormatimi(h.v) == h.k);
    }
    for (const auto& h : kSiyosat) {
        const Qaror q = siyosatQarori(h.joriy, h.versiya, h.min, h.muhlat, h.foiz, h.chelak,
                                      h.korilgan, h.hozir);
        tengmi(L"siyosat: " + w(h.nom), w(qarorNomi(q)), w(kutilganNomi(h.k)));
    }
    for (const auto& h : kUrl) {
        tekshir(L"url «" + w(h.url) + L"»",
                urlRuxsatmi(h.url, h.tur == FAYL_URL ? UrlTuri::Fayl : UrlTuri::Manifest) == h.k);
    }
    for (const auto& h : kBase64) {
        std::vector<uint8_t> d;
        const bool ok = base64Och(h.kirish, d);
        tengmi(L"base64: " + w(h.nom), w(ok ? hexga(d) : "RAD"), w(h.k));
    }
    for (const auto& h : kEd) {
        const auto k = hexdan(h.kalit), x = hexdan(h.xabar), i = hexdan(h.imzo);
        tekshir(L"ed25519: " + w(h.nom),
                ed25519Tekshir(k.data(), k.size(), x.data(), x.size(), i.data(), i.size()) == h.k);
    }

    const auto sinovOchiq = hexdan(kKalit.ochiq), boshqaOchiq = hexdan(kKalit.boshqa);
    for (const auto& h : kManifest) {
        Manifest m;
        const auto& kalit = h.rejim == BOSHQA_KALIT ? boshqaOchiq : sinovOchiq;
        const bool ok = javobniTekshir(imzoliJavob(h.json, h.rejim), kalit.data(), h.platforma, m);
        tengmi(L"manifest: " + w(h.nom), w(ok ? xulosa(m) : "RAD"), w(h.k));
    }
    for (const auto& h : kXom) {
        Manifest m;
        const bool ok = javobniTekshir(h.javob, sinovOchiq.data(), "mac", m);
        tengmi(L"javob: " + w(h.nom), w(ok ? xulosa(m) : "RAD"), w(h.k));
    }

    for (const auto& h : kQayta) {
        tengmi(L"qayta urinish " + std::to_wstring(h.urinish),
               std::to_wstring(qaytaUrinishKechikishi(h.urinish)), std::to_wstring(h.k));
    }
    for (const auto& h : kUygonish) {
        tekshir(L"tekshirish vaqti: " + w(h.nom), uygonishdaTekshirish(h.oxirgi, h.hozir) == h.k);
    }
    for (const auto& h : kBoshPayt) {
        tekshir(L"boʻsh payt: " + w(h.nom),
                ornatishMumkinmi(h.band, h.faollik, h.kutish, h.hozir) == h.k);
    }
    for (const auto& h : kMajburiy) {
        tengmi(L"majburiy: " + w(h.nom),
               w(qarorNomi(majburiyQaror(h.joriy, h.min, h.muhlat, h.korilgan, h.hozir))),
               w(kutilganNomi(h.k)));
    }
    for (const auto& h : kIzoh) { tengmi(L"izoh: " + w(h.nom), w(qisqaIzoh(h.xom)), w(h.k)); }

    // Yuklangan faylni tekshirish (faqat Windows — macOS'da faylni Sparkle
    // tekshiradi). Tartib: hajm → sha256 → Ed25519; birinchi xato qaytadi.
    {
        const std::string mazmun = "abc";
        const char* sha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        std::vector<uint8_t> urug = hexdan(kKalit.urug);
        uint8_t maxfiy[64], ochiq[32], imzo[64];
        crypto_ed25519_key_pair(maxfiy, ochiq, urug.data());
        const auto* b = reinterpret_cast<const uint8_t*>(mazmun.data());
        crypto_ed25519_sign(imzo, maxfiy, b, mazmun.size());

        YangilanishFayli f;
        f.hajm = 3;
        f.sha256 = sha;
        f.imzo.assign(imzo, imzo + 64);
        tekshir(L"fayl: toʻgʻri", faylniTekshir(b, 3, sha, f, ochiq) == FaylXatosi::Yoq);
        tekshir(L"fayl: yarim yuklangan", faylniTekshir(b, 2, sha, f, ochiq) == FaylXatosi::Hajm);
        tekshir(L"fayl: sha256 boshqa",
                faylniTekshir(b, 3, std::string(64, '0'), f, ochiq) == FaylXatosi::Sha256);
        tekshir(L"fayl: sha256 boʻsh", faylniTekshir(b, 3, "", f, ochiq) == FaylXatosi::Sha256);
        const std::string boshqa = "abd";  // sha mos deb faraz: imzo baribir ushlaydi
        tekshir(L"fayl: mazmun boshqa, imzo ushlaydi",
                faylniTekshir(reinterpret_cast<const uint8_t*>(boshqa.data()), 3, sha, f, ochiq) ==
                    FaylXatosi::Imzo);
        tekshir(L"fayl: boshqa kalit",
                faylniTekshir(b, 3, sha, f, boshqaOchiq.data()) == FaylXatosi::Imzo);
        f.hajm = 0;
        tekshir(L"fayl: manifestda hajm 0", faylniTekshir(b, 0, sha, f, ochiq) == FaylXatosi::Hajm);
    }
}
