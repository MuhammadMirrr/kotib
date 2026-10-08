// `json.cpp` testlari.
//
// Nega bu yerda test koʻp: JSON oʻqiruvchi qoʻlda yozilgan va u foydalanuvchi
// maʼlumotini (hujjatlar, diktovka tarixi) oʻqiydi. Buzuq faylda u ilovani
// yiqitmasligi, notoʻgʻri maʼlumot qaytarmasligi kerak — buni faqat test
// kafolatlaydi.
#include "../core/json.h"

#include <clocale>
#include <cmath>
#include <string>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan);
}  // namespace kotib_test

using kotib_test::tekshir;
using kotib_test::tengmi;
using namespace rubai;

namespace {

// Kasr ajratgichi vergul boʻlgan lokalni oʻrnatadi (ru/uz). Topilmasa false.
// Nomlar platformaga qarab har xil: macOS «ru_RU.UTF-8», Windows UCRT «ru-RU».
bool vergulliLokal() {
    for (const char* nom :
         {"ru_RU.UTF-8", "ru-RU", "uz_UZ.UTF-8", "uz-Latn-UZ", "de_DE.UTF-8", "de-DE"}) {
        if (std::setlocale(LC_ALL, nom) && std::localeconv()->decimal_point[0] == ',') {
            return true;
        }
    }
    return false;
}

void lokalTestlari() {
    // Barqarorlik E2: tarjimon `setlocale` chaqirgach ru/uz Windows'da kasr
    // sonlar «12,5» boʻlib yozilar va «12.5» → 12 oʻqilardi.
    const bool bor = vergulliLokal();
    tekshir(L"lokal: vergulli lokal oʻrnatildi", bor);
    if (bor) {
        JsonYozuvchi y;
        y.royxatBoshla();
        y.son(12.5);
        y.son(-0.25);
        y.son(1e-7);
        y.son(3);
        y.royxatTugat();
        tengmi(L"lokal: yozish nuqta bilan", std::wstring(y.matn().begin(), y.matn().end()),
               L"[12.5,-0.25,1e-07,3]");
        const Json j = Json::ajrat(y.matn());
        tekshir(L"lokal: aylanma", j.hajmi() == 4 && j[0].son() == 12.5 && j[1].son() == -0.25 &&
                                       j[2].son() == 1e-7 && j[3].son() == 3);
        tekshir(L"lokal: «12.5» → 12.5", Json::ajrat("12.5").son() == 12.5);
        tengmi(L"lokal: kasrSon",
               std::wstring(L"") + (kasrSon(1.234, 2) == "1.23" ? L"1.23" : L"xato"), L"1.23");
    }
    std::setlocale(LC_ALL, "C");
    tengmi(L"kasrSon manfiy → 0", kasrSon(-3, 2) == "0.00" ? L"ok" : L"xato", L"ok");
    tengmi(L"kasrSon NaN → 0", kasrSon(NAN, 1) == "0.0" ? L"ok" : L"xato", L"ok");
    tekshir(L"son: buzuq «1-2» rad", !Json::ajrat("1-2").bormi());
    tekshir(L"son: «+5» (eski yumshoqlik)", Json::ajrat("+5").son() == 5);
    tekshir(L"son: oʻta katta rad", !Json::ajrat("1e999").bormi());
}

}  // namespace

