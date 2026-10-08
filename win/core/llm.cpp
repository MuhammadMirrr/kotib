// Ixtiyoriy LLM qatlamining TIZIM qismi: API kalitni Credential Manager'da
// saqlash, sozlama va WinHTTP ustidagi tarmoq oqimi.
//
// Sof mantiq (provayderlar, soʻrov tanasi, SSE, boʻlaklash) `llm_sof.cpp` da —
// u macOS'da ham kompilyatsiya boʻladi va testlar u yerdan oʻtadi.
//
// **Ilova hech qanday API kalit bilan kelmaydi.** Diktovka va Studiya kalitsiz
// toʻliq ishlaydi; LLM — qoʻshimcha. Kalit foydalanuvchining oʻzi kiritadi va
// u Windows Credential Manager'da saqlanadi (macOS'da Keychain), hech qachon
// logga yozilmaydi.
#include "llm.h"

#include "config.h"
#include "json.h"
#include "util.h"

#include <windows.h>
#include <wincred.h>
#include <winhttp.h>

#include <algorithm>
#include <set>

namespace rubai {

namespace {

// Kalit Credential Manager'da shu nom ostida turadi. macOS'dagi
// `com.rubaistt.dictation.llm` xizmat nomining ekvivalenti.
std::wstring kalitNomi(const std::string& provayderId) {
    return L"Kotib.llm." + toWide(provayderId);
}

}  // namespace

// ---- Kalit saqlash ---------------------------------------------------------

namespace Kalitlar {

bool saqla(const std::string& provayderId, const std::wstring& kalit) {
    if (kalit.empty()) return ochir(provayderId);

    const std::wstring nom = kalitNomi(provayderId);
    const std::string utf8 = toUtf8(kalit);

    CREDENTIALW c{};
    c.Type = CRED_TYPE_GENERIC;
    c.TargetName = const_cast<LPWSTR>(nom.c_str());
    c.CredentialBlobSize = static_cast<DWORD>(utf8.size());
    c.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(utf8.data()));
    c.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (CredWriteW(&c, 0)) return true;
    // Kalitning OʻZI hech qachon logga tushmaydi — faqat xato kodi.
    logWrite(L"XATO: API kalit saqlanmadi, kod " + std::to_wstring(GetLastError()));
    return false;
}

std::wstring oqi(const std::string& provayderId) {
    PCREDENTIALW c = nullptr;
    if (!CredReadW(kalitNomi(provayderId).c_str(), CRED_TYPE_GENERIC, 0, &c)) return {};

    std::string utf8(reinterpret_cast<const char*>(c->CredentialBlob), c->CredentialBlobSize);
    CredFree(c);
    return toWide(utf8);
}

bool ochir(const std::string& provayderId) {
    return CredDeleteW(kalitNomi(provayderId).c_str(), CRED_TYPE_GENERIC, 0) != FALSE;
}

}  // namespace Kalitlar

// ---- Sozlama ---------------------------------------------------------------

namespace LLMSozlama {

std::string tanlanganId() { return loadSettings().llmProvayder; }

const Provayder* tanlangan() { return provayderTop(tanlanganId()); }

std::wstring baseURL() {
    const Settings s = loadSettings();
    if (!s.llmBaseURL.empty()) return s.llmBaseURL;
    const Provayder* p = provayderTop(s.llmProvayder);
    return p ? p->baseURL : std::wstring();
}

std::wstring model() {
    const Settings s = loadSettings();
    if (!s.llmModel.empty()) return s.llmModel;
    const Provayder* p = provayderTop(s.llmProvayder);
    return p ? p->standartModel : std::wstring();
}

std::wstring joriyKalit() {
    const std::string id = tanlanganId();
    return id.empty() ? std::wstring() : Kalitlar::oqi(id);
}

bool sozlanganmi() {
    const Settings s = loadSettings();
    const Provayder* p = provayderTop(s.llmProvayder);
    if (!p) return false;
    if (Kalitlar::oqi(p->id).empty()) return false;
    return !(s.llmBaseURL.empty() ? p->baseURL : s.llmBaseURL).empty() &&
           !(s.llmModel.empty() ? p->standartModel : s.llmModel).empty();
}

void provayderniTanla(const std::string& id) {
    Settings s = loadSettings();
    s.llmProvayder = id;
    // URL va modelni presetdan qaytadan qoʻyamiz — eski provayderning
    // modeli yangisida mavjud boʻlmasligi mumkin va soʻrov 400 qaytarardi.
    const Provayder* p = provayderTop(id);
    s.llmBaseURL = p ? p->baseURL : std::wstring();
    s.llmModel = p ? p->standartModel : std::wstring();
    saveSettings(s);
}

}  // namespace LLMSozlama

// ---- Oqim ------------------------------------------------------------------

