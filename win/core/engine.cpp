// Whisper yadrosi — model yuklash, transkripsiya navbati, RAM'ni boʻshatish.
// Interfeys va izohlar — `engine.h`.

#include "engine.h"

#include <windows.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "samples.h"
#include "util.h"
#include "statistika.h"
#include "whisper_bridge.h"

namespace rubai {

namespace {

constexpr wchar_t kModelFileName[] = L"ggml-rubaistt.bin";

int defaultThreads() {
    // macOS versiyasi bilan bir xil qoida: yadrolar soni - 2, kamida 4.
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors - 2;
    return n < 4 ? 4 : n;
}

// whisper.cpp/ggml xabarlari -> bizning log fayl.
// (Grafik ilovada stderr yoʻq, bu xabarlar aks holda yoʻqoladi.)
void forwardWhisperLog(const char* msg) {
    if (!msg) return;
    std::wstring w = toWide(msg);
    while (!w.empty() && (w.back() == L'\n' || w.back() == L'\r')) w.pop_back();
    if (!w.empty()) logWrite(L"whisper: " + w);
}

}  // namespace

// Qulflash qoidasi:
//   - `mu` navbat, stopping, preloadQueued va lastUse ni himoya qiladi.
//   - `loaded` atomik: model holatini uzoq davom etadigan yuklash paytida
//     ham toʻsqinliksiz oʻqish mumkin boʻlishi kerak.
//   - `backend` alohida qisqa qulf ostida — u faqat matn koʻrsatish uchun.
//   - whisper_context'ga FAQAT ishchi oqim tegadi (u global singleton).
struct Engine::Impl {
    struct Job {
        std::vector<float> samples;
        std::function<void(TranscribeResult)> done;
        // Toʻldirilgan boʻlsa yuqoridagi ikkita maydon ishlatilmaydi:
        // ishchi oqim shunchaki shu vazifani bajaradi. Studiya yoʻli shu
        // orqali diktovka bilan bitta navbatda turadi.
        std::function<void()> vazifa;
    };

    std::wstring modelPath;  // faqat setModelPath/ishchi oqim, mu ostida
    std::atomic<bool> useGpu{true};
    std::atomic<int> idleSeconds{180};
    // Uzoq ish davomida idle boʻshatishni toʻxtatib turadi (`setBand`).
    std::atomic<bool> band{false};
    std::atomic<bool> loaded{false};

    mutable std::mutex mu;
    std::condition_variable cv;
    std::deque<Job> queue;
    bool stopping = false;
    // `stopping` ning qulfsiz nusxasi — whisper'ning abort callback'i ichidan
    // (ggml oqimlaridan) oʻqiladi: ilova yopilayotganda fayl ishi keyingi
    // tekshiruvda toʻxtaydi (E4).
    std::atomic<bool> toxtatilmoqda{false};
    // Ishchi oqim `run()` dan chiqdi — `shutdown` buni chegaralangan kutadi.
    bool tugadi = false;
    std::condition_variable cvTugadi;
    bool preloadQueued = false;
    // `Engine::unload()` soʻrovi. Ilgari u `idleSeconds` ni 10 ga tushirardi
    // va qiymat sessiya oxirigacha shunday qolardi: GPU almashtirilgandan
    // keyin model har 10 s boʻsh turishda RAM'dan chiqib ketardi (E3).
    bool unloadQueued = false;
    std::chrono::steady_clock::time_point lastUse = std::chrono::steady_clock::now();

    mutable std::mutex backendMu;
    std::wstring backend;

    std::thread worker;

    void run();
    // `ensureLoaded` ni faqat ishchi oqim chaqirishi kerak — nomdagi
    // «FromWorker» buni eslatib turadi.
    bool ensureLoadedFromWorker(std::wstring& e) { return ensureLoaded(e, false); }
    TranscribeResult doTranscribe(const std::vector<float>& samples);
    // `isitish` — faqat oldindan yuklashda (foydalanuvchi kutmayotganda).
    bool ensureLoaded(std::wstring& errOut, bool isitish);
    void warmUp();
    void unloadFromWorker();

