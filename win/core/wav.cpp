// Oddiy WAV oʻquvchi — testlar va CLI vositasi uchun.
// Interfeys va izohlar — `wav.h`.

#include "wav.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace rubai {

namespace {

constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatFloat = 3;
constexpr uint16_t kFormatExtensible = 0xFFFE;

struct Fmt {
    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
};

bool readExact(std::ifstream& f, void* buf, size_t n) {
    f.read(static_cast<char*>(buf), (std::streamsize)n);
    return (size_t)f.gcount() == n;
}

// Bitta namunani -1..1 oraligʻidagi float ga oʻgiradi.
float decodeSample(const uint8_t* p, const Fmt& fmt) {
    if (fmt.format == kFormatFloat) {
        if (fmt.bitsPerSample == 32) {
            float v;
            std::memcpy(&v, p, 4);
            return v;
        }
        double v;
        std::memcpy(&v, p, 8);
        return (float)v;
    }
    switch (fmt.bitsPerSample) {
        case 8:  // 8-bit PCM ishorasiz, markazi 128
            return ((float)p[0] - 128.0f) / 128.0f;
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

// Chiziqli qayta namunalash. Sifat jihatidan sinc'dan past, lekin bu kod
// faqat test vositasida ishlatiladi — ilovada WASAPI oʻzi 16 kHz beradi.
std::vector<float> resampleLinear(const std::vector<float>& in, uint32_t fromRate,
                                  uint32_t toRate) {
    if (in.empty() || fromRate == toRate || fromRate == 0) return in;
    const double ratio = (double)fromRate / (double)toRate;
    const size_t n = (size_t)((double)in.size() / ratio);
    std::vector<float> out;
    out.reserve(n);
    for (size_t i = 0; i < n; i++) {
        const double src = i * ratio;
        const size_t i0 = (size_t)src;
        const size_t i1 = i0 + 1 < in.size() ? i0 + 1 : i0;
        const float t = (float)(src - (double)i0);
        out.push_back(in[i0] * (1.0f - t) + in[i1] * t);
    }
    return out;
}

}  // namespace

bool readWav16kMono(const std::wstring& path, std::vector<float>& out, std::wstring& error) {
    out.clear();
    error.clear();

    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) {
        error = L"Fayl ochilmadi";
        return false;
    }

    char riff[4], wave[4];
    uint32_t riffSize = 0;
    if (!readExact(f, riff, 4) || std::memcmp(riff, "RIFF", 4) != 0 ||
        !readExact(f, &riffSize, 4) || !readExact(f, wave, 4) ||
        std::memcmp(wave, "WAVE", 4) != 0) {
        error = L"WAV formati emas";
        return false;
    }

    Fmt fmt;
    bool haveFmt = false;

    for (;;) {
        char id[4];
        uint32_t size = 0;
        if (!readExact(f, id, 4) || !readExact(f, &size, 4)) {
            error = haveFmt ? L"data boʻlimi topilmadi" : L"fmt boʻlimi topilmadi";
            return false;
        }

        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (size < 16) {
                error = L"fmt boʻlimi buzuq";
                return false;
            }
            std::vector<uint8_t> body(size);
            if (!readExact(f, body.data(), size)) {
                error = L"fmt oʻqilmadi";
                return false;
            }
            std::memcpy(&fmt.format, body.data() + 0, 2);
            std::memcpy(&fmt.channels, body.data() + 2, 2);
            std::memcpy(&fmt.sampleRate, body.data() + 4, 4);
            std::memcpy(&fmt.bitsPerSample, body.data() + 14, 2);

            // WAVE_FORMAT_EXTENSIBLE: haqiqiy format SubFormat GUID'ining
            // birinchi 2 baytida yashiringan.
            if (fmt.format == kFormatExtensible && size >= 26) {
                uint16_t sub = 0;
                std::memcpy(&sub, body.data() + 24, 2);
                fmt.format = sub;
            }
            haveFmt = true;

        } else if (std::memcmp(id, "data", 4) == 0) {
            if (!haveFmt) {
                error = L"fmt boʻlimi data'dan keyin";
                return false;
            }
            if (fmt.channels == 0 || fmt.sampleRate == 0) {
                error = L"WAV parametrlari notoʻgʻri";
                return false;
            }
            if (fmt.format != kFormatPcm && fmt.format != kFormatFloat) {
                error = L"Qoʻllab-quvvatlanmaydigan WAV kodlashi (siqilgan)";
                return false;
            }
            const size_t bytesPerSample = fmt.bitsPerSample / 8u;
            if (bytesPerSample == 0) {
                error = L"bitsPerSample notoʻgʻri";
                return false;
            }

            std::vector<uint8_t> data(size);
            if (!readExact(f, data.data(), size)) {
                // Fayl kesilgan boʻlsa ham oʻqilgan qismini ishlatamiz
                data.resize((size_t)f.gcount());
            }

            const size_t frameBytes = bytesPerSample * fmt.channels;
            const size_t frames = frameBytes ? data.size() / frameBytes : 0;

            std::vector<float> mono;
            mono.reserve(frames);
            for (size_t i = 0; i < frames; i++) {
                const uint8_t* p = data.data() + i * frameBytes;
                float sum = 0.0f;
                for (uint16_t c = 0; c < fmt.channels; c++) {
                    sum += decodeSample(p + c * bytesPerSample, fmt);
                }
                mono.push_back(sum / (float)fmt.channels);
            }

            out = resampleLinear(mono, fmt.sampleRate, 16000);
            return true;

        } else {
            // Notanish boʻlim — oʻtkazib yuboramiz
            f.seekg(size, std::ios::cur);
            if (!f) {
                error = L"Fayl kutilmaganda tugadi";
                return false;
            }
        }

        if (size % 2) f.seekg(1, std::ios::cur);  // boʻlimlar juft baytga tekislanadi
    }
}

}  // namespace rubai
