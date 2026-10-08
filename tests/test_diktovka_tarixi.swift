import Foundation

private func vaqtinchalikYol() -> URL {
    FileManager.default.temporaryDirectory
        .appendingPathComponent("tarix-test-\(UUID().uuidString).json")
}

func diktovkaTarixiTestlari() {
    testQosh("boʻsh tarix — fayl yoʻq boʻlsa boʻsh massiv") {
        let t = DiktovkaTarixi(fayl: vaqtinchalikYol())
        tengmi("boʻsh", t.oqi().count, 0)
    }

    testQosh("yozish → oʻqish — aynan oʻsha yozuv qaytadi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        let t = DiktovkaTarixi(fayl: yol)
        let y = DiktovkaYozuvi(matn: "salom dunyo", davomiylik: 2.5)
        tekshir("yozildi", t.qoshish(y))

        let oqilgan = t.oqi()
        tengmi("bitta yozuv", oqilgan.count, 1)
        tengmi("matn saqlandi", oqilgan.first?.matn, "salom dunyo")
        tengmi("davomiylik saqlandi", oqilgan.first?.davomiylik, 2.5)
        tengmi("id saqlandi", oqilgan.first?.id, y.id)
    }

    testQosh("tartib — eng yangisi birinchi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        let t = DiktovkaTarixi(fayl: yol)
        t.qoshish(DiktovkaYozuvi(matn: "birinchi", davomiylik: 1))
        t.qoshish(DiktovkaYozuvi(matn: "ikkinchi", davomiylik: 1))
        t.qoshish(DiktovkaYozuvi(matn: "uchinchi", davomiylik: 1))

        let oqilgan = t.oqi()
        tengmi("eng yangisi birinchi", oqilgan.first?.matn, "uchinchi")
        tengmi("eng eskisi oxirgi", oqilgan.last?.matn, "birinchi")
    }

    testQosh("chegara — 205 ta qoʻshilsa 200 ta qoladi, eng eskisi tushadi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        let t = DiktovkaTarixi(fayl: yol)
        for i in 1...205 {
            t.qoshish(DiktovkaYozuvi(matn: "yozuv \(i)", davomiylik: 1))
        }
        let oqilgan = t.oqi()
        tengmi("200 ta qoldi", oqilgan.count, DiktovkaTarixi.chegara)
        tengmi("eng yangisi 205", oqilgan.first?.matn, "yozuv 205")
        tengmi("eng eskisi 6", oqilgan.last?.matn, "yozuv 6")
    }

    testQosh("oʻchirish — id boʻyicha bitta yozuvni olib tashlaydi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        let t = DiktovkaTarixi(fayl: yol)
        let a = DiktovkaYozuvi(matn: "a", davomiylik: 1)
        let b = DiktovkaYozuvi(matn: "b", davomiylik: 1)
        t.qoshish(a); t.qoshish(b)

        tekshir("oʻchirildi", t.ochir(id: a.id))
        let oqilgan = t.oqi()
        tengmi("bittasi qoldi", oqilgan.count, 1)
        tengmi("qolgani b", oqilgan.first?.matn, "b")
    }

    testQosh("oʻchirish — yoʻq id false qaytaradi va tarixni buzmaydi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        let t = DiktovkaTarixi(fayl: yol)
        t.qoshish(DiktovkaYozuvi(matn: "a", davomiylik: 1))
        tekshir("false qaytdi", !t.ochir(id: "yoʻq-id"))
        tengmi("tarix buzilmadi", t.oqi().count, 1)
    }

    testQosh("tozalash — hammasini oʻchiradi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        let t = DiktovkaTarixi(fayl: yol)
        t.qoshish(DiktovkaYozuvi(matn: "a", davomiylik: 1))
        tekshir("tozalandi", t.tozala())
        tengmi("boʻsh", t.oqi().count, 0)
    }

    testQosh("buzuq JSON — boʻsh massiv qaytadi, xato otmaydi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        try? "bu JSON emas {{{".write(to: yol, atomically: true, encoding: .utf8)
        let t = DiktovkaTarixi(fayl: yol)
        tengmi("boʻsh", t.oqi().count, 0)
    }

    testQosh("buzuq fayl ustiga yozish — keyingi qoʻshish ishlaydi") {
        let yol = vaqtinchalikYol()
        defer { try? FileManager.default.removeItem(at: yol) }
        try? "buzuq".write(to: yol, atomically: true, encoding: .utf8)
        let t = DiktovkaTarixi(fayl: yol)
        tekshir("qoʻshildi", t.qoshish(DiktovkaYozuvi(matn: "yangi", davomiylik: 1)))
        tengmi("bitta yozuv", t.oqi().count, 1)
    }
}
