// Kotib — matnni tarjimaga tayyorlash va natijani qayta yigʻish.
//
// Model BIR JUMLANI eng yaxshi tarjima qiladi: butun abzats berilsa sifat
// tushadi. Shuning uchun matn avval qatorlarga, keyin jumlalarga boʻlinadi.
//
// Nega natija `[[Bolak]]` — qatorlar ichida boʻlaklar: aks holda matnni qayta
// yigʻib boʻlmaydi. Yassi roʻyxatda "Salom. Xayr." (bitta qator, ikki jumla)
// va "Salom.\nXayr." (ikki qator) bir xil koʻrinadi, va tarjimadan keyin
// qatorlar chegarasi yoʻqoladi. Qatorlar boʻyicha guruhlash buni hal qiladi:
// har bir manba qatori — natijada ham bitta qator.
//
// Ikki muammo alohida hal qilinadi (ikkalasi ham lokal sinovda topilgan —
// model bunday belgilarni bilmaydi va `<unk>` qaytaradi):
//   • Uzun tire (— –) oddiy `-` ga almashtiriladi. Model uni biladi va
//     tarjimada oʻzi ham shunday chiqaradi.
//   • Emoji jumladan ajratiladi va tarjima OXIRIGA qaytariladi. Jumla ichida
//     joyida qaytarib boʻlmaydi — tarjimada soʻz tartibi oʻzgaradi. Amalda
//     emoji deyarli doim jumla oxirida turadi.
//
// Tillar roʻyxati alohida faylda — `src/tillar.swift`.
//
// Foundation'dan boshqa hech narsa import qilmaydi — test.sh buni qamraydi.

import Foundation

enum MatnBoluvchi {

    /// Qatorning bitta boʻlagi.
    /// - `jumla`: modelga beriladigan matn + tarjimadan keyin oxiriga
    ///   qaytariladigan qoʻshimcha (emoji va h.k.).
    /// - `xom`: tarjimaga umuman berilmaydi (faqat belgilardan iborat).
    enum Bolak: Equatable {
        case jumla(matn: String, qoshimcha: String)
        case xom(String)
    }

