// Transkriptlarni diskda saqlaydi. Ilova yopilsa ham mehnat yoʻqolmaydi.
// Joylashuv: ~/Library/Application Support/Kotib/hujjatlar/<uuid>/

import Foundation

struct Hujjat: Codable, Identifiable {
    let id: String
    let manbaNomi: String
    let manbaYol: String
    let davomiylik: Double
    let yaratilgan: Date
}

enum MatnTuri: String {
    case chiroyli = "matn"
    case xom = "xom"
}

enum HujjatOmbori {

    /// ~/Library/Application Support/Kotib/hujjatlar (`Yollar`ga qarang)
    static var ildiz: URL { Yollar.hujjatlar }

    static func papka(id: String) -> URL {
        ildiz.appendingPathComponent(id, isDirectory: true)
    }

    /// Yoʻl boʻlgishi mumkin boʻlgan qiymatlarni tekshiradi (path traversal oldini olish).
    /// Faqat a-z, A-Z, 0-9, hyphen, underscore ruxsat beradi.
    private static func isValidComponentName(_ s: String) -> Bool {
        !s.isEmpty
            && s.allSatisfy { c in
                c.isLetter || c.isNumber || c == "-" || c == "_"
            }
    }

    /// Yangi hujjatni segmentlari bilan saqlaydi va uni qaytaradi.
    @discardableResult
    static func saqla(
        manba: URL, davomiylik: Double,
        segmentlar: [Segment], apostrof: Apostrof
    ) throws -> Hujjat {
        let h = Hujjat(
            id: UUID().uuidString,
            manbaNomi: manba.lastPathComponent,
            manbaYol: manba.path,
            davomiylik: davomiylik,
            yaratilgan: Date())
        let dir = papka(id: h.id)

        do {
            try FileManager.default.createDirectory(
                at: dir.appendingPathComponent("natijalar"),
                withIntermediateDirectories: true)

            // Segmentlar — keyinchalik qayta formatlash yoki SRT uchun
            struct SegDTO: Codable { let t0: Double; let t1: Double; let matn: String }
            let dto = segmentlar.map { SegDTO(t0: $0.t0, t1: $0.t1, matn: $0.matn) }
            try JSONEncoder().encode(dto).write(
                to: dir.appendingPathComponent("segmentlar.json"),
                options: .atomic)

            try matnSaqla(id: h.id, tur: .xom, matn: xomMatn(segmentlar))
            try matnSaqla(id: h.id, tur: .chiroyli, matn: chiroyliMatn(segmentlar, apostrof: apostrof))

            // Metadarata OXIRIDA saqlash — bu hujjat saqlanganligining belgilanishi.
            // Agar bu fayl mavjud boʻlsa, hujjat toʻliq saqlangandir.
            let enc = JSONEncoder()
            enc.dateEncodingStrategy = .iso8601
            enc.outputFormatting = [.prettyPrinted]
            try enc.encode(h).write(
                to: dir.appendingPathComponent("hujjat.json"),
                options: .atomic)

            return h
        } catch {
            // Qisman yozilgan hujjatni olib tashlash
            try? FileManager.default.removeItem(at: dir)
            throw error
        }
    }

    static func matnFayl(id: String, tur: MatnTuri) -> URL {
        papka(id: id).appendingPathComponent("\(tur.rawValue).txt")
    }

    static func matnSaqla(id: String, tur: MatnTuri, matn: String) throws {
        try matn.write(to: matnFayl(id: id, tur: tur), atomically: true, encoding: .utf8)
    }

    static func matnOqi(id: String, tur: MatnTuri) -> String {
        (try? String(contentsOf: matnFayl(id: id, tur: tur), encoding: .utf8)) ?? ""
    }

    /// LLM natijasi. `amal` — amal identifikatori (masalan "xulosa").
    static func natijaSaqla(id: String, amal: String, matn: String) throws {
        // Yoʻl traversal hujumlari oldini olish
        guard isValidComponentName(id) && isValidComponentName(amal) else {
            throw NSError(
                domain: "HujjatOmbori", code: -1,
                userInfo: [NSLocalizedDescriptionKey: "Xato nomi: \(amal)"])
        }
        let dir = papka(id: id).appendingPathComponent("natijalar")
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        try matn.write(
            to: dir.appendingPathComponent("\(amal).md"),
            atomically: true, encoding: .utf8)
    }

    static func natijaOqi(id: String, amal: String) -> String? {
        // Yoʻl traversal hujumlari oldini olish
        guard isValidComponentName(id) && isValidComponentName(amal) else {
            return nil
        }
        let f = papka(id: id).appendingPathComponent("natijalar/\(amal).md")
        return try? String(contentsOf: f, encoding: .utf8)
    }

    /// Barcha hujjatlar, yangilari birinchi.
    static func royxat() -> [Hujjat] {
        let fm = FileManager.default
        guard
            let papkalar = try? fm.contentsOfDirectory(
                at: ildiz,
                includingPropertiesForKeys: nil)
        else {
            return []
        }
        let dec = JSONDecoder()
        dec.dateDecodingStrategy = .iso8601
        return papkalar.compactMap { p -> Hujjat? in
            guard let d = try? Data(contentsOf: p.appendingPathComponent("hujjat.json")) else {
                return nil
            }
            return try? dec.decode(Hujjat.self, from: d)
        }.sorted { $0.yaratilgan > $1.yaratilgan }
    }

    static func ochir(id: String) {
        try? FileManager.default.removeItem(at: papka(id: id))
    }
}
