// Studiya amallari (LLM) va ularning prompt'lari.
// Interfeys va izohlar — `amallar.h`.

#include "amallar.h"

#include "llm.h"
#include "util.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace rubai {

namespace {

// Har bir prompt'ga qoʻshiladigan umumiy qoidalar. macOS'dagi `umumiyQoida`
// bilan BELGIMA-BELGI bir xil.
const wchar_t* kUmumiyQoida =
    L"Sen oʻzbek tilidagi nutq transkripti ustida ishlaysan.\n"
    L"\n"
    L"Qatʼiy qoidalar:\n"
    L"- Javobni FAQAT oʻzbek lotin alifbosida yoz. Rus, turk yoki ingliz tiliga oʻtma.\n"
    L"- «oʻ» va «gʻ» harflarini toʻgʻri yoz.\n"
    L"- Maʼnoni OʻZGARTIRMA. Matnda boʻlmagan maʼlumotni QOʻSHMA.\n"
    L"- Faqat natijani qaytar. «Mana natija», «Albatta» kabi muqaddima yozma.";

std::wstring qoida(const wchar_t* vazifa) { return std::wstring(kUmumiyQoida) + L"\n\n" + vazifa; }

// Bitta boʻlakka beriladigan maksimal soʻz soni — macOS bilan bir xil.
constexpr size_t kBolakSozLimiti = 3000;

}  // namespace

const std::vector<Amal>& amallar() {
    static const std::vector<Amal> royxat = {
        {"tozalash", L"Tozalash", false,
         qoida(L"Vazifang: transkriptni oʻqishga qulay holga keltirish.\n"
               L"- Tinish belgilarini toʻgʻri qoʻy.\n"
               L"- Maʼnoli paragraflarga boʻl.\n"
               L"- Ogʻzaki axlatni olib tashla: «e-e», «ha shunday», «anavi», «yaʼni»,\n"
               L"  takrorlangan soʻzlar, toʻxtab qolishlar.\n"
               L"- Yarim aytilgan jumlalarni toʻliq jumlaga aylantir.\n"
               L"- Raqamlarni raqam bilan yoz: «yigirma besh» → «25».")},

        {"xulosa", L"Qisqacha xulosa", true,
         qoida(L"Vazifang: matnning 3–5 jumlalik xulosasini yoz.")},

        {"asosiy_fikrlar", L"Asosiy fikrlar", true,
         qoida(L"Vazifang: asosiy fikrlarni belgili roʻyxat («- » bilan) shaklida yoz.")},

        {"bayonnoma", L"Bayonnoma", true,
         qoida(L"Vazifang: yigʻilish bayonnomasini tuz. Uchta boʻlim:\n"
               L"«Muhokama qilinganlar», «Qarorlar», «Topshiriqlar».\n"
               L"Matnda boʻlmagan boʻlimni boʻsh qoldir, oʻylab topma.")},

        {"maqola", L"Maqola", false,
         qoida(L"Vazifang: matnni maqola yoki bloq posti shakliga keltir.\n"
               L"Sarlavha va kichik sarlavhalar qoʻy, paragraflarga boʻl.")},

        {"savol_javob", L"Savol-javob", true,
         qoida(L"Vazifang: matn asosida savol-javob (FAQ) tuz.\n"
               L"Har bir savol «**S:** », javob «**J:** » bilan boshlansin.")},

        {"qisqartirish", L"Qisqartirish", false,
         qoida(L"Vazifang: matnni taxminan ikki barobar qisqartir.\n"
               L"Barcha muhim maʼlumot saqlanib qolsin.")},

        {"uzaytirish", L"Uzaytirish", false,
         qoida(L"Vazifang: matnni batafsilroq yozib chiq — jumlalarni toʻliqroq,\n"
               L"fikrlarni ochiqroq qil. Yangi FAKT qoʻshma.")},
    };
    return royxat;
}

