// Avto-yangilanishning sof mantigʻi: versiya taqqoslash, siyosat qarori,
// server manifestini tekshirish va URL ruxsat roʻyxati.
//
// Hammasi tarmoqsiz va UI'siz — `src/test.sh` sinaydi. Windows'dagi egizagi:
// `win/core/yangilanish_siyosat.cpp` (+ `versiya.cpp`). Ikkalasi BITTA holatlar
// jadvalidan sinaladi (`tests/umumiy/yangilanish_holatlari.def`), chunki ikki
// platforma bir serverdan bir xil manifest oladi va bir xil qaror qabul
// qilishi shart.
//
// Spec: docs/superpowers/specs/2026-10-07-avto-yangilanish-design.md
// («Siyosat manifesti», «Majburiy yangilanish», «Xavfsizlik»).

import Foundation

enum YangilanishSiyosati {

    // MARK: Versiya

    /// `a` va `b` ni solishtiradi: -1 (`a` eski), 0, 1 (`a` yangi).
    ///
    /// Raqamli boʻlaklar son sifatida (`1.10.0 > 1.9.0` — satr solishtiruvida
    /// teskari chiqardi va foydalanuvchi yangilanishni hech qachon koʻrmasdi).
    /// `-qoʻshimcha` li versiya shu raqamning oʻzidan eski (`1.2.0-rc1 < 1.2.0`);
    /// qoʻshimchalar ichida raqamlar ham son sifatida (`sinov9 < sinov10`).
    /// Buzuq kirish yiqitmaydi: raqam boʻlmagan boʻlak 0 deb olinadi.
    static func taqqosla(_ a: String, _ b: String) -> Int {
        let (ar, aq) = ikkigaBol(a), (br, bq) = ikkigaBol(b)
        let x = raqamliBolaklar(ar), y = raqamliBolaklar(br)
        for i in 0..<max(x.count, y.count) {
            let xi = i < x.count ? x[i] : 0
            let yi = i < y.count ? y[i] : 0
            if xi != yi { return xi < yi ? -1 : 1 }
        }
        switch (aq == nil, bq == nil) {
        case (true, true): return 0
        case (true, false): return 1
        case (false, true): return -1
        default: return qoshimchaTaqqosla(aq!, bq!)
        }
    }

    static func yangiroqmi(_ a: String, _ b: String) -> Bool { taqqosla(a, b) > 0 }

    /// Manifest va `VERSION` fayli qabul qiladigan shakl:
    /// `KATTA.KICHIK.TUZATISH` va ixtiyoriy `-qoʻshimcha` (`[0-9A-Za-z.]`).
    /// `scripts/versiya.sh` va `win/CMakeLists.txt` dagi andoza bilan bir xil.
    static func versiyaFormatimi(_ v: String) -> Bool {
        let (r, q) = ikkigaBol(v)
        let bolaklar = r.split(separator: ".", omittingEmptySubsequences: false)
        guard bolaklar.count == 3,
            bolaklar.allSatisfy({ !$0.isEmpty && $0.utf8.allSatisfy(raqammi) })
        else { return false }
        guard let q else { return true }
        return !q.isEmpty && q.utf8.allSatisfy { raqammi($0) || harfmi($0) || $0 == UInt8(ascii: ".") }
    }

    // MARK: Siyosat qarori

    enum Qaror: String {
        /// Hech narsa qilinmaydi.
        case hech
        /// Oddiy yangilanish: fonda yuklab, ilova boʻsh turganda oʻrnatiladi.
        case yukla
        /// Majburiy: boʻsh paytni kutmasdan oʻrnatiladi (faqat yozuv tugashini kutadi).
        case ornat
        /// Majburiy va muhlat tugagan: oʻrnatilmaguncha diktovka bloklanadi.
        case blokla
    }

