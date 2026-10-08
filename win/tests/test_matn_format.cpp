// `matn_format.cpp` testlari.
//
// Holatlar macOS'dagi `tests/test_text_format.swift` dan olingan — maqsad
// aynan shu: ikkala platforma BIR XIL matn chiqarishini kafolatlash.
// Bu yerda test qoʻshsangiz, macOS tomonida ham qoʻshing.
#include "../core/matn_format.h"

#include <string>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan);
}  // namespace kotib_test

using kotib_test::tekshir;
using kotib_test::tengmi;
using namespace rubai;

void matnFormatTestlari() {
    // ---- Takrorlanish halqasi (S23) -------------------------------------------
    tengmi(L"halqa — ikki soʻzli ibora ×6 → bir marta",
           takrorniQisqartir(
               L"bu oʻzbekiston respublikasi oʻzbekiston respublikasi oʻzbekiston respublikasi "
               L"oʻzbekiston respublikasi oʻzbekiston respublikasi oʻzbekiston respublikasi"),
           L"bu oʻzbekiston respublikasi");
    tengmi(L"halqa — bitta soʻz ×8, oldingi soʻzlar qoladi",
           takrorniQisqartir(L"qaynon shirin toʻla toʻla toʻla toʻla toʻla toʻla toʻla toʻla"),
           L"qaynon shirin toʻla");
    tengmi(L"halqa — toʻrt soʻzli ibora, keyin davom etadi",
           takrorniQisqartir(L"oʻn toʻrt yuz ellik ming toʻrt yuz ellik ming toʻrt yuz ellik ming "
                             L"toʻrt yuz ellik ming oʻn olti"),
           L"oʻn toʻrt yuz ellik ming oʻn olti");
    tengmi(L"uch marta — tegilmaydi", takrorniQisqartir(L"ha ha ha keldik"), L"ha ha ha keldik");
    tengmi(L"ikki marta — tegilmaydi (oddiy nutq)", takrorniQisqartir(L"toʻla toʻla idish"),
           L"toʻla toʻla idish");
    tengmi(L"takrorsiz matn AYNAN qaytadi (qoʻsh boʻshliq ham)",
           takrorniQisqartir(L"Salom,  dunyo!  Bugun"), L"Salom,  dunyo!  Bugun");
    tengmi(L"tinish belgisi bilan farq qiluvchi soʻz — boshqa soʻz",
           takrorniQisqartir(L"ha ha ha ha."), L"ha ha ha ha.");
    tengmi(L"boʻsh satr", takrorniQisqartir(L""), L"");
    tengmi(L"matnniTayyorla ham qisqartiradi (apostrofdan oldin)",
           matnniTayyorla(L"men o'zbek o'zbek o'zbek o'zbek", Apostrof::Standart), L"men oʻzbek");

    // ---- Apostrof ----------------------------------------------------------
    // Eng muhim qoida: `o`/`g` dan keyin ʻ (U+02BB), boshqa joyda ʼ (U+02BC).
    tengmi(L"oʻ — burilgan vergul", apostrofniBirxillashtir(L"o'zbek", Apostrof::Standart),
           L"oʻzbek");
    tengmi(L"gʻ — burilgan vergul", apostrofniBirxillashtir(L"g'alaba", Apostrof::Standart),
           L"gʻalaba");
    tengmi(L"tutuq belgisi", apostrofniBirxillashtir(L"sa'nat", Apostrof::Standart), L"saʼnat");
    tengmi(L"bosh harfdan keyin ham", apostrofniBirxillashtir(L"O'zbekiston", Apostrof::Standart),
           L"Oʻzbekiston");

    // Whisper turli apostroflarni aralash chiqaradi — hammasi bir xil boʻlsin.
    tengmi(L"qiyshiq apostrof", apostrofniBirxillashtir(L"o’zbek", Apostrof::Standart), L"oʻzbek");
    tengmi(L"teskari tirnoq", apostrofniBirxillashtir(L"o`zbek", Apostrof::Standart), L"oʻzbek");
    tengmi(L"akut belgisi", apostrofniBirxillashtir(L"sa´nat", Apostrof::Standart), L"saʼnat");
    tengmi(L"allaqachon toʻgʻri boʻlsa oʻzgarmaydi",
           apostrofniBirxillashtir(L"oʻzbek", Apostrof::Standart), L"oʻzbek");

    tengmi(L"oddiy uslub — ASCII", apostrofniBirxillashtir(L"oʻzbek saʼnat", Apostrof::Oddiy),
           L"o'zbek sa'nat");

    tengmi(L"apostrofsiz matn tegilmaydi",
           apostrofniBirxillashtir(L"salom dunyo", Apostrof::Standart), L"salom dunyo");
    tengmi(L"boʻsh satr", apostrofniBirxillashtir(L"", Apostrof::Standart), L"");

    // ---- Bosh harf ---------------------------------------------------------
    tengmi(L"birinchi harf katta", jumlaBoshiniKattalashtir(L"salom"), L"Salom");
    tengmi(L"nuqtadan keyin katta", jumlaBoshiniKattalashtir(L"salom. dunyo"), L"Salom. Dunyo");
    tengmi(L"soʻroq belgisidan keyin", jumlaBoshiniKattalashtir(L"nima? bilmayman"),
           L"Nima? Bilmayman");
    tengmi(L"undov belgisidan keyin", jumlaBoshiniKattalashtir(L"voy! qara"), L"Voy! Qara");
    tengmi(L"yangi qatordan keyin", jumlaBoshiniKattalashtir(L"bir\nikki"), L"Bir\nIkki");
    tengmi(L"vergul bosh harf yasamaydi", jumlaBoshiniKattalashtir(L"salom, dunyo"),
           L"Salom, dunyo");

    // ---- Xom matn ----------------------------------------------------------
    {
        std::vector<Segment> segs = {{0, 1, L"birinchi"}, {1, 2, L"ikkinchi"}};
        tengmi(L"segmentlar boʻshliq bilan qoʻshiladi", xomMatn(segs), L"birinchi ikkinchi");
    }
    {
        std::vector<Segment> segs = {{0, 1, L"  bir  "}, {1, 2, L"   "}, {2, 3, L"ikki"}};
        tengmi(L"boʻsh segment tashlanadi, chetlari kesiladi", xomMatn(segs), L"bir ikki");
    }
    tengmi(L"segmentsiz — boʻsh", xomMatn({}), L"");

    // ---- Paragraflar -------------------------------------------------------
    {
        // 1.5 s dan uzun pauza — shartsiz yangi paragraf.
        std::vector<Segment> segs = {{0, 1, L"birinchi gap."}, {3, 4, L"ikkinchi gap."}};
        tengmi(L"uzun pauza paragrafni boʻladi", chiroyliMatn(segs, Apostrof::Standart),
               L"Birinchi gap.\n\nIkkinchi gap.");
    }
    {
        // Qisqa pauza — bitta paragrafda qoladi.
        std::vector<Segment> segs = {{0, 1, L"birinchi gap."}, {1.2, 2, L"ikkinchi gap."}};
        tengmi(L"qisqa pauza boʻlmaydi", chiroyliMatn(segs, Apostrof::Standart),
               L"Birinchi gap. Ikkinchi gap.");
    }
    {
        std::vector<Segment> segs = {{0, 1, L"salom , dunyo ."}};
        tengmi(L"tinish belgisi oldidagi boʻshliq olinadi", chiroyliMatn(segs, Apostrof::Standart),
               L"Salom, dunyo.");
    }
    {
        std::vector<Segment> segs = {{0, 1, L"bir    ikki"}};
        tengmi(L"ketma-ket boʻshliqlar bittaga tushadi", chiroyliMatn(segs, Apostrof::Standart),
               L"Bir ikki");
    }
    {
        std::vector<Segment> segs = {{0, 1, L"o'zbek tilida so'z"}};
        tengmi(L"apostrof va bosh harf birga ishlaydi", chiroyliMatn(segs, Apostrof::Standart),
               L"Oʻzbek tilida soʻz");
    }
    tengmi(L"segmentsiz chiroyli matn boʻsh", chiroyliMatn({}, Apostrof::Standart), L"");

    // Faqat boʻshliqdan iborat segmentlar — natija boʻsh boʻlishi kerak,
    // «\n\n» kabi axlat emas.
    {
        std::vector<Segment> segs = {{0, 1, L"   "}, {2, 3, L"\t"}};
        tengmi(L"faqat boʻshliqli segmentlar", chiroyliMatn(segs, Apostrof::Standart), L"");
    }
}
