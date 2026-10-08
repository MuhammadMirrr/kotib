// Segmentlardan oʻqishga qulay matn yasaydi — macOS'dagi `src/text_format.swift`
// ning ekvivalenti.
//
// Ikkala platforma BIR XIL natija berishi shart: bu ilovaning standart
// koʻrinishi va foydalanuvchi macOS'da bir xil, Windows'da boshqacha matn
// olsa — bu bitta ilova emas, ikkita boshqa ilova boʻlib qoladi. Shuning
// uchun chegaralar (pauza uzunligi, paragraf uzunligi) va apostrof qoidasi
// aynan koʻchirilgan, "yaxshilash" kiritilmagan.
//
// Fayl UI'dan mustaqil: faqat matn bilan ishlaydi.
#pragma once

#include <string>
#include <vector>

namespace rubai {

// Whisper qaytargan bitta segment. Vaqtlar soniyada.
struct Segment {
    double t0 = 0;
    double t1 = 0;
    std::wstring matn;
};

// Apostrof uslubi.
//   Standart — oʻzbek lotin meʼyori: ʻ (U+02BB) va ʼ (U+02BC)
//   Oddiy    — ASCII ' ; boshqa dasturlarga nusxalashda muammosiz
enum class Apostrof { Standart, Oddiy };

// Barcha apostrof variantlarini bitta uslubga keltiradi.
// `o` yoki `g` dan keyin kelsa — harf modifikatori (oʻ, gʻ), aks holda
// tutuq belgisi (ʼ). Whisper bu belgilarni aralash chiqaradi.
std::wstring apostrofniBirxillashtir(const std::wstring& s, Apostrof uslub);

// Diktovka natijasini foydalanuvchiga berishdan oldingi YAGONA qadam: tarix
// va kiritish shu natijani oladi. Ilgari diktovka whisper matnini toʻgʻridan-
// toʻgʻri kiritardi — ASCII ' qolardi va «Oddiy apostrof» sozlamasi diktovkaga
// taʼsir qilmasdi. macOS'dagi egizagi — `text_format.swift` dagi
// `matnniTayyorla`; ikkalasi korpus ustida solishtiriladi
// (`win/tests/mac/taqqoslash/`). Boʻshliqni transkripsiya qatlami kesadi.
std::wstring matnniTayyorla(const std::wstring& xom, Apostrof apostrof);

// Whisper takrorlanish halqasini qisqartiradi: 1–6 soʻzli ibora ketma-ket
// kamida 4 marta kelsa, bittasi qoladi («oʻzbekiston respublikasi» ×15 →
// bir marta). Buzuq audioda model baʼzan hamma haroratda ham shunday chiqaradi
// (S23 oʻlchovi). Takror topilmasa satr AYNAN qaytariladi; topilsa soʻzlar
// bitta boʻshliq bilan qayta yigʻiladi. macOS egizagi — `text_format.swift`.
std::wstring takrorniQisqartir(const std::wstring& s);
inline constexpr size_t kTakrorIboraMax = 6;
inline constexpr size_t kTakrorMin = 4;

// Paragraf boshi va `.`, `!`, `?` dan keyingi birinchi harfni bosh harf qiladi.
std::wstring jumlaBoshiniKattalashtir(const std::wstring& s);

// Segmentlarni oʻzgartirmasdan birlashtiradi — «Asl» koʻrinishi uchun.
std::wstring xomMatn(const std::vector<Segment>& segmentlar);

// Standart koʻrinish: paragraflarga boʻlingan, tozalangan, bosh harfli matn.
std::wstring chiroyliMatn(const std::vector<Segment>& segmentlar, Apostrof apostrof);

}  // namespace rubai
