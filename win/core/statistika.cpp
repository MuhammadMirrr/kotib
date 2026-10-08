// Anonim foydalanish statistikasi. Interfeys va izohlar — `statistika.h`.

#include "statistika.h"

#include "config.h"
#include "json.h"
#include "kotib_versiya.h"
#include "util.h"

#include <windows.h>
#include <winhttp.h>

#include <ctime>
#include <thread>

namespace rubai {

namespace {

// Server manzili — macOS tomonidagi `statistika.swift` bilan bir xil.
constexpr wchar_t kHost[] = L"stat.mirqobilov.com";
constexpr wchar_t kPingYoli[] = L"/v1/ping";

// 20 soat, 24 emas: aks holda har kuni bir xil vaqtda ochadigan foydalanuvchi
// chegaradan bir necha daqiqa oldin qolib, kun tashlab ketardi.
constexpr long long kOraliq = 20 * 3600;

// Windows versiyasi, masalan "Windows 11 26100".
//
// `GetVersionEx` eskirgan va manifestga bogʻliq yolgʻon qiymat qaytaradi.
// `RtlGetVersion` esa haqiqiy raqamni beradi — u ntdll ichida, shuning uchun
// ish paytida topamiz.
std::wstring osVersiyasi() {
    typedef LONG(WINAPI * RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return L"Windows";

    auto fn = reinterpret_cast<RtlGetVersionFn>(
        reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
    if (!fn) return L"Windows";

    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (fn(&v) != 0) return L"Windows";

    // Windows 11 oʻzini 10.0 deb koʻrsatadi; ularni build raqami ajratadi
    // (22000 dan boshlab — Windows 11).
    const wchar_t* nom =
        (v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000) ? L"Windows 11" : L"Windows 10";
    if (v.dwMajorVersion < 10) nom = L"Windows";

    return std::wstring(nom) + L" " + std::to_wstring(v.dwBuildNumber);
}

// ── Qurilma muhiti ─────────────────────────────────────────────────────
// Faqat MUHIT: model, hajm, arxitektura. Qurilmani shaxsan belgilaydigan
// hech narsa (seriya raqami, MachineGuid, MAC, foydalanuvchi nomi) OʻQILMAYDI.

// Ishlayotgan arxitektura — Windows'da x64 va ARM64 alohida .exe.
const char* arxitektura() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "x64";
#endif
}

// Protsessor modeli — registrdan (masalan "AMD Ryzen 7 5800H").
std::wstring cpuModeli() {
    wchar_t bufer[256]{};
    DWORD oʻlcham = sizeof(bufer);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, bufer,
                     &oʻlcham) == ERROR_SUCCESS) {
        std::wstring s(bufer);
        // Ortiqcha boʻshliqlarni qisqartiramiz.
        size_t oxiri = s.find_last_not_of(L' ');
        if (oxiri != std::wstring::npos) s.erase(oxiri + 1);
        return s;
    }
    return L"?";
}

// Videokarta modeli — birlamchi displey adapteri (masalan "NVIDIA GeForce RTX 3060").
// EnumDisplayDevices ishlatamiz: qoʻshimcha kutubxona (DXGI/WMI) kerak emas.
std::wstring gpuModeli() {
    DISPLAY_DEVICEW d{};
    d.cb = sizeof(d);
    if (EnumDisplayDevicesW(nullptr, 0, &d, 0)) return d.DeviceString;
    return L"?";
}

// Operativ xotira, gigabaytda.
int ramGb() {
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    if (GlobalMemoryStatusEx(&m)) {
        return static_cast<int>((m.ullTotalPhys + (1ull << 29)) / (1ull << 30));
    }
    return 0;
}

// CPU yadrolari (mantiqiy).
int yadroSoni() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return static_cast<int>(si.dwNumberOfProcessors);
}

