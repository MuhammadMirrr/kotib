// Sozlamalar oynasi: sunʼiy intellekt (LLM) boʻlimi — provayder, base URL,
// model roʻyxati, API kalit (Credential Manager) va ulanishni tekshirish.
// `SettingsWindow::Impl` — `settings_window_ichki.h`.

#include "settings_window_ichki.h"

namespace rubai {

// ------------------------------------------------ sunʼiy intellekt boʻlimi

std::wstring SettingsWindow::Impl::maydonMatni(int id) const {
    HWND c = ctrl(id);
    if (!c) return {};
    const int n = GetWindowTextLengthW(c);
    if (n <= 0) return {};
    std::wstring b(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(c, b.data(), n + 1);
    b.resize(static_cast<size_t>(n));
    // Boshi va oxiridagi boʻshliqlar — nusxa-koʻchirishda deyarli doim
    // qoʻshilib qoladi va ular URL yoki kalitni buzadi.
    const size_t a = b.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return {};
    const size_t z = b.find_last_not_of(L" \t\r\n");
    return b.substr(a, z - a + 1);
}

void SettingsWindow::Impl::maydonQoy(int id, const std::wstring& s) {
    if (HWND c = ctrl(id)) SetWindowTextW(c, s.c_str());
}

void SettingsWindow::Impl::llmniYukla() {
    // Provayderni tanlaymiz: 0 — «tanlanmagan».
    int tanlov = 0;
    const auto& royxat = provayderlar();
    for (size_t i = 0; i < royxat.size(); ++i) {
        if (royxat[i].id == settings.llmProvayder) {
            tanlov = static_cast<int>(i) + 1;
            break;
        }
    }
    SendMessageW(ctrl(kIdProvayder), CB_SETCURSEL, tanlov, 0);

    maydonQoy(kIdKalit, settings.llmProvayder.empty() ? std::wstring()
                                                      : Kalitlar::oqi(settings.llmProvayder));
    maydonQoy(kIdBaseURL, LLMSozlama::baseURL());
    maydonQoy(kIdModel, LLMSozlama::model());
    maydonQoy(kIdModelHolati, L"");
    maydonQoy(kIdTekshirNatija, L"");
    xosQatorniYangila();
}

void SettingsWindow::Impl::xosQatorniYangila() {
    // Base URL faqat «custom» provayderda va faqat qoʻshimcha boʻlim
    // ochiq boʻlganda koʻrinadi.
    const bool xos = qoshimchaOchiq && settings.llmProvayder == "custom";
    if (HWND c = ctrl(kIdBaseURL)) ShowWindow(c, xos ? SW_SHOW : SW_HIDE);
}

void SettingsWindow::Impl::provayderOzgardi() {
    const int sel = static_cast<int>(SendMessageW(ctrl(kIdProvayder), CB_GETCURSEL, 0, 0));
    const auto& royxat = provayderlar();

    if (sel <= 0 || static_cast<size_t>(sel - 1) >= royxat.size()) {
        settings.llmProvayder.clear();
        settings.llmBaseURL.clear();
        settings.llmModel.clear();
    } else {
        const Provayder& p = royxat[static_cast<size_t>(sel - 1)];
        settings.llmProvayder = p.id;
        // Provayder almashganda URL va model presetga qaytadi: eski
        // provayderning modeli yangisida deyarli hech qachon mavjud emas.
        settings.llmBaseURL = p.baseURL;
        settings.llmModel = p.standartModel;
    }

    maydonQoy(kIdBaseURL, settings.llmBaseURL);
    maydonQoy(kIdModel, settings.llmModel);
    maydonQoy(kIdKalit, settings.llmProvayder.empty() ? std::wstring()
                                                      : Kalitlar::oqi(settings.llmProvayder));
    SendMessageW(ctrl(kIdModel), CB_RESETCONTENT, 0, 0);
    maydonQoy(kIdModel, settings.llmModel);
    maydonQoy(kIdModelHolati, L"");
    maydonQoy(kIdTekshirNatija, L"");
    xosQatorniYangila();
}

void SettingsWindow::Impl::llmniSaqla() {
    settings.llmBaseURL = maydonMatni(kIdBaseURL);
    settings.llmModel = maydonMatni(kIdModel);
    // Kalit sozlama faylida EMAS — Credential Manager'da.
    if (!settings.llmProvayder.empty()) {
        Kalitlar::saqla(settings.llmProvayder, maydonMatni(kIdKalit));
    }
}

void SettingsWindow::Impl::modellarniKorsat(const std::vector<std::wstring>& royxat) {
    HWND combo = ctrl(kIdModel);
    const std::wstring joriy = maydonMatni(kIdModel);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (const auto& m : royxat) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m.c_str()));
    }
    // Tanlangan model roʻyxatdan yoʻqolgan boʻlishi mumkin — matnni
    // OʻZGARTIRMAYMIZ: model tanlovi pulga va sifatga tegadi, qaror
    // foydalanuvchiniki (macOS'da ham shunday).
    maydonQoy(kIdModel, joriy);
}

