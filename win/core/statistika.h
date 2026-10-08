// Anonim foydalanish statistikasi.
//
// macOS tomonidagi `src/statistika.swift` ning ekvivalenti — server manzili,
// soʻrov shakli va kunlik chegara bir xil. Ikkala platforma bitta Worker'ga
// murojaat qiladi (`statistika/` papkasidagi kod).
//
// Yangilanish bu yerda EMAS — `yangilovchi.h` (spec: statistika va
// yangilanish kodda ajratiladi, biri yiqilsa ikkinchisi ishlayveradi).
// 1.1.0 dagi «ping javobidagi versiya → banner» yoʻli olib tashlandi; server
// u maydonni faqat eski 1.1.0 lar uchun («koʻprik») beradi.
//
// ---------------------------------------------------------------------------
// MAXFIYLIK. Bu faylni oʻzgartirishdan oldin oʻqing. macOS tomonidagi
// `src/statistika.swift` va server (`statistika/src/index.js`) bilan MOS
// boʻlishi SHART.
//
// Yuboriladi:  tasodifiy oʻrnatma ID, platforma ("win"), ilova va Windows
//              versiyasi, qurilma muhiti (arxitektura, CPU/GPU modeli, RAM,
//              yadro soni), va har transkripsiya/tarjima uchun OʻLCHOVLAR:
//              ovoz uzunligi, ishlov vaqti, backend (vulkan/cpu), natija.
//              Taxminiy joylashuv serverda IP'dan aniqlanadi.
// YUBORILMAYDI — hech qachon: ovoz, transkripsiya/tarjima MATNI, diktovka
//              tarixi, fayl nomlari, mikrofon nomi, email, hisob maʼlumotlari,
//              XOM IP, seriya raqami, MachineGuid, MAC, foydalanuvchi nomi,
//              oʻrnatilgan dasturlar yoki jarayonlar roʻyxati.
//
// Statistika DOIM yoqiq — oʻchirish tugmasi yoʻq. Yagona huquqiy asosi —
// oʻrnatishda koʻrsatilgan foydalanish shartlari va maxfiylik siyosatidagi
// OCHIQ eʼlon. Shu sabab bu izoh doim haqiqatni aytishi SHART.
// ---------------------------------------------------------------------------
#pragma once

#include <string>

namespace rubai {

// Ilova ishga tushganda chaqiriladi: qurilma muhiti bilan ping. Ish ALOHIDA
// oqimda — tarmoq javob bermasa ham ilova kutib qolmaydi. Kuniga bir
// martadan koʻp yuborilmaydi (oxirgi vaqt sozlamalarda).
void pingYubor();

// Har transkripsiya/tarjima tugagach chaqiriladi. Kontent yuborilmaydi —
// faqat oʻlchov. Alohida oqimda yuboriladi, UI'ni kutdirmaydi.
//   tur      — "stt" yoki "tarjima"
//   ovoz_s   — ovoz uzunligi soniyada (stt); tarjimada 0 qoldiring
//   belgi    — belgi soni (tarjima); stt'da 0 qoldiring
//   ishlov_s — qancha vaqtda tayyor boʻldi (soniya)
//   backend  — "vulkan" | "cpu"
//   natija   — "ok" yoki "xato:<kod>"
void amalYubor(const char* tur, double ovoz_s, long long belgi, double ishlov_s,
               const char* backend, const char* natija);

// Ilova versiyasi, qoʻshimchasi bilan (`1.2.0-sinov1`) — `VERSION` faylidan
// kompilyatsiya vaqtida (`kotib_versiya.h`).
std::wstring joriyVersiya();

// Logdagi ishga tushish qatori uchun: «Kotib 1.2.0 x64, Windows 11 26100»
// (barqarorlik A6). Statistika bilan bir xil manbalardan.
std::wstring ishgaTushishMuhiti();

}  // namespace rubai