// Tasodifiy 32-belgili hex. Kriptografik generator ishlatiladi — `rand()`
// bir xil sekundda ochilgan ikki oʻrnatmaga bir xil ID berishi mumkin edi.
std::wstring yangiId() {
    unsigned char bayt[16]{};
    // BCryptGenRandom oʻrniga RtlGenRandom: u advapi32 da, qoʻshimcha
    // kutubxona ulashni talab qilmaydi va shu maqsad uchun yetarli.
    HMODULE advapi = LoadLibraryW(L"advapi32.dll");
    bool tayyor = false;
    if (advapi) {
        typedef BOOLEAN(WINAPI * GenRandomFn)(PVOID, ULONG);
        auto fn = reinterpret_cast<GenRandomFn>(
            reinterpret_cast<void*>(GetProcAddress(advapi, "SystemFunction036")));
        if (fn) tayyor = fn(bayt, sizeof(bayt)) != FALSE;
        FreeLibrary(advapi);
    }
    if (!tayyor) {
        // Zaxira yoʻl: vaqt va jarayon raqamidan. Ideal emas, lekin ID'siz
        // qolishdan yaxshiroq — u faqat «bu bitta oʻrnatma» degani.
        const auto t = static_cast<unsigned long long>(GetTickCount64()) ^
                       (static_cast<unsigned long long>(GetCurrentProcessId()) << 32);
        memcpy(bayt, &t, sizeof(t));
        memcpy(bayt + 8, &t, sizeof(t));
    }

    std::wstring s;
    wchar_t bufer[3];
    for (unsigned char b : bayt) {
        swprintf(bufer, 3, L"%02x", b);
        s += bufer;
    }
    return s;
}

