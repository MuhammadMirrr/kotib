// Matn boʻluvchi — tarjimaga tayyorlash va natijani qayta yigʻish testlari.
//
// Nega bu muhim: model bir jumlani eng yaxshi tarjima qiladi, lekin abzatslar
// va emoji foydalanuvchi matnining bir qismi. Ular yoʻqolsa — natija buzilgan
// koʻrinadi. Lokal sinovda 🇺🇿 va uzun tire (—) `<unk>` boʻlib chiqqandi.

import Foundation

func matnBoluvchiTestlari() {

    testQosh("bir qatordagi bir necha jumla ajraladi") {
        let b = MatnBoluvchi.bol("Salom. Qalaysiz? Yaxshi!")
        tengmi("bitta qator", b.count, 1)
        tengmi("uchta jumla", b[0].count, 3)
        tengmi("birinchi", b[0][0], .jumla(matn: "Salom.", qoshimcha: ""))
        tengmi("ikkinchi", b[0][1], .jumla(matn: "Qalaysiz?", qoshimcha: ""))
        tengmi("uchinchi", b[0][2], .jumla(matn: "Yaxshi!", qoshimcha: ""))
    }

    testQosh("boʻsh qatorlar saqlanadi") {
        let b = MatnBoluvchi.bol("Birinchi.\n\nIkkinchi.")
        tengmi("uchta qator", b.count, 3)
        tengmi("oʻrtadagi qator boʻsh", b[1].count, 0)
    }

    testQosh("bitta \\n bilan ajratilgan qatorlar ham saqlanadi") {
        let b = MatnBoluvchi.bol("Birinchi.\nIkkinchi.")
        tengmi("ikkita qator", b.count, 2)
        tengmi(
            "yigʻilganda \\n qaytadi",
            MatnBoluvchi.yig(b, tarjimalar: ["Первое.", "Второе."]),
            "Первое.\nВторое.")
    }

    testQosh("uzun tire oddiy tirega almashadi") {
        let b = MatnBoluvchi.bol("Mustaqillik — bu erkinlik.")
        tengmi("tire almashdi", b[0][0], .jumla(matn: "Mustaqillik - bu erkinlik.", qoshimcha: ""))
    }

    testQosh("emoji jumladan ajratiladi va qoshimchada saqlanadi") {
        let b = MatnBoluvchi.bol("Bayramingiz muborak! 🇺🇿")
        tengmi("bitta boʻlak", b[0].count, 1)
        tengmi("emoji ajratildi", b[0][0], .jumla(matn: "Bayramingiz muborak!", qoshimcha: " 🇺🇿"))
    }

    testQosh("faqat emoji'dan iborat qator tarjimaga berilmaydi") {
        let b = MatnBoluvchi.bol("🇺🇿 🎉")
        tengmi("bitta boʻlak", b[0].count, 1)
        tengmi("xom qoldi", b[0][0], .xom("🇺🇿 🎉"))
    }

    testQosh("yigʻish — tarjimalar oʻz joyiga tushadi, qatorlar saqlanadi") {
        let b = MatnBoluvchi.bol("Salom.\n\nXayr! 🇺🇿")
        let n = MatnBoluvchi.yig(b, tarjimalar: ["Привет.", "Пока!"])
        tengmi("natija", n, "Привет.\n\nПока! 🇺🇿")
    }

    testQosh("yigʻish — tarjimalar yetmasa asl matn qoladi") {
        let b = MatnBoluvchi.bol("Salom. Xayr.")
        tengmi(
            "ikkinchisi asl", MatnBoluvchi.yig(b, tarjimalar: ["Привет."]),
            "Привет. Xayr.")
    }

    testQosh("<unk> tozalanadi, ortiqcha boʻshliq qolmaydi") {
        tengmi("oʻrtada", MatnBoluvchi.tozala("Salom <unk> dunyo"), "Salom dunyo")
        tengmi("oxirida", MatnBoluvchi.tozala("Salom! <unk>"), "Salom!")
        tengmi("tegmaydi", MatnBoluvchi.tozala("Salom dunyo"), "Salom dunyo")
    }

    testQosh("boʻsh matn boʻsh roʻyxat beradi") {
        tengmi("boʻsh", MatnBoluvchi.bol("").count, 0)
        tengmi("faqat boʻshliq", MatnBoluvchi.bol("   ").count, 0)
    }

    testQosh("tarjima qilinadigan jumlalar sanaladi") {
        let b = MatnBoluvchi.bol("Salom.\n\n🇺🇿\n\nXayr.")
        tengmi("ikkita jumla", MatnBoluvchi.jumlalarSoni(b), 2)
        tengmi("boʻsh matnda nol", MatnBoluvchi.jumlalarSoni(MatnBoluvchi.bol("")), 0)
    }

    // MARK: Jumla chegaralari — ICU
    //
    // Nuqta jumla oxiri BOʻLMAGAN holatlar: havola, oʻnlik son, versiya
    // raqami. Ilgari `t.me/dr_azamoff` → `["t.", "me/dr_azamoff"]` boʻlib
    // ketardi va `t.` alohida jumla sifatida modelga borib `п.` boʻlib
    // qaytardi — havola ishlamay qolardi.

    testQosh("URL nuqtasi jumlani kesmaydi") {
        let b = MatnBoluvchi.bol("t.me/dr_azamoff")
        tengmi("bitta boʻlak", b[0].count, 1)
    }

    // MARK: Himoyalangan boʻlaklar
    //
    // Havola, email va handle modelga UMUMAN berilmaydi — u ularni tarjima
    // qilishga urinadi va buzadi. `.xom` boʻlib oʻz joyida qoladi.

    testQosh("yolgʻiz havola tarjimaga berilmaydi") {
        let b = MatnBoluvchi.bol("t.me/dr_azamoff")
        tengmi("xom qoldi", b[0][0], .xom("t.me/dr_azamoff"))
        tengmi(
            "yigʻilganda oʻzgarmaydi",
            MatnBoluvchi.yig(b, tarjimalar: []), "t.me/dr_azamoff")
    }

    testQosh("jumla ichidagi havola ajratiladi") {
        let b = MatnBoluvchi.bol("Manba: t.me/dr_azamoff")
        tengmi("ikkita boʻlak", b[0].count, 2)
        tengmi("matn qismi", b[0].first, .jumla(matn: "Manba:", qoshimcha: ""))
        tengmi("havola xom", b[0].last, .xom("t.me/dr_azamoff"))
        tengmi(
            "yigʻildi",
            MatnBoluvchi.yig(b, tarjimalar: ["Источник:"]),
            "Источник: t.me/dr_azamoff")
    }

    testQosh("toʻliq URL himoyalanadi") {
        let b = MatnBoluvchi.bol("Yuklab oling: https://cdn.mirqobilov.com/dl/x.tar.gz")
        tengmi("URL xom", b[0].last, .xom("https://cdn.mirqobilov.com/dl/x.tar.gz"))
    }

    testQosh("email himoyalanadi") {
        let b = MatnBoluvchi.bol("Xat: user@example.com")
        tengmi("email xom", b[0].last, .xom("user@example.com"))
    }

    testQosh("handle himoyalanadi") {
        let b = MatnBoluvchi.bol("Obuna: @dr_azamoff")
        tengmi("handle xom", b[0].last, .xom("@dr_azamoff"))
    }

    testQosh("oddiy son himoyalanmaydi") {
        let b = MatnBoluvchi.bol("Pi soni 3.14 ga teng.")
        tengmi("bitta jumla, ajratilmadi", b[0].count, 1)
    }

    testQosh("havola sanaladigan jumlalar orasiga kirmaydi") {
        let b = MatnBoluvchi.bol("Manba: t.me/dr_azamoff")
        tengmi("bitta jumla tarjima qilinadi", MatnBoluvchi.jumlalarSoni(b), 1)
    }

    // MARK: Chiqish normalizatsiyasi
    //
    // Model tinish belgisi oldiga boʻsh joy qoʻyib yuboradi («У женщин ,
    // принимающих») — foydalanuvchi matnida bu xato boʻlib koʻrinadi.
    // Uzun tire esa `bolakYasa` da `-` ga aylantirilgan, chiqishda tiklanadi.

    testQosh("tinish belgisi oldidagi boʻsh joy olinadi") {
        tengmi("vergul", MatnBoluvchi.tozala("У женщин , принимающих"), "У женщин, принимающих")
        tengmi("nuqta", MatnBoluvchi.tozala("Konец ."), "Konец.")
        tengmi("ikki nuqta", MatnBoluvchi.tozala("Natija : yaxshi"), "Natija: yaxshi")
        tengmi("qavs", MatnBoluvchi.tozala("matn ( izoh )"), "matn (izoh)")
    }

    testQosh("uzun tire tiklanadi") {
        tengmi(
            "tire", MatnBoluvchi.tozala("Альцгеймер - это болезнь"),
            "Альцгеймер — это болезнь")
    }

    testQosh("defis bilan yozilgan qoʻshma soʻz tegilmaydi") {
        tengmi("qoʻshma soʻz", MatnBoluvchi.tozala("ijtimoiy-iqtisodiy"), "ijtimoiy-iqtisodiy")
    }

    testQosh("oʻnlik son jumlani kesmaydi") {
        let b = MatnBoluvchi.bol("Pi soni 3.14 ga teng. Ikkinchi gap.")
        tengmi("ikkita jumla", b[0].count, 2)
        tengmi("birinchi butun", b[0][0], .jumla(matn: "Pi soni 3.14 ga teng.", qoshimcha: ""))
    }

    testQosh("versiya raqami jumlani kesmaydi") {
        let b = MatnBoluvchi.bol("Versiya v1.0 chiqdi.")
        tengmi("bitta jumla", b[0].count, 1)
    }

    testQosh("xitoy nuqtasi chegara boʻladi") {
        let b = MatnBoluvchi.bol("这是第一句。这是第二句。")
        tengmi("ikkita jumla", b[0].count, 2)
    }

    testQosh("arab savol belgisi chegara boʻladi") {
        let b = MatnBoluvchi.bol("هذه جملة؟ وهذه أخرى.")
        tengmi("ikkita jumla", b[0].count, 2)
    }
}
