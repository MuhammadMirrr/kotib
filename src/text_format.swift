// Segmentlardan oʻqishga qulay matn yasaydi. LLM'siz ishlaydi —
// bu ilovaning STANDART koʻrinishi.
//
// Bu fayl sof: faqat Foundation. Testlarda alohida kompilyatsiya qilinadi,
// shuning uchun bu yerga AppKit yoki whisper bogʻliqligi QOʻSHMANG.

import Foundation

/// Whisper qaytargan bitta segment. Vaqtlar soniyada.
struct Segment: Equatable {
    let t0: Double
    let t1: Double
    let matn: String
}

/// Apostrof uslubi. `standart` — oʻzbek lotin meʼyori (U+02BB / U+02BC).
/// `oddiy` — ASCII `'`; boshqa dasturlarga nusxalashda muammosiz.
enum Apostrof {
    case standart
    case oddiy
}

// oʻ / gʻ uchun — MODIFIER LETTER TURNED COMMA
private let burilganVergul = "\u{02BB}"
// tutuq belgisi uchun — MODIFIER LETTER APOSTROPHE
private let tutuq = "\u{02BC}"

/// Whisper aralash chiqaradigan apostrof variantlari.
private let apostrofVariantlari: Set<Character> = [
    "'",  // ASCII
    "\u{2018}",  // '
    "\u{2019}",  // '
    "\u{0060}",  // `
    "\u{00B4}",  // ´
    "\u{02BB}",  // ʻ
    "\u{02BC}"  // ʼ
]

/// Barcha apostrof variantlarini bitta uslubga keltiradi.
/// `o` yoki `g` dan keyin kelsa — harf modifikatori, aks holda tutuq belgisi.
func apostrofniBirxillashtir(_ s: String, _ uslub: Apostrof) -> String {
    var natija = ""
    natija.reserveCapacity(s.count)
    var oldingi: Character? = nil
    for ch in s {
        if apostrofVariantlari.contains(ch) {
            let harfdanKeyin = oldingi.map { "oOgG".contains($0) } ?? false
            switch uslub {
            case .oddiy:
                natija.append("'")
            case .standart:
                natija.append(harfdanKeyin ? burilganVergul : tutuq)
            }
        } else {
            natija.append(ch)
        }
        oldingi = ch
    }
    return natija
}

/// Diktovka natijasini foydalanuvchiga berishdan oldingi YAGONA qadam.
///
/// Ilgari diktovka whisper matnini toʻgʻridan-toʻgʻri kiritardi:
/// `apostrofniBirxillashtir` faqat Studiyada chaqirilardi, shuning uchun
/// diktovkada ASCII `'` qolardi (tarixda 515 ta `'`, bitta ham `ʻ` yoʻq) va
/// «Oddiy apostrof» sozlamasi diktovkaga umuman taʼsir qilmasdi. Endi log,
/// tarix va kiritish — hammasi shu natijani oladi. Windows'dagi egizagi —
/// `win/core/matn_format.cpp` dagi `matnniTayyorla`; ikkalasi korpus ustida
/// solishtiriladi (`win/tests/mac/taqqoslash/`). Yangi qoida kerak boʻlsa —
/// shu yerga va u yerga birga qoʻshiladi.
///
/// Boʻshliq kesilmaydi: buni transkripsiya qatlami oʻzi qiladi (ikkala platformada).
func matnniTayyorla(_ xom: String, apostrof: Apostrof) -> String {
    apostrofniBirxillashtir(takrorniQisqartir(xom), apostrof)
}

