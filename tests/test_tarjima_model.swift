// Tarjima modeli papkasi joyidami va toʻliqmi — tekshiruv testlari.
//
// Nega muhim: yarim yuklangan papka bilan ishga tushsak, CTranslate2 ichida
// tushunarsiz xato bilan quladi. Papka faqat BARCHA kerakli fayllar boʻlganda
// "tayyor" hisoblanadi. Uzilib qolgan koʻchirish 0 baytli fayl qoldirishi
// mumkin — hajm ham tekshiriladi.

import Foundation

private func vaqtinchalikPapka2() -> URL {
    let u = FileManager.default.temporaryDirectory
        .appendingPathComponent("kotib-tarjima-test-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}

private func modelFayllariniYoz(_ p: URL, tashqari: String? = nil, bosh: String? = nil) {
    for f in TarjimaModel.kerakliFayllar where f != tashqari {
        let matn = (f == bosh) ? "" : "x"
        try? matn.write(to: p.appendingPathComponent(f), atomically: true, encoding: .utf8)
    }
}

func tarjimaModelTestlari() {

    testQosh("toʻrtta fayl kerak") {
        tengmi("fayllar soni", TarjimaModel.kerakliFayllar.count, 4)
        tekshir("model.bin", TarjimaModel.kerakliFayllar.contains("model.bin"))
        tekshir("lugʻat", TarjimaModel.kerakliFayllar.contains("shared_vocabulary.json"))
        tekshir("sentencepiece", TarjimaModel.kerakliFayllar.contains("sentencepiece.bpe.model"))
        tekshir("config", TarjimaModel.kerakliFayllar.contains("config.json"))
        tekshir(
            "tokenizer.json kerak emas",
            !TarjimaModel.kerakliFayllar.contains("tokenizer.json"))
    }

    testQosh("hamma fayl boʻlsa tayyor") {
        let p = vaqtinchalikPapka2()
        defer { try? FileManager.default.removeItem(at: p) }
        modelFayllariniYoz(p)
        tekshir("tayyor", TarjimaModel.tayyormi(p))
    }

    testQosh("bitta fayl yetishmasa tayyor emas") {
        for yoq in TarjimaModel.kerakliFayllar {
            let p = vaqtinchalikPapka2()
            defer { try? FileManager.default.removeItem(at: p) }
            modelFayllariniYoz(p, tashqari: yoq)
            tekshir("\(yoq) yoʻq → tayyor emas", !TarjimaModel.tayyormi(p))
        }
    }

    testQosh("boʻsh fayl tayyor emas") {
        let p = vaqtinchalikPapka2()
        defer { try? FileManager.default.removeItem(at: p) }
        modelFayllariniYoz(p, bosh: "model.bin")
        tekshir("boʻsh model.bin", !TarjimaModel.tayyormi(p))
    }

    testQosh("boʻsh papka va mavjud boʻlmagan papka tayyor emas") {
        let p = vaqtinchalikPapka2()
        defer { try? FileManager.default.removeItem(at: p) }
        tekshir("boʻsh papka", !TarjimaModel.tayyormi(p))
        tekshir("yoʻq papka", !TarjimaModel.tayyormi(URL(fileURLWithPath: "/yoq/papka/xyz")))
    }

    testQosh("yoʻl Kotib papkasi ichida") {
        tengmi("papka nomi", Yollar.tarjimaModeli.lastPathComponent, "tarjima-model-33b")
        tekshir("Kotib ichida", Yollar.tarjimaModeli.path.contains("/Kotib/"))
        tengmi(
            "TarjimaModel.papka bilan bir xil",
            TarjimaModel.papka.path, Yollar.tarjimaModeli.path)
    }

    // MARK: Disk joyi
    //
    // Busiz toʻla diskda yuklash 3 GB dan keyin qulardi va foydalanuvchi
    // sababini bilmasdi.

    testQosh("disk joyi yetarli boʻlsa true") {
        tekshir(
            "1 KB uchun joy bor",
            TarjimaModel.yetarliJoyBormi(
                FileManager.default.temporaryDirectory,
                kerak: 1024))
    }

    testQosh("imkonsiz katta hajm uchun false") {
        tekshir(
            "1 PB uchun joy yoʻq",
            !TarjimaModel.yetarliJoyBormi(
                FileManager.default.temporaryDirectory,
                kerak: 1_000_000_000_000_000))
    }

    testQosh("kerakli joy arxiv va ochilgan nusxaga yetadi") {
        tekshir(
            "arxiv + ochilgan nusxa",
            TarjimaModel.kerakliJoy == TarjimaModel.taxminiyBayt + TarjimaModel.ochilganBayt)
        tekshir("arxiv 3 GB dan katta", TarjimaModel.taxminiyBayt > 3_000_000_000)
        tekshir(
            "ochilgani arxivdan katta — int8 siqilmaydi",
            TarjimaModel.ochilganBayt > TarjimaModel.taxminiyBayt)
        tekshir("kerakli joy 6 GB dan oshadi", TarjimaModel.kerakliJoy > 6_000_000_000)
    }
}
