// Tarjima modelini yuklab olish va arxivdan ochish.
// Interfeys va izohlar — `tarjima_yuklovchi.h`.

#include "tarjima_yuklovchi.h"

#include "tarjimon.h"
#include "util.h"
#include "yuklovchi.h"

#include <windows.h>
#include <shlwapi.h>
#include <winhttp.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace rubai {

const wchar_t* TarjimaYuklovchi::kUrl =
    L"https://cdn.mirqobilov.com/dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz";

namespace {

// Nomlar ATAYLAB ASCII: `tar` ularni nisbiy yoʻl sifatida oladi (`arxivniOch`).
constexpr wchar_t kArxivNomi[] = L"tarjima-model.tar.gz";
constexpr wchar_t kVaqtNomi[] = L"tarjima-model-33b.yangi";

std::wstring qollabPapka() { return localAppDataDir(); }
std::wstring arxivYoli() { return qollabPapka() + L"\\" + kArxivNomi; }
std::wstring qismYoli() { return arxivYoli() + L".part"; }
std::wstring vaqtinchalik() { return qollabPapka() + L"\\" + kVaqtNomi; }
// Arxiv bir marta ochilmay qolganining belgisi — qarang `ishla`.
std::wstring xatoBelgisi() { return arxivYoli() + L".xato"; }

// Papkani ichidagilari bilan oʻchiradi.
void papkaniOchir(const std::wstring& yol) {
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW((yol + L"\\*").c_str(), &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring nom = f.cFileName;
            if (nom == L"." || nom == L"..") continue;
            const std::wstring toliq = yol + L"\\" + nom;
            if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                papkaniOchir(toliq);
            else
                DeleteFileW(toliq.c_str());
        } while (FindNextFileW(h, &f));
        FindClose(h);
    }
    RemoveDirectoryW(yol.c_str());
}

// `tar.exe` — Windows 10 1803 dan beri tizimda bor (bsdtar). Bizning eng
// past tizimimiz 1809, shuning uchun u har doim topiladi; topilmasa
// foydalanuvchiga aniq xabar beramiz.
std::wstring tarYoli() {
    wchar_t win[MAX_PATH] = {0};
    if (!GetSystemDirectoryW(win, MAX_PATH)) return {};
    const std::wstring yol = std::wstring(win) + L"\\tar.exe";
    return (GetFileAttributesW(yol.c_str()) == INVALID_FILE_ATTRIBUTES) ? std::wstring() : yol;
}

// Arxivni ochadi. Qaytaradi: `tar` ning chiqish kodi, yoki -1 (ishga tushmadi).
//
// `tar.exe` argumentlarni ANSI kod sahifasida oʻqiydi: foydalanuvchi nomi
// lotin boʻlmasa (`C:\Users\Мухаммад\…`) toʻliq yoʻl buzilib yetib borardi
// va 3,1 GB arxiv «ochilmadi» deb oʻchirilardi (barqarorlik E7). Endi `tar`
// arxiv turgan papkada ishga tushiriladi (`CreateProcessW` ishchi papkani
// keng belgili yoʻl bilan oladi) va unga faqat nisbiy ASCII nomlar beriladi.
int arxivniOch(const std::wstring& ishchiPapka) {
    const std::wstring tar = tarYoli();
    if (tar.empty()) return -1;

    // `--strip-components 1` — arxiv ichidagi bitta tashqi papkani tashlaydi
    // (macOS bilan bir xil).
    std::wstring buyruq = L"\"" + tar + L"\" -xzf \"" + std::wstring(kArxivNomi) + L"\" -C \"" +
                          std::wstring(kVaqtNomi) + L"\" --strip-components 1";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // konsol oynasi koʻrinmasin
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(nullptr, buyruq.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        ishchiPapka.c_str(), &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD kod = 1;
    GetExitCodeProcess(pi.hProcess, &kod);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(kod);
}

}  // namespace

// ---- Ichki holat -----------------------------------------------------------

struct TarjimaYuklovchi::Ichki {
    std::atomic<bool> ketyapti{false};
    std::atomic<bool> bekor{false};

    std::mutex qulf;
    std::function<void(long long, long long)> onJarayon;
    std::function<void(bool, std::wstring)> onTugadi;

    void jarayon(long long a, long long b) {
        std::lock_guard<std::mutex> q(qulf);
        if (onJarayon) onJarayon(a, b);
    }
    void tugadi(bool ok, const std::wstring& xato) {
        ketyapti.store(false);
        std::lock_guard<std::mutex> q(qulf);
        if (onTugadi) onTugadi(ok, xato);
    }
    void uzil() {
        std::lock_guard<std::mutex> q(qulf);
        onJarayon = nullptr;
        onTugadi = nullptr;
    }

    void ishla();
    bool yuklab(std::wstring& xato);
};

// ---- Yuklash ---------------------------------------------------------------

bool TarjimaYuklovchi::Ichki::yuklab(std::wstring& xato) {
    const YuklashNatijasi n = faylYukla(
        TarjimaYuklovchi::kUrl, qismYoli(), TarjimaModel::kTaxminiyBayt,
        [this](long long a, long long b) { jarayon(a, b); }, [this] { return bekor.load(); });
    xato = n.xato;
    return n.ok;
}

// ---- Ochish va koʻchirish --------------------------------------------------

