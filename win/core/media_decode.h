// Audio/video faylni whisper kutadigan 16 kHz mono float32 ga oʻgiradi.
//
// macOS'dagi `src/media_decode.swift` ning ekvivalenti. U yerda AVFoundation,
// bu yerda **Media Foundation** — Windows'ning oʻz media quvuri. ffmpeg
// ISHLATILMAYDI: u 60 MB qoʻshimcha yuk, alohida litsenziya va yangilanish
// yuki demak, Windows esa kerakli kodeklarni oʻzi olib yuradi.
//
// Media Foundation qoʻllab-quvvatlaydi: mp4, m4a, mov, mp3, wav, wma, aac,
// flac (Windows 10 1703+). Qoʻllab-quvvatlamaydi: mkv, webm, ogg, opus —
// ular uchun tushunarli xato beriladi, chunki «hech narsa boʻlmadi» eng yomon
// javob.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace rubai {

enum class MediaXato {
    Yoq,           // muvaffaqiyat
    Ochilmadi,     // format qoʻllab-quvvatlanmaydi
    AudioYoliYoq,  // faylda ovoz yoʻli yoʻq
    Bosh,          // fayl boʻsh yoki davomiyligi noaniq
    OqishXatosi,   // buzuq fayl
    BekorQilindi,
};

// Foydalanuvchiga koʻrsatiladigan xabar — muammoni VA yechimni aytadi.
// `kengaytma` nuqtasiz, kichik harflarda (masalan L"mkv").
std::wstring mediaXatoXabari(MediaXato xato, const std::wstring& kengaytma);

struct MediaMalumot {
    double davomiylik = 0;  // soniya
};

// Faylni ochib davomiyligini qaytaradi.
MediaXato mediaMalumot(const std::wstring& yol, MediaMalumot& natija);

// Faylning `boshi…oxiri` oraligʻini 16 kHz mono float32 qilib oʻqiydi.
// `oxiri <= 0` — fayl oxirigacha.
//
// `progress` 0…1 — shu oraliq ichidagi ulush. UI oqimini bloklamasligi kerak.
// `bekor` true qaytarsa oʻqish toʻxtaydi va `BekorQilindi` qaytadi.
//
// Namunalar `natija` ga QOʻSHILADI (tozalanmaydi) — uzun fayl boʻlaklab
// oʻqilganda har boʻlak ketma-ket toʻplanadi.
MediaXato namunalarniOqi(const std::wstring& yol, double boshi, double oxiri,
                         const std::function<void(double)>& progress,
                         const std::function<bool()>& bekor, std::vector<float>& natija);

}  // namespace rubai