    void setBackend(const std::wstring& b) {
        std::lock_guard<std::mutex> lock(backendMu);
        backend = b;
    }
};

// ---------------------------------------------------------------- yuklash

namespace {

std::wstring gpuBelgisi() {
    const std::wstring p = localAppDataDir();
    return p.empty() ? std::wstring() : p + L"\\gpu-ochilmoqda";
}

// Belgini yozadi; obʼyekt yoʻq qilinganda (funksiyadan har qanday chiqishda)
// oʻchiradi. Jarayon yiqilsa destruktor ishlamaydi — belgi qoladi.
struct GpuBelgisi {
    bool faol = false;
    explicit GpuBelgisi(bool yoz) {
        if (!yoz) return;
        const std::wstring b = gpuBelgisi();
        if (b.empty()) return;
        HANDLE h = CreateFileW(b.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        CloseHandle(h);
        faol = true;
    }
    ~GpuBelgisi() {
        if (faol) Engine::gpuBelgisiniOchir();
    }
    GpuBelgisi(const GpuBelgisi&) = delete;
    GpuBelgisi& operator=(const GpuBelgisi&) = delete;
};

}  // namespace

bool Engine::Impl::ensureLoaded(std::wstring& errOut, bool isitish) {
    if (loaded.load()) return true;

    std::wstring path;
    {
        std::lock_guard<std::mutex> lock(mu);
        path = modelPath;
    }
    if (path.empty()) path = Engine::findModel();
    if (path.empty()) {
        errOut = L"Model fayli topilmadi. Ilovani qayta oʻrnating.";
        logWrite(L"XATO: model fayli topilmadi");
        return false;
    }

    // whisper.cpp yoʻlni char* sifatida oladi — lotin boʻlmagan foydalanuvchi
    // nomlarida (C:\Users\Аброр\...) qisqa 8.3 yoʻlga oʻtkaziladi. Qisqa
    // nomlar oʻchirilgan diskda bu ham boʻlmaydi — u holda fayl `_wfopen`
    // bilan ochilib, oʻz oʻquvchimiz orqali beriladi (`rubai_load_w`). 1.2 dan
    // ilova va model foydalanuvchi profilida (per-user, S7), shuning uchun
    // kirillcha nomli har bir foydalanuvchi shu yoʻldan oʻtishi mumkin.
    const std::string cpath = pathForC(path);
    if (cpath.empty()) logWrite(L"model yoʻli ASCII emas va qisqa nom yoʻq — keng belgili ochish");
    auto yukla = [&](bool gpu) {
        return cpath.empty() ? rubai_load_w(path.c_str(), gpu ? 1 : 0)
                             : rubai_load_ex(cpath.c_str(), gpu ? 1 : 0);
    };

    const bool wantGpu = useGpu.load();
    // GPU yuklash va isitish (birinchi GPU hisobi) — drayver yiqilishi
    // mumkin boʻlgan yagona joylar. Belgi shu funksiya oxirigacha turadi.
    const GpuBelgisi belgi(wantGpu);
    logWrite(L"model yuklanmoqda: " + path + (wantGpu ? L" (GPU)" : L" (CPU)"));
    const auto t0 = std::chrono::steady_clock::now();
    int rc = yukla(wantGpu);

    if (rc != 0 && wantGpu) {
        // GPU'da yuklanmadi — CPU'ga tushamiz. Obunachilarning eski yoki
        // drayveri buzuq GPU'larida ilova baribir ishlashi kerak.
        logWrite(L"GPU'da yuklanmadi (" + toWide(rubai_last_error()) + L") — CPU'ga oʻtilmoqda");
        rc = yukla(false);
    }

    if (rc != 0) {
        errOut = L"Model yuklanmadi. Fayl buzilgan boʻlishi mumkin —\n"
                 L"ilovani qayta oʻrnating.";
        logWrite(L"XATO: rubai_load = " + std::to_wstring(rc) + L" (" + toWide(rubai_last_error()) +
                 L")");
        return false;
    }

    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    setBackend(toWide(rubai_backend_name()));
    loaded.store(true);
    logWrite(L"model yuklandi: " + toWide(rubai_backend_name()) + L", " +
             std::to_wstring((int)(secs * 1000)) + L" ms");

    // Isitish faqat oldindan yuklashda va faqat GPU'da (E3). Ilgari u HAR
    // yuklanishda — 180 s boʻsh turishdan keyin foydalanuvchi diktovkani
    // kutib turganda ham, CPU'da ham — ishlardi va birinchi diktovkani
    // ~3,4 s kechiktirardi. CPU'da kompilyatsiya qilinadigan shader yoʻq;
    // diktovka paytidagi yuklanishda esa isitish shunchaki ortiqcha bir
    // transkripsiya — quvur baribir haqiqiy ovoz bilan isiydi.
    if (isitish && std::string(rubai_backend_name()) != "CPU") warmUp();
    return true;
}

// Vulkan (va qisman CUDA) birinchi inference'da shader/pipeline'larni
// kompilyatsiya qiladi. Uni shu yerda, foydalanuvchi kutmayotgan paytda
// oʻtkazamiz — aks holda BIRINCHI diktovka oʻnlab soniya davom etadi.
//
// Encoder har doim 30 soniyalik oynada ishlaydi, shuning uchun qisqa
// namuna ham butun quvurni isitadi.
void Engine::Impl::warmUp() {
    const auto t0 = std::chrono::steady_clock::now();

    // Toza sukunat emas — juda past shovqin. Sukunatda whisper erta
    // toʻxtashi va quvurning bir qismi isimay qolishi mumkin.
    std::vector<float> dummy(16000);
    for (size_t i = 0; i < dummy.size(); i++) {
        dummy[i] = (float)((i * 2654435761u) % 2001) / 1000.0f * 0.001f - 0.001f;
    }

    char* c = rubai_transcribe(dummy.data(), (int)dummy.size(), defaultThreads());
    if (c) rubai_free_str(c);

    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    logWrite(L"isitish tugadi: " + std::to_wstring((int)(secs * 1000)) + L" ms");
}

void Engine::Impl::unloadFromWorker() {
    if (!loaded.load()) return;
    rubai_unload();
    loaded.store(false);
    setBackend(L"");
}

// ------------------------------------------------------------ transkripsiya

TranscribeResult Engine::Impl::doTranscribe(const std::vector<float>& samples) {
    TranscribeResult r;

    if (samples.empty()) {
        r.error = L"Ovoz yozilmadi";
        return r;
    }

    std::wstring err;
    if (!ensureLoaded(err, false)) {
        r.error = err;
        return r;
    }

    const auto t0 = std::chrono::steady_clock::now();
    char* c = rubai_transcribe(samples.data(), (int)samples.size(), defaultThreads());
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    // Anonim oʻlchov (kontentsiz): ovoz uzunligi, ishlov vaqti, backend.
    // Backend nomini kichik harfga oʻtkazamiz (server 'vulkan'/'cpu'/'cuda' kutadi).
    {
        std::wstring bw;
        {
            std::lock_guard<std::mutex> lk(backendMu);
            bw = backend;
        }
        std::string b;
        for (wchar_t ch : bw) b += (char)std::tolower((unsigned char)ch);
        const double ovoz_s = (double)samples.size() / 16000.0;
        amalYubor("stt", ovoz_s, 0, r.seconds, b.empty() ? "cpu" : b.c_str(),
                  c ? "ok" : "xato:transkripsiya");
    }

    if (!c) {
        r.error = L"Transkripsiya bajarilmadi";
        logWrite(L"XATO: rubai_transcribe = NULL (" + toWide(rubai_last_error()) + L")");
        return r;
    }

    r.text = toWide(c);
    rubai_free_str(c);

    // Bosh/oxirgi boʻshliqlarni olib tashlaymiz (whisper segmentlari
    // odatda " " bilan boshlanadi).
    const size_t a = r.text.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) {
        r.text.clear();
    } else {
        const size_t b = r.text.find_last_not_of(L" \t\r\n");
        r.text = r.text.substr(a, b - a + 1);
    }
    return r;
}

// ------------------------------------------------------------- ishchi oqim

void Engine::Impl::run() {
    for (;;) {
        Job job;
        bool haveJob = false;
        bool bosat = false;
        bool yukla = false;

        {
            std::unique_lock<std::mutex> lock(mu);

            // Navbat boʻsh boʻlsa: idle vaqtigacha kutamiz, soʻng modelni
            // RAM'dan boʻshatamiz (macOS versiyasidagi 180 s qoidasi).
            while (queue.empty() && !stopping && !preloadQueued && !unloadQueued) {
                if (!loaded.load() || band.load()) {
                    // Band boʻlsa ham shu yerda kutamiz, lekin taymersiz —
                    // ish tugaganda `setBand(false)` bizni uygʻotadi.
                    cv.wait(lock);
                    continue;
                }
                const auto deadline = lastUse + std::chrono::seconds(idleSeconds.load());
                if (cv.wait_until(lock, deadline) == std::cv_status::timeout && queue.empty() &&
                    !preloadQueued && std::chrono::steady_clock::now() >= deadline) {
                    unloadFromWorker();
                    logWrite(L"model RAM'dan boʻshatildi (ishlatilmadi)");
                }
            }

            if (stopping && queue.empty()) break;

            bosat = unloadQueued;
            unloadQueued = false;
            if (!queue.empty()) {
                job = std::move(queue.front());
                queue.pop_front();
                haveJob = true;
            }
            yukla = preloadQueued;
            preloadQueued = false;
        }

        // Boʻshatish soʻrovi oldindan yuklashdan OLDIN bajariladi: GPU
        // almashtirilganda ikkalasi birga keladi (`applySettings`).
        if (bosat && loaded.load()) {
            unloadFromWorker();
            logWrite(L"model RAM'dan boʻshatildi (soʻrov boʻyicha)");
        }

        if (!haveJob) {
            if (yukla) {
                // Oldindan yuklash — model va GPU quvurini tayyorlaymiz.
                std::wstring err;
                ensureLoaded(err, true);
            }
            std::lock_guard<std::mutex> lock(mu);
            lastUse = std::chrono::steady_clock::now();
            continue;
        }

        if (job.vazifa) {
            job.vazifa();
            std::lock_guard<std::mutex> lock(mu);
            lastUse = std::chrono::steady_clock::now();
            continue;
        }

        TranscribeResult r = doTranscribe(job.samples);

        {
            std::lock_guard<std::mutex> lock(mu);
            lastUse = std::chrono::steady_clock::now();
        }
        if (job.done) job.done(std::move(r));
    }

    unloadFromWorker();
    std::lock_guard<std::mutex> lock(mu);
    tugadi = true;
    cvTugadi.notify_all();
}

// ------------------------------------------------------------------- API

Engine::Engine() : d(new Impl) {
    rubai_set_log(forwardWhisperLog);
    // Silero VAD (S12) — exe yonida; har model yuklanishida VAD ham yuklanadi.
    // Yoʻq boʻlsa diktovka 1.1 dagi kabi boʻlaksiz ishlaydi.
    const std::wstring vad = exeDir() + L"\\ggml-silero-v6.2.0.bin";
    if (fileExists(vad)) {
        rubai_set_vad_path_w(vad.c_str());
    } else {
        logWrite(L"VAD modeli topilmadi (" + vad + L") — ovoz boʻlaklarga boʻlinmaydi");
    }
    d->worker = std::thread([this] { d->run(); });
}

Engine::~Engine() {
    shutdown();
    delete d;
}

Engine& Engine::instance() {
    static Engine e;
    return e;
}

std::wstring Engine::findModel() {
    const std::wstring exe = exeDir();
    const std::wstring local = localAppDataDir();
    const std::wstring candidates[] = {
        exe.empty() ? std::wstring() : exe + L"\\models\\" + kModelFileName,
        exe.empty() ? std::wstring() : exe + L"\\" + kModelFileName,
        local.empty() ? std::wstring() : local + L"\\models\\" + kModelFileName,
    };
    for (const auto& p : candidates) {
        if (!p.empty() && fileExists(p)) return p;
    }
    return {};
}

void Engine::setModelPath(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(d->mu);
    d->modelPath = path;
}

void Engine::setUseGpu(bool on) { d->useGpu = on; }

void Engine::setIdleUnloadSeconds(int seconds) { d->idleSeconds = seconds < 10 ? 10 : seconds; }

void Engine::preload() {
    {
        // `loaded` bu yerda TEKSHIRILMAYDI: `unload()` dan keyin darhol
        // chaqirilganda (GPU almashtirish) model hali boʻshatilmagan boʻladi
        // va ilgari preload jim qaytardi (E3). `ensureLoaded` yuklangan
        // modelda hech narsa qilmaydi.
        std::lock_guard<std::mutex> lock(d->mu);
        if (d->stopping || d->preloadQueued) return;
        d->preloadQueued = true;
    }
    d->cv.notify_one();
}

void Engine::transcribeAsync(std::vector<float> samples,
                             std::function<void(TranscribeResult)> done) {
    {
        std::lock_guard<std::mutex> lock(d->mu);
        if (d->stopping) return;
        d->queue.push_back({std::move(samples), std::move(done), nullptr});
        d->lastUse = std::chrono::steady_clock::now();
    }
    d->cv.notify_one();
}

TranscribeResult Engine::transcribe(const std::vector<float>& samples) {
    // Sinxron chaqiruv — ishchi oqimga qoʻyib, natijani kutamiz.
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    TranscribeResult result;

    transcribeAsync(samples, [&](TranscribeResult r) {
        std::lock_guard<std::mutex> lock(m);
        result = std::move(r);
        ready = true;
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&] { return ready; });
    return result;
}

int Engine::transcribeSegments(const std::vector<float>& samples, rubai_progress_cb pcb, void* pud,
                               rubai_abort_cb acb, void* aud, std::vector<Segment>& chiqish,
                               std::wstring& xato) {
    std::mutex m;
    std::condition_variable cv;
    bool tayyor = false;
    int rc = 1;

    // Chaqiruvchining bekor qilish tekshiruviga «ilova yopilmoqda» qoʻshiladi (E4).
    struct Toxtat {
        rubai_abort_cb acb;
        void* aud;
        const std::atomic<bool>* yopilmoqda;
    };
    Toxtat toxtat{acb, aud, &d->toxtatilmoqda};
    const rubai_abort_cb birga = [](void* ud) -> bool {
        const auto* t = static_cast<const Toxtat*>(ud);
        return t->yopilmoqda->load() || (t->acb && t->acb(t->aud));
    };

    auto vazifa = [&] {
        std::wstring err;
        if (!d->ensureLoadedFromWorker(err)) {
            xato = err;
            rc = 1;
        } else {
            rc = rubai_transcribe_segments(samples.data(), (int)samples.size(), defaultThreads(),
                                           pcb, pud, birga, &toxtat);
            if (rc == 0) {
                const int n = rubai_n_segments();
                chiqish.reserve(chiqish.size() + (n > 0 ? (size_t)n : 0));
                for (int i = 0; i < n; ++i) {
                    const char* c = rubai_segment_text(i);
                    if (!c) continue;
                    // MUHIM: koʻrsatkich whisper'ning ichki buferiga —
                    // darhol nusxa olamiz, `rubai_free_str` chaqirilmaydi.
                    std::wstring t = toWide(c);
                    const size_t a = t.find_first_not_of(L" \t\r\n");
                    if (a == std::wstring::npos) continue;  // faqat boʻshliq
                    const size_t b = t.find_last_not_of(L" \t\r\n");
                    Segment s;
                    s.matn = t.substr(a, b - a + 1);
                    s.t0 = rubai_segment_t0(i) / 100.0;
                    s.t1 = rubai_segment_t1(i) / 100.0;
                    chiqish.push_back(std::move(s));
                }
            } else if (rc != 2) {
                xato = L"Matnga oʻgirib boʻlmadi.";
                logWrite(L"XATO: rubai_transcribe_segments = " + std::to_wstring(rc) + L" (" +
                         toWide(rubai_last_error()) + L")");
            }
        }
        std::lock_guard<std::mutex> lock(m);
        tayyor = true;
        cv.notify_one();
    };

    {
        std::lock_guard<std::mutex> lock(d->mu);
        if (d->stopping) {
            xato = L"Ilova yopilmoqda.";
            return 1;
        }
        d->queue.push_back({{}, nullptr, vazifa});
        d->lastUse = std::chrono::steady_clock::now();
    }
    d->cv.notify_one();

    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&] { return tayyor; });
    return rc;
}

