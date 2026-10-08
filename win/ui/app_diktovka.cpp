// Diktovka oqimi: tugma → yozish → whisper → kiritish, va matnga oʻgirib
// boʻlmagan ovozni saqlash / qayta urinish (barqarorlik A2). `App` — `app.h`.

#include "app.h"

namespace rubai {

// ------------------------------------------------------------- diktovka

void App::toggleDictation() {
    if (busy_) return;  // transkripsiya ketmoqda — kutamiz

    const ULONGLONG hozir = GetTickCount64();
    if (hozir - oxirgiToggle_ < kToggleOynasiMs) return;
    oxirgiToggle_ = hozir;

    if (recording_)
        stopAndTranscribe();
    else
        startRecording();
}

void App::startRecording() {
    // Majburiy yangilanish muhlati tugagan (S9): yangi yozuv boshlanmaydi —
    // ketayotganini toʻxtatish esa doim mumkin (`stopAndTranscribe`). Sabab va
    // «Qayta urinish» / «Saytdan yuklab olish» oynadagi bannerda.
    if (yangilovchi_.bloklanganmi()) {
        logWrite(L"yozish boshlanmadi: majburiy yangilanish muhlati tugagan");
        overlay_.show(OverlayIcon::Warning, L"Yangilanish majburiy — oynada batafsil", 4000);
        oynaniOch();
        return;
    }
    // Fayl ishi ketayotganda diktovka BOSHLANMAYDI (D1): navbat bitta, matn
    // fayl tugaguncha kutib, keyin boshqa oynaga tushardi. macOS bilan bir xil.
    if (TranskripsiyaIshi::ishlayapti()) {
        overlay_.show(OverlayIcon::Warning, L"Fayl ustida ish ketmoqda — kuting", 2000);
        return;
    }
    // Model yoʻq boʻlsa yozuv BOSHLANMAYDI (A4): ilgari bu foydalanuvchi
    // gapirib boʻlgandan keyingina — «Model topilmadi» xatosi bilan —
    // bilinardi. Oynada «Yuklab olish» banneri (`mikrofonBanneri`) turadi.
    if (Engine::findModel().empty()) {
        logWrite(L"yozish boshlanmadi: model topilmadi");
        overlay_.show(OverlayIcon::Warning,
                      modelYuklovchi_.ketyaptimi() ? L"Model hali yuklanmoqda — kuting"
                                                   : L"Nutq modeli topilmadi — oynada yuklab oling",
                      4000);
        oynaniOch();
        return;
    }
    if (modelBanneri_) mikrofonBanneri();  // model qaytgan — eski banner qolmasin
    std::wstring error;
    if (!capture_.start(settings_.micDeviceId, error)) {
        logWrite(L"XATO: yozish boshlanmadi — " + error);
        overlay_.show(OverlayIcon::Warning, L"Mikrofon ochilmadi", 4000);
        notify(L"Mikrofon ochilmadi", error, NIIF_ERROR);
        return;
    }
    recording_ = true;
    DiktovkaBand::mikrofon = true;
    faollikBoldi();
    yozishBoshlandi_ = GetTickCount64();
    updateTrayTip(L"Yozilmoqda…  (" + settings_.hotkeyDisplay() + L" — toʻxtatish)");
    asosiy_.diktovkaHolati(true);
    SetTimer(hwnd_, kDarajaTaymer, kDarajaOraligi, nullptr);
    SetTimer(hwnd_, kMaxYozishTaymer, kMaxYozishMs, nullptr);
    // Suzuvchi koʻrsatkich foydalanuvchi Kotib oynasiga QARAB turmagan
    // paytdagina kerak: oyna oldinda boʻlsa u holatni allaqachon kartada
    // koʻrib turibdi. Ilgari bu yerda «oyna koʻrinadimi» tekshiruvi turardi
    // va u ikki holatda yanglishardi — `IsWindowVisible` yigʻilgan oyna
    // uchun ham, boshqa ilova ostida qolgan oyna uchun ham TRUE qaytaradi.
    // Ikkalasi ham ilovaning ODATDAGI ishlatilishi: matn boshqa ilovaga
    // yoziladi, demak Kotib oynasi deyarli har doim orqada boʻladi.
    if (GetForegroundWindow() != asosiy_.deskriptor()) {
        overlay_.show(OverlayIcon::Recording, L"Yozilmoqda…  (" + settings_.hotkeyDisplay() + L")");
    }
    logWrite(L"yozish boshlandi");
}

void App::stopAndTranscribe() {
    recording_ = false;
    DiktovkaBand::mikrofon = false;
    faollikBoldi();
    KillTimer(hwnd_, kDarajaTaymer);
    KillTimer(hwnd_, kMaxYozishTaymer);
    asosiy_.diktovkaHolati(false);
    yozishDavomiyligi_ = static_cast<double>(GetTickCount64() - yozishBoshlandi_) / 1000.0;
    std::vector<float> samples = capture_.stop();

    const AudioCheck check = checkSamples(samples);
    logWrite(L"yozish tugadi: " + std::to_wstring((int)(check.seconds * 10) / 10.0) + L"s, peak=" +
             std::to_wstring(check.peak));

    // Avval uzunlik, keyin signal (A7) — macOS bilan bir xil. Uzunlik
    // namunalardan emas, soatdan olinadi: tez ikki bosishda namuna umuman
    // boʻlmaydi va bu «Mikrofon jim» emas. Mikrofon uzoq gapirilganda ham
    // namuna bermasa esa — baribir «jim» (`checkSamples` izohidagi tashvish).
    if (check.verdict == AudioVerdict::Silent && yozishDavomiyligi_ < 1.0) {
        updateTrayTip(kAppName);
        overlay_.show(OverlayIcon::Warning, L"Juda qisqa — kamida 1 soniya gapiring", 3000);
        return;
    }
    if (check.verdict == AudioVerdict::Silent) {
        updateTrayTip(kAppName);
        // Ikkalasi ham ataylab: suzuvchi koʻrsatkich 4 soniyadan keyin
        // yoʻqoladi, bildirishnoma esa Bildirishnomalar markazida qoladi.
        // Bu xato foydalanuvchidan ISH talab qiladi (mikrofonni ulash yoki
        // maxfiylik sozlamasini yoqish), shuning uchun uni oʻtkazib yuborib
        // boʻlmaydigan qilib qoʻyamiz. macOS'da faqat koʻrsatkich bor —
        // u yerda ruxsat masalasi tizimning oʻz dialogi bilan hal boʻladi.
        overlay_.show(OverlayIcon::Warning, L"Mikrofon jim — signal yoʻq", 4000);
        notify(L"Mikrofon jim",
               L"Signal aniqlanmadi. Mikrofon ulanganini va Windows\n"
               L"maxfiylik sozlamalarida ruxsat berilganini tekshiring.",
               NIIF_WARNING);
        return;
    }
    if (check.verdict == AudioVerdict::TooShort) {
        updateTrayTip(kAppName);
        overlay_.show(OverlayIcon::Warning, L"Juda qisqa — kamida 1 soniya gapiring", 3000);
        return;
    }

    busy_ = true;
    updateTrayTip(L"Matnga oʻgirilmoqda…");
    overlay_.show(OverlayIcon::Working, L"Matnga oʻgirilmoqda…");

    std::vector<float> tayyor = prepareSamples(samples);
    kutilayotganOvoz_ = std::move(samples);
    ++DiktovkaBand::transkripsiya;
    Engine::instance().transcribeAsync(std::move(tayyor), [this](TranscribeResult r) {
        // Bu ishchi oqim — UI'ga tegmaymiz, natijani asosiy oqimga uzatamiz.
        pending_ = std::make_unique<TranscribeResult>(std::move(r));
        PostMessageW(hwnd_, WM_RUBAI_RESULT, 0, 0);
    });
}

void App::onResult(const TranscribeResult& r) {
    busy_ = false;
    faollikBoldi();
    if (DiktovkaBand::transkripsiya > 0) --DiktovkaBand::transkripsiya;
    updateTrayTip(kAppName);
    std::vector<float> ovoz;
    ovoz.swap(kutilayotganOvoz_);

    if (!r.ok()) {
        logWrite(L"XATO: " + r.error);
        // Ovoz tashlanmaydi (A2) — 1.1.0 gacha shu yerda yoʻqolardi.
        ovozniSaqla(std::move(ovoz), r.error);
        return;
    }
    // Model ishlayapti — oldin saqlangan ovoz boʻlsa, bir oz kutib sinaymiz.
    SetTimer(hwnd_, kQaytaTaymer, kQaytaKutishMs, nullptr);

    if (r.text.empty()) {
        overlay_.show(OverlayIcon::Warning, L"Ovoz aniqlanmadi — balandroq gapiring", 3500);
        return;
    }

    // Yagona tayyorlash qadami — tarix va kiritish shu natijani oladi
    // (oʻ/gʻ/ʼ va «Oddiy apostrof» sozlamasi; core/matn_format.h).
    const std::wstring matn =
        matnniTayyorla(r.text, settings_.oddiyApostrof ? Apostrof::Oddiy : Apostrof::Standart);

    // Matn faqat «Diagnostika rejimi» yoqiq boʻlsa (G1) — standart holatda
    // logda shaxsiy matn yoʻq, faqat uzunligi.
    const bool diagnostika =
        diagnostikaFaolmi(settings_.diagnostikaTugash, static_cast<long long>(std::time(nullptr)));
    logWrite(L"natija (" + std::to_wstring(matn.size()) + L" belgi, " +
             std::to_wstring((int)(r.seconds * 1000)) + L" ms)" +
             (diagnostika ? L": «" + matn + L"»" : std::wstring()));

    // Tarix — qoʻshimcha qulaylik: matn baribir fokusdagi ilovaga tushadi.
    // Saqlanmasa ham diktovka toʻxtamaydi (`tarix.h` dagi qoida).
    DiktovkaTarixi::birgalik().qoshish(matn, yozishDavomiyligi_);
    asosiy_.diktovkaTugadi(matn);

    // Administrator huquqidagi oynaga Windows (UIPI) sunʼiy klavishani jimgina
    // yetkazmaydi va SendInput xato qaytarmaydi — matn izsiz yoʻqolardi.
    // Shuning uchun tekshiruv kiritishdan OLDIN (E5): matn clipboard'da qoladi.
    if (foregroundWindowIsElevated()) {
        matnniClipboardgaQoy(matn);
        logWrite(L"faol oyna administrator huquqida — matn clipboard'da qoldirildi");
        overlay_.show(OverlayIcon::Warning, L"Matn clipboard'da — Ctrl+V bosing", 5000);
        notify(L"Matn joylanmadi",
               L"Faol oyna administrator huquqi bilan ishlayapti —\n"
               L"Windows unga matn yuborishga ruxsat bermaydi.\n"
               L"Matn clipboard'da: Ctrl+V bosing.",
               NIIF_WARNING);
        return;
    }

    const InsertResult ins = insertText(matn, settings_.insertMode);
    if (ins.ok) {
        // Matn joyiga tushdi — overlay'ni darhol yashiramiz, chunki
        // foydalanuvchi natijani oʻz oynasida koʻrib turibdi.
        overlay_.hide();
        return;
    }

    // Joylanmadi — sababini aytamiz.
    const std::wstring& msg = ins.error;
    logWrite(L"matn joylanmadi: " + msg);
    overlay_.show(OverlayIcon::Warning, L"Matn clipboard'da — Ctrl+V bosing", 5000);
    notify(L"Matn joylanmadi", msg, NIIF_WARNING);
}

// ------------------------------------------------------ saqlanmagan ovoz (A2)

std::wstring App::saqlanmaganPapka() const { return localAppDataDir() + L"\\saqlanmagan"; }

void App::ovozniSaqla(std::vector<float> ovoz, const std::wstring& sabab) {
    const std::wstring yol =
        ovoz.empty() ? std::wstring()
                     : saqlanmagan::saqla(ovoz, saqlanmaganPapka(), saqlanmagan::hozirMs());
    if (yol.empty()) {
        logWrite(L"XATO: ovozni saqlab boʻlmadi");
        overlay_.show(OverlayIcon::Warning, sabab, 5000);
        notify(L"Xatolik", sabab, NIIF_ERROR);
        return;
    }
    logWrite(L"ovoz saqlandi: " + yol + L" (" + std::to_wstring(ovoz.size()) + L" namuna)");
    overlay_.show(OverlayIcon::Warning, sabab + L" — ovoz saqlandi", 6000);
    notify(L"Ovoz saqlandi",
           sabab + L"\nGapirganingiz yoʻqolmadi: tray menyusida «Saqlangan ovozni\n"
                   L"matnga oʻgirish» — yoki model tayyor boʻlgach oʻzi oʻgiriladi.",
           NIIF_WARNING);
}

// Saqlangan ovozlarni eskisidan boshlab ketma-ket matnga oʻgiradi. Natija
// tarixga yoziladi (asl sana bilan); qoʻlda bosilganda clipboard'ga ham —
// lekin HECH QACHON fokusdagi ilovaga kiritilmaydi: u kutilmagan paytda,
// boshqa oynaga tushib qolardi.
void App::saqlanganlarniOgir(bool qolda) {
    if (qaytaIshlanyapti_) return;
    qaytaNavbat_.clear();
    for (const auto& y : saqlanmagan::royxat(saqlanmaganPapka())) {
        if (qolda || !avtoUrinilgan_.count(y)) qaytaNavbat_.push_back(y);
    }
    if (qaytaNavbat_.empty()) {
        if (qolda) overlay_.show(OverlayIcon::Warning, L"Saqlangan ovoz yoʻq", 2000);
        return;
    }
    qaytaIshlanyapti_ = true;
    qaytaQolda_ = qolda;
    qaytaIndeks_ = 0;
    qaytaMatnlar_.clear();
    qaytaXato_.clear();
    logWrite(L"saqlangan ovoz: " + std::to_wstring(qaytaNavbat_.size()) + L" ta qayta urinish (" +
             (qolda ? L"qoʻlda" : L"avtomatik") + L")");
    if (qolda) overlay_.show(OverlayIcon::Working, L"Saqlangan ovoz matnga oʻgirilmoqda…");
    qaytaKeyingi();
}

void App::qaytaKeyingi() {
    for (;;) {
        // Avtomatik rejimda foydalanuvchi diktovkani boshlasa — toʻxtaymiz;
        // qolganlari keyingi muvaffaqiyatli diktovkadan keyin sinaladi.
        if (qaytaIndeks_ >= qaytaNavbat_.size() || (!qaytaQolda_ && (recording_ || busy_))) {
            qaytaTugadi();
            return;
        }
        const std::wstring& yol = qaytaNavbat_[qaytaIndeks_];
        avtoUrinilgan_.insert(yol);
        std::vector<float> s;
        if (!saqlanmagan::oqi(yol, s)) {
            logWrite(L"XATO: saqlangan ovoz oʻqilmadi: " + yol);
            ++qaytaIndeks_;
            continue;
        }
        qaytaNamunaSoni_ = s.size();
        HWND oyna = hwnd_;
        Engine::instance().transcribeAsync(prepareSamples(s), [this, oyna](TranscribeResult r) {
            pendingQayta_ = std::make_unique<TranscribeResult>(std::move(r));
            PostMessageW(oyna, WM_RUBAI_QAYTA, 0, 0);
        });
        return;
    }
}

void App::onQaytaNatija(const TranscribeResult& r) {
    if (qaytaIndeks_ >= qaytaNavbat_.size()) return;
    const std::wstring yol = qaytaNavbat_[qaytaIndeks_];
    if (!r.ok()) {
        // Model hali ham ishlamayapti — qolganini sinash befoyda.
        qaytaXato_ = r.error;
        qaytaTugadi();
        return;
    }
    if (!r.text.empty()) {
        const std::wstring matn =
            matnniTayyorla(r.text, settings_.oddiyApostrof ? Apostrof::Oddiy : Apostrof::Standart);
        const long long ms = saqlanmagan::sana(yol.substr(yol.find_last_of(L"\\/") + 1));
        DiktovkaTarixi::birgalik().qoshish(
            matn, static_cast<double>(qaytaNamunaSoni_) / kNamunaTezligi, ms > 0 ? ms / 1000 : 0);
        qaytaMatnlar_.push_back(matn);
    }
    // Boʻsh natija — ovozda nutq yoʻq edi; saqlashdan maʼno yoʻq.
    DeleteFileW(yol.c_str());
    ++qaytaIndeks_;
    qaytaKeyingi();
}

void App::qaytaTugadi() {
    qaytaIshlanyapti_ = false;
    if (!qaytaMatnlar_.empty()) asosiy_.tarixniYangila();
    logWrite(L"saqlangan ovoz: " + std::to_wstring(qaytaMatnlar_.size()) + L" ta matnga oʻgirildi" +
             (qaytaXato_.empty() ? std::wstring() : L", xato: " + qaytaXato_));
    if (qaytaQolda_) {
        if (!qaytaMatnlar_.empty()) {
            std::wstring hammasi;
            for (const auto& m : qaytaMatnlar_) {
                if (!hammasi.empty()) hammasi += L"\r\n\r\n";
                hammasi += m;
            }
            buferGaQoy(hwnd_, hammasi);
            overlay_.show(OverlayIcon::Done,
                          std::to_wstring(qaytaMatnlar_.size()) +
                              L" ta ovoz matnga oʻgirildi — clipboard'da (Ctrl+V) va tarixda",
                          5000);
        } else if (!qaytaXato_.empty()) {
            overlay_.show(OverlayIcon::Warning, qaytaXato_ + L" — ovoz saqlanib qoldi", 4000);
        } else {
            overlay_.show(OverlayIcon::Warning, L"Saqlangan ovozda nutq topilmadi", 3000);
        }
    } else if (!qaytaMatnlar_.empty() && !recording_) {
        overlay_.show(OverlayIcon::Done,
                      L"Saqlangan " + std::to_wstring(qaytaMatnlar_.size()) +
                          L" ta ovoz matnga oʻgirildi — Kotib oynasida",
                      4000);
    }
}

}  // namespace rubai