    /// Matnni qatorlarga, har qatorni boʻlaklarga ajratadi.
    /// Boʻsh qator — boʻsh roʻyxat.
    static func bol(_ matn: String) -> [[Bolak]] {
        guard !matn.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return [] }
        var qatorlar = matn.components(separatedBy: "\n").map(qatorniBol)
        // Oxiridagi boʻsh qatorlar natijaga hech narsa qoʻshmaydi.
        while let oxirgi = qatorlar.last, oxirgi.isEmpty { qatorlar.removeLast() }
        return qatorlar
    }

    /// Tarjimaga beriladigan matnlar — `yig` ularning tarjimasini SHU TARTIBDA kutadi.
    static func jumlalar(_ qatorlar: [[Bolak]]) -> [String] {
        qatorlar.flatMap { $0 }.compactMap {
            if case .jumla(let matn, _) = $0 { return matn }
            return nil
        }
    }

    static func jumlalarSoni(_ qatorlar: [[Bolak]]) -> Int { jumlalar(qatorlar).count }

    /// Tarjimalarni oʻz joyiga qoʻyib matnni qayta yigʻadi.
    /// Tarjima yetmasa — oʻsha jumlaning asl matni qoladi (ish yarmida
    /// toʻxtasa foydalanuvchi baribir toʻliq matn koʻradi).
    static func yig(_ qatorlar: [[Bolak]], tarjimalar: [String]) -> String {
        var i = 0
        var chiqish: [String] = []
        for qator in qatorlar {
            var qismlar: [String] = []
            for bolak in qator {
                switch bolak {
                case .xom(let s):
                    qismlar.append(s)
                case .jumla(let asl, let qoshimcha):
                    let t = i < tarjimalar.count ? tozala(tarjimalar[i]) : asl
                    i += 1
                    qismlar.append(t + qoshimcha)
                }
            }
            chiqish.append(qismlar.joined(separator: " "))
        }
        return chiqish.joined(separator: "\n")
    }

    /// Model chiqishini tozalaydi.
    ///
    /// Uch ish: bilmagan belgi oʻrniga qoʻyilgan `<unk>` ni olib tashlash;
    /// tinish belgisi atrofidagi ortiqcha boʻsh joyni yigʻish (model buni
    /// muntazam chiqaradi — «У женщин , принимающих»); `bolakYasa` da `-` ga
    /// aylantirilgan uzun tireni tiklash.
    ///
    /// Tire faqat ikki tomonida boʻsh joy boʻlgandagina tiklanadi — aks holda
    /// `ijtimoiy-iqtisodiy` kabi qoʻshma soʻz buzilardi.
    static func tozala(_ s: String) -> String {
        var t = s.replacingOccurrences(of: "<unk>", with: "")
        for belgi in [",", ".", ":", ";", "!", "?", ")", "»", "”"] {
            t = t.replacingOccurrences(of: " " + belgi, with: belgi)
        }
        for belgi in ["(", "«", "“"] {
            t = t.replacingOccurrences(of: belgi + " ", with: belgi)
        }
        t = t.replacingOccurrences(of: " - ", with: " — ")
        while t.contains("  ") { t = t.replacingOccurrences(of: "  ", with: " ") }
        return t.trimmingCharacters(in: .whitespaces)
    }

    // MARK: Ichki

    private static func qatorniBol(_ qator: String) -> [Bolak] {
        guard !qator.trimmingCharacters(in: .whitespaces).isEmpty else { return [] }
        var natija: [Bolak] = []
        for jumla in jumlalargaBol(qator) {
            for bolak in himoyaBoyichaAjrat(jumla) {
                qoshib(&natija, bolak)
            }
        }
        return natija
    }

    /// Harfsiz boʻlak (emoji, belgi) oʻzidan oldingi jumlaga qoʻshiladi —
    /// tarjimadan keyin oʻz joyiga qaytadi. Oldida jumla boʻlmasa (masalan
    /// butun qator emoji) — oʻzi xom boʻlib qoladi.
    ///
    /// Havola bundan MUSTASNO: u alohida xom boʻlib turishi kerak. Aks holda
    /// jumla oxiriga surilib ketardi va «Manba: havola» → «havola Manba:»
    /// tartibi buzilardi.
    private static func qoshib(_ natija: inout [Bolak], _ bolak: Bolak) {
        if case .xom(let s) = bolak, himoyalanganlar(s).isEmpty,
            let oxirgi = natija.last, case .jumla(let m, let q) = oxirgi
        {
            let qq = q + " " + s.trimmingCharacters(in: .whitespaces)
            natija[natija.count - 1] = .jumla(matn: m, qoshimcha: qq)
        } else {
            natija.append(bolak)
        }
    }

    /// Jumlani himoyalangan oraliqlar boʻyicha kesadi: matn qismlari
    /// `bolakYasa` dan oʻtadi, himoyalanganlari toʻgʻridan-toʻgʻri `.xom`.
    private static func himoyaBoyichaAjrat(_ jumla: String) -> [Bolak] {
        let oraliqlar = himoyalanganlar(jumla)
        guard !oraliqlar.isEmpty else { return [bolakYasa(jumla)] }
        var natija: [Bolak] = []
        var joriy = jumla.startIndex
        for r in oraliqlar {
            let oldi = String(jumla[joriy..<r.lowerBound])
            if !oldi.trimmingCharacters(in: .whitespaces).isEmpty {
                natija.append(bolakYasa(oldi))
            }
            natija.append(.xom(String(jumla[r])))
            joriy = r.upperBound
        }
        let qoldiq = String(jumla[joriy...])
        if !qoldiq.trimmingCharacters(in: .whitespaces).isEmpty {
            natija.append(bolakYasa(qoldiq))
        }
        return natija
    }

    /// Havola va email aniqlagichi. Bir marta quriladi — `NSDataDetector`
    /// yasash qimmat.
    private static let havolaDetektor: NSRegularExpression? =
        try? NSDataDetector(types: NSTextCheckingResult.CheckingType.link.rawValue)

    /// `@handle` va `#hashtag` — bular havola emas, alohida naqsh kerak.
    private static let handleNaqsh: NSRegularExpression? =
        try? NSRegularExpression(pattern: "[@#][\\p{L}\\p{N}_]{2,}")

    /// Jumla ichidagi tarjima qilinmaydigan oraliqlar — chapdan oʻngga,
    /// kesishmagan holda.
    ///
    /// Nega `NSDataDetector`: u `t.me/dr_azamoff` va `www.example.uz` ni
    /// topadi, lekin `3.14` va `v1.0` ga tegmaydi. Qoʻlda yozilgan TLD
    /// roʻyxati bunday aniqlikni bermaydi va yangi domen chiqqanda eskiradi.
    static func himoyalanganlar(_ jumla: String) -> [Range<String.Index>] {
        let toliq = NSRange(jumla.startIndex..., in: jumla)
        var oraliqlar: [Range<String.Index>] = []
        for d in [havolaDetektor, handleNaqsh] {
            guard let d else { continue }
            for m in d.matches(in: jumla, range: toliq) {
                if let r = Range(m.range, in: jumla) { oraliqlar.append(r) }
            }
        }
        oraliqlar.sort { $0.lowerBound < $1.lowerBound }
        // Kesishganini tashlaymiz — chapdagisi ustun.
        var toza: [Range<String.Index>] = []
        for r in oraliqlar where toza.last.map({ $0.upperBound <= r.lowerBound }) ?? true {
            toza.append(r)
        }
        return toza
    }

    /// Jumla chegaralari — ICU qoidalari boʻyicha (`enumerateSubstrings`).
    ///
    /// Nega qoʻlda `.` sanamaymiz: nuqta jumla oxiri BOʻLMAGAN holatlar koʻp —
    /// `t.me/dr_azamoff`, `3.14`, `v1.0`, qisqartmalar. Ilgari shu sabab
    /// havolalar buzilardi: `t.` alohida jumla boʻlib modelga ketib, `п.`
    /// boʻlib qaytardi. ICU bularning hammasini biladi va bu 202 tilga baravar
    /// ishlaydi — xitoy `。`, arab `؟` chegaralari ham shu yerdan chiqadi.
    private static func jumlalargaBol(_ qator: String) -> [String] {
        var natija: [String] = []
        qator.enumerateSubstrings(in: qator.startIndex..., options: .bySentences) { sub, _, _, _ in
            guard let t = sub?.trimmingCharacters(in: .whitespacesAndNewlines), !t.isEmpty else { return }
            natija.append(t)
        }
        return natija
    }

    /// Jumladan tarjima qilinmaydigan belgilarni ajratadi.
    private static func bolakYasa(_ jumla: String) -> Bolak {
        var toza = ""
        var qoshimcha = ""
        for ch in jumla {
            if ch == "\u{2014}" || ch == "\u{2013}" {  // — –
                toza.append("-")
            } else if tarjimaQilinmaydi(ch) {
                qoshimcha.append(ch)
            } else {
                toza.append(ch)
            }
        }
        // Belgi olib tashlangan joyda ikki boʻshliq qolishi mumkin.
        while toza.contains("  ") { toza = toza.replacingOccurrences(of: "  ", with: " ") }
        let kesilgan = toza.trimmingCharacters(in: .whitespaces)
        // Harf ham, raqam ham yoʻq — tarjima qiladigan narsa qolmadi.
        guard kesilgan.contains(where: { $0.isLetter || $0.isNumber }) else {
            return .xom(jumla)
        }
        let q = qoshimcha.isEmpty ? "" : " " + qoshimcha.trimmingCharacters(in: .whitespaces)
        return .jumla(matn: kesilgan, qoshimcha: q)
    }

    /// Emoji va boshqa belgi-simvollar. Model ularni bilmaydi.
    private static func tarjimaQilinmaydi(_ ch: Character) -> Bool {
        if ch.isLetter || ch.isNumber || ch.isPunctuation || ch.isWhitespace { return false }
        guard let u = ch.unicodeScalars.first else { return false }
        return u.properties.isEmoji || u.properties.isEmojiPresentation
            || (0x1F000...0x1FAFF).contains(u.value)
            || (0x2600...0x27BF).contains(u.value)
            || (0x1F1E6...0x1F1FF).contains(u.value)  // bayroq harflari
    }
}
