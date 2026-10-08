// Audio namunalarni whisper uchun tayyorlash.
#pragma once

#include <vector>

namespace rubai {

// Audio sifati haqida qaror — foydalanuvchiga aniq xabar berish uchun.
enum class AudioVerdict {
    Ok,         // normal signal
    TooShort,   // 1 soniyadan qisqa
    Silent,     // signal deyarli yoʻq — mikrofon jim/ulanmagan
    VeryQuiet,  // juda past, lekin kuchaytirsa boʻladi
};

struct AudioCheck {
    AudioVerdict verdict = AudioVerdict::Ok;
    float peak = 0.0f;  // eng baland namuna, 0..1
    float seconds = 0.0f;
};

// Signalni tekshiradi. Jim oqimni aniqlash Windows'da muhim: Iriun, OBS,
// VB-Cable kabi virtual mikrofonlar va ulanmagan Bluetooth qurilmalari
// "OK" holatida koʻrinib, aslida raqamli sukunat beradi.
AudioCheck checkSamples(const std::vector<float>& samples);

// macOS versiyasidagi prepareSamples bilan bir xil: eng baland namunani
// 0.95 ga olib chiqadi (maksimal 40x), past mikrofon signali uchun.
std::vector<float> prepareSamples(const std::vector<float>& raw);

// Berilgan oynada eng past energiyali nuqtani topadi — uzun faylni
// boʻlaklarga boʻlganda soʻz oʻrtasidan kesib qoʻymaslik uchun.
//
// macOS'dagi `audio_util.swift`'ning `jimlikNuqtasi` funksiyasi bilan bir
// xil: 0.5 s oyna, 0.1 s qadam, har 8-namuna boʻyicha taxminiy energiya.
// Qadam va namuna oraligʻi ATAYLAB qoʻpol — aniqlik yetarli, hisob esa
// sakkiz barobar arzon.
//
// Qaytaradi: massiv boshidan namunalardagi indeks.
size_t jimlikNuqtasi(const std::vector<float>& namunalar, size_t oynaBoshi, size_t oynaOxiri);

// Whisper kutadigan namuna tezligi.
inline constexpr int kNamunaTezligi = 16000;

}  // namespace rubai
