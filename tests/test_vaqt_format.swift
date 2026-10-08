// Vaqt formatlash testlari — dizayndagi "bugun 16:04" / "kecha 18:22" yozuvlari
// va yozib olish taymeri ("0:04").
//
// Sana testlari NISBIY hisoblanadi (Date() dan siljish bilan), shuning uchun ular
// istalgan kunda ishlaydi. Aynan chegaralar — kecha yarim tunidan bir daqiqa
// oldin/keyin — alohida tekshiriladi: "24 soat oldin" bilan "kecha" bir xil emas.

import Foundation

func vaqtFormatTestlari() {

    // MARK: Taymer

    testQosh("taymer — soniyalar 0:SS koʻrinishida") {
        tengmi("0 soniya", VaqtFormat.taymer(0), "0:00")
        tengmi("4 soniya", VaqtFormat.taymer(4), "0:04")
        tengmi("59 soniya", VaqtFormat.taymer(59), "0:59")
    }

    testQosh("taymer — daqiqadan oshgani") {
        tengmi("60 soniya", VaqtFormat.taymer(60), "1:00")
        tengmi("83 soniya", VaqtFormat.taymer(83), "1:23")
        tengmi("725 soniya", VaqtFormat.taymer(725), "12:05")
    }

    testQosh("taymer — kasr soniya pastga yaxlitlanadi") {
        tengmi("4.9s hali 0:04", VaqtFormat.taymer(4.9), "0:04")
    }

    testQosh("taymer — manfiy qiymat 0:00 beradi") {
        // Soat sozlanib qolsa hisob manfiy chiqishi mumkin — taymer buzilmasin.
        tengmi("manfiy", VaqtFormat.taymer(-3), "0:00")
    }

    // MARK: Nisbiy sana

    testQosh("sana — bugungi yozuv 'bugun HH:mm'") {
        let hozir = Date()
        let matn = VaqtFormat.nisbiy(hozir, hozir: hozir)
        tekshir("'bugun' bilan boshlanadi", matn.hasPrefix("bugun "))
        tekshir("vaqt qoʻshilgan", matn.count > "bugun ".count)
    }

    testQosh("sana — kechagi yozuv 'kecha HH:mm'") {
        let hozir = Date()
        let kecha = Calendar.current.date(byAdding: .day, value: -1, to: hozir)!
        tekshir(
            "'kecha' bilan boshlanadi",
            VaqtFormat.nisbiy(kecha, hozir: hozir).hasPrefix("kecha "))
    }

    testQosh("sana — eski yozuvda oy nomi boʻladi, 'bugun/kecha' emas") {
        let hozir = Date()
        let eski = Calendar.current.date(byAdding: .day, value: -8, to: hozir)!
        let matn = VaqtFormat.nisbiy(eski, hozir: hozir)
        tekshir("'bugun' yoʻq", !matn.hasPrefix("bugun"))
        tekshir("'kecha' yoʻq", !matn.hasPrefix("kecha"))
        tekshir("boʻsh emas", !matn.isEmpty)
    }

    testQosh("sana — chegara: kecha kechqurun 23:59 hali 'kecha'") {
        // 24 soatlik oyna emas, KALENDAR kuni muhim: bugun 00:30 da turib
        // kecha 23:59 dagi yozuv "kecha" boʻlishi kerak, "bugun" emas.
        let taqvim = Calendar.current
        let bugunTong = taqvim.startOfDay(for: Date())
        let hozir = taqvim.date(byAdding: .minute, value: 30, to: bugunTong)!
        let kechaKech = taqvim.date(byAdding: .minute, value: -1, to: bugunTong)!
        tekshir(
            "kecha deb belgilandi",
            VaqtFormat.nisbiy(kechaKech, hozir: hozir).hasPrefix("kecha "))
    }

    testQosh("sana — chegara: bugun 00:00 'bugun'") {
        let taqvim = Calendar.current
        let bugunTong = taqvim.startOfDay(for: Date())
        let hozir = taqvim.date(byAdding: .hour, value: 10, to: bugunTong)!
        tekshir(
            "bugun deb belgilandi",
            VaqtFormat.nisbiy(bugunTong, hozir: hozir).hasPrefix("bugun "))
    }
}

/// Fayl roʻyxatidagi "12 daqiqa · bugun" yozuvi uchun ikkita alohida qism.
func vaqtFormatFaylTestlari() {

    testQosh("davomiylik — daqiqalar") {
        tengmi("12 daqiqa", VaqtFormat.davomiylik(725), "12 daqiqa")
        tengmi("34 daqiqa", VaqtFormat.davomiylik(34 * 60 + 20), "34 daqiqa")
        tengmi("59 daqiqa", VaqtFormat.davomiylik(59 * 60), "59 daqiqa")
    }

    testQosh("davomiylik — bir daqiqadan qisqa fayl") {
        // "0 daqiqa" deb koʻrsatish fayl boʻsh degan taassurot beradi.
        tengmi("30 soniya", VaqtFormat.davomiylik(30), "1 daqiqadan kam")
        tengmi("0 soniya", VaqtFormat.davomiylik(0), "1 daqiqadan kam")
    }

    testQosh("davomiylik — soatlar") {
        tengmi("rosa 1 soat", VaqtFormat.davomiylik(3600), "1 soat")
        tengmi("1 soat 5 daqiqa", VaqtFormat.davomiylik(3900), "1 soat 5 daqiqa")
        tengmi("2 soat 30 daqiqa", VaqtFormat.davomiylik(9000), "2 soat 30 daqiqa")
    }

    testQosh("kun — bugun / kecha / sana") {
        let hozir = Date()
        let taqvim = Calendar.current
        tengmi("bugun", VaqtFormat.kun(hozir, hozir: hozir), "bugun")
        tengmi(
            "kecha",
            VaqtFormat.kun(
                taqvim.date(byAdding: .day, value: -1, to: hozir)!,
                hozir: hozir), "kecha")
        let eski = VaqtFormat.kun(taqvim.date(byAdding: .day, value: -9, to: hozir)!, hozir: hozir)
        tekshir("eski sanada 'bugun/kecha' yoʻq", eski != "bugun" && eski != "kecha")
        tekshir("eski sana boʻsh emas", !eski.isEmpty)
    }
}
