// UI'ni yadrosiz sinash uchun kichik dastur.
//
// Nega kerak: asosiy ilova model yuklashni, mikrofonni va tray'ni talab
// qiladi — UI'dagi bitta rangni tekshirish uchun ularning hammasini
// koʻtarish ortiqcha. Bu dastur faqat oynani ochadi va soxta maʼlumot
// bilan toʻldiradi, shuning uchun uni virtual mashinada bir soniyada
// ishga tushirib, chizilishini koʻrish mumkin.
//
// Relizga kirmaydi — faqat ishlab chiqish vositasi.
#include "../ui/asosiy_oyna.h"
#include "../ui/yozish_tab.h"

#include <windows.h>

#include <ctime>
#include <vector>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Har bir ekran uchun alohida DPI — oynani boshqa monitorga sudraganda
    // qayta masshtablanadi. Bu asosiy ilovadagi bilan bir xil rejim.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    rubai::AsosiyOyna oyna;
    if (!oyna.qur(instance)) {
        MessageBoxW(nullptr, L"Oyna ochilmadi", L"Kotib — sinov", MB_ICONERROR);
        return 1;
    }

    bool yozilyapti = false;
    oyna.onDiktovka = [&] {
        yozilyapti = !yozilyapti;
        oyna.diktovkaHolati(yozilyapti);
    };

    // Tarixni soxta yozuvlar bilan toʻldiramiz — roʻyxat, surish va
    // qisqartirish qanday koʻrinishini shusiz tekshirib boʻlmaydi.
    const long long hozir = static_cast<long long>(std::time(nullptr));
    std::vector<rubai::TarixYozuvi> tarix = {
        {L"1", L"Salom, bu Kotib ilovasining Windows uchun birinchi native oynasi.", hozir - 60},
        {L"2", L"Oʻzbek tilida gapirasiz, matn kursor turgan joyga oʻzi yoziladi.", hozir - 3600},
        {L"3", L"Ertaga soat oʻnda yigʻilish bor, hujjatlarni tayyorlab qoʻying.", hozir - 86400},
        {L"4",
         L"Bu uzun yozuv — qatorga sigʻmaydi va uch nuqta bilan qisqartirilishi "
         L"kerak, aks holda dizayn buziladi va matn chetdan chiqib ketadi.",
         hozir - 90000},
    };
    oyna.yozishTabi()->tarixniQoy(tarix);
    oyna.yozishTabi()->hotkeyMatni(L"Ctrl+Alt+D");

    // Ogohlantirish banneri — u faqat mikrofon muammosida chiqadi, shuning
    // uchun uni koʻrishning boshqa yoʻli yoʻq.
    oyna.banner(L"Mikrofon tanlanmagan — Windows standart qurilmani beradi.", L"Tanlash", [] {});

    // Yangilanish banneri — u faqat yangi versiya chiqqanda koʻrinadi.
    oyna.yangilanishBanneri(L"Kotib 1.2.0 ga yangilandi — Tarjimon tezlashdi.", L"");

    oyna.onSozlamalar = [&] {
        MessageBoxW(oyna.deskriptor(), L"Sinov dasturida sozlamalar oynasi yoʻq.", L"Kotib — sinov",
                    MB_OK | MB_ICONINFORMATION);
    };

    oyna.korsat();

    MSG xabar;
    while (GetMessageW(&xabar, nullptr, 0, 0)) {
        TranslateMessage(&xabar);
        DispatchMessageW(&xabar);
    }
    return 0;
}
