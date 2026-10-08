import Foundation

func audioUtilTestlari() {
    testQosh("jimlik nuqtasi — oʻrtadagi jimlikni topadi") {
        // 3 soniya: baland | jim | baland
        var s = [Float](repeating: 0.5, count: namunaTezligi)  // 0–1s baland
        s += [Float](repeating: 0.0001, count: namunaTezligi)  // 1–2s jim
        s += [Float](repeating: 0.5, count: namunaTezligi)  // 2–3s baland

        let n = jimlikNuqtasi(s, oynaBoshi: 0, oynaOxiri: s.count)
        // Jim oraliq oʻrtasiga yaqin boʻlishi kerak (1.0–2.0s)
        tekshir("jim oraliqda", n >= namunaTezligi && n <= 2 * namunaTezligi)
    }

    testQosh("jimlik nuqtasi — oyna chegarasidan chiqmaydi") {
        let s = [Float](repeating: 0.3, count: namunaTezligi * 3)
        let n = jimlikNuqtasi(s, oynaBoshi: namunaTezligi, oynaOxiri: 2 * namunaTezligi)
        tekshir("oyna ichida", n >= namunaTezligi && n <= 2 * namunaTezligi)
    }

    testQosh("jimlik nuqtasi — juda qisqa kirish") {
        let s = [Float](repeating: 0.1, count: 100)
        tengmi("oxirini qaytaradi", jimlikNuqtasi(s, oynaBoshi: 0, oynaOxiri: 100), 100)
    }

    testQosh("kuchaytirish — peak 0.95 ga chiqadi") {
        let s: [Float] = [0.1, -0.2, 0.05]
        let k = kuchaytir(s)
        let peak = k.map { abs($0) }.max() ?? 0
        tekshir("peak ~0.95", abs(peak - 0.95) < 0.01)
    }

    testQosh("kuchaytirish — jim signal oʻzgarmaydi") {
        let s: [Float] = [0.00001, -0.00001]
        tengmi("oʻzgarmadi", kuchaytir(s), s)
    }

    testQosh("kuchaytirish — joyida variant bitma-bit bir xil (F7)") {
        for s: [Float] in [[0.1, -0.05, 0.02, 0], [0.00005, -0.00002], [], [0.9, -0.9], [0.001, 0.5, -0.25]] {
            var j = s
            kuchaytirJoyida(&j)
            tengmi("\(s.count) namuna", j, kuchaytir(s))
        }
    }

    testQosh("kuchaytirish — boʻsh massiv") {
        tengmi("boʻsh", kuchaytir([]), [])
    }
}