void Engine::setBand(bool band) {
    d->band.store(band);
    // Uygʻotamiz: band olib tashlangan boʻlsa idle hisobi qaytadan boshlansin.
    {
        std::lock_guard<std::mutex> lock(d->mu);
        d->lastUse = std::chrono::steady_clock::now();
    }
    d->cv.notify_all();
}

void Engine::unload() {
    // Modelni faqat ishchi oqim boʻshatishi mumkin (whisper_context unga
    // tegishli) — soʻrov qoʻyib, uni uygʻotamiz.
    {
        std::lock_guard<std::mutex> lock(d->mu);
        if (d->stopping) return;
        d->unloadQueued = true;
    }
    d->cv.notify_one();
}

bool Engine::isLoaded() const { return d->loaded.load(); }

bool Engine::gpuOldinYiqilgan() {
    const std::wstring b = gpuBelgisi();
    return !b.empty() && fileExists(b);
}

void Engine::gpuBelgisiniOchir() {
    const std::wstring b = gpuBelgisi();
    if (!b.empty()) DeleteFileW(b.c_str());
}

std::wstring Engine::backendName() const {
    std::lock_guard<std::mutex> lock(d->backendMu);
    return d->backend;
}

void Engine::shutdown() {
    {
        std::lock_guard<std::mutex> lock(d->mu);
        if (d->stopping) return;
        d->stopping = true;
    }
    d->toxtatilmoqda.store(true);
    d->cv.notify_all();
    if (!d->worker.joinable()) return;

    // Chegaralangan kutish (E4). Ilgari `join()` tugamagan fayl ishini
    // oxirigacha kutardi: Windows oʻchayotganda jarayon osilib qolar va
    // tizim uni majburan oʻldirardi. Fayl ishi endi abort orqali tez
    // toʻxtaydi, diktovka transkripsiyasi (abort'siz) esa bir necha soniya.
    std::unique_lock<std::mutex> lock(d->mu);
    if (d->cvTugadi.wait_for(lock, std::chrono::seconds(5), [&] { return d->tugadi; })) {
        lock.unlock();
        d->worker.join();
    } else {
        lock.unlock();
        logWrite(L"ishchi oqim 5 s ichida toʻxtamadi — kutilmaydi");
        d->worker.detach();
    }
}

}  // namespace rubai