void SettingsWindow::Impl::modellarniOlish() {
    if (llmSoravKetyapti) return;

    const int sel = static_cast<int>(SendMessageW(ctrl(kIdProvayder), CB_GETCURSEL, 0, 0));
    const auto& royxat = provayderlar();
    if (sel <= 0 || static_cast<size_t>(sel - 1) >= royxat.size()) {
        maydonQoy(kIdModelHolati, L"Avval provayderni tanlang");
        return;
    }
    const Provayder p = royxat[static_cast<size_t>(sel - 1)];
    const std::wstring kalit = maydonMatni(kIdKalit);
    const std::wstring baza = maydonMatni(kIdBaseURL);
    if (kalit.empty()) {
        maydonQoy(kIdModelHolati, L"Avval API kalitni kiriting");
        return;
    }

    llmSoravKetyapti = true;
    maydonQoy(kIdModelHolati, L"Modellar olinmoqda…");

    HWND oyna = hwnd;
    std::thread([oyna, p, baza, kalit] {
        std::vector<std::wstring> chiqish;
        const std::wstring xato = modellarniOl(p, baza, kalit, chiqish);
        if (!xato.empty()) {
            PostMessageW(oyna, WM_LLM_HOLAT, kIdModelHolati,
                         reinterpret_cast<LPARAM>(new std::wstring(L"⚠ " + xato)));
        } else {
            PostMessageW(
                oyna, WM_LLM_MODELLAR, 0,
                reinterpret_cast<LPARAM>(new std::vector<std::wstring>(std::move(chiqish))));
        }
    }).detach();
}

void SettingsWindow::Impl::ulanishniTekshir() {
    if (llmSoravKetyapti) return;
    llmniSaqla();

    const Provayder* p = provayderTop(settings.llmProvayder);
    if (!p) {
        maydonQoy(kIdTekshirNatija, L"Avval provayderni tanlang");
        return;
    }

    LLMSorov sorov;
    sorov.provayder = *p;
    sorov.baseURL = maydonMatni(kIdBaseURL);
    sorov.model = maydonMatni(kIdModel);
    sorov.kalit = maydonMatni(kIdKalit);
    sorov.system = L"Faqat «ha» deb javob ber.";
    sorov.user = L"Salom";

    llmSoravKetyapti = true;
    maydonQoy(kIdTekshirNatija, L"Tekshirilmoqda…");

    HWND oyna = hwnd;
    std::thread([oyna, sorov] {
        std::wstring javob;
        const std::wstring xato =
            llmOqim(sorov, [&javob](const std::wstring& b) { javob += b; }, nullptr);

        std::wstring natija;
        if (!xato.empty())
            natija = L"⚠ " + xato;
        else if (javob.empty())
            natija = L"⚠ Javob boʻsh keldi";
        else
            natija = L"✓ Ulanish ishlayapti";

        PostMessageW(oyna, WM_LLM_HOLAT, kIdTekshirNatija,
                     reinterpret_cast<LPARAM>(new std::wstring(std::move(natija))));
    }).detach();
}

}  // namespace rubai
