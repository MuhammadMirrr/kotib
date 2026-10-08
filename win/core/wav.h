// Oddiy WAV oʻquvchi — testlar va CLI vositasi uchun.
//
// Ilovaning oʻzi audio fayllarni Media Foundation orqali oʻqiydi (mp3, mp4,
// m4a va h.k.). Bu yerdagi kod faqat WAV bilan ishlaydi va tashqi bogʻliqliksiz
// boʻlgani uchun unit testlarda qulay.
#pragma once

#include <string>
#include <vector>

namespace rubai {

// WAV faylni 16 kHz mono float32 ga oʻqiydi.
// PCM 8/16/24/32-bit va IEEE float 32/64-bit formatlarini qoʻllab-quvvatlaydi.
// Xato boʻlsa false qaytaradi va `error` ga sabab yoziladi.
bool readWav16kMono(const std::wstring& path, std::vector<float>& out, std::wstring& error);

}  // namespace rubai
