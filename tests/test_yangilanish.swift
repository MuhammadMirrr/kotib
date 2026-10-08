// Avto-yangilanish mantigʻi testlari — `yangilanish_siyosat.swift` va `imzo.swift`.
//
// Holatlarning oʻzi bu yerda EMAS: ular `tests/umumiy/yangilanish_holatlari.def`
// da va Windows testi (`win/tests/test_yangilanish.cpp`) xuddi shu faylni
// `#include` qiladi. Bu fayl jadvalni ish paytida oʻqiydi va har qatorni
// macOS kodiga beradi — shunda ikki platforma bitta jadvaldan sinaladi.
//
// Nega versiya taqqoslash muhim: satr sifatida "1.10.0" < "1.9.0" chiqadi va
// foydalanuvchi oʻn birinchi relizdan keyin yangilanishni umuman koʻrmasdi.

import CryptoKit
import Foundation

func yangilanishTestlari() {
    let qatorlar = umumiyJadval()
    tekshir("umumiy jadval topildi va oʻqildi", qatorlar.count > 100)

    var sinovUrugʻi = Data(), sinovOchiq = Data(), boshqaOchiq = Data()
    for (nom, a) in qatorlar {
        switch nom {
        case "SINOV_KALITI": sinovUrugʻi = hexdan(a[0]); sinovOchiq = hexdan(a[1])
        case "BOSHQA_KALIT": boshqaOchiq = hexdan(a[0])
        default: break
        }
    }

    for (nom, a) in qatorlar {
        switch nom {
        case "VERSIYA":
            tengmi("taqqosla(\(a[0]), \(a[1]))", YangilanishSiyosati.taqqosla(a[0], a[1]), Int(a[2])!)

        case "FORMAT":
            tengmi("format «\(a[0])»", YangilanishSiyosati.versiyaFormatimi(a[0]), a[1] == "true")

        case "SIYOSAT":
            let k = Int64(a[7])!
            let q = YangilanishSiyosati.qaror(
                joriy: a[1], versiya: a[2], minVersiya: a[3],
                muhlatSoat: Int(a[4])!, foiz: Int(a[5])!, chelak: Int(a[6])!,
                koʻrilgan: k < 0 ? nil : k, hozir: Int64(a[8])!)
            tengmi("siyosat: \(a[0])", q.rawValue, a[9].lowercased())

        case "URL":
            let tur: YangilanishSiyosati.URLTuri = a[1] == "FAYL_URL" ? .fayl : .manifest
            tengmi("url \(a[1]) «\(a[0])»", YangilanishSiyosati.urlRuxsatmi(a[0], tur), a[2] == "true")

        case "BASE64":
            let d = Data(base64Encoded: a[1])
            tengmi("base64: \(a[0])", d.map { hexga($0) } ?? "RAD", a[2])

        case "ED25519":
            let ok = Imzo.ed25519Tekshir(ochiqKalit: hexdan(a[1]), xabar: hexdan(a[2]), imzo: hexdan(a[3]))
            tengmi("ed25519: \(a[0])", ok, a[4] == "true")

        case "MANIFEST":
            let javob = imzoliJavob(manifest: Data(a[2].utf8), rejim: a[3], urugʻ: sinovUrugʻi)
            let kalit = a[3] == "BOSHQA_KALIT" ? boshqaOchiq : sinovOchiq
            let m = YangilanishSiyosati.javobniTekshir(javob, ochiqKalit: kalit, platforma: a[1])
            tengmi("manifest: \(a[0])", m.map(xulosa) ?? "RAD", a[4])

        case "QAYTA":
            let k = Int(a[1])!
            tengmi(
                "qayta urinish \(a[0])", YangilanishSiyosati.qaytaUrinishKechikishi(Int(a[0])!),
                k < 0 ? nil : TimeInterval(k))

        case "UYGONISH":
            tengmi(
                "tekshirish vaqti: \(a[0])",
                YangilanishSiyosati.uygʻonishdaTekshirish(oxirgiMuvaffaqiyat: sana(a[1]), hozir: sana(a[2])!),
                a[3] == "true")

        case "BOSH_PAYT":
            tengmi(
                "boʻsh payt: \(a[0])",
                YangilanishSiyosati.ornatishMumkinmi(
                    band: a[1] == "true", oxirgiFaollik: sana(a[2]), kutishBoshlandi: sana(a[3])!,
                    hozir: sana(a[4])!),
                a[5] == "true")

        case "MAJBURIY":
            let k = Int64(a[4])!
            let q = YangilanishSiyosati.majburiyQaror(
                joriy: a[1], min: a[2], muhlatSoat: Int(a[3])!, koʻrilgan: k < 0 ? nil : k,
                hozir: Int64(a[5])!)
            tengmi("majburiy: \(a[0])", q.rawValue, a[6].lowercased())

        case "IZOH":
            tengmi("izoh: \(a[0])", YangilanishSiyosati.qisqaIzoh(a[1]), a[2])

        case "JAVOB_XOM":
            let m = YangilanishSiyosati.javobniTekshir(Data(a[1].utf8), ochiqKalit: sinovOchiq, platforma: "mac")
            tengmi("javob: \(a[0])", m.map(xulosa) ?? "RAD", a[2])

        default: break
        }
    }

    // Eski chaqiruv yoʻli (`Yangilanish.yangiroqmi`) shu funksiyaga
    // yoʻnaltirilgan — yangilanish.swift Metal import qilgani uchun bu yerda
    // kompilyatsiya qilinmaydi, shuning uchun toʻgʻridan-toʻgʻri tekshiramiz.
    tekshir("yangiroqmi = taqqosla > 0", YangilanishSiyosati.yangiroqmi("1.10.0", "1.9.0"))
}

