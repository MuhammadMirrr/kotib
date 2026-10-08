// Umumiy yordamchilar: matn kodlash, yoʻllar, log.
// Interfeys va izohlar — `util.h`.

#include "util.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <mutex>

namespace rubai {

// ---- Matn kodlash --------------------------------------------------------

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

// ---- Yoʻllar -------------------------------------------------------------

std::wstring exeDir() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);  // yoʻl uzunroq — buferni kattalashtiramiz
    }
    size_t slash = buf.find_last_of(L'\\');
    return slash == std::wstring::npos ? buf : buf.substr(0, slash);
}

static std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p))) return {};
    std::wstring s = p ? p : L"";
    CoTaskMemFree(p);
    return s;
}

// Ilova nomi 1.1.0 da «Audio-Matnga» dan «Kotib» ga oʻzgardi. Eski
// foydalanuvchining sozlamalari va logi eski papkada qolgan — ularni bir marta
// koʻchiramiz, aks holda u yangilanishdan keyin mikrofon tanlovini va
// tugmasini qaytadan sozlashga majbur boʻladi.
//
// macOS tomonida ayni shu ish `src/yollar.swift` da bajariladi. Qoida bir xil:
// koʻchirish FAQAT yangi papka hali yoʻq boʻlsa ishlaydi, shuning uchun uni
// takroran chaqirish xavfsiz va u hech qachon yangi maʼlumot ustiga yozmaydi.
static void eskiPapkadanKochir(const std::wstring& eski, const std::wstring& yangi) {
    const DWORD eskiHolat = GetFileAttributesW(eski.c_str());
    if (eskiHolat == INVALID_FILE_ATTRIBUTES) return;                          // eski yoʻq
    if (GetFileAttributesW(yangi.c_str()) != INVALID_FILE_ATTRIBUTES) return;  // yangi bor

    // MoveFileW butun papkani bir amalda koʻchiradi (bitta diskda). Muvaffaqiyatsiz
    // boʻlsa jim qolamiz: eski papka joyida turadi va ilova standart sozlama bilan
    // ishlayveradi — bu maʼlumotni yoʻqotishdan koʻra yaxshiroq.
    MoveFileW(eski.c_str(), yangi.c_str());
}

static std::wstring subDir(REFKNOWNFOLDERID id) {
    std::wstring base = knownFolder(id);
    if (base.empty()) return {};
    const std::wstring dir = base + L"\\Kotib";
    eskiPapkadanKochir(base + L"\\Audio-Matnga", dir);
    ensureDir(dir);
    return dir;
}

std::wstring appDataDir() { return subDir(FOLDERID_RoamingAppData); }
std::wstring localAppDataDir() { return subDir(FOLDERID_LocalAppData); }

bool fileExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool ensureDir(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesW(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES) return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    // ota-papkalarni ham yaratamiz
    size_t slash = path.find_last_of(L'\\');
    if (slash != std::wstring::npos && slash > 2) ensureDir(path.substr(0, slash));
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool isAscii(const std::wstring& s) {
    for (wchar_t c : s)
        if (c > 127) return false;
    return true;
}

std::string pathForC(const std::wstring& path) {
    if (path.empty()) return {};

    // Oddiy holat: yoʻl butunlay ASCII — toʻgʻridan-toʻgʻri uzatamiz.
    if (isAscii(path)) return toUtf8(path);

    // ASCII boʻlmagan belgi bor: 8.3 qisqa nomga oʻgirib koʻramiz.
    DWORD n = GetShortPathNameW(path.c_str(), nullptr, 0);
    if (n > 0) {
        std::wstring shortPath(n, L'\0');
        DWORD got = GetShortPathNameW(path.c_str(), shortPath.data(), n);
        if (got > 0 && got < n) {
            shortPath.resize(got);
            if (isAscii(shortPath)) return toUtf8(shortPath);
        }
    }

    // Qisqa nomlar ham yordam bermadi — chaqiruvchi xatoni koʻrsatishi kerak.
    return {};
}

// ---- Log -----------------------------------------------------------------

namespace {
std::mutex g_logMutex;
std::wstring g_logPath;

std::wstring timestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf(buf, 32, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour,
             st.wMinute, st.wSecond);
    return buf;
}
}  // namespace

std::wstring logPath() { return g_logPath; }

namespace {

// Log 1 MB dan oshsa aylantiriladi: dictation.log → .1.log → .2.log (eng
// eskisi tashlanadi). macOS'dagi `LogSiyosati` bilan bir xil (src/log_siyosati.swift).
// Ilgari faqat ishga tushishda va 2 MB dan keyin bitta zaxira olinardi — trey
// ilovasi haftalab yopilmaydi va log shu orada cheksiz oʻsardi (spec G1).
constexpr ULONGLONG kLogChegarasi = 1024ull * 1024;

std::wstring logNusxasi(int i) {
    if (i == 0) return g_logPath;
    std::wstring s = g_logPath;
    const size_t nuqta = s.rfind(L'.');
    return s.substr(0, nuqta) + L"." + std::to_wstring(i) + s.substr(nuqta);
}

// g_logMutex ushlangan holda chaqiriladi.
void kerakBolsaAylantir() {
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    if (!GetFileAttributesExW(g_logPath.c_str(), GetFileExInfoStandard, &fa)) return;
    const ULONGLONG hajm = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    if (hajm <= kLogChegarasi) return;
    // Eski nomdagi zaxira (1.1.0: dictation.log.1) — bir marta tozalanadi.
    DeleteFileW((g_logPath + L".1").c_str());
    MoveFileExW(logNusxasi(1).c_str(), logNusxasi(2).c_str(), MOVEFILE_REPLACE_EXISTING);
    MoveFileExW(logNusxasi(0).c_str(), logNusxasi(1).c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace

void logInit() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::wstring dir = localAppDataDir();
    if (dir.empty()) return;
    g_logPath = dir + L"\\dictation.log";
    kerakBolsaAylantir();
}

void logWrite(const std::wstring& msg) {
    OutputDebugStringW((L"[rubai] " + msg + L"\n").c_str());

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logPath.empty()) return;
    kerakBolsaAylantir();

    std::string line = toUtf8(timestamp() + L" " + msg + L"\r\n");
    HANDLE h = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
    CloseHandle(h);
}

}  // namespace rubai
