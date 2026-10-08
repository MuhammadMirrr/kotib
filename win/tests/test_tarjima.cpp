// Tarjima qatlamining sof mantiq testlari: matn boʻluvchi va tillar jadvali.
//
// Nega bu muhim: model bir jumlani eng yaxshi tarjima qiladi, lekin abzatslar,
// havolalar va emoji foydalanuvchi matnining bir qismi. Ular yoʻqolsa —
// natija buzilgan koʻrinadi. Holatlar macOS'dagi `tests/test_matn_boluvchi.swift`
// va `tests/test_tillar.swift` dan olingan: ikkala platforma AYNAN bir xil
// natija berishi kerak.
#include "../core/matn_boluvchi.h"
#include "../core/tillar.h"

#include <set>
#include <string>
#include <vector>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan);
}  // namespace kotib_test

using kotib_test::tekshir;
using kotib_test::tengmi;
using namespace rubai;

namespace {

void sonTeng(const std::wstring& nom, size_t olingan, size_t kutilgan) {
    tengmi(nom, std::to_wstring(olingan), std::to_wstring(kutilgan));
}

void jumlaTeng(const std::wstring& nom, const Bolak& b, const std::wstring& matn,
               const std::wstring& qoshimcha) {
    tengmi(nom + L" (tur)", b.tur == Bolak::Tur::Jumla ? L"jumla" : L"xom", L"jumla");
    tengmi(nom + L" (matn)", b.matn, matn);
    tengmi(nom + L" (qoʻshimcha)", b.qoshimcha, qoshimcha);
}

void xomTeng(const std::wstring& nom, const Bolak& b, const std::wstring& matn) {
    tengmi(nom + L" (tur)", b.tur == Bolak::Tur::Xom ? L"xom" : L"jumla", L"xom");
    tengmi(nom + L" (matn)", b.matn, matn);
}

}  // namespace

