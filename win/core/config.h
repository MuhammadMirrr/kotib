// Foydalanuvchi sozlamalari — %APPDATA%\Kotib\settings.ini
//
// macOS versiyasida bu UserDefaults edi. Windows'da oddiy `kalit=qiymat`
// UTF-8 matn fayli ishlatiladi: qoʻlda tahrirlash oson, parser xatolari yoʻq,
// tashqi kutubxona kerak emas.
#pragma once

#include <string>

namespace rubai {

// Matnni faol oynaga qanday joylash.
enum class InsertMode {
    Paste,  // clipboard + Ctrl+V — tez, uzun matnlar uchun (standart)
    Type,   // belgima-belgi Unicode kiritish — paste'ni bloklaydigan ilovalar uchun
};

struct Settings {
    // Hotkey. Standart: Ctrl+Alt+D (macOS'dagi ⌃⌥D ning ekvivalenti).
    unsigned vkCode = 'D';   // virtual key code
    unsigned modifiers = 0;  // MOD_CONTROL | MOD_ALT (0 = standartni qoʻllash)
    std::wstring hotkeyLabel = L"D";

    // Mikrofon. Boʻsh = tizim standarti.
    // MUHIM: Windows'da standart qurilma "Stereo Mix" boʻlib qolishi mumkin —
    // u ovoz oʻrniga kompyuter ovozini yozadi. Shuning uchun aniq tanlash bor.
    std::wstring micDeviceId;
    std::wstring micDeviceName;  // faqat koʻrsatish uchun

    InsertMode insertMode = InsertMode::Paste;
    bool autoStart = true;
    bool useGpu = true;           // false = majburan CPU
    int idleUnloadSeconds = 180;  // model RAM'dan boʻshash vaqti
    bool didOnboard = false;      // Welcome oynasi koʻrsatilganmi

    // Oddiy ASCII apostrof (') ishlatilsinmi. Sukut boʻyicha yoʻq — oʻzbek
    // lotin meʼyori ʻ va ʼ ni talab qiladi. macOS'da bu `Prefs.apostrof`.
    bool oddiyApostrof = false;

    // «Diagnostika rejimi» tugash vaqti (unix soniya, 0 — oʻchiq). Yoqiq
    // boʻlsa diktovka matni ham logga yoziladi; 24 soatdan keyin oʻzi oʻchadi —
    // unutilib qolsa ham abadiy yozib turmaydi (barqarorlik G1, macOS'dagi
    // `Prefs.diagnostika`). Standart holatda logda matn YOʻQ, faqat uzunligi.
    long long diagnostikaTugash = 0;

    // ---- Statistika va yangilanish (`statistika.h`, `yangilovchi.h`) ----
    // Statistika doim yoqiq (Q2) — sozlama maydoni yoʻq. Ilgari bu yerda
    // hech narsaga taʼsir qilmaydigan `statistika` maydoni bor edi (E9).
    // Tasodifiy 32-belgili hex. Ilova birinchi ochilganda yasaladi va
    // oʻzgarmaydi. Bu QURILMA identifikatori emas: ilova oʻchirib qayta
    // oʻrnatilsa yangisi paydo boʻladi.
    std::wstring ornatmaId;
    // Oxirgi tekshiruv vaqti (Unix soniya). Kuniga bir marta soʻrov uchun.
    long long oxirgiTekshiruv = 0;
    // Foydalanuvchi yopgan banner kaliti (masalan «yangilandi-1.2.1») —
    // shu banner boshqa koʻrsatilmaydi.
    std::wstring yangilanishYopildi;

    // ---- Avto-yangilanish (`yangilovchi.h`) ----
    // Oxirgi MUVAFFAQIYATLI tekshiruv (unix soniya, 0 — hali yoʻq). Tarmoqsiz
    // urinish uni yangilamaydi — `oxirgiTekshiruv` dan farqi shu (u ping uchun).
    long long yangilanishMuvaffaqiyat = 0;
    // Bosqichli tarqatish chelagi, 0–99; -1 — hali tanlanmagan. Serverga
    // yuborilmaydi.
    int yangilanishChelak = -1;
    // Majburiy talab (S9): oxirgi imzosi toʻgʻri manifestdagi min_versiya, uning
    // muhlati va birinchi koʻrilgan vaqti (-1 — yoʻq). Diskda turadi: ilova qayta
    // ochilganda internetsiz ham blok holatini bilsin, muhlat esa boshidan
    // boshlanmasin. Hech qachon internetga chiqmagan ilovada boʻsh — u bloklanmaydi.
    std::wstring majburiyMin;
    int majburiyMuhlat = 72;
    long long majburiyKorilgan = -1;
    // «Kotib X ga yangilandi» xabari uchun: oxirgi ishlagan versiya va
    // oʻrnatishga yuborilgan versiya bilan uning izohi.
    std::wstring oxirgiIshlaganVersiya;
    std::wstring kutilganVersiya;
    std::wstring kutilganIzoh;
    // Boʻsh — asosiy kanal; "sinov" — sinov manifesti (faqat qoʻlda yoziladi).
    std::wstring yangilanishKanali;

