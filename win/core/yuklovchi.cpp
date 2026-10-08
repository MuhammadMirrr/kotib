// Katta faylni yuklab olish (Range bilan davom ettirish) va SHA-256.
// Interfeys va izohlar — `yuklovchi.h`.

#include "yuklovchi.h"

#include "util.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <string>
#include <vector>

namespace rubai {

namespace {

long long faylHajmi(const std::wstring& yol) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(yol.c_str(), GetFileExInfoStandard, &d)) return -1;
    return (static_cast<long long>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
}

}  // namespace

YuklashNatijasi faylYukla(const std::wstring& url, const std::wstring& qism, long long taxminiyHajm,
                          const std::function<void(long long, long long)>& jarayon,
                          const std::function<bool()>& bekor) {
    YuklashNatijasi n;

    // Yarim fayl bor boʻlsa — oʻsha joydan davom etamiz.
    long long ofset = faylHajmi(qism);
    if (ofset < 0) ofset = 0;
    if (taxminiyHajm > 0 && ofset > taxminiyHajm) {
        // Kutilgandan katta — fayl buzuq, davom ettirib boʻlmaydi.
        logWrite(L"yuklash: .part kutilgandan katta — noldan boshlanadi");
        ofset = 0;
    }
    n.bayt = ofset;
    if (taxminiyHajm > 0 && ofset == taxminiyHajm) {
        // Oldingi urinishda toʻliq yuklangan, lekin yakunlanmagan (ilova
        // yopilgan, tekshiruv yiqilgan…). Tarmoqqa chiqmaymiz (F2).
        logWrite(L"yuklash: .part allaqachon toʻliq — tarmoqsiz yakunlanadi");
        n.ok = true;
        return n;
    }

    std::wstring host, yol;
    INTERNET_PORT port = 0;
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof(u);
    u.dwHostNameLength = static_cast<DWORD>(-1);
    u.dwUrlPathLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) {
        n.xato = L"Manzil notoʻgʻri.";
        return n;
    }
    host.assign(u.lpszHostName, u.dwHostNameLength);
    yol.assign(u.lpszUrlPath, u.dwUrlPathLength);
    port = u.nPort;

    HINTERNET sessiya = WinHttpOpen(L"Kotib", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessiya) {
        n.xato = L"Tarmoq ochilmadi.";
        return n;
    }
    // Katta fayl uzoq yuklanadi — oʻqish chegarasi katta boʻlsin.
    WinHttpSetTimeouts(sessiya, 15000, 20000, 60000, 120000);

    HINTERNET ulanish = WinHttpConnect(sessiya, host.c_str(), port, 0);
    if (!ulanish) {
        WinHttpCloseHandle(sessiya);
        n.xato = L"Internetga ulanib boʻlmadi.";
        return n;
    }

    HINTERNET sorov = WinHttpOpenRequest(
        ulanish, L"GET", yol.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        (u.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!sorov) {
        WinHttpCloseHandle(ulanish);
        WinHttpCloseHandle(sessiya);
        n.xato = L"Soʻrov yaratilmadi.";
        return n;
    }

    std::wstring sarlavhalar;
    if (ofset > 0) sarlavhalar = L"Range: bytes=" + std::to_wstring(ofset) + L"-\r\n";

    if (WinHttpSendRequest(
            sorov, sarlavhalar.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : sarlavhalar.c_str(),
            static_cast<DWORD>(sarlavhalar.size()), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(sorov, nullptr)) {
        DWORD holat = 0, hajm = sizeof(holat);
        WinHttpQueryHeaders(sorov, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &holat, &hajm, WINHTTP_NO_HEADER_INDEX);

        if (holat == 200 && ofset > 0) {
            // Server Range'ni qoʻllamadi — noldan boshlaymiz.
            logWrite(L"yuklash: server Range'ni qoʻllamadi — noldan boshlanadi");
            ofset = 0;
        }

        if (holat == 416 && ofset > 0) {
            // Soʻralgan joy fayl oxiridan keyin — `.part` allaqachon toʻliq (F2).
            logWrite(L"yuklash: server 416 — .part toʻliq deb olinadi");
            n.ok = true;
        } else if (holat != 200 && holat != 206) {
            n.xato = L"Server javob bermadi (" + std::to_wstring(holat) + L").";
        } else {
            // Content-Length ni SATR sifatida oʻqiymiz: `WINHTTP_QUERY_FLAG_NUMBER`
            // DWORD beradi va 4 GB dan katta fayl jimgina notoʻgʻri chiqadi.
            long long qolgan = 0;
            {
                wchar_t bufer[32] = {0};
                DWORD baytlar = sizeof(bufer);
                if (WinHttpQueryHeaders(sorov, WINHTTP_QUERY_CONTENT_LENGTH,
                                        WINHTTP_HEADER_NAME_BY_INDEX, bufer, &baytlar,
                                        WINHTTP_NO_HEADER_INDEX)) {
                    qolgan = static_cast<long long>(_wcstoui64(bufer, nullptr, 10));
                }
            }
            const long long jami = qolgan > 0 ? ofset + qolgan : taxminiyHajm;

            HANDLE f = CreateFileW(qism.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                   ofset > 0 ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
            if (f == INVALID_HANDLE_VALUE) {
                n.xato = L"Faylni ochib boʻlmadi. Diskda joy borligini tekshiring.";
            } else {
                LARGE_INTEGER joy;
                joy.QuadPart = ofset;
                SetFilePointerEx(f, joy, nullptr, FILE_BEGIN);
                SetEndOfFile(f);

                std::vector<char> bufer(256 * 1024);
                long long olingan = ofset;
                n.ok = true;
                for (;;) {
                    if (bekor && bekor()) {
                        n.ok = false;
                        n.xato = L"Bekor qilindi.";
                        break;
                    }

                    DWORD bor = 0;
                    if (!WinHttpQueryDataAvailable(sorov, &bor)) {
                        n.ok = false;
                        n.xato = L"Ulanish uzildi. Qayta urinib koʻring.";
                        break;
                    }
                    if (bor == 0) break;

                    const DWORD soralgan = std::min<DWORD>(bor, static_cast<DWORD>(bufer.size()));
                    DWORD oqildi = 0;
                    if (!WinHttpReadData(sorov, bufer.data(), soralgan, &oqildi) || oqildi == 0) {
                        n.ok = false;
                        n.xato = L"Ulanish uzildi. Qayta urinib koʻring.";
                        break;
                    }
                    DWORD yozildi = 0;
                    if (!WriteFile(f, bufer.data(), oqildi, &yozildi, nullptr) ||
                        yozildi != oqildi) {
                        n.ok = false;
                        n.xato = L"Diskka yozib boʻlmadi. Joy yetarli emas.";
                        break;
                    }
                    olingan += oqildi;
                    if (jarayon) jarayon(olingan, jami);
                }
                // Server ulanishni erta yopsa WinHTTP buni xato emas, oddiy
                // tugash (`bor == 0`) deb qaytaradi. Ilgari chala fayl «tayyor»
                // hisoblanib tekshiruvga ketardi, «hajm mos emas» bilan oʻchirilardi
                // va keyingi urinish noldan boshlanardi (VM'da sinovda topildi).
                // Endi bu — uzilish: `.part` qoladi, keyingi safar davom etadi.
                if (n.ok && jami > 0 && olingan < jami) {
                    n.ok = false;
                    n.xato = L"Ulanish uzildi. Qayta urinib koʻring.";
                    logWrite(L"yuklash: ulanish erta yopildi (" + std::to_wstring(olingan) +
                             L" / " + std::to_wstring(jami) + L" bayt) — .part saqlandi");
                }
                CloseHandle(f);
                n.bayt = olingan;
            }
        }
    } else {
        n.xato = L"Internetga ulanib boʻlmadi.";
    }

    WinHttpCloseHandle(sorov);
    WinHttpCloseHandle(ulanish);
    WinHttpCloseHandle(sessiya);
    return n;
}

std::string faylSha256(const std::wstring& yol) {
    HANDLE f = CreateFileW(yol.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE xesh = nullptr;
    std::string natija;
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
        BCRYPT_SUCCESS(BCryptCreateHash(alg, &xesh, nullptr, 0, nullptr, 0, 0))) {
        std::vector<unsigned char> bufer(4 << 20);
        bool ok = true;
        for (;;) {
            DWORD oqildi = 0;
            if (!ReadFile(f, bufer.data(), static_cast<DWORD>(bufer.size()), &oqildi, nullptr)) {
                ok = false;
                break;
            }
            if (oqildi == 0) break;
            if (!BCRYPT_SUCCESS(BCryptHashData(xesh, bufer.data(), oqildi, 0))) {
                ok = false;
                break;
            }
        }
        unsigned char d[32];
        if (ok && BCRYPT_SUCCESS(BCryptFinishHash(xesh, d, sizeof(d), 0))) {
            static const char hex[] = "0123456789abcdef";
            for (unsigned char b : d) {
                natija += hex[b >> 4];
                natija += hex[b & 15];
            }
        }
    }
    if (xesh) BCryptDestroyHash(xesh);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    return natija;
}

std::string baytlarSha256(const void* bayt, size_t uzunlik) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE xesh = nullptr;
    std::string natija;
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
        BCRYPT_SUCCESS(BCryptCreateHash(alg, &xesh, nullptr, 0, nullptr, 0, 0))) {
        // BCryptHashData ULONG oladi — 4 GB dan kattasi boʻlaklab.
        const auto* p = static_cast<const unsigned char*>(bayt);
        bool ok = true;
        for (size_t qoldi = uzunlik; ok && qoldi > 0;) {
            const ULONG n = static_cast<ULONG>(std::min<size_t>(qoldi, 1u << 30));
            ok = BCRYPT_SUCCESS(BCryptHashData(xesh, const_cast<PUCHAR>(p), n, 0));
            p += n;
            qoldi -= n;
        }
        unsigned char d[32];
        if (ok && BCRYPT_SUCCESS(BCryptFinishHash(xesh, d, sizeof(d), 0))) {
            static const char hex[] = "0123456789abcdef";
            for (unsigned char b : d) {
                natija += hex[b >> 4];
                natija += hex[b & 15];
            }
        }
    }
    if (xesh) BCryptDestroyHash(xesh);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return natija;
}

}  // namespace rubai