    /// Manifest va mahalliy holatdan qaror chiqaradi.
    ///
    /// - `minVersiya`: boʻsh satr — majburiy chegara yoʻq.
    /// - `chelak`: ilova birinchi ishga tushganda tanlagan 0–99 son (bosqichli
    ///   tarqatish; serverga yuborilmaydi).
    /// - `koʻrilgan`: majburiy siyosat birinchi koʻrilgan vaqt (unix soniya),
    ///   `nil` — hali koʻrilmagan. Chaqiruvchi uni diskda saqlaydi: muhlat
    ///   ilova qayta ochilganda boshidan boshlanmasligi kerak.
    static func qaror(
        joriy: String, versiya: String, minVersiya: String,
        muhlatSoat: Int, foiz: Int, chelak: Int,
        koʻrilgan: Int64?, hozir: Int64
    ) -> Qaror {
        let majburiyBor = !minVersiya.isEmpty
        // Ilova bajara olmaydigan shart (min > versiya) bilan uni abadiy
        // bloklab qoʻymaslik uchun bunday manifest yaroqsiz hisoblanadi.
        if majburiyBor && taqqosla(minVersiya, versiya) > 0 { return .hech }

        if majburiyBor && taqqosla(joriy, minVersiya) < 0 {
            // Soat orqaga surilgan boʻlsa oʻtgan vaqt manfiy chiqardi —
            // nol deb olamiz: shubhada bloklamaslik kerak.
            let otgan = koʻrilgan.map { max(0, hozir - $0) } ?? 0
            return otgan >= Int64(muhlatSoat) * 3600 ? .blokla : .ornat
        }

        if taqqosla(versiya, joriy) > 0 {
            return chelak < foiz ? .yukla : .hech
        }
        return .hech
    }

    /// Diskda saqlangan majburiy talabdan qaror (spec «Majburiy yangilanish»):
    /// ilova qayta ochilganda internetsiz ham blok holatini biladi. Natija —
    /// `.hech`, `.ornat` (muhlat ichida) yoki `.blokla`. `min` boʻsh — talab yoʻq.
    static func majburiyQaror(
        joriy: String, min: String, muhlatSoat: Int, koʻrilgan: Int64?, hozir: Int64
    ) -> Qaror {
        guard !min.isEmpty, taqqosla(joriy, min) < 0 else { return .hech }
        return qaror(
            joriy: joriy, versiya: min, minVersiya: min, muhlatSoat: muhlatSoat, foiz: 0, chelak: 0,
            koʻrilgan: koʻrilgan, hozir: hozir)
    }

    // MARK: URL ruxsat roʻyxati

    enum URLTuri {
        /// Yangilanish fayllari — faqat CDN'dan.
        case fayl
        /// Siyosat manifesti — faqat statistika Worker'idan.
        case manifest
    }

    /// Reliz izohini (Markdown yoki HTML) bannerga sigʻadigan bir qatorga
    /// aylantiradi: teglar, `#` sarlavhalar, `-`/`*` roʻyxat belgilari va
    /// `**` olib tashlanadi, qatorlar «; » bilan qoʻshiladi, 140 belgigacha.
    /// «Belgi» — Unicode skalyari, grafema emas: Windows'dagi egizagi
    /// (`qisqaIzoh`, `yangilanish_siyosat.cpp`) aynan shunday sanaydi va ikkalasi
    /// bitta jadvaldan sinaladi.
    static func qisqaIzoh(_ xom: String) -> String {
        // HTML sarlavhalari (`<h2>…</h2>`) — Markdown'dagi `#` qatorlari kabi tashlanadi.
        let sarlavhasiz = xom.replacingOccurrences(
            of: "(?is)<h[1-6][^>]*>.*?</h[1-6]>", with: "\n", options: .regularExpression)
        let tegsiz = sarlavhasiz.replacingOccurrences(of: "<[^>]+>", with: "\n", options: .regularExpression)
        var qatorlar: [String] = []
        for q in tegsiz.unicodeScalars.split(whereSeparator: yangiQatormi) {
            var t = Array(q)
            while let b = t.first, boshliqmi(b) { t.removeFirst() }
            while let o = t.last, boshliqmi(o) { t.removeLast() }
            guard let bosh = t.first, bosh != "#" else { continue }
            if t.count >= 2, t[0] == "-" || t[0] == "*", t[1] == " " { t.removeFirst(2) }
            var y: [Unicode.Scalar] = []
            var i = 0
            while i < t.count {
                if t[i] == "*", i + 1 < t.count, t[i + 1] == "*" {
                    i += 2
                } else {
                    y.append(t[i])
                    i += 1
                }
            }
            let sozlar = y.split(whereSeparator: boshliqmi).map { String(String.UnicodeScalarView($0)) }
            if !sozlar.isEmpty { qatorlar.append(sozlar.joined(separator: " ")) }
        }
        let s = Array(qatorlar.joined(separator: "; ").unicodeScalars)
        let kesilgan = s.count > 140 ? Array(s.prefix(139)) + ["…"] : s
        return String(String.UnicodeScalarView(kesilgan))
    }

