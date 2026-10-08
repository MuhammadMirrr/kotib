// Foydalanuvchiga koʻrsatiladigan vaqt yozuvlari.
//
// macOS'dagi `src/vaqt_format.swift` ning ekvivalenti. Matnlar AYNAN bir xil
// boʻlishi shart: «bugun 16:04», «kecha 18:22», «22 avg 18:22», «1 soat 5
// daqiqa». Bitta ilovaning ikki tanasi bir xil gapirishi kerak.
//
// Oy qisqartmalari macOS'ning `uz_Latn` lokalidan olingan (yan, fev, mar, apr,
// may, iyn, iyl, avg, sen, okt, noy, dek) — Windows'da bunday lokal yoʻq va
// `GetDateFormatEx` boshqacha (koʻpincha ruscha yoki inglizcha) beradi.
//
// Sof mantiq: fayl UI'dan ham, tarmoqdan ham mustaqil, shuning uchun
// `win/tests/` uni toʻliq qamrab oladi.
#pragma once

#include <string>

namespace rubai {
namespace VaqtFormat {

// Yozib olish taymeri: «0:04», «1:23», «12:05».
// Soniyalar PASTGA yaxlitlanadi — 4.9 soniya hali «0:04».
std::wstring taymer(double soniya);

// Tarix qatoridagi sana: «bugun 16:04», «kecha 18:22», aks holda «22 avg 18:22».
// Taqqoslash KALENDAR kuni boʻyicha, «24 soat oldin» boʻyicha emas.
// `hozir` — sinov uchun; 0 berilsa joriy vaqt olinadi.
std::wstring nisbiy(long long unix, long long hozir = 0);

// Fayl uzunligi: «12 daqiqa», «1 soat 5 daqiqa», «1 daqiqadan kam».
std::wstring davomiylik(double soniya);

// Faqat kun: «bugun», «kecha», aks holda «22 avg».
std::wstring kun(long long unix, long long hozir = 0);

}  // namespace VaqtFormat
}  // namespace rubai
