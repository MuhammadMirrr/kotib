// Mikrofondan yozib olish — WASAPI, 16 kHz mono float32 ga oʻgirish.
// Interfeys va izohlar — `audio_capture.h`.

#include "audio_capture.h"

#include <windows.h>

// initguid.h mmdeviceapi.h dan OLDIN kelishi shart — busiz
// PKEY_Device_FriendlyName va KSDATAFORMAT_SUBTYPE_* linkda topilmaydi.
#include <initguid.h>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <thread>

#include "util.h"

namespace rubai {

namespace {

constexpr uint32_t kTargetRate = 16000;
// WASAPI buferi. 200 ms yetarli zaxira beradi — tizim band boʻlganda ham
// namunalar yoʻqolmaydi.
constexpr REFERENCE_TIME kBufferDuration = 2000000;  // 100-ns birliklarda = 200 ms

struct ComInit {
    HRESULT hr;
    ComInit() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComInit() {
        if (SUCCEEDED(hr)) CoUninitialize();
    }
};

template <class T> void safeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// MUHIM: towlower() faqat ASCII bilan ishlaydi — kirill harflarni
// oʻzgartirmaydi. Qurilma nomlari esa rus tilidagi Windows'da kirillcha
// ("Стерео микшер"), shuning uchun Win32 CharLowerBuffW ishlatiladi.
std::wstring lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(s.data(), (DWORD)s.size());
    return s;
}

MicKind classify(const std::wstring& name) {
    const std::wstring n = lower(name);

    // Loopback qurilmalari — ovoz emas, tizim ovozini yozadi.
    // Ruscha/inglizcha Windows'da turlicha nomlanadi.
    if (n.find(L"stereo mix") != std::wstring::npos ||
        n.find(L"стерео микшер") != std::wstring::npos ||
        n.find(L"what u hear") != std::wstring::npos || n.find(L"wave out") != std::wstring::npos) {
        return MicKind::Loopback;
    }

    // Bluetooth hands-free profili (HFP) — 8-16 kHz, kuchli siqilgan.
    if (n.find(L"hands-free") != std::wstring::npos || n.find(L"handsfree") != std::wstring::npos ||
        n.find(L"головной телефон") != std::wstring::npos ||
        n.find(L"bluetooth") != std::wstring::npos) {
        return MicKind::Bluetooth;
    }

    // Virtual qurilmalar — manba ilovasi ishlamasa jim oqim beradi.
    if (n.find(L"iriun") != std::wstring::npos || n.find(L"obs") != std::wstring::npos ||
        n.find(L"vb-audio") != std::wstring::npos ||
        n.find(L"cable output") != std::wstring::npos ||
        n.find(L"voicemeeter") != std::wstring::npos || n.find(L"virtual") != std::wstring::npos) {
        return MicKind::Virtual;
    }

    return MicKind::Normal;
}

// Bitta namunani float ga oʻgiradi (WASAPI mix format odatda float32,
// lekin PCM 16/24/32 ham uchraydi).
float decode(const BYTE* p, WORD bits, bool isFloat) {
    if (isFloat) {
        if (bits == 32) {
            float v;
            std::memcpy(&v, p, 4);
            return v;
        }
        double v;
        std::memcpy(&v, p, 8);
        return (float)v;
    }
    switch (bits) {
        case 16: {
            int16_t v;
            std::memcpy(&v, p, 2);
            return v / 32768.0f;
        }
        case 24: {
            int32_t v =
                (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24);
            return (float)(v >> 8) / 8388608.0f;
        }
        case 32: {
            int32_t v;
            std::memcpy(&v, p, 4);
            return (float)v / 2147483648.0f;
        }
        default: return 0.0f;
    }
}

bool formatIsFloat(const WAVEFORMATEX* wf) {
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wf->cbSize >= 22) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wf);
        return IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
    }
    return false;
}

}  // namespace

std::wstring MicDevice::warning() const {
    switch (kind) {
        case MicKind::Loopback:
            return L"Bu mikrofon emas — u kompyuterdagi ovozni yozadi.\n"
                   L"Ovozingiz yozilmaydi. Boshqa qurilma tanlang.";
        case MicKind::Bluetooth:
            return L"Bluetooth quloqchin mikrofoni. Windows uni past sifatga\n"
                   L"(telefon ovozi) oʻtkazadi — matn aniqligi pasayadi.\n"
                   L"Imkon boʻlsa oddiy mikrofon ishlating.";
        case MicKind::Virtual:
            return L"Virtual mikrofon. Manba ilova ishlamasa ovoz oʻrniga\n"
                   L"sukunat yoziladi.";
        default: return {};
    }
}