    /// Swift `Character.isNewline` / `isWhitespace` (Unicode White_Space) — skalyar
    /// darajasida, Windows bilan bir xil toʻplam.
    private static func yangiQatormi(_ c: Unicode.Scalar) -> Bool {
        (0x0A...0x0D).contains(c.value) || c.value == 0x85 || c.value == 0x2028 || c.value == 0x2029
    }
    private static func boshliqmi(_ c: Unicode.Scalar) -> Bool {
        yangiQatormi(c) || [0x09, 0x20, 0xA0, 0x1680, 0x202F, 0x205F, 0x3000].contains(c.value)
            || (0x2000...0x200A).contains(c.value)
    }

    // MARK: Tekshiruv va oʻrnatish jadvali (spec «Tekshiruv jadvali», «Oʻrnatish oqimi»)

    /// Muvaffaqiyatsiz tekshiruvdan keyin keyingi urinishgacha kutish:
    /// 15 daqiqa → 1 soat → 4 soat, keyin nil — odatdagi 24 soatlik jadvalga
    /// qaytiladi. `urinish` — ketma-ket muvaffaqiyatsizliklar soni, 0 dan.
    static func qaytaUrinishKechikishi(_ urinish: Int) -> TimeInterval? {
        switch urinish {
        case 0: return 15 * 60
        case 1: return 60 * 60
        case 2: return 4 * 3600
        default: return nil
        }
    }

    /// Tekshirish vaqti keldimi (uygʻonganda ham): oxirgi MUVAFFAQIYATLI
    /// tekshiruvdan 24 soat oʻtgan, hech qachon boʻlmagan yoki u kelajakda —
    /// soat orqaga surilgan boʻlsa oʻsha sanagacha tekshiruv boʻlmay qolardi.
    static func uygʻonishdaTekshirish(oxirgiMuvaffaqiyat: Date?, hozir: Date) -> Bool {
        guard let o = oxirgiMuvaffaqiyat, hozir >= o else { return true }
        return hozir.timeIntervalSince(o) >= 24 * 3600
    }

    /// Tayyor yangilanish hozir oʻrnatilsinmi. Boʻsh payt: yozuv, fayl ishi va
    /// tarjima yoʻq (`band`), oxirgi diktovkadan kamida 2 daqiqa oʻtgan.
    /// Yangilanish 24 soatdan beri kutayotgan boʻlsa — 1 daqiqa yetadi
    /// («keyingi boʻsh daqiqada»), aks holda u faol foydalanuvchida hech qachon
    /// oʻrnatilmasligi mumkin edi.
    static func ornatishMumkinmi(
        band: Bool, oxirgiFaollik: Date?, kutishBoshlandi: Date,
        hozir: Date
    ) -> Bool {
        if band { return false }
        let tinch = oxirgiFaollik.map { hozir.timeIntervalSince($0) } ?? .infinity
        let kerak: TimeInterval = hozir.timeIntervalSince(kutishBoshlandi) >= 24 * 3600 ? 60 : 120
        return tinch >= kerak
    }

    /// URL ruxsat etilganmi.
    ///
    /// 1.1.0 da server bergan URL tekshirilmasdan ochilardi (Windows'da
    /// `ShellExecuteW` — UNC yoʻl yoki lokal `.exe` boʻlsa ishga tushardi).
    /// Qoida ataylab URL tahlilchisiz va qatʼiy: aniq prefiks, keyin faqat
    /// `[A-Za-z0-9._~/-]`, boʻsh, `.` va `..` segmentlarsiz. Shunday qilib
    /// ikki platforma bir xil javob beradi va `@`, `\`, port, `?`, `#`, katta
    /// harfli xost yoki `.evil.com` qoʻshimchasi hech qachon oʻtmaydi.
    static func urlRuxsatmi(_ url: String, _ tur: URLTuri) -> Bool {
        let prefiks = tur == .fayl ? "https://cdn.mirqobilov.com/" : "https://stat.mirqobilov.com/"
        let b = Array(url.utf8), p = Array(prefiks.utf8)
        guard b.count > p.count, b.count <= 2048, Array(b[..<p.count]) == p else { return false }
        let yol = b[p.count...]
        guard yol.allSatisfy({ raqammi($0) || harfmi($0) || "._~/-".utf8.contains($0) }) else { return false }
        let segmentlar = yol.split(separator: UInt8(ascii: "/"), omittingEmptySubsequences: false)
        return segmentlar.allSatisfy { s in
            !s.isEmpty && !s.elementsEqual(".".utf8) && !s.elementsEqual("..".utf8)
        }
    }

