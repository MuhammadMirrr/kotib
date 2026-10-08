// Bitta faylni matnga oʻgirish ishi: dekodlash → whisper → saqlash.
// Interfeys va izohlar — `ish.h`.

#include "ish.h"

#include "config.h"
#include "engine.h"
#include "media_decode.h"
#include "samples.h"
#include "util.h"
#include "whisper_bridge.h"

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

namespace rubai {

namespace {

// 90 daqiqadan uzun fayllar boʻlaklanadi — 1 soat ≈ 230 MB RAM.
constexpr double kBolaklashChegarasi = 90 * 60;
constexpr double kBolakUzunligi = 30 * 60;
// Boʻlak chegarasini shu oynada jimlik boʻyicha tanlaymiz.
constexpr double kChegaraOynasi = 30;

std::mutex g_holatQulfi;
bool g_ishlayapti = false;

// Tekshirish va oʻrnatishni BITTA qulf ostida birlashtiradi — ular orasida
// poyga holati boʻlmasin.
bool ishniOlish() {
    std::lock_guard<std::mutex> q(g_holatQulfi);
    if (g_ishlayapti) return false;
    g_ishlayapti = true;
    return true;
}

void holatniQoy(bool v) {
    std::lock_guard<std::mutex> q(g_holatQulfi);
    g_ishlayapti = v;
}

}  // namespace

// ---- Ichki holat -----------------------------------------------------------

struct TranskripsiyaIshi::Ichki {
    std::wstring yol;
    std::atomic<bool> bekor{false};

    std::function<void(double, std::wstring)> onProgress;
    std::function<void(Hujjat)> onTayyor;
    std::function<void(std::wstring)> onXato;

    // Koʻrilgan eng yuqori qiymat. Progress bar hech qachon orqaga
    // qaytmasligi uchun shu yerda monoton qilib qisqichlanadi: boʻlak
    // chegaralarida ikkita ulush hisoblash usuli orasida bir necha
    // soniyalik nomuvofiqlik boʻlishi mumkin.
    double oxirgiProgress = 0;

    void progress(double p, const std::wstring& s) {
        oxirgiProgress = (p > oxirgiProgress) ? p : oxirgiProgress;
        if (onProgress) onProgress(oxirgiProgress, s);
    }

    void tugat() {
        holatniQoy(false);
        // Model band bayrogʻini boʻshatamiz — `engine` uni koʻrib idle
        // taymerini qayta ishga tushiradi.
        Engine::instance().setBand(false);
    }

    void xato(const std::wstring& m) {
        tugat();
        if (onXato) onXato(m);
    }

    void ishla();

