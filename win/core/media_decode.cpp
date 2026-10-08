// Audio/video faylni 16 kHz mono float32 ga dekodlash (Media Foundation).
// Interfeys va izohlar — `media_decode.h`.

#include "media_decode.h"

#include "util.h"

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#include <algorithm>

namespace rubai {

namespace {

constexpr int kNamunaChastotasi = 16000;  // whisper aynan shuni kutadi

// Media Foundation'ni bir marta ishga tushiradi va jarayon oxirigacha ochiq
// qoldiradi. Har chaqiruvda MFStartup/MFShutdown qilish notoʻgʻri: shutdown
// boshqa oqimda ochiq turgan reader'ni ham buzadi.
struct MfIshgaTushirish {
    bool tayyor = false;
    MfIshgaTushirish() {
        tayyor = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE));
        if (!tayyor) logWrite(L"XATO: Media Foundation ishga tushmadi");
    }
    // Ataylab MFShutdown chaqirilmaydi — jarayon tugaganda OS oʻzi tozalaydi.
};

bool mfTayyormi() {
    static MfIshgaTushirish bir;
    return bir.tayyor;
}

template <typename T> void bosat(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// Kengaytmani nuqtasiz, kichik harflarda qaytaradi.
std::wstring kengaytma(const std::wstring& yol) {
    const size_t nuqta = yol.find_last_of(L'.');
    const size_t slash = yol.find_last_of(L"\\/");
    if (nuqta == std::wstring::npos || (slash != std::wstring::npos && nuqta < slash)) {
        return {};
    }
    std::wstring k = yol.substr(nuqta + 1);
    for (auto& ch : k) ch = static_cast<wchar_t>(towlower(ch));
    return k;
}

bool qollanmaydiganFormat(const std::wstring& k) {
    // Media Foundation bularni ochmaydi.
    //
    // macOS roʻyxatida (`media_decode.swift`) bularga qoʻshimcha `wma` ham
    // bor: AVFoundation uni ocholmaydi, Media Foundation esa ochadi. Bu
    // ataylab qoldirilgan farq — tizim imkoniyati, va Windows foydalanuvchisini
    // yoʻq sababdan cheklash notoʻgʻri boʻlardi.
    static const wchar_t* roʻyxat[] = {L"mkv", L"webm", L"ogg", L"opus", L"amr"};
    for (const wchar_t* x : roʻyxat) {
        if (k == x) return true;
    }
    return false;
}

// Faylni ochib, chiqishini 16 kHz mono float32 ga sozlaydi.
// Media Foundation kerakli qayta namunalash va aralashtirish (mono'ga
// keltirish) ni oʻzi qiladi — qoʻlda resampler yozish shart emas.
MediaXato oqiyOch(const std::wstring& yol, IMFSourceReader** natija) {
    *natija = nullptr;
    if (!mfTayyormi()) return MediaXato::OqishXatosi;

    IMFAttributes* xossalar = nullptr;
    if (FAILED(MFCreateAttributes(&xossalar, 1))) return MediaXato::OqishXatosi;
    // Baʼzi kodeklar faqat "software" rejimda ishonchli — GPU dekodlash bu
    // yerda foyda bermaydi, chunki bizga faqat ovoz kerak.
    xossalar->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, FALSE);

    IMFSourceReader* oqiy = nullptr;
    const HRESULT hr = MFCreateSourceReaderFromURL(yol.c_str(), xossalar, &oqiy);
    bosat(xossalar);

    if (FAILED(hr)) {
        return qollanmaydiganFormat(kengaytma(yol)) ? MediaXato::Ochilmadi : MediaXato::OqishXatosi;
    }

    // Faqat ovoz yoʻli kerak — video yoʻlini oʻchiramiz, aks holda MF uni
    // ham dekodlaydi va vaqt behuda ketadi.
    oqiy->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (FAILED(oqiy->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE))) {
        bosat(oqiy);
        return MediaXato::AudioYoliYoq;
    }

    IMFMediaType* tur = nullptr;
    if (FAILED(MFCreateMediaType(&tur))) {
        bosat(oqiy);
        return MediaXato::OqishXatosi;
    }

    tur->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    tur->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    tur->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kNamunaChastotasi);
    tur->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
    tur->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);

    const HRESULT turHr =
        oqiy->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, tur);
    bosat(tur);

    if (FAILED(turHr)) {
        bosat(oqiy);
        // Ovoz yoʻli bor, lekin uni bizga kerakli shaklga keltirib boʻlmadi.
        return MediaXato::AudioYoliYoq;
    }

    *natija = oqiy;
    return MediaXato::Yoq;
}

}  // namespace