    // MARK: Manifest

    struct Fayl: Equatable {
        let url: String
        let hajm: Int64
        let sha256: String
        let imzo: Data
    }

    struct Manifest: Equatable {
        let platforma: String
        let versiya: String
        /// Boʻsh satr — majburiy chegara yoʻq.
        let minVersiya: String
        let muhlatSoat: Int
        let foiz: Int
        let izoh: String
        /// Faqat Windows: arxitektura (`x64`, `arm64`) → fayl. macOS'da boʻsh —
        /// u yerda yuklashni Sparkle appcast orqali qiladi.
        let fayllar: [String: Fayl]
    }

    /// Server javobini tekshiradi: `{"m": base64(manifest), "s": base64(imzo)}`.
    /// Avval imzo `m` ning AYNAN baytlari ustidan tekshiriladi, faqat shundan
    /// keyin `m` oʻqiladi — imzosiz yoki buzilgan javob tahlilga yetib bormaydi.
    static func javobniTekshir(_ javob: Data, ochiqKalit: Data, platforma: String) -> Manifest? {
        guard let obj = try? JSONSerialization.jsonObject(with: javob) as? [String: Any],
            let mS = obj["m"] as? String, let sS = obj["s"] as? String,
            let m = Data(base64Encoded: mS), let s = Data(base64Encoded: sS),
            Imzo.ed25519Tekshir(ochiqKalit: ochiqKalit, xabar: m, imzo: s)
        else { return nil }
        return manifestniAjrat(m, platforma: platforma)
    }

    /// Imzosi tekshirilgan manifest baytlarini oʻqiydi. Biror maydon notoʻgʻri
    /// boʻlsa butun manifest rad etiladi (yarim toʻgʻri manifest bilan ishlash
    /// — xavfli taxmin).
    static func manifestniAjrat(_ bayt: Data, platforma: String) -> Manifest? {
        guard let o = try? JSONSerialization.jsonObject(with: bayt) as? [String: Any],
            o["platforma"] as? String == platforma,
            let versiya = o["versiya"] as? String, versiyaFormatimi(versiya)
        else { return nil }

        var min = ""
        if let qiymat = o["min_versiya"] {
            guard let s = qiymat as? String, versiyaFormatimi(s), taqqosla(s, versiya) <= 0 else { return nil }
            min = s
        }
        guard let muhlat = butunSon(o["majburiy_muhlat_soat"], sukut: 72, oraliq: 0...8760),
            let foiz = butunSon(o["tarqatish_foiz"], sukut: 100, oraliq: 0...100)
        else { return nil }

        var izoh = ""
        if let qiymat = o["izoh"] {
            guard let s = qiymat as? String else { return nil }
            izoh = s
        }

        var fayllar: [String: Fayl] = [:]
        if platforma == "win" {
            guard let f = o["fayllar"] as? [String: Any], !f.isEmpty else { return nil }
            for (arx, qiymat) in f {
                guard arx == "x64" || arx == "arm64",
                    let d = qiymat as? [String: Any],
                    let url = d["url"] as? String, urlRuxsatmi(url, .fayl),
                    let hajm = butunSon(d["hajm"], sukut: nil, oraliq: 1...1_000_000_000_000),
                    let sha = d["sha256"] as? String, sha256Formatimi(sha),
                    let imzoS = d["imzo"] as? String, let imzo = Data(base64Encoded: imzoS),
                    imzo.count == 64
                else { return nil }
                fayllar[arx] = Fayl(url: url, hajm: Int64(hajm), sha256: sha, imzo: imzo)
            }
        }

        return Manifest(
            platforma: platforma, versiya: versiya, minVersiya: min,
            muhlatSoat: muhlat, foiz: foiz, izoh: izoh, fayllar: fayllar)
    }

    // MARK: Yordamchilar

    /// "1.2.0-rc1" → ("1.2.0", "rc1"); qoʻshimchasiz — ("1.2.0", nil).
    private static func ikkigaBol(_ v: String) -> (String, String?) {
        guard let i = v.firstIndex(of: "-") else { return (v, nil) }
        return (String(v[..<i]), String(v[v.index(after: i)...]))
    }

