// Tarjima dvigatelining C++ qatlami — navbat va boʻsh turishda boʻshatish.
// Interfeys va izohlar — `tarjimon.h`.

#include "tarjimon.h"

#include "matn_boluvchi.h"
#include "tarjima_bridge.h"
#include "util.h"
#include "statistika.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace rubai {

// ---- Model papkasi ---------------------------------------------------------

namespace TarjimaModel {

namespace {

// `tokenizer.json` ATAYLAB yoʻq: u HuggingFace tokenizatori uchun (17 MB),
// biz esa `sentencepiece.bpe.model` ni toʻgʻridan-toʻgʻri ishlatamiz.
const wchar_t* kKerakliFayllar[] = {
    L"model.bin",
    L"shared_vocabulary.json",
    L"sentencepiece.bpe.model",
    L"config.json",
};

}  // namespace

std::wstring papka() {
    // Nomda `-33b` boʻlishi SHART va macOS bilan bir xil (`yollar.swift`):
    // model versiyasi almashsa papka nomi ham almashadi, aks holda
    // `tayyormi` eski papkani toʻliq deb oʻqiydi va yangilanish hech qachon
    // boshlanmaydi. Windows'da 1.3B hech qachon chiqmagan, lekin qoida
    // ikkala platformada bir xil turishi kerak.
    const std::wstring dir = localAppDataDir();
    return dir.empty() ? std::wstring() : dir + L"\\tarjima-model-33b";
}

bool tayyormi(const std::wstring& p) {
    if (p.empty()) return false;
    for (const wchar_t* f : kKerakliFayllar) {
        WIN32_FILE_ATTRIBUTE_DATA d{};
        if (!GetFileAttributesExW((p + L"\\" + f).c_str(), GetFileExInfoStandard, &d)) {
            return false;
        }
        const long long hajm = (static_cast<long long>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
        if (hajm <= 0) return false;
    }
    return true;
}

bool tayyor() { return tayyormi(papka()); }

bool yetarliJoyBormi(const std::wstring& p, long long kerak) {
    if (p.empty()) return true;
    // Papkaning oʻzi hali yoʻq boʻlishi mumkin — disk ildizini soʻraymiz.
    std::wstring ildiz = p;
    const size_t ikkiNuqta = ildiz.find(L':');
    if (ikkiNuqta != std::wstring::npos) ildiz = ildiz.substr(0, ikkiNuqta + 2);

    ULARGE_INTEGER bosh{};
    if (!GetDiskFreeSpaceExW(ildiz.c_str(), &bosh, nullptr, nullptr)) return true;
    return static_cast<long long>(bosh.QuadPart) >= kerak;
}

}  // namespace TarjimaModel

// ---- Xato xabarlari --------------------------------------------------------

std::wstring tarjimaXatoXabari(TarjimaXatosi x) {
    switch (x) {
        case TarjimaXatosi::ModelYuklanmadi:
            return L"Tarjima modeli yuklanmadi. Xotira yetarli boʻlmasa, "
                   L"boshqa ilovalarni yopib koʻring.";
        case TarjimaXatosi::BekorQilindi: return L"Tarjima bekor qilindi.";
        case TarjimaXatosi::Yoq: break;
    }
    return {};
}

// ---- Tarjimon --------------------------------------------------------------

namespace {

std::mutex g_qulf;  // yukla/bosat ni seriyalash uchun
std::atomic<bool> g_band{false};
std::atomic<bool> g_bekor{false};
// Oxirgi ishlatilgan payt (steady_clock ticks). Idle taymer shunga qaraydi.
std::atomic<long long> g_oxirgiIsh{0};
std::atomic<bool> g_taymerKetyapti{false};

constexpr int kIdleSoniya = 180;

long long hozirSoniya() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// 180 soniya ishlatilmasa modelni boʻshatadi. Ish ketayotgan boʻlsa taymer
// SHUNCHAKI oʻtkazib yuborilmaydi — kutishda davom etadi, shu bilan ish
// tugagach ham boʻshatish amalga oshadi (macOS'dagi bilan bir xil).
void bosatishniRejalashtir() {
    bool kutilgan = false;
    if (!g_taymerKetyapti.compare_exchange_strong(kutilgan, true)) return;

    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (g_band.load()) continue;
            if (hozirSoniya() - g_oxirgiIsh.load() < kIdleSoniya) continue;

            std::lock_guard<std::mutex> q(g_qulf);
            if (rubai_tarjima_yuklanganmi() == 1) {
                rubai_tarjima_bosat();
                logWrite(L"tarjima: model RAM'dan boʻshatildi");
            }
            g_taymerKetyapti.store(false);
            return;
        }
    }).detach();
}

}  // namespace