void jsonTestlari() {
    lokalTestlari();
    // ---- Oddiy qiymatlar ---------------------------------------------------
    tekshir(L"son oʻqiladi", Json::ajrat("42").son() == 42);
    tekshir(L"manfiy son", Json::ajrat("-7").son() == -7);
    tekshir(L"kasr son", Json::ajrat("1.5").son() == 1.5);
    tekshir(L"ilmiy yozuv", Json::ajrat("1e3").son() == 1000);
    tekshir(L"true", Json::ajrat("true").mantiq());
    tekshir(L"false", !Json::ajrat("false").mantiq(true));
    tekshir(L"null turi", Json::ajrat("null").tur() == Json::Tur::Null);
    tengmi(L"satr", Json::ajrat("\"salom\"").satr(), L"salom");

    // ---- Qochish belgilari -------------------------------------------------
    tengmi(L"yangi qator", Json::ajrat("\"a\\nb\"").satr(), L"a\nb");
    tengmi(L"tirnoq", Json::ajrat("\"a\\\"b\"").satr(), L"a\"b");
    tengmi(L"teskari chiziq", Json::ajrat("\"a\\\\b\"").satr(), L"a\\b");
    tengmi(L"tab", Json::ajrat("\"a\\tb\"").satr(), L"a\tb");

    // \uXXXX — oʻzbek apostrofi aynan shunday kelishi mumkin.
    tengmi(L"unicode kod", Json::ajrat("\"o\\u02BBzbek\"").satr(), L"oʻzbek");

    // Surrogat juftlik (emoji). Notoʻgʻri birlashtirilsa matn buziladi.
    //
    // Faqat Windows'da: u yerda `wchar_t` 16-bitli va `\uD83D\uDE00`
    // aynan ikkita kod birligi boʻlib saqlanadi. macOS sinov quvurida
    // (`win/tests/mac/sinov.sh`) `wchar_t` 32-bitli va bu tekshiruv
    // maʼnosini yoʻqotadi — u yerda VM'dagi test javob beradi.
#ifdef _WIN32
    tengmi(L"surrogat juftlik", Json::ajrat("\"\\uD83D\\uDE00\"").satr(), L"😀");
#endif

    // ---- Obyekt va roʻyxat -------------------------------------------------
    {
        const Json j = Json::ajrat("{\"a\":1,\"b\":\"x\",\"c\":[1,2,3]}");
        tekshir(L"obyekt turi", j.tur() == Json::Tur::Obyekt);
        tekshir(L"maydon soni", j.hajmi() == 3);
        tekshir(L"son maydoni", j["a"].son() == 1);
        tengmi(L"satr maydoni", j["b"].satr(), L"x");
        tekshir(L"ichki roʻyxat", j["c"].hajmi() == 3);
        tekshir(L"roʻyxat elementi", j["c"][1].son() == 2);
    }

    // Yoʻq maydon — xavfsiz boʻsh qiymat qaytarishi kerak, qulamasligi kerak.
    {
        const Json j = Json::ajrat("{\"a\":1}");
        tekshir(L"yoʻq maydon boʻsh", !j["yoq"].bormi());
        tengmi(L"yoʻq maydon satri boʻsh", j["yoq"].satr(), L"");
        tekshir(L"yoʻq maydon soni sukut", j["yoq"].son(-1) == -1);
        tekshir(L"zanjir xavfsiz", !j["yoq"]["yana"]["chuqur"].bormi());
        tekshir(L"chegaradan tashqari indeks", !j[99].bormi());
    }

    // Boʻsh obyekt va roʻyxat.
    tekshir(L"boʻsh obyekt", Json::ajrat("{}").tur() == Json::Tur::Obyekt);
    tekshir(L"boʻsh roʻyxat", Json::ajrat("[]").hajmi() == 0);

    // Boʻshliqlar.
    tekshir(L"boʻshliqli json", Json::ajrat("  {\n  \"a\" : 1 \n}  ")["a"].son() == 1);

    // ---- Buzuq kirish ------------------------------------------------------
    // Hech biri qulamasligi va `bormi()` false qaytarishi kerak.
    tekshir(L"boʻsh satr", !Json::ajrat("").bormi());
    tekshir(L"yopilmagan qavs", !Json::ajrat("{\"a\":1").bormi());
    tekshir(L"yopilmagan tirnoq", !Json::ajrat("\"abc").bormi());
    tekshir(L"ortiqcha vergul", !Json::ajrat("[1,2,]").bormi());
    tekshir(L"kalitsiz obyekt", !Json::ajrat("{1:2}").bormi());
    tekshir(L"axlat", !Json::ajrat("nimadir").bormi());
    tekshir(L"oxirida ortiqcha", !Json::ajrat("{} qoldiq").bormi());

    // Juda chuqur ichma-ichlik stekni toʻldirmasligi kerak.
    {
        std::string chuqur(200, '[');
        tekshir(L"chuqur ichma-ichlik yiqitmaydi", !Json::ajrat(chuqur).bormi());
    }

    // ---- Yozish ------------------------------------------------------------
    {
        JsonYozuvchi y;
        y.obyektBoshla();
        y.kalit("a");
        y.butun(1);
        y.kalit("b");
        y.satr(L"x\"y");
        y.obyektTugat();
        tekshir(L"yozilgan obyekt", y.matn() == "{\"a\":1,\"b\":\"x\\\"y\"}");
    }
    {
        JsonYozuvchi y;
        y.royxatBoshla();
        y.son(1.5);
        y.son(2);  // butun son butun koʻrinishda yozilsin
        y.mantiq(true);
        y.nul();
        y.royxatTugat();
        tekshir(L"yozilgan roʻyxat", y.matn() == "[1.5,2,true,null]");
    }

    // Yozilganni qayta oʻqish — eng muhim kafolat.
    {
        JsonYozuvchi y;
        y.royxatBoshla();
        y.obyektBoshla();
        y.kalit("matn");
        y.satr(L"Oʻzbek tili — saʼnat.\nIkkinchi qator");
        y.kalit("t0");
        y.son(1.25);
        y.obyektTugat();
        y.royxatTugat();

        const Json j = Json::ajrat(y.matn());
        tengmi(L"aylanma: matn", j[0]["matn"].satr(), L"Oʻzbek tili — saʼnat.\nIkkinchi qator");
        tekshir(L"aylanma: son", j[0]["t0"].son() == 1.25);
    }
}