namespace {

// URL'ni host, yoʻl va portga ajratadi.
bool urlniAjrat(const std::wstring& url, std::wstring& host, std::wstring& yol, INTERNET_PORT& port,
                bool& xavfsiz) {
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof(u);
    u.dwHostNameLength = static_cast<DWORD>(-1);
    u.dwUrlPathLength = static_cast<DWORD>(-1);
    u.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) return false;

    host.assign(u.lpszHostName, u.dwHostNameLength);
    yol.assign(u.lpszUrlPath, u.dwUrlPathLength);
    if (u.dwExtraInfoLength) yol.append(u.lpszExtraInfo, u.dwExtraInfoLength);
    port = u.nPort;
    xavfsiz = (u.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

}  // namespace

// ---- Model roʻyxati --------------------------------------------------------

std::wstring modellarniOl(const Provayder& p, const std::wstring& baseURL,
                          const std::wstring& kalit, std::vector<std::wstring>& chiqish) {
    chiqish.clear();
    if (kalit.empty()) return L"API kalit kiritilmagan. Sozlamalar → LLM boʻlimiga oʻting.";

    std::wstring baza = baseURL.empty() ? p.baseURL : baseURL;
    if (baza.empty()) return L"Server manzili koʻrsatilmagan.";
    while (!baza.empty() && baza.back() == L'/') baza.pop_back();

    std::wstring host, yol;
    INTERNET_PORT port = 0;
    bool xavfsiz = true;
    if (!urlniAjrat(baza + L"/models", host, yol, port, xavfsiz)) {
        return L"Server manzili notoʻgʻri.";
    }

    HINTERNET sessiya = WinHttpOpen(L"Kotib", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessiya) return L"Tarmoq ochilmadi.";
    WinHttpSetTimeouts(sessiya, 10000, 15000, 20000, 20000);

    HINTERNET ulanish = WinHttpConnect(sessiya, host.c_str(), port, 0);
    if (!ulanish) {
        WinHttpCloseHandle(sessiya);
        return L"Internetga ulanish yoʻq. Matn xom holda qoldi.";
    }

    HINTERNET sorov =
        WinHttpOpenRequest(ulanish, L"GET", yol.c_str(), nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, xavfsiz ? WINHTTP_FLAG_SECURE : 0);
    if (!sorov) {
        WinHttpCloseHandle(ulanish);
        WinHttpCloseHandle(sessiya);
        return L"Soʻrov yaratilmadi.";
    }

    std::wstring sarlavhalar;
    if (p.adapter == Adapter::Anthropic) {
        sarlavhalar = L"x-api-key: " + kalit + L"\r\nanthropic-version: 2023-06-01\r\n";
    } else {
        sarlavhalar = L"Authorization: Bearer " + kalit + L"\r\n";
    }

    std::wstring xato;
    if (!WinHttpSendRequest(sorov, sarlavhalar.c_str(), static_cast<DWORD>(sarlavhalar.size()),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(sorov, nullptr)) {
        xato = L"Internetga ulanish yoʻq. Matn xom holda qoldi.";
    } else {
        DWORD holat = 0, hajm = sizeof(holat);
        WinHttpQueryHeaders(sorov, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &holat, &hajm, WINHTTP_NO_HEADER_INDEX);

        if (holat == 401 || holat == 403) {
            xato = L"API kalit notoʻgʻri yoki muddati tugagan.";
        } else if (holat == 404) {
            // Provayderda `/models` yoʻq — bu xato emas.
        } else if (holat == 429) {
            xato = L"Provayder limiti tugadi. Biroz kutib qayta urinib koʻring.";
        } else if (holat >= 400) {
            xato = L"Provayder xatosi (" + std::to_wstring(holat) + L"). Keyinroq urinib koʻring.";
        } else {
            std::string tana;
            for (;;) {
                DWORD bor = 0;
                if (!WinHttpQueryDataAvailable(sorov, &bor) || bor == 0) break;
                std::string bufer(bor, '\0');
                DWORD oqildi = 0;
                if (!WinHttpReadData(sorov, bufer.data(), bor, &oqildi)) break;
                tana.append(bufer, 0, oqildi);
                // Model roʻyxati kattaligi cheklangan: OpenRouter ~400 model
                // beradi va bu ~1 MB. Undan oshgani — kutilmagan javob.
                if (tana.size() > 8u * 1024 * 1024) break;
            }
            chiqish = modellarniAjrat(tana);
        }
    }

    WinHttpCloseHandle(sorov);
    WinHttpCloseHandle(ulanish);
    WinHttpCloseHandle(sessiya);
    return xato;
}

std::wstring llmOqim(const LLMSorov& s, const std::function<void(const std::wstring&)>& bolak,
                     const std::function<bool()>& bekor) {
    if (s.kalit.empty()) return L"API kalit kiritilmagan. Sozlamalar → LLM boʻlimiga oʻting.";

    std::wstring baza = s.baseURL.empty() ? s.provayder.baseURL : s.baseURL;
    if (baza.empty()) return L"Server manzili koʻrsatilmagan.";
    while (!baza.empty() && baza.back() == L'/') baza.pop_back();

    const std::wstring toliq =
        baza + (s.provayder.adapter == Adapter::Anthropic ? L"/messages" : L"/chat/completions");

    std::wstring host, yol;
    INTERNET_PORT port = 0;
    bool xavfsiz = true;
    if (!urlniAjrat(toliq, host, yol, port, xavfsiz)) return L"Server manzili notoʻgʻri.";

    HINTERNET sessiya = WinHttpOpen(L"Kotib", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessiya) return L"Tarmoq ochilmadi.";
    // LLM javobi uzoq davom etadi — oʻqish chegarasi katta boʻlsin.
    WinHttpSetTimeouts(sessiya, 10000, 15000, 30000, 120000);

    std::wstring xato;
    HINTERNET ulanish = WinHttpConnect(sessiya, host.c_str(), port, 0);
    if (!ulanish) {
        WinHttpCloseHandle(sessiya);
        return L"Internetga ulanish yoʻq. Matn xom holda qoldi.";
    }

    HINTERNET sorov =
        WinHttpOpenRequest(ulanish, L"POST", yol.c_str(), nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, xavfsiz ? WINHTTP_FLAG_SECURE : 0);

    if (!sorov) {
        WinHttpCloseHandle(ulanish);
        WinHttpCloseHandle(sessiya);
        return L"Soʻrov yaratilmadi.";
    }

    std::wstring sarlavhalar = L"Content-Type: application/json\r\n";
    if (s.provayder.adapter == Adapter::Anthropic) {
        sarlavhalar += L"x-api-key: " + s.kalit + L"\r\n";
        sarlavhalar += L"anthropic-version: 2023-06-01\r\n";
    } else {
        sarlavhalar += L"Authorization: Bearer " + s.kalit + L"\r\n";
    }

    const std::string tana = soravTanasi(s.provayder, s.model, s.system, s.user);

    if (!WinHttpSendRequest(sorov, sarlavhalar.c_str(), static_cast<DWORD>(sarlavhalar.size()),
                            const_cast<char*>(tana.data()), static_cast<DWORD>(tana.size()),
                            static_cast<DWORD>(tana.size()), 0) ||
        !WinHttpReceiveResponse(sorov, nullptr)) {
        xato = L"Internetga ulanish yoʻq. Matn xom holda qoldi.";
    } else {
        DWORD holat = 0, hajm = sizeof(holat);
        WinHttpQueryHeaders(sorov, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &holat, &hajm, WINHTTP_NO_HEADER_INDEX);

        if (holat == 401 || holat == 403) {
            xato = L"API kalit notoʻgʻri yoki muddati tugagan.";
        } else if (holat == 404) {
            xato = L"Model topilmadi: «" + s.model + L"». Sozlamalarda model nomini tekshiring.";
        } else if (holat == 429) {
            xato = L"Provayder limiti tugadi. Biroz kutib qayta urinib koʻring.";
        } else if (holat >= 400) {
            xato = L"Provayder xatosi (" + std::to_wstring(holat) + L"). Keyinroq urinib koʻring.";
        } else {
            // SSE oqimi: satrlarni ajratib boramiz.
            std::string qoldiq;
            for (;;) {
                if (bekor && bekor()) break;

                DWORD bor = 0;
                if (!WinHttpQueryDataAvailable(sorov, &bor) || bor == 0) break;

                std::string bufer(bor, '\0');
                DWORD oqildi = 0;
                if (!WinHttpReadData(sorov, bufer.data(), bor, &oqildi)) break;
                qoldiq.append(bufer, 0, oqildi);

                size_t joy;
                while ((joy = qoldiq.find('\n')) != std::string::npos) {
                    std::string satr = qoldiq.substr(0, joy);
                    qoldiq.erase(0, joy + 1);
                    if (!satr.empty() && satr.back() == '\r') satr.pop_back();
                    if (satr.empty()) continue;

                    const SSEHodisa h = (s.provayder.adapter == Adapter::Anthropic)
                                            ? anthropicSSE(satr)
                                            : openaiSSE(satr);
                    if (h.tur == SSETuri::Matn && bolak) bolak(h.matn);
                    if (h.tur == SSETuri::Tugadi) {
                        qoldiq.clear();
                        break;
                    }
                }
            }
        }
    }

    WinHttpCloseHandle(sorov);
    WinHttpCloseHandle(ulanish);
    WinHttpCloseHandle(sessiya);
    return xato;
}

}  // namespace rubai
