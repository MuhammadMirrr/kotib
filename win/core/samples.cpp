// Audio namunalarni whisper uchun tayyorlash: signal tekshiruvi, kuchaytirish, jimlik nuqtasi.
// Interfeys va izohlar — `samples.h`.

#include "samples.h"

#include <limits>

#include <algorithm>
#include <cmath>

namespace rubai {

namespace {
constexpr float kSampleRate = 16000.0f;
constexpr float kMinSeconds = 1.0f;
// Bu chegaradan past boʻlsa signal deyarli yoʻq. Amalda oʻlchangan:
// ulanmagan Iriun Webcam qurilmasi peak ~0.00003 (-90 dB) beradi.
constexpr float kSilentPeak = 0.005f;
constexpr float kQuietPeak = 0.05f;
}  // namespace

AudioCheck checkSamples(const std::vector<float>& samples) {
    AudioCheck c;
    c.seconds = (float)samples.size() / kSampleRate;

    float peak = 0.0f;
    for (float s : samples) peak = std::max(peak, std::fabs(s));
    c.peak = peak;

    // Tartib muhim: avval "jim" tekshiriladi. Jim mikrofonda uzoq gapirgan
    // foydalanuvchiga "qisqa gapirdingiz" deyish chalgʻitadi.
    if (peak < kSilentPeak)
        c.verdict = AudioVerdict::Silent;
    else if (c.seconds < kMinSeconds)
        c.verdict = AudioVerdict::TooShort;
    else if (peak < kQuietPeak)
        c.verdict = AudioVerdict::VeryQuiet;
    else
        c.verdict = AudioVerdict::Ok;
    return c;
}

std::vector<float> prepareSamples(const std::vector<float>& raw) {
    if (raw.empty()) return raw;

    float peak = 0.0f;
    for (float s : raw) peak = std::max(peak, std::fabs(s));
    if (peak <= 0.0001f) return raw;

    const float gain = std::min(0.95f / peak, 40.0f);  // juda past mikrofon signali uchun
    std::vector<float> out;
    out.reserve(raw.size());
    for (float s : raw) out.push_back(s * gain);
    return out;
}

size_t jimlikNuqtasi(const std::vector<float>& namunalar, size_t oynaBoshi, size_t oynaOxiri) {
    const size_t boshi = oynaBoshi;
    const size_t oxiri = oynaOxiri < namunalar.size() ? oynaOxiri : namunalar.size();

    const size_t oynaUzunligi = kNamunaTezligi / 2;  // 0.5 s
    if (oxiri <= boshi || oxiri - boshi <= oynaUzunligi * 2) return oxiri;

    size_t engYaxshiIndeks = oxiri;
    float engPastEnergiya = std::numeric_limits<float>::max();
    const size_t qadam = kNamunaTezligi / 10;  // 0.1 s

    for (size_t i = boshi; i + oynaUzunligi <= oxiri; i += qadam) {
        float yigindi = 0;
        for (size_t j = i; j < i + oynaUzunligi; j += 8) { yigindi += namunalar[j] * namunalar[j]; }
        if (yigindi < engPastEnergiya) {
            engPastEnergiya = yigindi;
            engYaxshiIndeks = i + oynaUzunligi / 2;
        }
    }
    return engYaxshiIndeks;
}

}  // namespace rubai
