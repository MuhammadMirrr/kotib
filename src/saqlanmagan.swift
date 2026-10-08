// Matnga oʻgirib boʻlmagan diktovka ovozi (barqarorlik A2).
//
// 1.1.0 gacha transkripsiya yiqilsa (model yuklanmadi, whisper xatosi)
// yozilgan ovoz shunchaki tashlanardi — foydalanuvchi gapirgan hamma narsa
// yoʻqolardi. Endi u 16 kHz mono 16-bit WAV boʻlib `Yollar.saqlanmagan` ga
// yoziladi (~2 MB/daqiqa), model tayyor boʻlgach qayta oʻgiriladi va
// muvaffaqiyatda oʻchiriladi. Eng yangi 20 tasi va 7 kundan yangilari qoladi.
//
// Sof Foundation — `src/test.sh` testlaydi. Qayta urinish (whisper, menyu,
// overlay) `ilova_saqlanmagan.swift` da.

import Foundation

enum SaqlanmaganOvoz {
    static let chegara = 20
    static let muddat: TimeInterval = 7 * 24 * 3600
    static let chastota = 16000

    // MARK: WAV

    /// Float32 namunalar → 16-bit PCM mono WAV. [-1, 1] dan tashqarisi kesiladi.
    static func wav(_ s: [Float], chastota: Int = chastota) -> Data {
        let malumot = s.count * 2
        var d = Data(capacity: 44 + malumot)
        func u32(_ v: Int) { withUnsafeBytes(of: UInt32(v).littleEndian) { d.append(contentsOf: $0) } }
        func u16(_ v: Int) { withUnsafeBytes(of: UInt16(v).littleEndian) { d.append(contentsOf: $0) } }
        d.append(contentsOf: Array("RIFF".utf8)); u32(36 + malumot)
        d.append(contentsOf: Array("WAVE".utf8))
        d.append(contentsOf: Array("fmt ".utf8)); u32(16)
        u16(1)  // PCM
        u16(1)  // mono
        u32(chastota); u32(chastota * 2)
        u16(2); u16(16)  // blok, bit
        d.append(contentsOf: Array("data".utf8)); u32(malumot)
        let pcm = s.map { x -> Int16 in
            guard x.isFinite else { return 0 }
            return Int16((max(-1, min(1, x)) * 32767).rounded()).littleEndian
        }
        pcm.withUnsafeBytes { d.append(contentsOf: $0) }
        return d
    }

    /// `wav` yozgan formatni oʻqiydi (PCM16 mono, `chastota`). Boshqa format — nil:
    /// bu papkaga faqat ilovaning oʻzi yozadi, begona faylni taxmin qilib
    /// oʻgirishdan koʻra tashlab ketgan maʼqul.
    static func namunalar(_ d: Data, chastota: Int = chastota) -> [Float]? {
        guard d.count >= 44 else { return nil }
        let b = [UInt8](d)
        func u16(_ i: Int) -> Int { Int(b[i]) | Int(b[i + 1]) << 8 }
        func u32(_ i: Int) -> Int { u16(i) | u16(i + 2) << 16 }
        func teg(_ i: Int) -> String { String(decoding: b[i..<i + 4], as: UTF8.self) }
        guard teg(0) == "RIFF", teg(8) == "WAVE", teg(12) == "fmt ", u32(16) == 16,
            u16(20) == 1, u16(22) == 1, u32(24) == chastota, u16(34) == 16,
            teg(36) == "data"
        else { return nil }
        let n = min(u32(40), b.count - 44) / 2
        var s = [Float](repeating: 0, count: n)
        for i in 0..<n {
            s[i] = Float(Int16(bitPattern: UInt16(u16(44 + 2 * i)))) / 32767
        }
        return s
    }

    // MARK: Nom va saqlash qoidasi

    private static let df: DateFormatter = {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "yyyy-MM-dd_HH-mm-ss-SSS"
        return f
    }()

    /// `2026-10-08_14-03-22-123.wav` — alifbo tartibi = vaqt tartibi.
    static func nom(_ sana: Date) -> String { df.string(from: sana) + ".wav" }

    /// `nom` ning teskarisi. Bizning nomimiz boʻlmasa — nil.
    static func sana(nomdan n: String) -> Date? {
        guard n.hasSuffix(".wav") else { return nil }
        return df.date(from: String(n.dropLast(4)))
    }

    /// Oʻchirilishi kerak boʻlganlar: 7 kundan eskilari va eng yangi 20 tadan
    /// ortigʻi. Nomi bizniki boʻlmagan fayllarga tegilmaydi.
    static func ortiqcha(_ fayllar: [URL], hozir: Date) -> [URL] {
        let sanali =
            fayllar
            .compactMap { u in sana(nomdan: u.lastPathComponent).map { (u, $0) } }
            .sorted { $0.1 > $1.1 }
        return sanali.enumerated()
            .filter { i, e in i >= chegara || hozir.timeIntervalSince(e.1) > muddat }
            .map { $0.element.0 }
    }

    // MARK: Fayl tizimi

    /// Saqlangan ovozlar, eskisidan yangisiga — qayta urinish shu tartibda.
    static func royxat(_ papka: URL = Yollar.saqlanmagan, fm: FileManager = .default) -> [URL] {
        let hammasi = (try? fm.contentsOfDirectory(at: papka, includingPropertiesForKeys: nil)) ?? []
        return
            hammasi
            .filter { sana(nomdan: $0.lastPathComponent) != nil }
            .sorted { $0.lastPathComponent < $1.lastPathComponent }
    }

    /// Ovozni yozadi va eskilarini tozalaydi. Muvaffaqiyatda fayl yoʻli.
    @discardableResult
    static func saqla(
        _ s: [Float], papka: URL = Yollar.saqlanmagan, hozir: Date = Date(),
        fm: FileManager = .default
    ) -> URL? {
        do {
            try fm.createDirectory(at: papka, withIntermediateDirectories: true)
            let url = papka.appendingPathComponent(nom(hozir))
            try wav(s).write(to: url, options: .atomic)
            tozala(papka, hozir: hozir, fm: fm)
            return url
        } catch {
            return nil
        }
    }

    static func tozala(
        _ papka: URL = Yollar.saqlanmagan, hozir: Date = Date(),
        fm: FileManager = .default
    ) {
        for u in ortiqcha(royxat(papka, fm: fm), hozir: hozir) { try? fm.removeItem(at: u) }
    }
}