    // ---- LLM (Studiya «amallari») ----
    // macOS'da bular UserDefaults'dagi `llm.*` kalitlari (`llm_providers.swift`).
    // API kalitning OʻZI bu yerda EMAS — u Credential Manager'da (`llm.h`).
    std::string llmProvayder;  // boʻsh — sozlanmagan
    std::wstring llmBaseURL;   // boʻsh — provayderning oʻz manzili
    std::wstring llmModel;     // boʻsh — provayderning standart modeli

    // ---- Tarjimon ----
    // Oxirgi tanlangan tillar (NLLB kodlari). macOS'da bular UserDefaults
    // dagi `tr.manba` va `tr.maqsad`.
    std::string tarjimaManba;   // boʻsh — oʻzbekcha
    std::string tarjimaMaqsad;  // boʻsh — ruscha

    // Ekranda koʻrinishi, masalan "Ctrl+Alt+D"
    std::wstring hotkeyDisplay() const;
};

// Sozlamalar oynasi tahrirlaydigan maydonlarni `oyna` dan `fayl` ga koʻchiradi,
// qolganini (ornatmaId, oxirgiTekshiruv, yangilanish holati, tarjima tillari,
// didOnboard, idleUnloadSeconds) fayldagidek qoldiradi (barqarorlik E1).
//
// Nega kerak: oyna ochilganda sozlamalar NUSXASINI oladi va saqlashda butun
// faylni yozardi. Oraliqda boshqa modul yozgan maydonlar nusxadagi eski
// qiymatga qaytardi — birinchi ishga tushishda statistika yasagan `ornatmaId`
// oʻchib ketar, keyingi tekshiruv yangisini yasar va bitta oʻrnatma
// statistikada ikkita boʻlib koʻrinardi.
//
// Yangi maydon qoʻshilsa va uni sozlamalar oynasi tahrirlasa — shu yerga ham.
inline Settings sozlamaOynasidan(Settings fayl, const Settings& oyna) {
    fayl.vkCode = oyna.vkCode;
    fayl.modifiers = oyna.modifiers;
    fayl.hotkeyLabel = oyna.hotkeyLabel;
    fayl.micDeviceId = oyna.micDeviceId;
    fayl.micDeviceName = oyna.micDeviceName;
    fayl.insertMode = oyna.insertMode;
    fayl.autoStart = oyna.autoStart;
    fayl.useGpu = oyna.useGpu;
    fayl.oddiyApostrof = oyna.oddiyApostrof;
    fayl.diagnostikaTugash = oyna.diagnostikaTugash;
    fayl.llmProvayder = oyna.llmProvayder;
    fayl.llmBaseURL = oyna.llmBaseURL;
    fayl.llmModel = oyna.llmModel;
    return fayl;
}

// Diagnostika rejimi hali amaldami. macOS'dagi `LogSiyosati.diagnostikaFaolmi`
// bilan bir xil: soat orqaga surilgan boʻlsa ham muddat 24 soatdan uzaymaydi.
constexpr long long kDiagnostikaMuddati = 24 * 3600;
inline bool diagnostikaFaolmi(long long tugash, long long hozir) {
    return tugash > 0 && hozir < tugash && tugash - hozir <= kDiagnostikaMuddati;
}

// Diskdan oʻqiydi. Fayl yoʻq/buzuq boʻlsa standart qiymatlar qaytadi —
// hech qachon xato bermaydi (obunachi kompyuterida ilova ishga tushmay
// qolmasligi uchun).
Settings loadSettings();

bool saveSettings(const Settings& s);

std::wstring settingsPath();

}  // namespace rubai
