// Umumiy yordamchilar: matn kodlash, yoʻllar, log.
#pragma once

#include <string>
#include <vector>

namespace rubai {

// ---- Matn kodlash --------------------------------------------------------
// Windows'da ichkarida hamma joyda wstring (UTF-16), C kutubxonalari bilan
// muloqotda UTF-8. Bu ikkalasi orasidagi koʻprik.
std::string toUtf8(const std::wstring& w);
std::wstring toWide(const std::string& s);

// ---- Yoʻllar -------------------------------------------------------------

// Ilova .exe joylashgan papka (oxirida \ yoʻq).
std::wstring exeDir();

// %APPDATA%\Kotib — sozlamalar. Papka yaratiladi.
std::wstring appDataDir();

// %LOCALAPPDATA%\Kotib — log va vaqtinchalik fayllar. Papka yaratiladi.
std::wstring localAppDataDir();

bool fileExists(const std::wstring& path);
bool ensureDir(const std::wstring& path);

// Yoʻlni C kutubxonalari (whisper.cpp) uchun tayyorlaydi.
//
// MUHIM: whisper.cpp fayl yoʻlini oddiy `char*` sifatida oladi va uni
// Windows'da ANSI kodlashda ochadi. Foydalanuvchi nomi lotin boʻlmasa
// (masalan C:\Users\Аброр\...) model ochilmaydi.
//
// Shuning uchun: yoʻlda ASCII boʻlmagan belgi boʻlsa, GetShortPathNameW
// orqali 8.3 qisqa nomga oʻgiramiz (C:\Users\ABROR~1\...). Qisqa nomlar
// oʻchirilgan disklarda bu ishlamaydi — shu sababli model baribir ASCII
// yoʻlga (ilova papkasiga) oʻrnatiladi, bu faqat zaxira mexanizm.
//
// Boʻsh satr = yoʻlni xavfsiz uzatib boʻlmadi.
std::string pathForC(const std::wstring& path);

// ---- Log -----------------------------------------------------------------
// %LOCALAPPDATA%\Kotib\dictation.log ga yozadi (thread-safe).
// macOS versiyasidagi RubaiLog bilan bir xil vazifa.
void logInit();
void logWrite(const std::wstring& msg);
std::wstring logPath();

}  // namespace rubai
