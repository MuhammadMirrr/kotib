// Kichik JSON oʻqiruvchi va yozuvchi (raqamlar lokalga bogʻliq emas).
// Interfeys va izohlar — `json.h`.

#include "json.h"

#include "util.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <system_error>

namespace rubai {

namespace {

const Json kBoshQiymat;  // topilmagan maydon uchun qaytariladi

}  // namespace

// ---- Oʻqish -----------------------------------------------------------------

class JsonOquvchi {
public:
    explicit JsonOquvchi(const std::string& s) : s_(s) {}

    Json oqi() {
        boshliqniOt();
        Json j = qiymat();
        boshliqniOt();
        // Oxirida ortiqcha narsa qolsa — bu bizning faylimiz emas.
        if (!xato_ && i_ != s_.size()) xato_ = true;
        return xato_ ? Json{} : j;
    }

private:
    const std::string& s_;
    size_t i_ = 0;
    bool xato_ = false;

    // Chuqurlik chegarasi: buzuq (yoki yovuz) fayl rekursiya bilan stekni
    // toʻldirib ilovani yiqitmasligi kerak.
    int chuqurlik_ = 0;
    static constexpr int kMaksChuqurlik = 64;

    void boshliqniOt() {
        while (i_ < s_.size() &&
               (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) {
            ++i_;
        }
    }

    bool kutilgan(char c) {
        boshliqniOt();
        if (i_ < s_.size() && s_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }

    Json qiymat() {
        if (xato_ || chuqurlik_ > kMaksChuqurlik) {
            xato_ = true;
            return {};
        }
        boshliqniOt();
        if (i_ >= s_.size()) {
            xato_ = true;
            return {};
        }

        switch (s_[i_]) {
            case '{': return obyekt();
            case '[': return royxat();
            case '"': return satr();
            case 't': return soz("true", true);
            case 'f': return soz("false", false);
            case 'n': return nul();
            default: return son();
        }
    }

    Json soz(const char* kutilgan_, bool qiymat_) {
        const size_t n = strlen(kutilgan_);
        if (s_.compare(i_, n, kutilgan_) != 0) {
            xato_ = true;
            return {};
        }
        i_ += n;
        Json j;
        j.tur_ = Json::Tur::Mantiq;
        j.mantiq_ = qiymat_;
        return j;
    }

    Json nul() {
        if (s_.compare(i_, 4, "null") != 0) {
            xato_ = true;
            return {};
        }
        i_ += 4;
        Json j;
        j.tur_ = Json::Tur::Null;
        return j;
    }

    Json son() {
        const size_t boshi = i_;
        if (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+')) ++i_;
        while (i_ < s_.size() &&
               ((s_[i_] >= '0' && s_[i_] <= '9') || s_[i_] == '.' || s_[i_] == 'e' ||
                s_[i_] == 'E' || s_[i_] == '-' || s_[i_] == '+')) {
            ++i_;
        }
        if (i_ == boshi) {
            xato_ = true;
            return {};
        }

        Json j;
        j.tur_ = Json::Tur::Son;
        // `std::from_chars` — lokalga bogʻliq EMAS. Ilgari `std::stod` (strtod)
        // joriy lokalning kasr ajratgichini kutardi: ru/uz lokalida «12.5» → 12
        // (barqarorlik E2). `+` ni JSON ham, from_chars ham qabul qilmaydi —
        // eski xatti-harakat saqlanadi (stod uni oʻtkazib yuborardi).
        const char* b = s_.data() + boshi;
        const char* e = s_.data() + i_;
        if (b < e && *b == '+') ++b;
        const auto [p, ec] = std::from_chars(b, e, j.son_);
        if (ec != std::errc() || p != e) {
            xato_ = true;
            return {};
        }
        return j;
    }

    Json satr() {
        if (!kutilgan('"')) {
            xato_ = true;
            return {};
        }

        std::string xom;
        while (i_ < s_.size() && s_[i_] != '"') {
            if (s_[i_] == '\\' && i_ + 1 < s_.size()) {
                ++i_;
                switch (s_[i_]) {
                    case 'n': xom += '\n'; break;
                    case 't': xom += '\t'; break;
                    case 'r': xom += '\r'; break;
                    case 'b': xom += '\b'; break;
                    case 'f': xom += '\f'; break;
                    case '/': xom += '/'; break;
                    case '"': xom += '"'; break;
                    case '\\': xom += '\\'; break;
                    case 'u': {
                        // \uXXXX — UTF-16 kodi. Surrogat juftliklarni
                        // (emoji va boshqalar) birlashtirib qoʻyish kerak,
                        // aks holda matn buziladi.
                        if (i_ + 4 >= s_.size()) {
                            xato_ = true;
                            return {};
                        }
                        const std::wstring w = onOltilik(i_ + 1);
                        if (xato_) return {};
                        i_ += 4;
                        xom += toUtf8(w);
                        break;
                    }
                    default: xato_ = true; return {};
                }
                ++i_;
                continue;
            }
            xom += s_[i_++];
        }
        if (i_ >= s_.size()) {
            xato_ = true;
            return {};
        }
        ++i_;  // yopuvchi tirnoq

        Json j;
        j.tur_ = Json::Tur::Satr;
        j.satr_ = toWide(xom);
        return j;
    }

    // `\uXXXX` dan boshlab bir yoki ikki kodni oʻqiydi (surrogat juftlik).
    std::wstring onOltilik(size_t joy) {
        auto raqam = [&](size_t p) -> int {
            int n = 0;
            for (size_t k = p; k < p + 4; ++k) {
                if (k >= s_.size()) {
                    xato_ = true;
                    return 0;
                }
                const char c = s_[k];
                n <<= 4;
                if (c >= '0' && c <= '9')
                    n |= c - '0';
                else if (c >= 'a' && c <= 'f')
                    n |= c - 'a' + 10;
                else if (c >= 'A' && c <= 'F')
                    n |= c - 'A' + 10;
                else {
                    xato_ = true;
                    return 0;
                }
            }
            return n;
        };

        const int birinchi = raqam(joy);
        if (xato_) return {};

        // Yuqori surrogat — keyingi `\uXXXX` bilan juftlik yasaydi.
        if (birinchi >= 0xD800 && birinchi <= 0xDBFF && joy + 6 < s_.size() &&
            s_[joy + 4] == '\\' && s_[joy + 5] == 'u') {
            const int ikkinchi = raqam(joy + 6);
            if (!xato_ && ikkinchi >= 0xDC00 && ikkinchi <= 0xDFFF) {
                i_ += 6;  // ikkinchi qismini ham «yeb» qoʻyamiz
                return std::wstring{static_cast<wchar_t>(birinchi), static_cast<wchar_t>(ikkinchi)};
            }
        }
        return std::wstring(1, static_cast<wchar_t>(birinchi));
    }

    Json royxat() {
        ++i_;  // '['
        ++chuqurlik_;
        Json j;
        j.tur_ = Json::Tur::Royxat;

        boshliqniOt();
        if (kutilgan(']')) {
            --chuqurlik_;
            return j;
        }

        for (;;) {
            j.royxat_.push_back(qiymat());
            if (xato_) return {};
            boshliqniOt();
            if (kutilgan(',')) continue;
            if (kutilgan(']')) break;
            xato_ = true;
            return {};
        }
        --chuqurlik_;
        return j;
    }

    Json obyekt() {
        ++i_;  // '{'
        ++chuqurlik_;
        Json j;
        j.tur_ = Json::Tur::Obyekt;

        boshliqniOt();
        if (kutilgan('}')) {
            --chuqurlik_;
            return j;
        }

        for (;;) {
            boshliqniOt();
            const Json k = satr();
            if (xato_) return {};
            if (!kutilgan(':')) {
                xato_ = true;
                return {};
            }
            j.obyekt_[toUtf8(k.satr_)] = qiymat();
            if (xato_) return {};

            boshliqniOt();
            if (kutilgan(',')) continue;
            if (kutilgan('}')) break;
            xato_ = true;
            return {};
        }
        --chuqurlik_;
        return j;
    }
};

Json Json::ajrat(const std::string& utf8) {
    JsonOquvchi o(utf8);
    return o.oqi();
}

bool Json::mantiq(bool sukut) const { return tur_ == Tur::Mantiq ? mantiq_ : sukut; }

double Json::son(double sukut) const { return tur_ == Tur::Son ? son_ : sukut; }

std::wstring Json::satr() const { return tur_ == Tur::Satr ? satr_ : std::wstring{}; }

const Json& Json::operator[](const std::string& kalit) const {
    if (tur_ != Tur::Obyekt) return kBoshQiymat;
    auto it = obyekt_.find(kalit);
    return it == obyekt_.end() ? kBoshQiymat : it->second;
}

const Json& Json::operator[](size_t i) const {
    if (tur_ != Tur::Royxat || i >= royxat_.size()) return kBoshQiymat;
    return royxat_[i];
}

std::vector<std::string> Json::kalitlar() const {
    std::vector<std::string> v;
    if (tur_ != Tur::Obyekt) return v;
    v.reserve(obyekt_.size());
    for (const auto& [k, _] : obyekt_) v.push_back(k);
    return v;
}

size_t Json::hajmi() const {
    if (tur_ == Tur::Royxat) return royxat_.size();
    if (tur_ == Tur::Obyekt) return obyekt_.size();
    return 0;
}

// ---- Yozish -----------------------------------------------------------------

void JsonYozuvchi::vergul() {
    if (kalitKutilyapti_) {
        kalitKutilyapti_ = false;
        return;
    }
    if (!bor_.empty()) {
        if (bor_.back()) chiqish_ += ',';
        bor_.back() = true;
    }
}

void JsonYozuvchi::obyektBoshla() {
    vergul();
    chiqish_ += '{';
    bor_.push_back(false);
}
void JsonYozuvchi::obyektTugat() {
    chiqish_ += '}';
    if (!bor_.empty()) bor_.pop_back();
}
void JsonYozuvchi::royxatBoshla() {
    vergul();
    chiqish_ += '[';
    bor_.push_back(false);
}
void JsonYozuvchi::royxatTugat() {
    chiqish_ += ']';
    if (!bor_.empty()) bor_.pop_back();
}

void JsonYozuvchi::kalit(const std::string& k) {
    vergul();
    chiqish_ += '"';
    for (char c : k) chiqish_ += c;  // kalitlar ASCII, qochirish kerak emas
    chiqish_ += "\":";
    kalitKutilyapti_ = true;
}

void JsonYozuvchi::satr(const std::wstring& s) {
    vergul();
    chiqish_ += '"';
    const std::string utf8 = toUtf8(s);
    for (unsigned char c : utf8) {
        switch (c) {
            case '"': chiqish_ += "\\\""; break;
            case '\\': chiqish_ += "\\\\"; break;
            case '\n': chiqish_ += "\\n"; break;
            case '\r': chiqish_ += "\\r"; break;
            case '\t': chiqish_ += "\\t"; break;
            default:
                // Boshqaruv belgilari JSON'da xom holda turolmaydi.
                if (c < 0x20) {
                    char bufer[8];
                    snprintf(bufer, sizeof(bufer), "\\u%04x", c);
                    chiqish_ += bufer;
                } else {
                    chiqish_ += static_cast<char>(c);
                }
        }
    }
    chiqish_ += '"';
}

void JsonYozuvchi::son(double x) {
    vergul();
    // Butun son butun koʻrinishda yozilsin — «1» va «1.0» ikkalasi ham
    // toʻgʻri JSON, lekin birinchisi faylda toza koʻrinadi.
    if (std::isfinite(x) && x == static_cast<double>(static_cast<long long>(x))) {
        chiqish_ += std::to_string(static_cast<long long>(x));
        return;
    }
    if (!std::isfinite(x)) {
        chiqish_ += '0';
        return;
    }  // JSON'da Inf/NaN yoʻq

    // `%.6g` bilan bir xil koʻrinish, lekin lokalga bogʻliq emas (E2).
    char bufer[40];
    const auto r = std::to_chars(bufer, bufer + sizeof(bufer), x, std::chars_format::general, 6);
    chiqish_.append(bufer, r.ptr);
}

std::string kasrSon(double x, int kasr) {
    if (!std::isfinite(x) || x < 0) x = 0;
    char b[64];
    const auto r = std::to_chars(b, b + sizeof(b), x, std::chars_format::fixed, kasr);
    return r.ec == std::errc() ? std::string(b, r.ptr) : std::string("0");
}

void JsonYozuvchi::butun(long long x) {
    vergul();
    chiqish_ += std::to_string(x);
}

void JsonYozuvchi::mantiq(bool b) {
    vergul();
    chiqish_ += b ? "true" : "false";
}
void JsonYozuvchi::nul() {
    vergul();
    chiqish_ += "null";
}

}  // namespace rubai