    /// Boʻsh boʻlak ham 0 — Windows'dagi `versiyaTaqqosla` bilan bir xil.
    private static func raqamliBolaklar(_ s: String) -> [Int] {
        s.split(separator: ".", omittingEmptySubsequences: false).map { sonYokiNol(Array($0.utf8)) }
    }

    /// Faqat raqamlardan iborat boʻlsa — son (milliarddan katta boʻlsa
    /// 999 999 999 da toʻxtaydi, toshib ketmasin), aks holda 0.
    private static func sonYokiNol(_ b: [UInt8]) -> Int {
        guard !b.isEmpty, b.allSatisfy(raqammi) else { return 0 }
        var n = 0
        for c in b {
            n = n * 10 + Int(c - UInt8(ascii: "0"))
            if n > 999_999_999 { return 999_999_999 }
        }
        return n
    }

    private static func qoshimchaTaqqosla(_ a: String, _ b: String) -> Int {
        let x = a.split(separator: ".", omittingEmptySubsequences: false)
        let y = b.split(separator: ".", omittingEmptySubsequences: false)
        for i in 0..<min(x.count, y.count) {
            let k = identifikatorTaqqosla(Array(x[i].utf8), Array(y[i].utf8))
            if k != 0 { return k }
        }
        return x.count == y.count ? 0 : (x.count < y.count ? -1 : 1)
    }

    /// "sinov9" va "sinov10": raqam va harf parchalari alohida solishtiriladi,
    /// raqamlar son sifatida. Raqam parchasi harf parchasidan kichik (semver).
    private static func identifikatorTaqqosla(_ a: [UInt8], _ b: [UInt8]) -> Int {
        let x = parchalar(a), y = parchalar(b)
        for i in 0..<min(x.count, y.count) {
            let (xr, xp) = x[i], (yr, yp) = y[i]
            if xr && yr {
                let (m, n) = (sonYokiNol(xp), sonYokiNol(yp))
                if m != n { return m < n ? -1 : 1 }
            } else if xr != yr {
                return xr ? -1 : 1
            } else if xp != yp {
                return xp.lexicographicallyPrecedes(yp) ? -1 : 1
            }
        }
        return x.count == y.count ? 0 : (x.count < y.count ? -1 : 1)
    }

    /// Ketma-ket raqamlar va raqam boʻlmaganlar parchalari: (raqammi, baytlar).
    private static func parchalar(_ b: [UInt8]) -> [(Bool, [UInt8])] {
        var natija: [(Bool, [UInt8])] = []
        for c in b {
            let r = raqammi(c)
            if let oxirgi = natija.last, oxirgi.0 == r {
                natija[natija.count - 1].1.append(c)
            } else {
                natija.append((r, [c]))
            }
        }
        return natija
    }

    /// JSON butun soni. `true`/`false` son emas (JSONSerialization ularni
    /// NSNumber qilib beradi va `as? Int` jim 1/0 ga aylantirardi), kasr ham emas.
    /// Maydon yoʻq boʻlsa — `sukut` (u ham `nil` boʻlsa — xato).
    private static func butunSon(_ qiymat: Any?, sukut: Int?, oraliq: ClosedRange<Int>) -> Int? {
        guard let qiymat else { return sukut }
        guard let n = qiymat as? NSNumber, CFGetTypeID(n) != CFBooleanGetTypeID() else { return nil }
        let d = n.doubleValue
        guard d.rounded() == d, d >= Double(oraliq.lowerBound), d <= Double(oraliq.upperBound) else { return nil }
        return Int(d)
    }

    private static func sha256Formatimi(_ s: String) -> Bool {
        s.utf8.count == 64 && s.utf8.allSatisfy { raqammi($0) || ($0 >= UInt8(ascii: "a") && $0 <= UInt8(ascii: "f")) }
    }

    private static func raqammi(_ c: UInt8) -> Bool { c >= UInt8(ascii: "0") && c <= UInt8(ascii: "9") }
    private static func harfmi(_ c: UInt8) -> Bool {
        (c >= UInt8(ascii: "a") && c <= UInt8(ascii: "z")) || (c >= UInt8(ascii: "A") && c <= UInt8(ascii: "Z"))
    }
}