std::wstring mediaXatoXabari(MediaXato xato, const std::wstring& k) {
    switch (xato) {
        case MediaXato::Ochilmadi:
            return L"«." + k +
                   L"» fayllari qoʻllab-quvvatlanmaydi. "
                   L"mp4, mov, m4a, mp3, wav yoki flac faylini tanlang.";
        case MediaXato::AudioYoliYoq: return L"Bu faylda ovoz yoʻli topilmadi.";
        case MediaXato::Bosh: return L"Fayl boʻsh.";
        case MediaXato::OqishXatosi: return L"Faylni oʻqib boʻlmadi — u buzilgan boʻlishi mumkin.";
        case MediaXato::BekorQilindi: return L"Bekor qilindi.";
        case MediaXato::Yoq: return {};
    }
    return {};
}

MediaXato mediaMalumot(const std::wstring& yol, MediaMalumot& natija) {
    IMFSourceReader* oqiy = nullptr;
    const MediaXato xato = oqiyOch(yol, &oqiy);
    if (xato != MediaXato::Yoq) return xato;

    PROPVARIANT qiymat;
    PropVariantInit(&qiymat);
    const HRESULT hr = oqiy->GetPresentationAttribute(
        static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &qiymat);

    double davomiylik = 0;
    if (SUCCEEDED(hr) && qiymat.vt == VT_UI8) {
        // MF davomiylikni 100 nanosekund birligida beradi.
        davomiylik = static_cast<double>(qiymat.uhVal.QuadPart) / 1e7;
    }
    PropVariantClear(&qiymat);
    bosat(oqiy);

    if (!(davomiylik > 0)) return MediaXato::Bosh;
    natija.davomiylik = davomiylik;
    return MediaXato::Yoq;
}

MediaXato namunalarniOqi(const std::wstring& yol, double boshi, double oxiri,
                         const std::function<void(double)>& progress,
                         const std::function<bool()>& bekor, std::vector<float>& natija) {
    IMFSourceReader* oqiy = nullptr;
    const MediaXato xato = oqiyOch(yol, &oqiy);
    if (xato != MediaXato::Yoq) return xato;

    // Boshlanish nuqtasiga oʻtamiz. `SetCurrentPosition` kalit kadrga
    // tushadi — ovoz uchun bu aniq, video kabi siljish muammosi yoʻq.
    if (boshi > 0) {
        PROPVARIANT joy;
        InitPropVariantFromInt64(static_cast<LONGLONG>(boshi * 1e7), &joy);
        oqiy->SetCurrentPosition(GUID_NULL, joy);
        PropVariantClear(&joy);
    }

    const double uzunlik = (oxiri > boshi) ? (oxiri - boshi) : 0;
    MediaXato holat = MediaXato::Yoq;

    for (;;) {
        if (bekor && bekor()) {
            holat = MediaXato::BekorQilindi;
            break;
        }

        DWORD bayroq = 0;
        LONGLONG vaqt = 0;
        IMFSample* namuna = nullptr;

        const HRESULT hr = oqiy->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr,
                                            &bayroq, &vaqt, &namuna);
        if (FAILED(hr)) {
            holat = MediaXato::OqishXatosi;
            bosat(namuna);
            break;
        }
        if (bayroq & MF_SOURCE_READERF_ENDOFSTREAM) {
            bosat(namuna);
            break;
        }
        if (!namuna) continue;  // vaqtinchalik boʻshliq — bu xato emas

        const double joriyVaqt = static_cast<double>(vaqt) / 1e7;
        if (uzunlik > 0 && joriyVaqt >= oxiri) {
            bosat(namuna);
            break;
        }

        IMFMediaBuffer* bufer = nullptr;
        if (SUCCEEDED(namuna->ConvertToContiguousBuffer(&bufer)) && bufer) {
            BYTE* malumot = nullptr;
            DWORD uzunligi = 0;
            if (SUCCEEDED(bufer->Lock(&malumot, nullptr, &uzunligi))) {
                const size_t n = uzunligi / sizeof(float);
                const float* f = reinterpret_cast<const float*>(malumot);
                natija.insert(natija.end(), f, f + n);
                bufer->Unlock();
            }
            bosat(bufer);
        }
        bosat(namuna);

        if (progress && uzunlik > 0) {
            progress(std::clamp((joriyVaqt - boshi) / uzunlik, 0.0, 1.0));
        }
    }

    bosat(oqiy);

    if (holat == MediaXato::Yoq && natija.empty()) return MediaXato::Bosh;
    if (progress && holat == MediaXato::Yoq) progress(1.0);
    return holat;
}

}  // namespace rubai