Tarjimon& Tarjimon::birgalik() {
    static Tarjimon t;
    return t;
}

bool Tarjimon::yuklanganmi() const { return rubai_tarjima_yuklanganmi() == 1; }
bool Tarjimon::bandmi() const { return g_band.load(); }
void Tarjimon::bekorQil() { g_bekor.store(true); }

bool Tarjimon::yukla() {
    std::lock_guard<std::mutex> q(g_qulf);
    if (rubai_tarjima_yuklanganmi() == 1) return true;

    if (!TarjimaModel::tayyor()) {
        logWrite(L"tarjima: model papkasi toʻliq emas");
        return false;
    }
    const std::string yol = toUtf8(TarjimaModel::papka());
    if (rubai_tarjima_yukla(yol.c_str()) == 0) return true;

    logWrite(L"tarjima: model yuklanmadi (xotira yoki buzuq papka)");
    return false;
}

void Tarjimon::bosat() {
    std::lock_guard<std::mutex> q(g_qulf);
    if (rubai_tarjima_yuklanganmi() != 1) return;
    rubai_tarjima_bosat();
    logWrite(L"tarjima: model RAM'dan boʻshatildi");
}

void Tarjimon::tarjimaQil(const std::wstring& matn, const Til& manba, const Til& maqsad,
                          std::function<void(int, int)> jarayon,
                          std::function<void(std::wstring, TarjimaXatosi)> tugadi) {
    const Qatorlar qatorlar = MatnBoluvchi::bol(matn);
    const std::vector<std::wstring> manbaJumlalar = MatnBoluvchi::jumlalar(qatorlar);

    // Tarjima qiladigan narsa yoʻq (boʻsh yoki faqat belgilar), yoki manba
    // va maqsad bir xil — matnni oʻz holicha qaytaramiz.
    if (manbaJumlalar.empty() || manba.nllb == maqsad.nllb) {
        if (tugadi) tugadi(matn, TarjimaXatosi::Yoq);
        return;
    }

    g_bekor.store(false);
    g_band.store(true);
    g_oxirgiIsh.store(hozirSoniya());

    const std::string manbaKod = manba.nllb;
    const std::string maqsadKod = maqsad.nllb;
    // Anonim oʻlchov uchun: belgi soni (matnning oʻzi EMAS) va boshlanish vaqti.
    const long long belgiSoni = static_cast<long long>(matn.size());

    std::thread([this, qatorlar, manbaJumlalar, manbaKod, maqsadKod, belgiSoni,
                 jarayon = std::move(jarayon), tugadi = std::move(tugadi)] {
        const auto boshlandi = std::chrono::steady_clock::now();
        auto ishlovSoniya = [&] {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - boshlandi)
                .count();
        };
        struct Tugatgich {
            ~Tugatgich() {
                g_band.store(false);
                g_oxirgiIsh.store(hozirSoniya());
                bosatishniRejalashtir();
            }
        } tugatgich;

        if (!yuklanganmi() && !yukla()) {
            amalYubor("tarjima", 0, belgiSoni, ishlovSoniya(), "cpu", "xato:model");
            if (tugadi) tugadi(L"", TarjimaXatosi::ModelYuklanmadi);
            return;
        }

        const int jami = static_cast<int>(manbaJumlalar.size());
        std::vector<std::wstring> tarjimalar;
        tarjimalar.reserve(manbaJumlalar.size());

        for (const auto& jumla : manbaJumlalar) {
            if (g_bekor.load()) {
                if (tugadi) tugadi(L"", TarjimaXatosi::BekorQilindi);
                return;
            }
            const std::string utf8 = toUtf8(jumla);
            char* p = rubai_tarjima(utf8.c_str(), manbaKod.c_str(), maqsadKod.c_str());
            if (!p) {
                // Bitta jumla chiqmasa butun ish toʻxtamaydi — asl matn
                // qoladi. Foydalanuvchi matni logga YOZILMAYDI, faqat raqam.
                logWrite(L"tarjima: " + std::to_wstring(tarjimalar.size() + 1) +
                         L"-jumla tarjima qilinmadi");
                tarjimalar.push_back(jumla);
                continue;
            }
            tarjimalar.push_back(toWide(p));
            rubai_tarjima_str_bosat(p);
            if (jarayon) jarayon(static_cast<int>(tarjimalar.size()), jami);
        }

        amalYubor("tarjima", 0, belgiSoni, ishlovSoniya(), "cpu", "ok");
        if (tugadi) tugadi(MatnBoluvchi::yig(qatorlar, tarjimalar), TarjimaXatosi::Yoq);
    }).detach();
}

}  // namespace rubai