    // Bitta oraliqni oʻqib whisper'ga beradi. Segment vaqtlari `siljish` ga
    // suriladi. `ulushBoshi`/`ulushOxiri` — butun bardagi shu boʻlakning
    // ulushi (0…1).
    // `oxirgiMi` false boʻlsa, boʻlak oxiri jimlik nuqtasi boʻyicha
    // kesiladi va haqiqiy kesim vaqti `haqiqiyOxir` ga yoziladi.
    bool bolakniQaytaIshla(double boshi, double oxiri, double umumiy, double siljish, bool oxirgiMi,
                           std::vector<Segment>& chiqish, double* haqiqiyOxir);
};

// ---- Bitta boʻlak ----------------------------------------------------------

bool TranskripsiyaIshi::Ichki::bolakniQaytaIshla(double boshi, double oxiri, double umumiy,
                                                 double siljish, bool oxirgiMi,
                                                 std::vector<Segment>& chiqish,
                                                 double* haqiqiyOxir) {
    // Shu boʻlakka ajratilgan ulush.
    const double ulushBoshi = umumiy > 0 ? boshi / umumiy : 0.0;
    const double ulushOxiri = umumiy > 0 ? oxiri / umumiy : 1.0;
    const double ulush = ulushOxiri - ulushBoshi;

    std::vector<float> namunalar;
    const MediaXato x = namunalarniOqi(
        yol, boshi, oxiri,
        [&](double p) {
            // Dekodlash — ulushning birinchi 10%i.
            progress(ulushBoshi + ulush * p * 0.10, L"Fayl oʻqilmoqda…");
        },
        [&] { return bekor.load(); }, namunalar);

    if (x == MediaXato::BekorQilindi) {
        xato(mediaXatoXabari(x, L""));
        return false;
    }
    if (x != MediaXato::Yoq) {
        xato(mediaXatoXabari(x, L""));
        return false;
    }
    if (namunalar.empty()) {
        xato(mediaXatoXabari(MediaXato::Bosh, L""));
        return false;
    }

    // Boʻlak chegarasini soʻz oʻrtasiga tushirmaslik uchun oxirgi 30
    // soniyada eng jim nuqtani topib, oʻsha yerda kesamiz. Oxirgi
    // boʻlakda kesish shart emas — undan keyin hech narsa yoʻq.
    size_t kesim = namunalar.size();
    if (!oxirgiMi) {
        const size_t oynaUzunligi = static_cast<size_t>(kChegaraOynasi) * kNamunaTezligi;
        const size_t oynaBoshi =
            namunalar.size() > oynaUzunligi ? namunalar.size() - oynaUzunligi : 0;
        kesim = jimlikNuqtasi(namunalar, oynaBoshi, namunalar.size());
    }
    if (kesim < namunalar.size()) namunalar.resize(kesim);

    if (haqiqiyOxir) { *haqiqiyOxir = boshi + static_cast<double>(kesim) / kNamunaTezligi; }

    // Past signalni koʻtaramiz — diktovka bilan bir xil funksiya.
    const std::vector<float> tayyor = prepareSamples(namunalar);

    struct Kontekst {
        Ichki* ichki;
        double ulushBoshi, ulush;
    } kontekst{this, ulushBoshi, ulush};

    // Engine orqali: model kerak boʻlsa yuklanadi va ish diktovka bilan
    // BITTA navbatda turadi. Toʻgʻridan-toʻgʻri `rubai_transcribe_segments`
    // chaqirish ikkalasini ham buzardi — whisper konteksti global.
    std::vector<Segment> yangilar;
    std::wstring xatoMatni;
    const int rc = Engine::instance().transcribeSegments(
        tayyor,
        [](int foiz, void* ud) {
            auto* k = static_cast<Kontekst*>(ud);
            // Transkripsiya — ulushning qolgan 90%i.
            k->ichki->progress(k->ulushBoshi + k->ulush * (0.10 + 0.90 * foiz / 100.0),
                               L"Matnga oʻgirilmoqda…");
        },
        &kontekst, [](void* ud) -> bool { return static_cast<Kontekst*>(ud)->ichki->bekor.load(); },
        &kontekst, yangilar, xatoMatni);

    if (rc == 2) {
        xato(mediaXatoXabari(MediaXato::BekorQilindi, L""));
        return false;
    }
    if (rc != 0) {
        xato(xatoMatni.empty() ? L"Matnga oʻgirib boʻlmadi." : xatoMatni);
        return false;
    }

    // Vaqtlar boʻlak boshidan hisoblanadi — fayl boshiga surib qoʻyamiz.
    for (Segment& s : yangilar) {
        s.t0 += siljish;
        s.t1 += siljish;
        chiqish.push_back(std::move(s));
    }
    return true;
}

// ---- Asosiy oqim -----------------------------------------------------------

void TranskripsiyaIshi::Ichki::ishla() {
    oxirgiProgress = 0;

    MediaMalumot malumot;
    const MediaXato x = mediaMalumot(yol, malumot);
    if (x != MediaXato::Yoq) {
        // Kengaytmani xabarga qoʻshamiz — «.mkv qoʻllab-quvvatlanmaydi»
        // «xato yuz berdi» dan ancha foydali.
        const size_t nuqta = yol.find_last_of(L'.');
        std::wstring k = (nuqta == std::wstring::npos) ? L"" : yol.substr(nuqta + 1);
        for (auto& c : k) c = static_cast<wchar_t>(towlower(c));
        xato(mediaXatoXabari(x, k));
        return;
    }

    const double davomiylik = malumot.davomiylik;
    logWrite(L"studiya: " + yol + L", " + std::to_wstring(static_cast<int>(davomiylik)) + L"s");

    std::vector<Segment> hammasi;

    if (davomiylik <= kBolaklashChegarasi) {
        if (!bolakniQaytaIshla(0, davomiylik, davomiylik, 0, true, hammasi, nullptr)) return;
    } else {
        double boshi = 0;
        for (;;) {
            if (bekor.load()) {
                xato(mediaXatoXabari(MediaXato::BekorQilindi, L""));
                return;
            }

            const double taxminiyOxir =
                std::min(boshi + kBolakUzunligi + kChegaraOynasi, davomiylik);
            const bool oxirgiMi = taxminiyOxir >= davomiylik;

            double haqiqiyOxir = taxminiyOxir;
            if (!bolakniQaytaIshla(boshi, taxminiyOxir, davomiylik, boshi, oxirgiMi, hammasi,
                                   &haqiqiyOxir)) {
                return;
            }

            // Oxirgi boʻlakdan keyin SHARTSIZ toʻxtaymiz — `oxirgiMi`
            // bayrogʻiga tayanamiz, `boshi < davomiylik` shartiga emas.
            // Haqiqiy dekodlangan oxir konteyner eʼlon qilgan davomiylikdan
            // deyarli doim biroz farq qiladi (yaxlitlash, kodek priming).
            // Shartga tayansak, oxirgi boʻlakdan keyin deyarli boʻsh oraliq
            // ustida yana bir marta uriniladi, «Fayl boʻsh» xatosi chiqadi
            // va bir necha soatlik tayyor transkripsiya yoʻqqa chiqadi.
            if (oxirgiMi) break;
            boshi = haqiqiyOxir;
        }
    }

    if (hammasi.empty()) {
        xato(L"Faylda nutq aniqlanmadi.");
        return;
    }

    const Settings s = loadSettings();
    const Hujjat h = HujjatOmbori::saqla(yol, davomiylik, hammasi,
                                         s.oddiyApostrof ? Apostrof::Oddiy : Apostrof::Standart);

    if (h.id.empty()) {
        xato(L"Diskka saqlab boʻlmadi. Joy yetarli boʻlmasligi mumkin.");
        return;
    }

    tugat();
    if (onTayyor) onTayyor(h);
}

// ---- Ommaviy interfeys -----------------------------------------------------

bool TranskripsiyaIshi::ishlayapti() {
    std::lock_guard<std::mutex> q(g_holatQulfi);
    return g_ishlayapti;
}

TranskripsiyaIshi::TranskripsiyaIshi(std::wstring yol) : ichki_(std::make_shared<Ichki>()) {
    ichki_->yol = std::move(yol);
}

TranskripsiyaIshi::~TranskripsiyaIshi() = default;

void TranskripsiyaIshi::bekorQil() { ichki_->bekor.store(true); }

void TranskripsiyaIshi::boshla() {
    if (!ishniOlish()) {
        if (onXato) onXato(L"Boshqa fayl ustida ish ketmoqda. Tugashini kuting.");
        return;
    }

    // Model butun ish davomida RAM'da qulflanadi — boʻlaklar orasidagi
    // dekodlash tanaffuslarida ham. Aks holda idle taymer uni boʻshatib
    // yuboradi va keyingi boʻlak modelni qaytadan yuklaydi.
    Engine::instance().setBand(true);

    ichki_->onProgress = onProgress;
    ichki_->onTayyor = onTayyor;
    ichki_->onXato = onXato;

    // `shared_ptr` nusxasi oqimga beriladi: ish oʻzini oʻzi tirik saqlaydi.
    // Chaqiruvchi obyektni darhol qoʻyib yuborsa ham ish tugaydi va
    // bayroqlarni tozalaydi — aks holda model abadiy qulflanib qolardi.
    auto ichki = ichki_;
    std::thread([ichki] { ichki->ishla(); }).detach();
}

}  // namespace rubai
