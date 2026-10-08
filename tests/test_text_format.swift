import Foundation

func textFormatTestlari() {
    // Diktovka natijasi (S11): ilgari umuman normallashtirilmasdi.
    testQosh("diktovka — matnniTayyorla") {
        tengmi(
            "aralash apostroflar → oʻ/gʻ/ʼ",
            matnniTayyorla("O`zbekiston ma´lumotlari: qo’shimcha g‘oya va e'tibor", apostrof: .standart),
            "Oʻzbekiston maʼlumotlari: qoʻshimcha gʻoya va eʼtibor")
        tengmi(
            "«Oddiy apostrof» sozlamasi diktovkaga ham taʼsir qiladi",
            matnniTayyorla("oʻzbek tili, maʼno", apostrof: .oddiy), "o'zbek tili, ma'no")
        tengmi(
            "apostrofsiz matn oʻzgarmaydi",
            matnniTayyorla("Salom, dunyo!", apostrof: .standart), "Salom, dunyo!")
        tengmi(
            "toʻgʻri matn oʻzgarmaydi",
            matnniTayyorla("oʻzbek gʻalaba maʼno", apostrof: .standart), "oʻzbek gʻalaba maʼno")
    }
    testQosh("diktovka — takrorlanish halqasi (S23)") {
        tengmi(
            "halqa — ikki soʻzli ibora ×6 → bir marta",
            takrorniQisqartir(
                "bu oʻzbekiston respublikasi oʻzbekiston respublikasi oʻzbekiston respublikasi oʻzbekiston respublikasi oʻzbekiston respublikasi oʻzbekiston respublikasi"
            ), "bu oʻzbekiston respublikasi")
        tengmi(
            "halqa — bitta soʻz ×8, oldingi soʻzlar qoladi",
            takrorniQisqartir("qaynon shirin toʻla toʻla toʻla toʻla toʻla toʻla toʻla toʻla"), "qaynon shirin toʻla")
        tengmi(
            "halqa — toʻrt soʻzli ibora, keyin davom etadi",
            takrorniQisqartir(
                "oʻn toʻrt yuz ellik ming toʻrt yuz ellik ming toʻrt yuz ellik ming toʻrt yuz ellik ming oʻn olti"),
            "oʻn toʻrt yuz ellik ming oʻn olti")
        tengmi("uch marta — tegilmaydi", takrorniQisqartir("ha ha ha keldik"), "ha ha ha keldik")
        tengmi("ikki marta — tegilmaydi (oddiy nutq)", takrorniQisqartir("toʻla toʻla idish"), "toʻla toʻla idish")
        tengmi(
            "takrorsiz matn AYNAN qaytadi (qoʻsh boʻshliq ham)", takrorniQisqartir("Salom,  dunyo!  Bugun"),
            "Salom,  dunyo!  Bugun")
        tengmi(
            "tinish belgisi bilan farq qiluvchi soʻz — boshqa soʻz", takrorniQisqartir("ha ha ha ha."), "ha ha ha ha.")
        tengmi("boʻsh satr", takrorniQisqartir(""), "")
        tengmi(
            "matnniTayyorla ham qisqartiradi (apostrofdan oldin)",
            matnniTayyorla("men o'zbek o'zbek o'zbek o'zbek", apostrof: .standart), "men oʻzbek")
    }
    testQosh("apostrof — standart uslub") {
        tengmi("o' → oʻ", apostrofniBirxillashtir("o'zbek", .standart), "oʻzbek")
        tengmi("g` → gʻ", apostrofniBirxillashtir("g`alaba", .standart), "gʻalaba")
        tengmi("o´ → oʻ", apostrofniBirxillashtir("o´zbek", .standart), "oʻzbek")
        tengmi("boshqa harf → tutuq", apostrofniBirxillashtir("ma'no", .standart), "maʼno")
        tengmi("aralash", apostrofniBirxillashtir("o'zbek ma'nosi", .standart), "oʻzbek maʼnosi")
    }

    testQosh("apostrof — oddiy uslub") {
        tengmi("oʻ → o'", apostrofniBirxillashtir("oʻzbek", .oddiy), "o'zbek")
        tengmi("maʼno → ma'no", apostrofniBirxillashtir("maʼno", .oddiy), "ma'no")
    }

    testQosh("jumla boshi bosh harf") {
        tengmi("birinchi harf", jumlaBoshiniKattalashtir("salom dunyo."), "Salom dunyo.")
        tengmi("nuqtadan keyin", jumlaBoshiniKattalashtir("salom. dunyo."), "Salom. Dunyo.")
        tengmi("savoldan keyin", jumlaBoshiniKattalashtir("nima? bu."), "Nima? Bu.")
        tengmi("undovdan keyin", jumlaBoshiniKattalashtir("voy! bu."), "Voy! Bu.")
        tengmi("allaqachon katta", jumlaBoshiniKattalashtir("Salom."), "Salom.")
        tengmi("bo'sh satr", jumlaBoshiniKattalashtir(""), "")
    }

    testQosh("paragraflarga boʻlish — pauza > 1.5s") {
        let segs = [
            Segment(t0: 0.0, t1: 2.0, matn: "Birinchi gap"),
            Segment(t0: 4.0, t1: 6.0, matn: "Ikkinchi gap")  // 2.0s pauza
        ]
        tengmi(
            "ikki paragraf",
            chiroyliMatn(segs, apostrof: .oddiy),
            "Birinchi gap\n\nIkkinchi gap")
    }

    testQosh("paragraflarga boʻlish — qisqa pauza birlashtiradi") {
        let segs = [
            Segment(t0: 0.0, t1: 2.0, matn: "Birinchi gap"),
            Segment(t0: 2.2, t1: 4.0, matn: "ikkinchi gap")  // 0.2s pauza
        ]
        tengmi(
            "bitta paragraf",
            chiroyliMatn(segs, apostrof: .oddiy),
            "Birinchi gap ikkinchi gap")
    }

    testQosh("0.8s pauza — faqat uzun paragrafdan keyin boʻladi") {
        // Qisqa paragraf: 0.9s pauza yetarli emas
        let qisqa = [
            Segment(t0: 0.0, t1: 1.0, matn: "Qisqa"),
            Segment(t0: 1.9, t1: 3.0, matn: "davomi")
        ]
        tengmi("qisqa — birlashadi", chiroyliMatn(qisqa, apostrof: .oddiy), "Qisqa davomi")

        // 200+ belgili paragraf: 0.9s pauza yetarli
        let uzunMatn = String(repeating: "so'z ", count: 60)  // ~300 belgi
        let uzun = [
            Segment(t0: 0.0, t1: 10.0, matn: uzunMatn),
            Segment(t0: 10.9, t1: 12.0, matn: "Yangi paragraf")
        ]
        tekshir(
            "uzun — bo'linadi",
            chiroyliMatn(uzun, apostrof: .oddiy).contains("\n\nYangi paragraf"))
    }

    testQosh("boʻshliqlarni tozalash") {
        let segs = [Segment(t0: 0, t1: 1, matn: "salom   dunyo ,  qalaysan ?")]
        tengmi(
            "ortiqcha bo'shliq",
            chiroyliMatn(segs, apostrof: .oddiy),
            "Salom dunyo, qalaysan?")
    }

    testQosh("boʻsh segmentlar tashlab ketiladi") {
        let segs = [
            Segment(t0: 0, t1: 1, matn: "Salom"),
            Segment(t0: 1, t1: 2, matn: "   "),
            Segment(t0: 2, t1: 3, matn: "dunyo")
        ]
        tengmi(
            "bo'sh o'tkazib yuborildi",
            chiroyliMatn(segs, apostrof: .oddiy), "Salom dunyo")
    }

    testQosh("xom matn — oʻzgartirilmaydi") {
        let segs = [
            Segment(t0: 0, t1: 1, matn: " salom  "),
            Segment(t0: 1, t1: 2, matn: "dunyo")
        ]
        tengmi("faqat birlashtiriladi", xomMatn(segs), "salom dunyo")
    }

    testQosh("boʻsh kirish") {
        tengmi("bo'sh massiv", chiroyliMatn([], apostrof: .oddiy), "")
        tengmi("bo'sh massiv — xom", xomMatn([]), "")
    }
}