// WinHTTP bilan soʻrov. `tana` boʻsh boʻlsa GET, aks holda POST.
// Xatoda boʻsh satr qaytaradi — chaqiruvchi jim qoladi.
std::string sorov(const wchar_t* yol, const std::string& tana) {
    HINTERNET sessiya = WinHttpOpen(L"Kotib", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessiya) return {};

    // Vaqt chegaralari: umumiy 10 soniyadan oshmasin. Tarmoq sekin boʻlsa
    // ham ilovaning ishiga taʼsir qilmaydi — bu alohida oqim.
    WinHttpSetTimeouts(sessiya, 5000, 5000, 5000, 5000);

    std::string natija;
    HINTERNET ulanish = WinHttpConnect(sessiya, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (ulanish) {
        HINTERNET sorovH = WinHttpOpenRequest(ulanish, tana.empty() ? L"GET" : L"POST", yol,
                                              nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

        if (sorovH) {
            const wchar_t* sarlavha = tana.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                   : L"Content-Type: application/json\r\n";

            if (WinHttpSendRequest(sorovH, sarlavha, tana.empty() ? 0 : (DWORD)-1L,
                                   tana.empty() ? nullptr : (LPVOID)tana.data(), (DWORD)tana.size(),
                                   (DWORD)tana.size(), 0) &&
                WinHttpReceiveResponse(sorovH, nullptr)) {
                DWORD bor = 0;
                while (WinHttpQueryDataAvailable(sorovH, &bor) && bor > 0) {
                    // Javob kichik — 64 KB dan oshsa uni oʻqishning maʼnosi yoʻq,
                    // demak bu bizning serverimiz emas.
                    if (natija.size() > 64 * 1024) break;
                    std::string bolak(bor, '\0');
                    DWORD oqildi = 0;
                    if (!WinHttpReadData(sorovH, bolak.data(), bor, &oqildi)) break;
                    natija.append(bolak, 0, oqildi);
                }
            }
            WinHttpCloseHandle(sorovH);
        }
        WinHttpCloseHandle(ulanish);
    }
    WinHttpCloseHandle(sessiya);
    return natija;
}

}  // namespace

void amalYubor(const char* tur, double ovoz_s, long long belgi, double ishlov_s,
               const char* backend, const char* natija) {
    Settings s = loadSettings();
    if (s.ornatmaId.size() != 32) {
        s.ornatmaId = yangiId();
        saveSettings(s);
    }
    const std::string id = toUtf8(s.ornatmaId);
    const std::string versiya = toUtf8(joriyVersiya());
    const std::string turS = tur ? tur : "";
    const std::string backendS = backend ? backend : "";
    // Natija toifasi — faqat harf/raqam/pastki chiziq. Xom xato matni yoki
    // fayl yoʻli hech qachon tushmasligi uchun server ham tekshiradi.
    const std::string natijaS = natija ? natija : "ok";

    // Kasr son lokalga bogʻliq boʻlmasligi shart: ilgari bu yerdagi
    // `snprintf("%.2f")` tarjimon `setlocale` chaqirgach ru/uz Windows'da
    // «1,23» yozardi va server buzuq JSON'ni rad etardi (E2).
    auto raqam = [](double x) { return kasrSon(x, 2); };

    std::string tana = "{\"id\":\"" + id + "\",\"platforma\":\"win\",\"versiya\":\"" + versiya +
                       "\",\"tur\":\"" + turS + "\",\"ishlov_s\":" + raqam(ishlov_s);
    if (ovoz_s > 0) tana += ",\"ovoz_s\":" + raqam(ovoz_s);
    if (belgi > 0) tana += ",\"belgi\":" + std::to_string(belgi);
    if (!backendS.empty()) tana += ",\"backend\":\"" + backendS + "\"";
    tana += ",\"natija\":\"" + natijaS + "\"}";

    // Javob kerak emas — alohida oqimda jim yuboramiz.
    std::thread([tana = std::move(tana)] { sorov(L"/v1/amal", tana); }).detach();
}

std::wstring ishgaTushishMuhiti() {
    return L"Kotib " + joriyVersiya() + L" " + toWide(arxitektura()) + L", " + osVersiyasi();
}

std::wstring joriyVersiya() {
    // `VERSION` → CMake → `kotib_versiya.h`. Ilgari .exe resursidagi raqamli
    // versiya oʻqilardi va `-sinov1` qoʻshimchasi yoʻqolardi.
    return toWide(KOTIB_VER_STR);
}

void pingYubor() {
    Settings s = loadSettings();

    const long long hozir = static_cast<long long>(std::time(nullptr));
    // Oxirgi vaqt kelajakda boʻlsa (soat orqaga surilgan) — kutmaymiz, aks
    // holda oʻsha sanagacha ping boʻlmasdi.
    if (hozir >= s.oxirgiTekshiruv && hozir - s.oxirgiTekshiruv < kOraliq) return;

    // Vaqtni DARHOL yozamiz, soʻrovdan oldin. Aks holda tarmoq yoʻq mashinada
    // ilova har ochilganda 5 soniyalik urinish qilardi.
    s.oxirgiTekshiruv = hozir;
    if (s.ornatmaId.size() != 32) s.ornatmaId = yangiId();
    saveSettings(s);

    const std::wstring id = s.ornatmaId;
    const std::wstring versiya = joriyVersiya();
    const std::wstring os = osVersiyasi();
    const std::string cpu = toUtf8(cpuModeli());
    const std::string gpu = toUtf8(gpuModeli());
    const int ram = ramGb();
    const int yadro = yadroSoni();

    // Alohida oqim: tarmoq javob bermasa ham UI muzlab qolmaydi.
    // `detach` — ilova yopilsa oqim oʻzi tugaydi, kutish shart emas.
    // Statistika DOIM yoqiq (oʻchirish tugmasi yoʻq) — maxfiylik siyosatida
    // eʼlon qilingan.
    std::thread([id, versiya, os, cpu, gpu, ram, yadro] {
        // JSON'ni qoʻlda yigʻamiz — CPU/GPU nomida qoʻshtirnoq boʻlishi mumkin,
        // shuning uchun oddiy qochirish qilamiz.
        auto qoch = [](const std::string& x) {
            std::string r;
            for (char c : x) {
                if (c == '"' || c == '\\') r += '\\';
                r += c;
            }
            return r;
        };
        const std::string tana =
            "{\"id\":\"" + toUtf8(id) + "\",\"platforma\":\"win\",\"versiya\":\"" +
            toUtf8(versiya) + "\",\"os\":\"" + qoch(toUtf8(os)) + "\",\"arx\":\"" + arxitektura() +
            "\",\"cpu\":\"" + qoch(cpu) + "\",\"gpu\":\"" + qoch(gpu) +
            "\",\"ram_gb\":" + std::to_string(ram) + ",\"yadro\":" + std::to_string(yadro) + "}";
        // Javob kerak emas: undagi «versiya» maydoni faqat 1.1.0 lar uchun.
        sorov(kPingYoli, tana);
    }).detach();
}

}  // namespace rubai