// ------------------------------------------------------------- qurilmalar

std::vector<MicDevice> listMicrophones() {
    std::vector<MicDevice> out;
    ComInit com;

    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumerator))) {
        logWrite(L"XATO: IMMDeviceEnumerator yaratilmadi");
        return out;
    }

    // Standart qurilma ID — roʻyxatda belgilash uchun.
    std::wstring defaultId;
    IMMDevice* def = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &def)) && def) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(def->GetId(&id)) && id) {
            defaultId = id;
            CoTaskMemFree(id);
        }
        safeRelease(def);
    }

    IMMDeviceCollection* collection = nullptr;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection)) &&
        collection) {
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; i++) {
            IMMDevice* dev = nullptr;
            if (FAILED(collection->Item(i, &dev)) || !dev) continue;

            MicDevice m;
            LPWSTR id = nullptr;
            if (SUCCEEDED(dev->GetId(&id)) && id) {
                m.id = id;
                CoTaskMemFree(id);
            }

            IPropertyStore* props = nullptr;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props) {
                PROPVARIANT v;
                PropVariantInit(&v);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR &&
                    v.pwszVal) {
                    m.name = v.pwszVal;
                }
                PropVariantClear(&v);
                safeRelease(props);
            }

            if (!m.id.empty()) {
                m.kind = classify(m.name);
                m.isDefault = (m.id == defaultId);
                out.push_back(std::move(m));
            }
            safeRelease(dev);
        }
        safeRelease(collection);
    }
    safeRelease(enumerator);

    // Haqiqiy mikrofonlarni yuqoriga chiqaramiz — foydalanuvchi roʻyxatning
    // birinchisini tanlashi ehtimoli yuqori, u ishlaydigan qurilma boʻlsin.
    std::stable_sort(out.begin(), out.end(), [](const MicDevice& a, const MicDevice& b) {
        auto rank = [](const MicDevice& d) {
            if (d.kind == MicKind::Loopback) return 3;
            if (d.kind == MicKind::Virtual) return 2;
            if (d.kind == MicKind::Bluetooth) return 1;
            return 0;
        };
        const int ra = rank(a), rb = rank(b);
        if (ra != rb) return ra < rb;
        return a.isDefault && !b.isDefault;
    });

    return out;
}

// --------------------------------------------------------------- yozish

struct AudioCapture::Impl {
    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> lost{false};
    // Oqim haqiqatan boshlandi (`client->Start()` muvaffaqiyatli). Har
    // `start()` da tiklanadi (D4): ilgari kutish `srcRate == 0` ga qarardi,
    // u esa birinchi yozuvdan keyin hech qachon 0 ga qaytmasdi — ikkinchi
    // yozuvdan boshlab «Yozilmoqda» oqim ochilmasidan oldin koʻrsatilardi.
    std::atomic<bool> ochildi{false};

    std::mutex mu;
    std::vector<float> samples;

    // Toʻlqin chizigʻi uchun oxirgi boʻlakning darajasi. `atomic` — u
    // yozuvchi oqimda yangilanadi, UI oqimida oʻqiladi.
    std::atomic<float> daraja{0.0f};

    // Manba formati (WASAPI bergan)
    uint32_t srcRate = 0;
    uint16_t srcChannels = 0;

    void captureLoop(std::wstring deviceId);
    void append(const std::vector<float>& mono16k);
};