const Amal* amalTop(const std::string& id) {
    for (const auto& a : amallar()) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

Amal erkinAmal(const std::wstring& korsatma) {
    return {"erkin", L"Oʻz soʻrovim", false,
            qoida((L"Foydalanuvchi koʻrsatmasi: " + korsatma).c_str())};
}

// ---- Ish -------------------------------------------------------------------

struct AmalIshi::Ichki {
    std::atomic<bool> ketyapti{false};
    std::atomic<bool> bekor{false};

    // Qayta chaqiruvlar ishchi oqimdan chaqiriladi, egasi esa UI oqimida
    // yoʻq qilinishi mumkin. Qulf ikkalasini ajratadi: `uzil` dan keyin
    // oqim endi yoʻq obyektga murojaat qilmaydi.
    std::mutex qulf;
    std::function<void(const std::wstring&)> onDelta;
    std::function<void(const std::wstring&)> onTayyor;
    std::function<void(const std::wstring&)> onXato;

    LLMSorov asos;  // provayder, URL, model, kalit — bir marta oʻqiladi

    void delta(const std::wstring& s) {
        std::lock_guard<std::mutex> q(qulf);
        if (onDelta) onDelta(s);
    }
    void tayyor(const std::wstring& s) {
        std::lock_guard<std::mutex> q(qulf);
        if (onTayyor) onTayyor(s);
    }
    void xatoBer(const std::wstring& s) {
        std::lock_guard<std::mutex> q(qulf);
        if (onXato) onXato(s);
    }
    void uzil() {
        std::lock_guard<std::mutex> q(qulf);
        onDelta = nullptr;
        onTayyor = nullptr;
        onXato = nullptr;
    }

    void tugat() { ketyapti.store(false); }

    // Bitta soʻrov. Yigʻilgan matnni qaytaradi; xato boʻlsa `xato` toʻladi.
    std::wstring bitta(const Amal& amal, const std::wstring& matn, bool deltalarniUzat,
                       std::wstring& xato);

    void ishla(Amal amal, std::wstring matn);
};

std::wstring AmalIshi::Ichki::bitta(const Amal& amal, const std::wstring& matn, bool deltalarniUzat,
                                    std::wstring& xato) {
    LLMSorov s = asos;
    s.system = amal.system;
    s.user = matn;

    std::wstring yigilgan;
    xato = llmOqim(
        s,
        [&](const std::wstring& b) {
            yigilgan += b;
            if (deltalarniUzat) delta(b);
        },
        [&] { return bekor.load(); });

    // Bekor qilish tarmoq xatosi emas — foydalanuvchi oʻzi bosgan tugma
    // uchun «Internetga ulanish yoʻq» deb yozish chalgʻitadi.
    if (bekor.load()) xato = L"Bekor qilindi.";
    return yigilgan;
}

void AmalIshi::Ichki::ishla(Amal amal, std::wstring matn) {
    const std::vector<std::wstring> bolaklar = paragrafBolaklari(matn, kBolakSozLimiti);

    if (bolaklar.empty()) {
        tugat();
        xatoBer(L"Matn boʻsh.");
        return;
    }

    std::wstring xato;
    std::wstring natija;

    if (bolaklar.size() == 1) {
        natija = bitta(amal, bolaklar[0], true, xato);
    } else if (amal.yiguvchi) {
        // Yigʻuvchi amallar: har bir boʻlak qisqartiriladi, soʻng yakuniy soʻrov.
        const Amal oraliqAmal{"oraliq", L"", false,
                              qoida(L"Vazifang: matnning asosiy mazmunini qisqacha yozib ber.\n"
                                    L"Bu keyingi bosqichda boshqa qismlar bilan birlashtiriladi.")};

        std::wstring oraliq;
        for (const auto& b : bolaklar) {
            const std::wstring q = bitta(oraliqAmal, b, false, xato);
            if (!xato.empty()) break;
            if (!oraliq.empty()) oraliq += L"\n\n";
            oraliq += q;
        }
        if (xato.empty()) natija = bitta(amal, oraliq, true, xato);
    } else {
        // Oʻzgartiruvchi amallar: har bir boʻlak alohida, natijalar ulanadi.
        for (size_t i = 0; i < bolaklar.size(); ++i) {
            if (i > 0) delta(L"\n\n");
            const std::wstring q = bitta(amal, bolaklar[i], true, xato);
            if (!xato.empty()) break;
            if (!natija.empty()) natija += L"\n\n";
            natija += q;
        }
    }

    tugat();
    if (!xato.empty()) {
        xatoBer(xato);
        return;
    }
    tayyor(natija);
}

AmalIshi::AmalIshi() : ichki_(std::make_shared<Ichki>()) {}
AmalIshi::~AmalIshi() {
    // Ketayotgan oqim `shared_ptr` ushlab turadi va biroz vaqt yashaydi.
    // Qayta chaqiruvlarni uzamiz — aks holda u yoʻq qilingan tabga yozardi.
    bekorQil();
    ichki_->uzil();
}

bool AmalIshi::ketyaptimi() const { return ichki_->ketyapti.load(); }

void AmalIshi::bekorQil() { ichki_->bekor.store(true); }

bool AmalIshi::bajar(const Amal& amal, const std::wstring& matn,
                     std::function<void(const std::wstring&)> onDelta,
                     std::function<void(const std::wstring&)> onTayyor,
                     std::function<void(const std::wstring&)> onXato) {
    if (ichki_->ketyapti.load()) {
        if (onXato) onXato(L"Amal allaqachon ketmoqda.");
        return false;
    }

    const Provayder* p = LLMSozlama::tanlangan();
    if (!p || !LLMSozlama::sozlanganmi()) {
        if (onXato) onXato(L"API kalit kiritilmagan. Sozlamalar → LLM boʻlimiga oʻting.");
        return false;
    }

    ichki_->asos = LLMSorov{};
    ichki_->asos.provayder = *p;
    ichki_->asos.baseURL = LLMSozlama::baseURL();
    ichki_->asos.model = LLMSozlama::model();
    ichki_->asos.kalit = LLMSozlama::joriyKalit();

    ichki_->onDelta = std::move(onDelta);
    ichki_->onTayyor = std::move(onTayyor);
    ichki_->onXato = std::move(onXato);
    ichki_->bekor.store(false);
    ichki_->ketyapti.store(true);

    // `shared_ptr` nusxasi oqimga beriladi — ish oʻzini oʻzi tirik saqlaydi
    // (`ish.cpp` dagi bilan bir xil naqsh).
    auto ichki = ichki_;
    std::thread([ichki, amal, matn] { ichki->ishla(amal, matn); }).detach();
    return true;
}

}  // namespace rubai