/// Whisper takrorlanish halqasini qisqartiradi: 1–6 soʻzli ibora ketma-ket
/// kamida 4 marta kelsa, bittasi qoladi («oʻzbekiston respublikasi» ×15 →
/// bir marta). Buzuq audioda (uzoq mikrofon, karnaydan qaytgan ovoz) model
/// baʼzan hamma haroratda ham shunday chiqaradi — `temperature_inc` uni
/// faqat qisman ushlaydi (S23 oʻlchovi). Takror topilmasa satr AYNAN
/// qaytariladi; topilsa soʻzlar bitta boʻshliq bilan qayta yigʻiladi.
/// Soʻz — ASCII boʻshliq bilan ajratilgan boʻlak, solishtirish kod nuqtalari
/// boʻyicha: `matn_format.cpp` dagi egizagi bilan natija bir xil boʻlsin.
func takrorniQisqartir(_ s: String) -> String {
    let soz = s.split(separator: " ").map { Array($0.unicodeScalars) }
    var natija: [[Unicode.Scalar]] = []
    natija.reserveCapacity(soz.count)
    var qisqardi = false
    var i = 0
    while i < soz.count {
        var olindi = false
        for k in 1...kTakrorIboraMax where i + k * kTakrorMin <= soz.count {
            var marta = 1
            while i + (marta + 1) * k <= soz.count,
                soz[(i + marta * k)..<(i + (marta + 1) * k)].elementsEqual(soz[i..<(i + k)])
            {
                marta += 1
            }
            if marta >= kTakrorMin {
                natija.append(contentsOf: soz[i..<(i + k)])
                i += marta * k
                qisqardi = true
                olindi = true
                break
            }
        }
        if !olindi {
            natija.append(soz[i])
            i += 1
        }
    }
    guard qisqardi else { return s }
    var u = String.UnicodeScalarView()
    for (j, w) in natija.enumerated() {
        if j > 0 { u.append(" ") }
        u.append(contentsOf: w)
    }
    return String(u)
}

/// `takrorniQisqartir` chegaralari — `matn_format.cpp` da ham shu qiymatlar.
let kTakrorIboraMax = 6
let kTakrorMin = 4

/// Paragraf boshi va `.`, `!`, `?` dan keyingi birinchi harfni bosh harf qiladi.
func jumlaBoshiniKattalashtir(_ s: String) -> String {
    var natija = ""
    natija.reserveCapacity(s.count)
    var kutilyapti = true  // keyingi harf bosh boʻlsinmi
    for ch in s {
        if kutilyapti, ch.isLetter {
            natija.append(contentsOf: String(ch).uppercased())
            kutilyapti = false
        } else {
            natija.append(ch)
            if ch == "." || ch == "!" || ch == "?" || ch == "\n" {
                kutilyapti = true
            }
        }
    }
    return natija
}

/// Ortiqcha boʻshliqlar, tinish belgisi atrofidagi boʻshliqlar.
private func boshliqlarniTozala(_ s: String) -> String {
    var t = s
    // Tinish belgisidan OLDINGI boʻshliqni olib tashlash
    for belgi in [",", ".", "!", "?", ":", ";"] {
        t = t.replacingOccurrences(of: " \(belgi)", with: belgi)
    }
    // Ketma-ket boʻshliqlarni bittaga
    while t.contains("  ") {
        t = t.replacingOccurrences(of: "  ", with: " ")
    }
    return t.trimmingCharacters(in: .whitespaces)
}

/// Segmentlarni oʻzgartirmasdan birlashtiradi — "Asl" tabi uchun.
func xomMatn(_ segmentlar: [Segment]) -> String {
    segmentlar
        .map { $0.matn.trimmingCharacters(in: .whitespaces) }
        .filter { !$0.isEmpty }
        .joined(separator: " ")
}

// Paragraf chegarasi qoidalari (spec, 8-band)
private let qatiyPauza = 1.5  // s — shartsiz yangi paragraf
private let yumshoqPauza = 0.8  // s — paragraf yetarlicha uzun boʻlsa
private let minParagraf = 200  // belgi

/// Standart koʻrinish: paragraflarga boʻlingan, tozalangan, bosh harfli matn.
func chiroyliMatn(_ segmentlar: [Segment], apostrof: Apostrof) -> String {
    let toza = segmentlar.filter {
        !$0.matn.trimmingCharacters(in: .whitespaces).isEmpty
    }
    guard !toza.isEmpty else { return "" }

    var paragraflar: [String] = []
    var joriy = ""

    for (i, seg) in toza.enumerated() {
        let matn = seg.matn.trimmingCharacters(in: .whitespaces)
        joriy = joriy.isEmpty ? matn : joriy + " " + matn

        guard i + 1 < toza.count else { break }
        let pauza = toza[i + 1].t0 - seg.t1
        let bolinsin =
            pauza > qatiyPauza
            || (pauza > yumshoqPauza && joriy.count >= minParagraf)
        if bolinsin {
            paragraflar.append(joriy)
            joriy = ""
        }
    }
    if !joriy.isEmpty { paragraflar.append(joriy) }

    let birlashgan =
        paragraflar
        .map { boshliqlarniTozala($0) }
        .joined(separator: "\n\n")

    return jumlaBoshiniKattalashtir(apostrofniBirxillashtir(birlashgan, apostrof))
}
