// Kichik JSON oʻqiruvchi va yozuvchi.
//
// Nega tashqi kutubxona emas: loyihada paket menejeri yoʻq va bu ataylab
// (`AGENTS.md`). Bizga kerak boʻlgani — uch xil faylni oʻqish-yozish
// (segmentlar, hujjat maʼlumoti, diktovka tarixi), ularning tuzilishi esa
// bizga toʻliq maʼlum. Shuning uchun mingqator kutubxona oʻrniga shu fayl.
//
// macOS tomonida bu ish `JSONEncoder`/`JSONDecoder` bilan bajariladi.
// Fayl formati bir xil boʻlishi uchun yozuv shakli ham bir xil saqlangan.
//
// Cheklovlar (ataylab): ilmiy son yozuvi (1e5) oʻqiladi, lekin yozilmaydi;
// surrogat juftliklar (😀) oʻqilganda toʻgʻri birlashtiriladi.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace rubai {

class Json {
public:
    enum class Tur { Yoq, Null, Mantiq, Son, Satr, Royxat, Obyekt };

    // UTF-8 matndan oʻqiydi. Xato boʻlsa turi `Yoq` boʻlgan qiymat qaytadi —
    // istisno tashlanmaydi, chunki buzuq fayl ilovani toʻxtatmasligi kerak.
    static Json ajrat(const std::string& utf8);

    Tur tur() const { return tur_; }
    bool bormi() const { return tur_ != Tur::Yoq; }

    bool mantiq(bool sukut = false) const;
    double son(double sukut = 0) const;
    std::wstring satr() const;

    // Obyekt maydoni. Yoʻq boʻlsa turi `Yoq` boʻlgan qiymat qaytadi, shuning
    // uchun `j["a"]["b"].satr()` xavfsiz.
    const Json& operator[](const std::string& kalit) const;

    // Roʻyxat elementi.
    const Json& operator[](size_t i) const;
    size_t hajmi() const;

    // Obyekt kalitlari (alifbo tartibida). Obyekt boʻlmasa — boʻsh roʻyxat.
    // Kalitlari oldindan nomaʼlum obyekt uchun (masalan yangilanish
    // manifestidagi `fayllar` — arxitektura → fayl).
    std::vector<std::string> kalitlar() const;

private:
    Tur tur_ = Tur::Yoq;
    bool mantiq_ = false;
    double son_ = 0;
    std::wstring satr_;
    std::vector<Json> royxat_;
    std::map<std::string, Json> obyekt_;

    friend class JsonOquvchi;
};

// Yozuvchi. Ketma-ket chaqiriladi va oxirida `matn()` beriladi.
//
//   JsonYozuvchi y;
//   y.royxatBoshla();
//     y.obyektBoshla();
//       y.kalit("t0"); y.son(1.5);
//       y.kalit("matn"); y.satr(L"salom");
//     y.obyektTugat();
//   y.royxatTugat();
class JsonYozuvchi {
public:
    void obyektBoshla();
    void obyektTugat();
    void royxatBoshla();
    void royxatTugat();

    void kalit(const std::string& k);
    void satr(const std::wstring& s);
    void son(double x);
    void butun(long long x);
    void mantiq(bool b);
    void nul();

    const std::string& matn() const { return chiqish_; }

private:
    void vergul();
    std::string chiqish_;
    // Har bir ochiq qavs uchun: unda allaqachon element bormi. Vergulni
    // toʻgʻri joyga qoʻyish uchun kerak.
    std::vector<bool> bor_;
    bool kalitKutilyapti_ = false;
};

// `x` ni nuqtadan keyin `kasr` xona bilan yozadi («1.23»). Manfiy — 0.
// Lokalga bogʻliq EMAS (`std::to_chars`): `snprintf("%.2f")` ru/uz lokalida
// «1,23» beradi va JSON buziladi — tarjimon `setlocale` chaqirgach aynan
// shunday boʻlardi (barqarorlik E2).
std::string kasrSon(double x, int kasr);

}  // namespace rubai