namespace {

// Manba tezligidan 16 kHz ga tushirish.
//
// Butun nisbatlarda (48000/16000 = 3) oʻrtacha qiymat olamiz — bu oddiy
// past-chastota filtri vazifasini bajaradi va aliasingni kamaytiradi.
// Chiziqli interpolyatsiya bunday nisbatlarda aliasing beradi.
std::vector<float> downsampleTo16k(const std::vector<float>& in, uint32_t srcRate) {
    if (in.empty() || srcRate == 0 || srcRate == kTargetRate) return in;

    if (srcRate > kTargetRate && srcRate % kTargetRate == 0) {
        const size_t factor = srcRate / kTargetRate;
        std::vector<float> out;
        out.reserve(in.size() / factor);
        for (size_t i = 0; i + factor <= in.size(); i += factor) {
            float sum = 0.0f;
            for (size_t k = 0; k < factor; k++) sum += in[i + k];
            out.push_back(sum / (float)factor);
        }
        return out;
    }

    // Butun boʻlmagan nisbat (masalan 44100 -> 16000): har chiqish namunasi
    // uchun mos kirish oynasining oʻrtachasini olamiz.
    const double ratio = (double)srcRate / (double)kTargetRate;
    const size_t n = (size_t)((double)in.size() / ratio);
    std::vector<float> out;
    out.reserve(n);
    for (size_t i = 0; i < n; i++) {
        const double a = i * ratio;
        const double b = a + ratio;
        size_t i0 = (size_t)a;
        size_t i1 = (size_t)b;
        if (i1 >= in.size()) i1 = in.size() - 1;
        if (i1 <= i0) {
            out.push_back(in[i0]);
            continue;
        }
        float sum = 0.0f;
        for (size_t k = i0; k <= i1; k++) sum += in[k];
        out.push_back(sum / (float)(i1 - i0 + 1));
    }
    return out;
}

}  // namespace

void AudioCapture::Impl::append(const std::vector<float>& mono16k) {
    if (!mono16k.empty()) {
        // RMS — oddiy pik emas: pik bitta shovqindan sakrab ketadi va
        // toʻlqin doim toʻliq balandlikda turib qoladi.
        double kvadrat = 0.0;
        for (float v : mono16k) kvadrat += static_cast<double>(v) * v;
        const double rms = std::sqrt(kvadrat / static_cast<double>(mono16k.size()));
        // Nutq RMS'i odatda 0.02…0.2 oraligʻida — koʻrinishi uchun kuchaytiramiz.
        daraja.store(static_cast<float>(std::min(1.0, rms * 6.0)));
    }
    std::lock_guard<std::mutex> lock(mu);
    samples.insert(samples.end(), mono16k.begin(), mono16k.end());
}

