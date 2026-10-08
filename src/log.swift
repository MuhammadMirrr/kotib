// Ilova logi — ~/Library/Logs/Kotib.log (yoʻl: `yollar.swift`, aylantirish va
// matn qoidasi: `log_siyosati.swift`). Har qator millisekundli vaqt bilan.

import Foundation

enum RubaiLog {
    // Millisekundlar bilan: 2026-09-04 da ikkita "toggle fired" bir soniya
    // ichida kelgan va logdan ular orasidagi haqiqiy vaqtni bilib boʻlmagan.
    private static let df: ISO8601DateFormatter = {
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return f
    }()
    static func write(_ msg: String) {
        NSLog("[rubai] \(msg)")
        let line = "\(df.string(from: Date())) \(msg)\n"
        let asosiy = Yollar.log
        let path = asosiy.path
        // 1 MB dan oshsa aylantiramiz (log_siyosati.swift): log cheksiz
        // oʻsmasin. Bir nechta oqim bir vaqtda yozsa ham eng yomoni — bitta
        // ortiqcha aylantirish, maʼlumot buzilmaydi.
        if let a = try? FileManager.default.attributesOfItem(atPath: path),
            let hajm = (a[.size] as? NSNumber)?.int64Value, hajm > LogSiyosati.chegaraBayt
        {
            for (eski, yangi) in LogSiyosati.aylantirishRejasi(asosiy) {
                try? FileManager.default.removeItem(at: yangi)
                try? FileManager.default.moveItem(at: eski, to: yangi)
            }
        }
        if let h = FileHandle(forWritingAtPath: path) {
            h.seekToEndOfFile(); h.write(line.data(using: .utf8)!); try? h.close()
        } else {
            FileManager.default.createFile(atPath: path, contents: line.data(using: .utf8))
        }
    }
}