// MARK: - Jadvalni oʻqish

/// `.def` faylidan (makro nomi, argumentlar) roʻyxati. Satr argumentlari
/// qoʻshtirnoqsiz va C qochishlari ochilgan holda qaytadi (`argumentlar`).
private func umumiyJadval() -> [(String, [String])] {
    let yol = URL(fileURLWithPath: #filePath)
        .deletingLastPathComponent()
        .appendingPathComponent("umumiy/yangilanish_holatlari.def")
    guard let matn = try? String(contentsOf: yol, encoding: .utf8) else { return [] }

    var natija: [(String, [String])] = []
    for xom in matn.split(separator: "\n", omittingEmptySubsequences: true) {
        let q = xom.trimmingCharacters(in: .whitespaces)
        guard !q.isEmpty, !q.hasPrefix("//"),
            let ochiq = q.firstIndex(of: "("), q.hasSuffix(")")
        else { continue }
        let nom = String(q[..<ochiq])
        let ichi = q[q.index(after: ochiq)..<q.index(before: q.endIndex)]
        natija.append((nom, argumentlar(ichi)))
    }
    return natija
}

/// `"a\"b", 72, TOGRI` → ["a\"b", "72", "TOGRI"]. Satr ichidagi C qochishlari
/// ochiladi: `\"`, `\\`, `\n`, `\r`, `\t` va `\uXXXX` — C++ kompilyatori
/// jadvalni aynan shunday oʻqiydi.
private func argumentlar(_ s: Substring) -> [String] {
    var natija: [String] = [], joriy = "", satrIchida = false
    var satrEdi = false
    let b = Array(s.unicodeScalars)
    var i = 0
    while i < b.count {
        let c = b[i]
        i += 1
        if satrIchida {
            if c == "\\", i < b.count {
                let e = b[i]
                i += 1
                switch e {
                case "n": joriy.unicodeScalars.append("\n")
                case "r": joriy.unicodeScalars.append("\r")
                case "t": joriy.unicodeScalars.append("\t")
                case "u" where i + 4 <= b.count:
                    let hex = String(String.UnicodeScalarView(b[i..<i + 4]))
                    joriy.unicodeScalars.append(Unicode.Scalar(UInt32(hex, radix: 16)!)!)
                    i += 4
                default: joriy.unicodeScalars.append(e)
                }
            } else if c == "\"" {
                satrIchida = false
            } else {
                joriy.unicodeScalars.append(c)
            }
        } else if c == "\"" {
            // Vergul va qoʻshtirnoq orasidagi boʻshliq satrga kirmasin.
            joriy = ""; satrIchida = true; satrEdi = true
        } else if c == "," {
            natija.append(satrEdi ? joriy : joriy.trimmingCharacters(in: .whitespaces))
            joriy = ""; satrEdi = false
        } else if !satrEdi {
            joriy.unicodeScalars.append(c)
        }
    }
    natija.append(satrEdi ? joriy : joriy.trimmingCharacters(in: .whitespaces))
    return natija
}

// MARK: - Yordamchilar

private func hexdan(_ s: String) -> Data {
    var d = Data(), b = Array(s.utf8), i = 0
    while i + 1 < b.count {
        d.append(UInt8(String(bytes: b[i...i + 1], encoding: .ascii)!, radix: 16)!)
        i += 2
    }
    return d
}

/// Jadvaldagi unix soniya → Date; -1 — «hech qachon» (nil).
private func sana(_ s: String) -> Date? {
    let n = Int64(s)!
    return n < 0 ? nil : Date(timeIntervalSince1970: TimeInterval(n))
}

private func hexga(_ d: Data) -> String { d.map { String(format: "%02x", $0) }.joined() }

/// Server javobini sinov kaliti bilan yasaydi. CryptoKit imzosi tasodifiy
/// (RFC 8032 deterministik imzosidan farq qiladi), lekin baribir haqiqiy
/// Ed25519 imzo — tekshiruv uchun shu kifoya.
private func imzoliJavob(manifest: Data, rejim: String, urugʻ: Data) -> Data {
    let kalit = try! Curve25519.Signing.PrivateKey(rawRepresentation: urugʻ)
    var imzo = try! kalit.signature(for: manifest)
    var m = manifest
    switch rejim {
    case "BUZUQ_IMZO": imzo[imzo.startIndex] ^= 1
    case "BUZUQ_XABAR": m.append(UInt8(ascii: " "))
    default: break
    }
    let json = "{\"m\":\"\(m.base64EncodedString())\",\"s\":\"\(imzo.base64EncodedString())\"}"
    return Data(json.utf8)
}

/// Tahlil natijasining matnli xulosasi — `.def` dagi kutilgan qiymat shu
/// shaklda yozilgan (Windows testi ham aynan shunday xulosa yasaydi).
private func xulosa(_ m: YangilanishSiyosati.Manifest) -> String {
    var s = "v=\(m.versiya);min=\(m.minVersiya);muhlat=\(m.muhlatSoat);foiz=\(m.foiz);izoh=\(m.izoh)"
    for arx in m.fayllar.keys.sorted() {
        let f = m.fayllar[arx]!
        s += ";\(arx)=\(f.url)|\(f.hajm)|\(f.sha256)|imzo\(f.imzo.count)"
    }
    return s
}