void AudioCapture::Impl::captureLoop(std::wstring deviceId) {
    ComInit com;
    if (FAILED(com.hr)) {
        lost = true;
        running = false;
        return;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* mix = nullptr;
    HANDLE event = nullptr;

    auto cleanup = [&] {
        if (client) client->Stop();
        if (event) CloseHandle(event);
        if (mix) CoTaskMemFree(mix);
        safeRelease(capture);
        safeRelease(client);
        safeRelease(device);
        safeRelease(enumerator);
    };

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumerator))) {
        logWrite(L"XATO: audio enumerator yaratilmadi");
        lost = true;
        running = false;
        return;
    }

    HRESULT hr = deviceId.empty()
                     ? enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &device)
                     : enumerator->GetDevice(deviceId.c_str(), &device);

    if (FAILED(hr) || !device) {
        // Saqlangan qurilma yoʻq (chiqarib olingan) — standartga qaytamiz.
        logWrite(L"qurilma ochilmadi, standart mikrofonga oʻtilmoqda");
        safeRelease(device);
        hr = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &device);
        if (FAILED(hr) || !device) {
            cleanup();
            lost = true;
            running = false;
            return;
        }
    }

    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client)) ||
        FAILED(client->GetMixFormat(&mix)) || !mix) {
        logWrite(L"XATO: IAudioClient ochilmadi");
        cleanup();
        lost = true;
        running = false;
        return;
    }

    srcRate = mix->nSamplesPerSec;
    srcChannels = mix->nChannels;
    const WORD bits = mix->wBitsPerSample;
    const bool isFloat = formatIsFloat(mix);
    const WORD frameBytes = mix->nBlockAlign;

    event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
        cleanup();
        lost = true;
        running = false;
        return;
    }

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                            kBufferDuration, 0, mix, nullptr);
    if (FAILED(hr)) {
        logWrite(L"XATO: IAudioClient::Initialize = 0x" + std::to_wstring((unsigned)hr));
        cleanup();
        lost = true;
        running = false;
        return;
    }

    if (FAILED(client->SetEventHandle(event)) ||
        FAILED(client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture)) ||
        FAILED(client->Start())) {
        logWrite(L"XATO: audio oqimi boshlanmadi");
        cleanup();
        lost = true;
        running = false;
        return;
    }

    ochildi.store(true);
    logWrite(L"yozish boshlandi: " + std::to_wstring(srcRate) + L" Hz, " +
             std::to_wstring(srcChannels) + L" kanal, " + std::to_wstring(bits) + L" bit" +
             (isFloat ? L" float" : L" pcm"));

    std::vector<float> mono;  // qayta ishlatiladigan bufer

    while (running.load()) {
        // Buferga maʼlumot tushishini kutamiz. Timeout — oqim toʻxtab
        // qolganini aniqlash uchun (masalan qurilma uzildi).
        const DWORD wait = WaitForSingleObject(event, 1000);
        if (wait == WAIT_TIMEOUT) continue;
        if (wait != WAIT_OBJECT_0) break;

        for (;;) {
            UINT32 packetFrames = 0;
            if (FAILED(capture->GetNextPacketSize(&packetFrames))) {
                lost = true;
                break;
            }
            if (packetFrames == 0) break;

            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            if (hr == AUDCLNT_S_BUFFER_EMPTY) break;
            if (FAILED(hr)) {
                lost = true;
                break;
            }

            mono.clear();
            mono.reserve(frames);

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Tizim "bu qism jim" deb belgilagan — nollar qoʻshamiz,
                // vaqt oʻlchovi buzilmasligi uchun.
                mono.assign(frames, 0.0f);
            } else if (data) {
                for (UINT32 i = 0; i < frames; i++) {
                    const BYTE* p = data + (size_t)i * frameBytes;
                    float sum = 0.0f;
                    for (WORD c = 0; c < srcChannels; c++) {
                        sum += decode(p + (size_t)c * (bits / 8), bits, isFloat);
                    }
                    mono.push_back(sum / (float)srcChannels);
                }
            }

            capture->ReleaseBuffer(frames);
            if (!mono.empty()) append(downsampleTo16k(mono, srcRate));
        }

        if (lost.load()) break;
    }

    cleanup();
    running = false;
}

AudioCapture::AudioCapture() : d(new Impl) {}

AudioCapture::~AudioCapture() {
    stop();
    delete d;
}

bool AudioCapture::start(const std::wstring& deviceId, std::wstring& error) {
    if (d->running.load()) {
        error = L"Yozuv allaqachon ketmoqda";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(d->mu);
        d->samples.clear();
    }
    d->lost = false;
    d->ochildi = false;
    d->running = true;

    // Har yozishda YANGI oqim — qurilma almashtirilgandan yoki uyqudan
    // keyin eski klient qotib qoladi (macOS versiyasidagi bilan bir xil sabab).
    d->thread = std::thread([this, deviceId] { d->captureLoop(deviceId); });

    // Oqim haqiqatan ochilganini kutamiz, aks holda foydalanuvchi
    // "yozilmoqda" deb oʻylab gapiradi, lekin hech narsa yozilmaydi.
    for (int i = 0; i < 200 && d->running.load() && !d->ochildi.load(); i++) { Sleep(5); }
    if (d->running.load() && !d->ochildi.load()) {
        logWrite(L"mikrofon 1 s ichida ochilmadi — oqim hali tayyorlanmoqda");
    }

    if (!d->running.load() || d->lost.load()) {
        if (d->thread.joinable()) d->thread.join();
        d->running = false;
        error = L"Mikrofon ochilmadi.\n"
                L"Windows sozlamalarida mikrofonga ruxsat berilganini tekshiring:\n"
                L"Sozlamalar > Maxfiylik > Mikrofon";
        return false;
    }
    return true;
}

std::vector<float> AudioCapture::stop() {
    if (!d->running.exchange(false) && !d->thread.joinable()) {
        std::lock_guard<std::mutex> lock(d->mu);
        return d->samples;
    }
    if (d->thread.joinable()) d->thread.join();

    std::lock_guard<std::mutex> lock(d->mu);
    return d->samples;
}

bool AudioCapture::isRecording() const { return d->running.load(); }
bool AudioCapture::deviceLost() const { return d->lost.load(); }
float AudioCapture::daraja() const { return d->daraja.load(); }

}  // namespace rubai
