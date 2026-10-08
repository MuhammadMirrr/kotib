// Avto-yangilanishning sof mantigʻi: versiya taqqoslash, siyosat qarori,
// server manifestini tekshirish va URL ruxsat roʻyxati.
//
// Win32'ga tegmaydi — `win/tests/mac/sinov.sh` uni macOS'da ham sinaydi.
// macOS'dagi egizagi: `src/yangilanish_siyosat.swift`. Ikkalasi BITTA holatlar
// jadvalidan sinaladi (`tests/umumiy/yangilanish_holatlari.def`): ikki platforma
// bir serverdan bir xil manifest oladi va bir xil qaror qabul qilishi shart.
//
// Satrlar UTF-8 (`std::string`): manifest tarmoqdan bayt sifatida keladi va
// versiyalar, URL'lar, sha256 — hammasi ASCII.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace rubai {

// ---- Versiya ---------------------------------------------------------------

// -1 (a eski) | 0 | 1 (a yangi). Raqamli boʻlaklar son sifatida
// (1.10.0 > 1.9.0); `-qoʻshimcha` li versiya shu raqamdan eski
// (1.2.0-rc1 < 1.2.0); qoʻshimcha ichidagi raqamlar ham son sifatida
// (sinov9 < sinov10). Buzuq kirish yiqitmaydi — raqam boʻlmagan boʻlak 0.
int versiyaTaqqosla(std::string_view a, std::string_view b);

// KATTA.KICHIK.TUZATISH[-qoʻshimcha], qoʻshimcha faqat [0-9A-Za-z.] —
// `VERSION` fayli va manifest qabul qiladigan shakl.
bool versiyaFormatimi(std::string_view v);

// ---- Siyosat qarori ----------------------------------------------------------

enum class Qaror {
    Hech,    // hech narsa qilinmaydi
    Yukla,   // oddiy yangilanish: fonda yuklab, boʻsh paytda oʻrnatiladi
    Ornat,   // majburiy: boʻsh paytni kutmasdan (faqat yozuv tugashini kutib)
    Blokla,  // majburiy, muhlat tugagan: oʻrnatilmaguncha diktovka toʻxtaydi
};

// minVersiya — boʻsh: majburiy chegara yoʻq. chelak — ilova birinchi ishga
// tushganda tanlagan 0–99 son. korilgan — majburiy siyosat birinchi koʻrilgan
// vaqt (unix soniya), manfiy: hali koʻrilmagan. Qoidalar —
// docs/superpowers/plans/2026-10-07-s2-imzo-va-siyosat.md.
Qaror siyosatQarori(std::string_view joriy, std::string_view versiya, std::string_view minVersiya,
                    int muhlatSoat, int foiz, int chelak, long long korilgan, long long hozir);

// Diskda saqlangan majburiy talabdan qaror (spec «Majburiy yangilanish»): ilova
// qayta ochilganda internetsiz ham blok holatini biladi. Hech | Ornat | Blokla.
// `min` boʻsh — talab yoʻq.
Qaror majburiyQaror(std::string_view joriy, std::string_view min, int muhlatSoat,
                    long long korilgan, long long hozir);

// ---- Tekshiruv va oʻrnatish jadvali ------------------------------------------
//
// Spec «Tekshiruv jadvali», «Oʻrnatish oqimi». Vaqtlar — soniya; ikki vaqt
// bir xil soatdan boʻlishi kifoya (Windows boʻsh paytni monoton soat bilan
// oʻlchaydi). Manfiy vaqt — «hech qachon» (macOS'dagi `nil`).

// Muvaffaqiyatsiz tekshiruvdan keyin keyingi urinishgacha kutish:
// 15 daqiqa → 1 soat → 4 soat, keyin -1 — odatdagi 24 soatlik jadvalga
// qaytiladi. `urinish` — ketma-ket muvaffaqiyatsizliklar soni, 0 dan.
long long qaytaUrinishKechikishi(int urinish);

// Tekshirish vaqti keldimi (uygʻonganda ham): oxirgi MUVAFFAQIYATLI
// tekshiruvdan 24 soat oʻtgan, hech qachon boʻlmagan yoki u kelajakda
// (soat orqaga surilgan — aks holda oʻsha sanagacha tekshiruv boʻlmasdi).
bool uygonishdaTekshirish(long long oxirgiMuvaffaqiyat, long long hozir);

