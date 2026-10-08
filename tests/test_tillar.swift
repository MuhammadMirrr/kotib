// Tillar roʻyxati — nomlar macOS'dan olinadi, kodlar modeldan.
//
// Nega tekshiriladi: nomlar tizimdan kelgani uchun ular notoʻgʻri apostrof
// bilan keladi ("o‘zbek"), va bir til bir necha yozuvda boʻlsa roʻyxatda
// ikkita bir xil band paydo boʻladi.

import Foundation

func tillarTestlari() {

    testQosh("modeldagi barcha tillar bor") {
        tengmi("202 ta til", Til.kodlar.count, 202)
        tengmi("hammasi saralangan roʻyxatda", Til.hammasi.count, 202)
    }

    testQosh("standart tillar toʻgʻri kodlarga ega") {
        tengmi("uz", Til.uz.nllb, "uzn_Latn")
        tengmi("ru", Til.ru.nllb, "rus_Cyrl")
        tengmi("en", Til.en.nllb, "eng_Latn")
        tekshir("uz roʻyxatda bor", Til.hammasi.contains(Til.uz))
        tekshir("ru roʻyxatda bor", Til.hammasi.contains(Til.ru))
        tekshir("en roʻyxatda bor", Til.hammasi.contains(Til.en))
    }

    testQosh("kod va yozuv ajratiladi") {
        tengmi("kod", Til.uz.kod, "uzn")
        tengmi("yozuv", Til.uz.yozuv, "Latn")
    }

    testQosh("nomlar boʻsh emas va bosh harf bilan") {
        for t in Til.hammasi {
            tekshir("nom boʻsh emas: \(t.nllb)", !t.nom.isEmpty)
            tekshir("bosh harf: \(t.nllb)", t.nom.first?.isUppercase ?? false)
        }
    }

    testQosh("nomlarda notoʻgʻri apostrof yoʻq") {
        let notogri: Set<Character> = ["'", "\u{2018}", "\u{2019}", "\u{0060}", "\u{00B4}"]
        for t in Til.hammasi {
            tekshir(
                "apostrof toʻgʻri: \(t.nllb) — \(t.nom)",
                !t.nom.contains(where: { notogri.contains($0) }))
        }
    }

    testQosh("bir xil nomli tillar yozuv bilan ajratiladi") {
        var korilgan = Set<String>()
        for t in Til.hammasi {
            tekshir("nom takrorlanmadi: \(t.nom)", !korilgan.contains(t.nom))
            korilgan.insert(t.nom)
        }
    }

    testQosh("nomlar kirill alifbosida emas") {
        // macOS oʻzbekcha nomni bilmasa ruschasini qaytaradi ("Динка") —
        // ilova butunlay lotin, bunday nom chiqmasligi kerak.
        for t in Til.hammasi {
            let kirilbor = t.nom.unicodeScalars.contains { (0x0400...0x04FF).contains($0.value) }
            tekshir("lotin: \(t.nllb) — \(t.nom)", !kirilbor)
        }
    }

    testQosh("har bir til uchun nom manbasi bor") {
        // Boʻsh boʻlmasa — `qoshimchaNomlar` ga oʻsha kodlar uchun nom qoʻshing.
        // ("Fon" va "Luo" kabi nomlar oʻz kodiga oʻxshaydi, lekin ular haqiqiy
        // nomlar — shuning uchun nom matni emas, MANBASI tekshiriladi.)
        tengmi("nomsiz til yoʻq", Til.nomsizlar, [])
    }

    testQosh("topilsin faqat mavjud kodni qaytaradi") {
        tengmi("mavjud", Til.topilsin("rus_Cyrl"), Til.ru)
        tekshir("mavjud emas", Til.topilsin("xyz_Abcd") == nil)
    }
}
