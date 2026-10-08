// Matnni tarjimaga tayyorlash va natijani qayta yigʻish.
//
// macOS'dagi `src/matn_boluvchi.swift` ning ekvivalenti va u bilan bir xil
// natija berishi kerak — test bilan qotirilgan.
//
// Model BIR JUMLANI eng yaxshi tarjima qiladi: butun abzats berilsa sifat
// tushadi. Shuning uchun matn avval qatorlarga, keyin jumlalarga boʻlinadi.
//
// Nega natija «qatorlar ichida boʻlaklar»: yassi roʻyxatda «Salom. Xayr.»
// (bitta qator, ikki jumla) va «Salom.\nXayr.» (ikki qator) bir xil koʻrinadi
// va tarjimadan keyin qator chegarasi yoʻqoladi.
//
// Ikki muammo alohida hal qilinadi (ikkalasi ham lokal sinovda topilgan —
// model bunday belgilarni bilmaydi va `<unk>` qaytaradi):
//   • Uzun tire (— –) oddiy `-` ga almashtiriladi.
//   • Emoji jumladan ajratiladi va tarjima OXIRIGA qaytariladi.
//
// Fayl UI'dan va tarmoqdan mustaqil — `win/tests/` uni toʻliq qamraydi.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace rubai {

// Qatorning bitta boʻlagi.
struct Bolak {
    enum class Tur {
        Jumla,  // modelga beriladi
        Xom,    // tarjimaga umuman berilmaydi (belgi, havola, emoji)
    };

    Tur tur = Tur::Jumla;
    // Jumla: modelga beriladigan matn. Xom: matnning oʻzi.
    std::wstring matn;
    // Jumla: tarjimadan keyin oxiriga qaytariladigan qoʻshimcha.
    std::wstring qoshimcha;

    bool operator==(const Bolak& o) const {
        return tur == o.tur && matn == o.matn && qoshimcha == o.qoshimcha;
    }
};

using Qatorlar = std::vector<std::vector<Bolak>>;

namespace MatnBoluvchi {

// Matnni qatorlarga, har qatorni boʻlaklarga ajratadi.
Qatorlar bol(const std::wstring& matn);

// Tarjimaga beriladigan matnlar — `yig` ularning tarjimasini SHU TARTIBDA kutadi.
std::vector<std::wstring> jumlalar(const Qatorlar& qatorlar);
size_t jumlalarSoni(const Qatorlar& qatorlar);

// Tarjimalarni oʻz joyiga qoʻyib matnni qayta yigʻadi. Tarjima yetmasa —
// oʻsha jumlaning asl matni qoladi (ish yarmida toʻxtasa foydalanuvchi
// baribir toʻliq matn koʻradi).
std::wstring yig(const Qatorlar& qatorlar, const std::vector<std::wstring>& tarjimalar);

// Model chiqishini tozalaydi: `<unk>` ni olib tashlaydi, tinish belgisi
// atrofidagi ortiqcha boʻshliqni yigʻadi, uzun tireni tiklaydi.
std::wstring tozala(const std::wstring& s);

// ---- Sinov uchun ochiq ----

// Jumla ichidagi tarjima qilinmaydigan oraliqlar (havola, email, @handle,
// #hashtag) — [boshi, oxiri) juftliklari, chapdan oʻngga, kesishmagan.
std::vector<std::pair<size_t, size_t>> himoyalanganlar(const std::wstring& jumla);

// Qatorni jumlalarga ajratadi.
std::vector<std::wstring> jumlalargaBol(const std::wstring& qator);

// Windows ICU topildimi (jumla chegaralari uchun). false — zaxira qoida
// ishlatiladi. Faqat diagnostika uchun.
bool icuBormi();

}  // namespace MatnBoluvchi
}  // namespace rubai
