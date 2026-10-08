// `config.h` dagi `sozlamaOynasidan` testlari (barqarorlik E1).
#include "../core/config.h"

#include <string>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
}  // namespace kotib_test

using kotib_test::tekshir;
using namespace rubai;

void sozlamaTestlari() {
    // Birinchi ishga tushish: oyna ochilganda nusxa olindi (ID hali yoʻq),
    // shu orada statistika ID yasab faylga yozdi, keyin «Saqlash» bosildi.
    Settings oyna;  // ochilgandagi nusxa
    oyna.vkCode = 'K';
    oyna.modifiers = 3;
    oyna.hotkeyLabel = L"K";
    oyna.micDeviceId = L"{mic}";
    oyna.micDeviceName = L"USB mikrofon";
    oyna.insertMode = InsertMode::Type;
    oyna.autoStart = false;
    oyna.useGpu = false;
    oyna.oddiyApostrof = true;
    oyna.llmProvayder = "openai";
    oyna.llmBaseURL = L"https://x";
    oyna.llmModel = L"m";
    oyna.diagnostikaTugash = 1791086400;

    Settings fayl;  // diskdagi, boshqa modullar yozgan
    fayl.ornatmaId = L"0123456789abcdef0123456789abcdef";
    fayl.oxirgiTekshiruv = 1791000000;
    fayl.yangilanishYopildi = L"1.2.0";
    fayl.tarjimaManba = "rus_Cyrl";
    fayl.tarjimaMaqsad = "eng_Latn";
    fayl.didOnboard = true;
    fayl.idleUnloadSeconds = 60;
    fayl.yangilanishMuvaffaqiyat = 1790990000;
    fayl.yangilanishChelak = 42;
    fayl.majburiyMin = L"1.2.1";
    fayl.majburiyMuhlat = 48;
    fayl.majburiyKorilgan = 1790900000;
    fayl.kutilganVersiya = L"1.2.1";
    fayl.yangilanishKanali = L"sinov";

    const Settings r = sozlamaOynasidan(fayl, oyna);
    tekshir(L"sozlama: ID saqlanadi", r.ornatmaId == fayl.ornatmaId);
    tekshir(L"sozlama: tekshiruv vaqti saqlanadi", r.oxirgiTekshiruv == 1791000000);
    tekshir(L"sozlama: yopilgan banner saqlanadi", r.yangilanishYopildi == L"1.2.0");
    tekshir(L"sozlama: tarjima tillari saqlanadi",
            r.tarjimaManba == "rus_Cyrl" && r.tarjimaMaqsad == "eng_Latn");
    tekshir(L"sozlama: onboarding belgisi saqlanadi", r.didOnboard);
    tekshir(L"sozlama: boʻsh turish vaqti saqlanadi", r.idleUnloadSeconds == 60);
    // Yangilovchi yozgan holat (S8/S9) — oyna uni hech qachon eskisiga qaytarmasin:
    // majburiy muhlat boshidan boshlanib qolardi.
    tekshir(L"sozlama: yangilanish holati saqlanadi",
            r.yangilanishMuvaffaqiyat == 1790990000 && r.yangilanishChelak == 42 &&
                r.majburiyMin == L"1.2.1" && r.majburiyMuhlat == 48 &&
                r.majburiyKorilgan == 1790900000 && r.kutilganVersiya == L"1.2.1" &&
                r.yangilanishKanali == L"sinov");
    tekshir(L"sozlama: diagnostika oynadan", r.diagnostikaTugash == 1791086400);

    // Diagnostika muddati (macOS'dagi log siyosati testlari bilan bir xil holatlar).
    const long long h = 1791000000;
    tekshir(L"diagnostika: oʻchiq", !diagnostikaFaolmi(0, h));
    tekshir(L"diagnostika: 1 soat qoldi", diagnostikaFaolmi(h + 3600, h));
    tekshir(L"diagnostika: tugagan", !diagnostikaFaolmi(h - 1, h));
    tekshir(L"diagnostika: aniq tugash payti", !diagnostikaFaolmi(h, h));
    tekshir(L"diagnostika: 24 soat qoldi", diagnostikaFaolmi(h + 24 * 3600, h));
    tekshir(L"diagnostika: soat orqaga — 24 soatdan uzaymaydi",
            !diagnostikaFaolmi(h + 24 * 3600 + 1, h));
    tekshir(L"sozlama: oyna maydonlari qoʻllanadi",
            r.vkCode == 'K' && r.modifiers == 3 && r.hotkeyLabel == L"K" &&
                r.micDeviceId == L"{mic}" && r.micDeviceName == L"USB mikrofon" &&
                r.insertMode == InsertMode::Type && !r.autoStart && !r.useGpu && r.oddiyApostrof &&
                r.llmProvayder == "openai" && r.llmBaseURL == L"https://x" && r.llmModel == L"m");
}
