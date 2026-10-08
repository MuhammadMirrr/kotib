// Diktovka tarixi — ⌃⌥D bilan aytilgan matnlar roʻyxati.
//
// ATAYLAB sof Foundation: AppKit, whisper yoki Keychain import qilinmaydi.
// Shu tufayli src/test.sh uni haqiqatan test qila oladi (test.sh AppKit'ga
// bogʻlangan fayllarni kompilyatsiya qila olmaydi).
//
// HujjatOmbori'dan alohida turadi: u fayl transkripsiyalari uchun — manba fayl
// yoʻli, segment vaqt belgilari, natijalar papkasi. Diktovkada bularning hech
// biri yoʻq, va har bir 3 soniyalik diktovka uchun papka yasash kutubxonani
// axlatga toʻldiradi.

import Foundation

struct DiktovkaYozuvi: Codable, Equatable, Identifiable {
    let id: String
    let matn: String
    let sana: Date
    let davomiylik: Double

    init(
        matn: String, davomiylik: Double,
        sana: Date = Date(), id: String = UUID().uuidString
    ) {
        self.id = id
        self.matn = matn
        self.sana = sana
        self.davomiylik = davomiylik
    }
}

extension Notification.Name {
    static let diktovkaTarixYangilandi = Notification.Name("diktovkaTarixYangilandi")
}

struct DiktovkaTarixi {

    /// Roʻyxatda saqlanadigan eng koʻp yozuv soni. Undan oshgani tushib ketadi.
    static let chegara = 200

    let fayl: URL

    init(fayl: URL) { self.fayl = fayl }

    /// ~/Library/Application Support/Kotib/diktovka-tarixi.json (`Yollar`ga qarang)
    static let birgalik = DiktovkaTarixi(fayl: Yollar.diktovkaTarixi)

    /// Eng yangisi birinchi. Fayl yoʻq, buzuq yoki oʻqilmasa — boʻsh massiv.
    /// ATAYLAB xato otmaydi: tarix qoʻshimcha qulaylik, uning nosozligi
    /// diktovkani toʻxtatmasligi kerak.
    func oqi() -> [DiktovkaYozuvi] {
        guard let data = try? Data(contentsOf: fayl) else { return [] }
        let dekoder = JSONDecoder()
        dekoder.dateDecodingStrategy = .iso8601
        return (try? dekoder.decode([DiktovkaYozuvi].self, from: data)) ?? []
    }

    /// Yangi yozuvni roʻyxat boshiga qoʻyadi va chegaradan oshganini kesadi.
    @discardableResult
    func qoshish(_ y: DiktovkaYozuvi) -> Bool {
        var royxat = oqi()
        royxat.insert(y, at: 0)
        if royxat.count > Self.chegara {
            royxat = Array(royxat.prefix(Self.chegara))
        }
        return yoz(royxat)
    }

    @discardableResult
    func ochir(id: String) -> Bool {
        var royxat = oqi()
        guard let i = royxat.firstIndex(where: { $0.id == id }) else { return false }
        royxat.remove(at: i)
        return yoz(royxat)
    }

    @discardableResult
    func tozala() -> Bool { yoz([]) }

    /// Atomik yozish — uzilib qolsa yarim fayl qolmaydi.
    private func yoz(_ royxat: [DiktovkaYozuvi]) -> Bool {
        let papka = fayl.deletingLastPathComponent()
        try? FileManager.default.createDirectory(at: papka, withIntermediateDirectories: true)
        let koder = JSONEncoder()
        koder.dateEncodingStrategy = .iso8601
        guard let data = try? koder.encode(royxat) else { return false }
        do {
            try data.write(to: fayl, options: .atomic)
            return true
        } catch {
            return false
        }
    }
}
