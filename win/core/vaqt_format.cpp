// Foydalanuvchiga koʻrsatiladigan vaqt yozuvlari.
// Interfeys va izohlar — `vaqt_format.h`.

#include "vaqt_format.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace rubai {
namespace VaqtFormat {

namespace {

// macOS'ning `uz_Latn` lokalidagi qisqartmalar bilan bir xil.
const wchar_t* kOylar[12] = {L"yan", L"fev", L"mar", L"apr", L"may", L"iyn",
                             L"iyl", L"avg", L"sen", L"okt", L"noy", L"dek"};

std::tm mahalliy(long long unix) {
    const std::time_t t = static_cast<std::time_t>(unix);
    std::tm m{};
    localtime_s(&m, &t);
    return m;
}

bool birKunmi(const std::tm& a, const std::tm& b) {
    return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday == b.tm_mday;
}

std::wstring soatMatni(const std::tm& m) {
    wchar_t b[8];
    swprintf(b, 8, L"%02d:%02d", m.tm_hour, m.tm_min);
    return b;
}

std::wstring sanaMatni(const std::tm& m) {
    wchar_t b[24];
    swprintf(b, 24, L"%d %ls", m.tm_mday, kOylar[m.tm_mon]);
    return b;
}

// «bugun» / «kecha» / «22 avg». Ikkala funksiya ham shundan foydalanadi.
std::wstring kunAsosi(const std::tm& sana, const std::tm& hozir) {
    if (birKunmi(sana, hozir)) return L"bugun";

    // Kechagi kunni «hozir minus 24 soat» dan olamiz va localtime bilan
    // qaytadan yoyamiz — oy/yil chegarasi va yozgi vaqt oʻzgarishi
    // qoʻlda hisoblashda xato beradi.
    std::tm h = hozir;
    h.tm_isdst = -1;
    std::time_t t = std::mktime(&h) - 24 * 60 * 60;
    std::tm kechaTm{};
    localtime_s(&kechaTm, &t);
    if (birKunmi(sana, kechaTm)) return L"kecha";

    return sanaMatni(sana);
}

}  // namespace

std::wstring taymer(double soniya) {
    // Tizim soati sozlanib qolsa hisob manfiy chiqishi mumkin.
    const long long jami = std::max<long long>(0, static_cast<long long>(std::floor(soniya)));
    wchar_t b[24];
    swprintf(b, 24, L"%lld:%02lld", jami / 60, jami % 60);
    return b;
}

std::wstring nisbiy(long long unix, long long hozir) {
    if (hozir == 0) hozir = static_cast<long long>(std::time(nullptr));
    const std::tm s = mahalliy(unix);
    const std::tm h = mahalliy(hozir);
    return kunAsosi(s, h) + L" " + soatMatni(s);
}

std::wstring davomiylik(double soniya) {
    const long long daqiqa = std::max<long long>(0, static_cast<long long>(soniya)) / 60;
    // Nol daqiqa «fayl boʻsh» degan taassurot beradi — buni ochiq aytamiz.
    if (daqiqa == 0) return L"1 daqiqadan kam";

    wchar_t b[48];
    if (daqiqa < 60) {
        swprintf(b, 48, L"%lld daqiqa", daqiqa);
        return b;
    }
    const long long soat = daqiqa / 60;
    const long long qoldiq = daqiqa % 60;
    if (qoldiq == 0)
        swprintf(b, 48, L"%lld soat", soat);
    else
        swprintf(b, 48, L"%lld soat %lld daqiqa", soat, qoldiq);
    return b;
}

std::wstring kun(long long unix, long long hozir) {
    if (hozir == 0) hozir = static_cast<long long>(std::time(nullptr));
    return kunAsosi(mahalliy(unix), mahalliy(hozir));
}

}  // namespace VaqtFormat
}  // namespace rubai
