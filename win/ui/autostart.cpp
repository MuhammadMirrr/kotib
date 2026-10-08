// Kompyuter yonganda avtomatik ishga tushirish (HKCU Run).
// Interfeys va izohlar — `autostart.h`.

#include "autostart.h"

#include <windows.h>

#include <string>

#include "../core/util.h"

namespace rubai {

namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Kotib";
// 1.1.0 gacha ilova shu nom bilan roʻyxatdan oʻtgan edi. Yozuv eski .exe ga
// koʻrsatadi va u oʻrnatuvchi tomonidan oʻchirilgan boʻladi — yaʼni har bir
// kirishda Windows mavjud boʻlmagan faylni ishga tushirishga urinadi.
constexpr wchar_t kEskiValueName[] = L"Audio-Matnga";

std::wstring exePath() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
}

// Registry qiymati tirnoq ichida boʻlishi kerak — yoʻlda boʻshliq boʻlsa
// ("C:\Program Files\...") Windows uni bir necha argumentga boʻlib yuboradi.
std::wstring quotedExePath() {
    const std::wstring p = exePath();
    return p.empty() ? p : L"\"" + p + L"\"";
}

}  // namespace

// Eski nomdagi yozuvni oʻchiradi va `bor` ga uning bor-yoʻqligini yozadi.
// Har chaqiruvda xavfsiz: yozuv boʻlmasa hech narsa qilmaydi.
static void eskiYozuvniOchir(bool* bor = nullptr) {
    if (bor) *bor = false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }
    if (RegDeleteValueW(key, kEskiValueName) == ERROR_SUCCESS) {
        if (bor) *bor = true;
        logWrite(L"eski nomdagi avtostart yozuvi oʻchirildi");
    }
    RegCloseKey(key);
}

// Qiymatni oʻqiydi. Yozuv boʻlmasa boʻsh satr.
static std::wstring yozuvniOqi() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return {};
    }
    wchar_t value[1024] = {};
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS st = RegQueryValueExW(key, kValueName, nullptr, &type, (LPBYTE)value, &size);
    RegCloseKey(key);
    if (st != ERROR_SUCCESS || type != REG_SZ) return {};
    return value;
}

void avtostartniTuzat() {
    bool eskiBorEdi = false;
    eskiYozuvniOchir(&eskiBorEdi);

    const std::wstring yozilgan = yozuvniOqi();

    // Eski nomdagi yozuv bor edi, yangisi esa yoʻq — demak foydalanuvchi
    // 1.1.0 dan oldingi versiyada avtostartni YOQIB qoʻygan. Nom oʻzgargani
    // uchun bu niyat yoʻqolib ketmasin.
    if (eskiBorEdi && yozilgan.empty()) {
        logWrite(L"avtostart eski nomdan yangisiga koʻchirildi");
        setAutoStart(true);
        return;
    }

    if (yozilgan.empty()) return;
    if (_wcsicmp(yozilgan.c_str(), quotedExePath().c_str()) == 0) return;

    // Tirnoqlarni olib tashlab, fayl hali turibdimi deb qaraymiz.
    std::wstring yol = yozilgan;
    if (yol.size() >= 2 && yol.front() == L'"' && yol.back() == L'"') {
        yol = yol.substr(1, yol.size() - 2);
    }
    if (fileExists(yol)) return;  // boshqa, TIRIK nusxa — tegmaymiz

    logWrite(L"avtostart yoʻli eskirgan — joriy .exe ga yangilanmoqda");
    setAutoStart(true);
}

bool autoStartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t value[1024] = {};
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS st = RegQueryValueExW(key, kValueName, nullptr, &type, (LPBYTE)value, &size);
    RegCloseKey(key);

    if (st != ERROR_SUCCESS || type != REG_SZ) return false;

    // Ilova boshqa papkaga koʻchirilgan boʻlishi mumkin — eski yozuv
    // "yoqilgan" deb hisoblanmasin.
    return _wcsicmp(value, quotedExePath().c_str()) == 0;
}

bool setAutoStart(bool enable) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        logWrite(L"XATO: Run kaliti ochilmadi");
        return false;
    }

    LSTATUS st;
    if (enable) {
        const std::wstring value = quotedExePath();
        st = RegSetValueExW(key, kValueName, 0, REG_SZ, (const BYTE*)value.c_str(),
                            (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    } else {
        st = RegDeleteValueW(key, kValueName);
        if (st == ERROR_FILE_NOT_FOUND) st = ERROR_SUCCESS;  // allaqachon oʻchirilgan
    }
    RegCloseKey(key);

    if (st != ERROR_SUCCESS) {
        logWrite(L"XATO: avtostart oʻzgartirilmadi, kod " + std::to_wstring(st));
        return false;
    }
    logWrite(enable ? L"avtostart yoqildi" : L"avtostart oʻchirildi");
    return true;
}

}  // namespace rubai
