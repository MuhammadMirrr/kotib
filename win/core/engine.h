// Whisper yadrosi — model yuklash, transkripsiya, RAM'ni boʻshatish.
//
// macOS versiyasidagi `Whisper` sinfining ekvivalenti (whisper.swift).
// Bitta ishchi oqim (thread) barcha inference chaqiruvlarini navbat bilan
// bajaradi — whisper_context global singleton boʻlgani uchun bu shart.
#pragma once

#include "matn_format.h"     // Segment
#include "whisper_bridge.h"  // rubai_progress_cb, rubai_abort_cb

#include <functional>
#include <string>
#include <vector>

namespace rubai {

struct TranscribeResult {
    std::wstring text;     // muvaffaqiyatda — tanilgan matn
    std::wstring error;    // xato boʻlsa — oʻzbekcha xabar
    double seconds = 0.0;  // qancha vaqt ketdi
    bool ok() const { return error.empty(); }
};

class Engine {
public:
    static Engine& instance();

    // Model faylini qidiradi. Tartib:
    //   1) <exe papkasi>\models\ggml-rubaistt.bin   (oʻrnatilgan holat)
    //   2) <exe papkasi>\ggml-rubaistt.bin          (portable holat)
    //   3) %LOCALAPPDATA%\Kotib\models\...       (qoʻlda qoʻyilgan)
    // Topilmasa boʻsh satr.
    static std::wstring findModel();

    // Model yoʻlini majburan belgilash (test va CLI uchun).
    void setModelPath(const std::wstring& path);

    void setUseGpu(bool on);
    void setIdleUnloadSeconds(int seconds);

    // Modelni fonda oldindan yuklaydi va GPU quvurini isitadi.
    //
    // Buni ilova ishga tushganda chaqirish kerak. Aks holda birinchi
    // diktovkada foydalanuvchi model yuklanishini VA Vulkan shader
    // kompilyatsiyasini kutadi — bu sekin kompyuterlarda oʻnlab soniya.
    // Bloklamaydi.
    void preload();

    // Transkripsiyani navbatga qoʻyadi. `done` ISHCHI OQIMDA chaqiriladi —
    // UI'ga tegishli ish qiluvchi chaqiruvchi uni asosiy oqimga oʻtkazishi kerak.
    void transcribeAsync(std::vector<float> samples, std::function<void(TranscribeResult)> done);

    // Sinxron variant (CLI va testlar uchun).
    TranscribeResult transcribe(const std::vector<float>& samples);

    // Studiya yoʻli: segment chegaralari bilan transkripsiya.
    //
    // BLOKLAYDI va ishni AYNAN SHU navbatda bajaradi. Ikkalasi ham shart:
    //   • model kerak boʻlsa shu yerda yuklanadi — Studiya diktovkadan
    //     oldin ishlatilsa `rubai_transcribe_segments` «model not loaded»
    //     deb qaytardi;
    //   • whisper konteksti global, shuning uchun diktovka bilan bir
    //     vaqtda ishlashi mumkin emas. macOS'da buni `q` serial navbati
    //     qiladi (`whisper.swift` → `transcribeSegments`).
    //
    // Segmentlar navbat ichida oʻqiladi: keyingi ish ularning buferini
    // qayta ishlatadi. Boʻsh va faqat boʻshliqdan iborat segmentlar
    // tashlab yuboriladi (macOS bilan bir xil).
    //
    // 0 = muvaffaqiyat, 1 = xato (`xato` toʻldiriladi), 2 = bekor qilindi.
    int transcribeSegments(const std::vector<float>& samples, rubai_progress_cb pcb, void* pud,
                           rubai_abort_cb acb, void* aud, std::vector<Segment>& chiqish,
                           std::wstring& xato);

    void unload();
    bool isLoaded() const;

    // Uzoq ish (Studiya fayl transkripsiyasi) davomida modelni RAM'da
    // ushlab turadi.
    //
    // Nega kerak: uzun fayl boʻlaklab oʻqiladi va boʻlaklar ORASIDA
    // dekodlash ketadi — navbat boʻsh boʻlib qoladi. Shu tanaffusda idle
    // taymer modelni boʻshatib yuborsa, keyingi boʻlak uni qaytadan
    // yuklaydi va bir soatlik fayl bir necha marta yuklanish kutadi.
    // macOS tomonida bu `Whisper.shared.band`.
    //
    // Chaqiruvchi buni ISHONCHLI tarzda `false` ga qaytarishi shart —
    // aks holda model abadiy RAM'da qoladi.
    void setBand(bool band);

    // Faol backend: "Vulkan", "CUDA", "CPU" yoki boʻsh (yuklanmagan).
    std::wstring backendName() const;

    // Xavfsiz rejim (barqarorlik E6). GPU'da yuklash va isitish oldidan
    // `%LOCALAPPDATA%\Kotib\gpu-ochilmoqda` belgisi yoziladi va undan
    // keyin oʻchiriladi. Drayver jarayonni yiqitsa belgi qoladi: keyingi
    // ishga tushish uni koʻrib CPU'ga oʻtadi — aks holda login'dagi
    // avtostart har safar yiqilaverardi.
    static bool gpuOldinYiqilgan();
    static void gpuBelgisiniOchir();

    // Ishchi oqimni toʻxtatadi. Ilova yopilishida chaqiriladi.
    void shutdown();

private:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    struct Impl;
    Impl* d;
};

}  // namespace rubai
