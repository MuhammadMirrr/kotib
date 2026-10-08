import Foundation

func saqlanmaganTestlari() {
    testQosh("saqlanmagan — WAV aylanma") {
        let s: [Float] = [0, 0.5, -0.5, 1, -1, 0.25, 1.7, -3, .nan]
        let d = SaqlanmaganOvoz.wav(s)
        tengmi("hajm", d.count, 44 + s.count * 2)
        tengmi("RIFF", String(decoding: d.prefix(4), as: UTF8.self), "RIFF")
        // RIFF hajmi = fayl − 8
        let riff = Int(d[4]) | Int(d[5]) << 8 | Int(d[6]) << 16 | Int(d[7]) << 24
        tengmi("RIFF hajmi", riff, d.count - 8)
        guard let q = SaqlanmaganOvoz.namunalar(d) else { tekshir("oʻqildi", false); return }
        tengmi("soni", q.count, s.count)
        // 16 bitga kvantlash xatosi — yarim qadamdan oshmaydi.
        let kutilgan: [Float] = [0, 0.5, -0.5, 1, -1, 0.25, 1, -1, 0]
        tekshir("qiymatlar", zip(q, kutilgan).allSatisfy { abs($0 - $1) <= 0.5 / 32767 + 1e-7 })
        tengmi("boʻsh", SaqlanmaganOvoz.namunalar(SaqlanmaganOvoz.wav([]))?.count, 0)
    }

    testQosh("saqlanmagan — begona WAV rad etiladi") {
        var d = SaqlanmaganOvoz.wav([0.1, 0.2])
        tengmi("44 baytdan qisqa", SaqlanmaganOvoz.namunalar(d.prefix(43)), nil)
        d[22] = 2  // stereo
        tengmi("stereo", SaqlanmaganOvoz.namunalar(d), nil)
        tengmi("boshqa chastota", SaqlanmaganOvoz.namunalar(SaqlanmaganOvoz.wav([0.1], chastota: 44100)), nil)
        tengmi("WAV emas", SaqlanmaganOvoz.namunalar(Data(repeating: 0, count: 100)), nil)
        // Sarlavhada koʻrsatilgandan kam maʼlumot (yozish uzilgan) — borini oʻqiydi.
        let toliq = SaqlanmaganOvoz.wav([0.1, 0.2, 0.3])
        tengmi("kesilgan", SaqlanmaganOvoz.namunalar(toliq.prefix(44 + 4))?.count, 2)
    }

    testQosh("saqlanmagan — nom ↔ sana") {
        let t = Date(timeIntervalSince1970: 1_791_000_000.123)
        let n = SaqlanmaganOvoz.nom(t)
        tekshir("kengaytma", n.hasSuffix(".wav"))
        tekshir(
            "aylanma (ms aniqlikda)",
            abs((SaqlanmaganOvoz.sana(nomdan: n)?.timeIntervalSince1970 ?? 0) - t.timeIntervalSince1970) < 0.001)
        tengmi("begona nom", SaqlanmaganOvoz.sana(nomdan: "ovoz.wav"), nil)
        tengmi("kengaytmasiz", SaqlanmaganOvoz.sana(nomdan: String(n.dropLast(4))), nil)
        let keyin = SaqlanmaganOvoz.nom(t.addingTimeInterval(1))
        tekshir("alifbo tartibi = vaqt tartibi", n < keyin)
    }

    testQosh("saqlanmagan — 20 ta va 7 kun qoidasi") {
        let hozir = Date(timeIntervalSince1970: 1_791_000_000)
        let papka = URL(fileURLWithPath: "/x")
        func f(_ soatOldin: Double) -> URL {
            papka.appendingPathComponent(SaqlanmaganOvoz.nom(hozir.addingTimeInterval(-soatOldin * 3600)))
        }
        // 25 ta yangi (har soatda bittadan) — eng eski 5 tasi ortiqcha.
        let yangilar = (0..<25).map { f(Double($0)) }
        tengmi("20 dan ortigʻi", Set(SaqlanmaganOvoz.ortiqcha(yangilar, hozir: hozir)), Set(yangilar[20...]))
        // 7 kundan eski — soni kam boʻlsa ham oʻchadi; 7 kun ichidagi qoladi.
        let eski = f(7 * 24 + 1), chegarada = f(7 * 24 - 1)
        tengmi("7 kun", SaqlanmaganOvoz.ortiqcha([eski, chegarada, f(0)], hozir: hozir), [eski])
        // Begona fayl hech qachon oʻchirilmaydi.
        let begona = papka.appendingPathComponent("muhim.wav")
        tengmi("begona", SaqlanmaganOvoz.ortiqcha([begona], hozir: hozir), [])
    }

    testQosh("saqlanmagan — fayl tizimi") {
        let fm = FileManager.default
        let papka = fm.temporaryDirectory.appendingPathComponent("kotib-saqlanmagan-\(UUID().uuidString)")
        defer { try? fm.removeItem(at: papka) }
        let hozir = Date(timeIntervalSince1970: 1_791_000_000)
        tengmi("papka yoʻq — boʻsh roʻyxat", SaqlanmaganOvoz.royxat(papka).count, 0)
        let birinchi = SaqlanmaganOvoz.saqla([0.1, 0.2], papka: papka, hozir: hozir)
        tekshir("saqlandi", birinchi != nil)
        try? Data("x".utf8).write(to: papka.appendingPathComponent("begona.txt"))
        for i in 1...21 {
            SaqlanmaganOvoz.saqla([0.3], papka: papka, hozir: hozir.addingTimeInterval(Double(i)))
        }
        let r = SaqlanmaganOvoz.royxat(papka)
        tengmi("20 tasi qoldi", r.count, 20)
        tekshir("eskidan yangiga", r.map(\.lastPathComponent) == r.map(\.lastPathComponent).sorted())
        tekshir("eng eskisi oʻchdi", !r.contains(birinchi!))
        tekshir("begona fayl joyida", fm.fileExists(atPath: papka.appendingPathComponent("begona.txt").path))
        let oxirgi = r.last.flatMap { try? Data(contentsOf: $0) }.flatMap { SaqlanmaganOvoz.namunalar($0) }
        tengmi("oxirgisi oʻqiladi", oxirgi?.count, 1)
    }
}
