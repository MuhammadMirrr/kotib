// Nutq modeli qayerda — maʼlum modellar va nomzodlarni tanlash (sof mantiq).
//
// Nega alohida: 1.1.0 da `ModelStore.existingPath()` bundle'dagi yoʻlni fayl
// borligini TEKSHIRMAY qaytarardi. `SKIP_MODEL=1` bilan qayta yigʻilgan
// bundle'da model yoʻq, CFBundle esa Resources roʻyxatini keshlab, ishlab
// turgan jarayonga oʻlik yoʻlni beraverardi — whisper bir zumda yiqilib,
// foydalanuvchi «Model yuklanmadi (kod 1)» ni koʻrardi, yozilgan ovoz esa
// yoʻqolardi (logda 12 marta; barqarorlik spec'i, A1). Endi har nomzod
// mavjudligi va HAJMI boʻyicha tekshiriladi va bittasi yuklanmasa
// keyingisi sinaladi (`Whisper` — whisper.swift).
//
// Foundation'dan boshqa hech narsa import qilmaydi — test.sh sinaydi.

import CryptoKit
import Foundation

struct MaʼlumModel: Equatable {
    let fayl: String
    let hajm: Int64
    let sha256: String
    let tur: String
}

enum ModelTanlov {

    /// Ilova qabul qiladigan modellar. BIRINCHISI — hozir tarqatilayotgani:
    /// yuklovchi va oʻrnatuvchilar shuni beradi. Model almashtirilsa (masalan
    /// q5_k — docs/olchovlar/2026-10-07-model-kvantlash.md) yangisi BOSHIGA
    /// qoʻshiladi va eskisi roʻyxatda QOLADI — foydalanuvchi 800 MB ni qayta
    /// yuklashga majbur boʻlmasin. Hajm, sha256 — scripts/bogliqliklar.env
    /// bilan bir xil (test tekshiradi).
    static let malum: [MaʼlumModel] = [
        MaʼlumModel(
            fayl: "ggml-rubaistt.bin", hajm: 823_369_796,
            sha256: "1b02df434902015e1464611a7748927e42fcb55c49791c85239cf713c8edc1a3",
            tur: "q8_0")
    ]

    /// Tarqatilayotgan model (yuklovchi shuni yuklaydi).
    static var joriy: MaʼlumModel { malum[0] }

    /// Mavjud va hajmi maʼlum modelnikiga AYNAN teng fayllar — `papkalar`
    /// tartibida (spec D2: foydalanuvchi → tizim → bundle → eski dasturchi
    /// yoʻli). Chala yuklangan, buzilgan yoki oʻchirilgan fayl tushmaydi.
    /// `hajmi` — fayl hajmi, yoʻq boʻlsa nil (sinovda soxtalashtiriladi).
    static func nomzodlar(papkalar: [URL], hajmi: (URL) -> Int64?) -> [URL] {
        var natija: [URL] = []
        for papka in papkalar {
            for m in malum {
                let yol = papka.appendingPathComponent(m.fayl)
                if hajmi(yol) == m.hajm { natija.append(yol) }
            }
        }
        return natija
    }

    /// Faylning sha256 i (kichik harfli hex), 4 MB lik boʻlaklar bilan oqimda —
    /// 823 MB modelni xotiraga butunlay yuklamaydi. Oʻqib boʻlmasa nil.
    static func faylSha256(_ yol: URL) -> String? {
        guard let h = try? FileHandle(forReadingFrom: yol) else { return nil }
        defer { try? h.close() }
        var xesh = SHA256()
        while true {
            // `read(upToCount:)` fayl oxirida xato emas, nil qaytaradi — xatoni
            // (throw) oxirdan ajratish uchun `try?` ishlatilmaydi.
            let bolak: Data?
            do { bolak = try h.read(upToCount: 4 << 20) } catch { return nil }
            guard let b = bolak, !b.isEmpty else { break }
            xesh.update(data: b)
        }
        return xesh.finalize().map { String(format: "%02x", $0) }.joined()
    }

    /// Haqiqiy fayl tizimi uchun `hajmi`: oddiy fayl boʻlsa hajmi, aks holda nil.
    static func faylHajmi(_ yol: URL) -> Int64? {
        guard let a = try? FileManager.default.attributesOfItem(atPath: yol.path),
            (a[.type] as? FileAttributeType) == .typeRegular,
            let n = a[.size] as? NSNumber
        else { return nil }
        return n.int64Value
    }
}