void TarjimaYuklovchi::Ichki::ishla() {
    const std::wstring papka = qollabPapka();
    if (papka.empty()) {
        tugadi(false, L"Maʼlumot papkasi topilmadi.");
        return;
    }

    if (!TarjimaModel::yetarliJoyBormi(papka, TarjimaModel::kKerakliJoy)) {
        const int gb = static_cast<int>(TarjimaModel::kKerakliJoy / (1024LL * 1024 * 1024)) + 1;
        tugadi(false, L"Diskda joy yetarli emas — kamida " + std::to_wstring(gb) + L" GB kerak.");
        return;
    }

    logWrite(L"tarjima modeli: yuklash boshlandi");

    std::wstring xato;
    if (!yuklab(xato)) {
        // Yarim fayl ATAYLAB oʻchirilmaydi — keyingi urinishda davom etadi.
        if (bekor.load())
            logWrite(L"tarjima modeli: bekor qilindi (yarim fayl saqlanib qoldi)");
        else
            logWrite(L"tarjima modeli: yuklanmadi");
        tugadi(false, xato);
        return;
    }

    const std::wstring arxiv = arxivYoli();
    const std::wstring vaqt = vaqtinchalik();

    DeleteFileW(arxiv.c_str());
    if (!MoveFileW(qismYoli().c_str(), arxiv.c_str())) {
        tugadi(false, L"Faylni koʻchirib boʻlmadi.");
        return;
    }

    papkaniOchir(vaqt);
    if (!CreateDirectoryW(vaqt.c_str(), nullptr)) {
        DeleteFileW(arxiv.c_str());
        tugadi(false, L"Vaqtinchalik papka yaratilmadi.");
        return;
    }

    const int kod = arxivniOch(papka);
    if (kod == -1) {
        papkaniOchir(vaqt);
        MoveFileW(arxiv.c_str(), qismYoli().c_str());  // arxiv saqlanadi
        tugadi(false, L"Windows'ning `tar` vositasi topilmadi. "
                      L"Windows 10 1803 yoki yangiroq versiya kerak.");
        return;
    }
    if (kod != 0 || !TarjimaModel::tayyormi(vaqt)) {
        papkaniOchir(vaqt);
        logWrite(L"tarjima modeli: arxiv ochilmadi (tar kodi " + std::to_wstring(kod) + L")");
        // Birinchi xatoda arxiv OʻCHIRILMAYDI (E7): sabab muhitda boʻlishi
        // mumkin (disk, antivirus) va 3,1 GB ni qayta yuklash qimmat. `.part`
        // ga qaytariladi — keyingi urinish uni tarmoqsiz oladi. Ketma-ket
        // ikkinchi xato — arxiv haqiqatan buzuq: oʻchiriladi, keyingisi boshidan.
        if (GetFileAttributesW(xatoBelgisi().c_str()) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW(arxiv.c_str());
            DeleteFileW(xatoBelgisi().c_str());
            tugadi(false, L"Arxiv buzilgan. Qayta urinib koʻring — model boshidan yuklanadi.");
        } else {
            MoveFileW(arxiv.c_str(), qismYoli().c_str());
            HANDLE b = CreateFileW(xatoBelgisi().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
            if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
            tugadi(false, L"Arxivni ochib boʻlmadi. Qayta urinib koʻring — "
                          L"fayl qayta yuklanmaydi.");
        }
        return;
    }
    DeleteFileW(xatoBelgisi().c_str());

    // Yangi papka joyiga faqat toʻliq boʻlgani tasdiqlangandan keyin oʻtadi.
    //
    // Eski model RAM'da ochiq turgan boʻlsa uning fayllari band boʻladi va
    // papkani oʻchirib boʻlmaydi — avval boʻshatamiz.
    Tarjimon::birgalik().bosat();
    papkaniOchir(TarjimaModel::papka());
    if (!MoveFileW(vaqt.c_str(), TarjimaModel::papka().c_str())) {
        papkaniOchir(vaqt);
        tugadi(false, L"Modelni joyiga koʻchirib boʻlmadi.");
        return;
    }
    DeleteFileW(arxiv.c_str());

    logWrite(L"tarjima modeli: tayyor");
    tugadi(true, L"");
}

// ---- Ommaviy interfeys -----------------------------------------------------

TarjimaYuklovchi::TarjimaYuklovchi() : ichki_(std::make_shared<Ichki>()) {}

TarjimaYuklovchi::~TarjimaYuklovchi() {
    bekorQil();
    ichki_->uzil();
}

bool TarjimaYuklovchi::ketyaptimi() const { return ichki_->ketyapti.load(); }

void TarjimaYuklovchi::bekorQil() { ichki_->bekor.store(true); }

void TarjimaYuklovchi::boshla(std::function<void(long long, long long)> jarayon,
                              std::function<void(bool, std::wstring)> tugadi) {
    if (ichki_->ketyapti.load()) return;

    {
        std::lock_guard<std::mutex> q(ichki_->qulf);
        ichki_->onJarayon = std::move(jarayon);
        ichki_->onTugadi = std::move(tugadi);
    }
    ichki_->bekor.store(false);
    ichki_->ketyapti.store(true);

    auto ichki = ichki_;
    std::thread([ichki] { ichki->ishla(); }).detach();
}

}  // namespace rubai
