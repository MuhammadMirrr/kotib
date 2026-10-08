// Matnga oʻgirib boʻlmagan diktovka ovozi (barqarorlik A2).
//
// macOS'dagi `src/saqlanmagan.swift` ning ekvivalenti: WAV formati, fayl nomi
// va «eng yangi 20 ta, 7 kundan eski emas» qoidasi AYNAN bir xil.
//
// 1.1.0 gacha transkripsiya yiqilsa (model yuklanmadi, whisper xatosi) yozilgan
// ovoz tashlanardi. Endi u `%LOCALAPPDATA%\Kotib\saqlanmagan\` ga 16 kHz mono
// 16-bit WAV boʻlib yoziladi, model tayyor boʻlgach qayta oʻgiriladi va
// muvaffaqiyatda oʻchiriladi.
//
// Win32'siz (`std::filesystem`) — `win/tests/mac/sinov.sh` macOS'da testlaydi.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace rubai {
namespace saqlanmagan {

inline constexpr size_t kChegara = 20;
inline constexpr long long kMuddatMs = 7LL * 24 * 3600 * 1000;
inline constexpr int kChastota = 16000;

// Float32 namunalar → 16-bit PCM mono WAV baytlari. [-1, 1] dan tashqarisi kesiladi.
std::string wav(const std::vector<float>& s, int chastota = kChastota);

// `wav` yozgan formatni oʻqiydi (PCM16 mono, `chastota`). Boshqa format — false:
// bu papkaga faqat ilovaning oʻzi yozadi, begona faylni taxmin qilib
// oʻgirishdan koʻra tashlab ketgan maʼqul.
bool namunalar(const std::string& baytlar, std::vector<float>& chiqish, int chastota = kChastota);

// `2026-10-08_14-03-22-123.wav` — mahalliy vaqt, alifbo tartibi = vaqt tartibi.
std::wstring nom(long long unixMs);

// `nom` ning teskarisi (Unix ms). Bizning nomimiz boʻlmasa — -1.
long long sana(const std::wstring& nom);

// Oʻchirilishi kerak boʻlgan NOMLAR: 7 kundan eskilari va eng yangi 20 tadan
// ortigʻi. Nomi bizniki boʻlmagan fayllarga tegilmaydi.
std::vector<std::wstring> ortiqcha(const std::vector<std::wstring>& nomlar, long long hozirMs);

// ---- fayl tizimi -------------------------------------------------------

// Saqlangan ovozlarning toʻliq yoʻllari, eskisidan yangisiga (qayta urinish
// shu tartibda). Papka yoʻq boʻlsa — boʻsh.
std::vector<std::wstring> royxat(const std::wstring& papka);

// Ovozni yozadi va eskilarini tozalaydi. Muvaffaqiyatda fayl yoʻli, aks holda boʻsh.
std::wstring saqla(const std::vector<float>& s, const std::wstring& papka, long long hozirMs);

void tozala(const std::wstring& papka, long long hozirMs);

// Saqlangan faylni oʻqiydi. Oʻqib boʻlmasa yoki format boshqa boʻlsa — false.
bool oqi(const std::wstring& yol, std::vector<float>& chiqish);

// Hozirgi vaqt, Unix ms.
long long hozirMs();

}  // namespace saqlanmagan
}  // namespace rubai