void tarjimaTestlari() {
    using namespace rubai::MatnBoluvchi;

    // ---- Qatorlar va jumlalar ----

    {
        const auto b = bol(L"Salom. Qalaysiz? Yaxshi!");
        sonTeng(L"bitta qator", b.size(), 1);
        sonTeng(L"uchta jumla", b[0].size(), 3);
        jumlaTeng(L"birinchi", b[0][0], L"Salom.", L"");
        jumlaTeng(L"ikkinchi", b[0][1], L"Qalaysiz?", L"");
        jumlaTeng(L"uchinchi", b[0][2], L"Yaxshi!", L"");
    }

    {
        const auto b = bol(L"Birinchi.\n\nIkkinchi.");
        sonTeng(L"uchta qator", b.size(), 3);
        sonTeng(L"oʻrtadagi qator boʻsh", b[1].size(), 0);
    }

    {
        const auto b = bol(L"Birinchi.\nIkkinchi.");
        sonTeng(L"ikkita qator", b.size(), 2);
        tengmi(L"yigʻilganda qator qaytadi", yig(b, {L"Первое.", L"Второе."}), L"Первое.\nВторое.");
    }

    // Windows qator oxiri — `\r\n`. `\r` matnga qoʻshilib ketmasligi kerak.
    {
        const auto b = bol(L"Birinchi.\r\nIkkinchi.");
        sonTeng(L"CRLF: ikkita qator", b.size(), 2);
        jumlaTeng(L"CRLF: birinchi", b[0][0], L"Birinchi.", L"");
    }

    // ---- Belgilar ----

    {
        const auto b = bol(L"Mustaqillik — bu erkinlik.");
        jumlaTeng(L"uzun tire oddiy tirega almashdi", b[0][0], L"Mustaqillik - bu erkinlik.", L"");
    }

    {
        const auto b = bol(L"Bayramingiz muborak! 🇺🇿");
        sonTeng(L"emoji bilan bitta boʻlak", b[0].size(), 1);
        jumlaTeng(L"emoji qoʻshimchaga oʻtdi", b[0][0], L"Bayramingiz muborak!", L" 🇺🇿");
    }

    {
        const auto b = bol(L"🇺🇿 🎉");
        sonTeng(L"faqat emoji: bitta boʻlak", b[0].size(), 1);
        xomTeng(L"faqat emoji xom qoldi", b[0][0], L"🇺🇿 🎉");
    }

    // ---- Yigʻish ----

    {
        const auto b = bol(L"Salom.\n\nXayr! 🇺🇿");
        tengmi(L"yigʻish qatorlarni saqlaydi", yig(b, {L"Привет.", L"Пока!"}),
               L"Привет.\n\nПока! 🇺🇿");
    }

    {
        const auto b = bol(L"Salom. Xayr.");
        tengmi(L"tarjima yetmasa asl matn qoladi", yig(b, {L"Привет."}), L"Привет. Xayr.");
    }

    // ---- Boʻsh kirish ----

    sonTeng(L"boʻsh matn", bol(L"").size(), 0);
    sonTeng(L"faqat boʻshliq", bol(L"   ").size(), 0);

    {
        const auto b = bol(L"Salom.\n\n🇺🇿\n\nXayr.");
        sonTeng(L"tarjima qilinadigan jumlalar sanaladi", jumlalarSoni(b), 2);
    }
    sonTeng(L"boʻsh matnda nol jumla", jumlalarSoni(bol(L"")), 0);

    // ---- Himoyalangan boʻlaklar ----
    //
    // Havola, email va handle modelga UMUMAN berilmaydi — u ularni tarjima
    // qilishga urinadi va buzadi. Asl muammo: `t.me/dr_azamoff` →
    // `["t.", "me/dr_azamoff"]` boʻlib ketardi va `t.` modelga borib `п.`
    // boʻlib qaytardi.

    {
        const auto b = bol(L"t.me/dr_azamoff");
        sonTeng(L"URL nuqtasi jumlani kesmaydi", b[0].size(), 1);
        xomTeng(L"yolgʻiz havola xom qoldi", b[0][0], L"t.me/dr_azamoff");
        tengmi(L"havola yigʻilganda oʻzgarmaydi", yig(b, {}), L"t.me/dr_azamoff");
    }

    {
        const auto b = bol(L"Manba: t.me/dr_azamoff");
        sonTeng(L"jumla ichidagi havola ajratildi", b[0].size(), 2);
        jumlaTeng(L"matn qismi", b[0][0], L"Manba:", L"");
        xomTeng(L"havola qismi", b[0][1], L"t.me/dr_azamoff");
        tengmi(L"havolali jumla yigʻildi", yig(b, {L"Источник:"}), L"Источник: t.me/dr_azamoff");
        sonTeng(L"havola jumlalar sanogʻiga kirmaydi", jumlalarSoni(b), 1);
    }

    {
        const auto b = bol(L"Yuklab oling: https://cdn.mirqobilov.com/dl/x.tar.gz");
        xomTeng(L"toʻliq URL himoyalandi", b[0].back(), L"https://cdn.mirqobilov.com/dl/x.tar.gz");
    }

    {
        const auto b = bol(L"Xat: user@example.com");
        xomTeng(L"email himoyalandi", b[0].back(), L"user@example.com");
    }

    {
        const auto b = bol(L"Obuna: @dr_azamoff");
        xomTeng(L"handle himoyalandi", b[0].back(), L"@dr_azamoff");
    }

    {
        const auto b = bol(L"Pi soni 3.14 ga teng.");
        sonTeng(L"oddiy son himoyalanmaydi", b[0].size(), 1);
    }

    // ---- Jumla chegaralari ----

    {
        const auto b = bol(L"Pi soni 3.14 ga teng. Ikkinchi gap.");
        sonTeng(L"oʻnlik son jumlani kesmaydi", b[0].size(), 2);
        jumlaTeng(L"birinchi jumla butun", b[0][0], L"Pi soni 3.14 ga teng.", L"");
    }

    {
        const auto b = bol(L"Versiya v1.0 chiqdi.");
        sonTeng(L"versiya raqami jumlani kesmaydi", b[0].size(), 1);
    }

    {
        const auto b = bol(L"这是第一句。这是第二句。");
        sonTeng(L"xitoy nuqtasi chegara boʻladi", b[0].size(), 2);
    }

    {
        const auto b = bol(L"هذه جملة؟ وهذه أخرى.");
        sonTeng(L"arab savol belgisi chegara boʻladi", b[0].size(), 2);
    }

    // Keyingi soʻz kichik harf bilan boshlansa — jumla tugamagan. ICU shu
    // qoidani biladi; zaxira qoida ham bilishi kerak, aks holda ogʻzaki
    // nutq transkripti (diktovka chiqishi aynan shunday) mayda-mayda
    // boʻlinib, tarjima sifati tushardi.
    {
        const auto b = bol(L"E-e, anavi... nima edi... ha, hujjat.");
        sonTeng(L"uch nuqtadan keyin kichik harf — jumla tugamaydi", b[0].size(), 1);
    }
    {
        const auto b = bol(L"Prof. Azamov aytdiki, ish tugadi. Lekin u yerda yoʻq edi.");
        sonTeng(L"bosh harf — jumla chegarasi", b[0].size(), 3);
    }
    {
        const auto b = bol(L"Он сказал: «Всё готово». Мы начали работу.");
        sonTeng(L"kirill: yopuvchi qoʻshtirnoq chegara ichida", b[0].size(), 2);
    }

    // ---- Chiqish normalizatsiyasi ----

    tengmi(L"<unk> oʻrtada", tozala(L"Salom <unk> dunyo"), L"Salom dunyo");
    tengmi(L"<unk> oxirida", tozala(L"Salom! <unk>"), L"Salom!");
    tengmi(L"toza matnga tegilmaydi", tozala(L"Salom dunyo"), L"Salom dunyo");

    tengmi(L"vergul oldidagi boʻshliq", tozala(L"У женщин , принимающих"),
           L"У женщин, принимающих");
    tengmi(L"nuqta oldidagi boʻshliq", tozala(L"Konец ."), L"Konец.");
    tengmi(L"ikki nuqta", tozala(L"Natija : yaxshi"), L"Natija: yaxshi");
    tengmi(L"qavs ichidagi boʻshliq", tozala(L"matn ( izoh )"), L"matn (izoh)");
    tengmi(L"uzun tire tiklanadi", tozala(L"Альцгеймер - это болезнь"),
           L"Альцгеймер — это болезнь");
    tengmi(L"qoʻshma soʻzga tegilmaydi", tozala(L"ijtimoiy-iqtisodiy"), L"ijtimoiy-iqtisodiy");

    // ---- Tillar jadvali ----

    const auto& tillar = Tillar::hammasi();
    sonTeng(L"202 ta til", tillar.size(), 202);

    {
        std::set<std::string> kodlar;
        std::set<std::wstring> nomlar;
        bool boshNom = false;
        for (const auto& t : tillar) {
            kodlar.insert(t.nllb);
            nomlar.insert(t.nom);
            if (t.nom.empty()) boshNom = true;
        }
        sonTeng(L"kodlar takrorlanmaydi", kodlar.size(), 202);
        // Nom takrorlanmasligi SHART: roʻyxatda ikkita bir xil band boʻlsa
        // foydalanuvchi qaysi biri nima ekanini bilmaydi.
        sonTeng(L"nomlar takrorlanmaydi", nomlar.size(), 202);
        tekshir(L"boʻsh nom yoʻq", !boshNom);
    }

    tekshir(L"oʻzbekcha topiladi", Tillar::top(Tillar::kUz) != nullptr);
    tekshir(L"ruscha topiladi", Tillar::top(Tillar::kRu) != nullptr);
    tekshir(L"inglizcha topiladi", Tillar::top(Tillar::kEn) != nullptr);
    tekshir(L"yoʻq kod topilmaydi", Tillar::top("yoq_Kod") == nullptr);

    if (const Til* uz = Tillar::top(Tillar::kUz)) {
        // `kod()` va `yozuv()` VAQTINCHALIK satr qaytaradi — ikkita alohida
        // chaqiruvdan iterator olib boʻlmaydi (ular boshqa-boshqa obyektga
        // tegishli boʻladi va uzunlik axlat chiqadi).
        const std::string kod = uz->kod();
        const std::string yozuv = uz->yozuv();
        tengmi(L"uzn kodi", std::wstring(kod.begin(), kod.end()), L"uzn");
        tengmi(L"uzn yozuvi", std::wstring(yozuv.begin(), yozuv.end()), L"Latn");
    }

    // Apostrof meʼyori: nomlarda ASCII `'` boʻlmasligi kerak — jadval
    // macOS'dan koʻchirilgan va u yerda `apostrofniBirxillashtir` dan oʻtgan.
    {
        bool asciiApostrof = false;
        for (const auto& t : tillar) {
            if (t.nom.find(L'\'') != std::wstring::npos) {
                asciiApostrof = true;
                break;
            }
        }
        tekshir(L"nomlarda ASCII apostrof yoʻq", !asciiApostrof);
    }
}