// Tayyor yangilanish hozir oʻrnatilsinmi. Boʻsh payt: yozuv, fayl ishi va
// tarjima yoʻq (`band`), oxirgi diktovkadan kamida 2 daqiqa oʻtgan.
// Yangilanish 24 soatdan beri kutayotgan boʻlsa — 1 daqiqa yetadi.
bool ornatishMumkinmi(bool band, long long oxirgiFaollik, long long kutishBoshlandi,
                      long long hozir);

// Reliz izohini (Markdown yoki HTML) bannerga sigʻadigan bir qatorga
// aylantiradi: teglar, `#` sarlavhalar, `-`/`*` roʻyxat belgilari va `**`
// olib tashlanadi, qatorlar «; » bilan qoʻshiladi, 140 belgigacha. «Belgi» —
// Unicode skalyari (grafema emas): ikki platforma aynan bir xil sanasin.
std::string qisqaIzoh(std::string_view xom);

// ---- URL ruxsat roʻyxati -----------------------------------------------------

enum class UrlTuri {
    Fayl,      // https://cdn.mirqobilov.com/…
    Manifest,  // https://stat.mirqobilov.com/…
};

// 1.1.0 da server bergan URL `ShellExecuteW` ga tekshirilmasdan berilardi —
// UNC yoʻl yoki lokal .exe boʻlsa ishga tushardi. Qoida URL tahlilchisiz va
// qatʼiy: aniq prefiks, keyin faqat [A-Za-z0-9._~/-], boʻsh, «.» va «..»
// segmentlarsiz.
bool urlRuxsatmi(std::string_view url, UrlTuri tur);

// ---- Base64 ------------------------------------------------------------------

// Standart alifbo, toʻldirish (=) bilan, boʻshliqsiz. Notoʻgʻri kirishda false.
bool base64Och(std::string_view kirish, std::vector<uint8_t>& chiqish);

// ---- Manifest ----------------------------------------------------------------

struct YangilanishFayli {
    std::string url;
    long long hajm = 0;
    std::string sha256;
    std::vector<uint8_t> imzo;  // 64 bayt, faylning Ed25519 imzosi
};

struct Manifest {
    std::string platforma;
    std::string versiya;
    std::string minVersiya;  // boʻsh — majburiy chegara yoʻq
    int muhlatSoat = 72;
    int foiz = 100;
    std::string izoh;                                 // UTF-8
    std::map<std::string, YangilanishFayli> fayllar;  // "x64" / "arm64"
};

// Server javobi: {"m": base64(manifest), "s": base64(imzo)}. Avval imzo
// `m` ning AYNAN baytlari ustidan tekshiriladi, faqat shundan keyin `m`
// oʻqiladi. Biror maydon notoʻgʻri boʻlsa — butun javob rad (false).
bool javobniTekshir(const std::string& javob, const uint8_t ochiqKalit[32],
                    std::string_view platforma, Manifest& natija);

// Imzosi allaqachon tekshirilgan manifest baytlarini oʻqiydi.
bool manifestniAjrat(const std::string& bayt, std::string_view platforma, Manifest& natija);

// ---- Yuklangan fayl -----------------------------------------------------------

enum class FaylXatosi {
    Yoq,
    Hajm,    // hajm manifestdagidan farq qiladi (yarim yoki ortiqcha yuklangan)
    Sha256,  // mazmun boshqa
    Imzo,    // Ed25519 imzosi bizning kalitga mos emas
};

// Spec tartibi: hajm → sha256 → Ed25519. SHA-256 ni chaqiruvchi hisoblaydi
// (Windows CNG) — bu fayl Win32'ga tegmasligi uchun; `sha256` kichik harfli hex.
// Imzo faylning BUTUN baytlari ustidan (Sparkle `sign_update` shunday imzolaydi).
FaylXatosi faylniTekshir(const uint8_t* bayt, size_t uzunlik, std::string_view sha256,
                         const YangilanishFayli& kutilgan, const uint8_t ochiqKalit[32]);

}  // namespace rubai
