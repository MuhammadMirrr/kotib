// Logga nima yoziladi — maxfiylik qoidasi (sof mantiq, test.sh sinaydi).
//
// 1.1.0 gacha log har transkriptning TOʻLIQ matnini cheksiz saqlardi
// (`~/Library/Logs/Kotib.log` 275 KB; tasodifan yozilgan 32 daqiqalik uy
// suhbati butunligicha logda edi). Qoʻllab-quvvatlash uchun log soʻralsa —
// shaxsiy matn chiqib ketardi (barqarorlik spec'i, G1). Endi:
//   • standart — matn YOZILMAYDI, faqat uzunligi;
//   • «Diagnostika rejimi» (Sozlamalar) — matn ham yoziladi, 24 soatdan keyin
//     oʻzi oʻchadi (unutilib qolsa ham abadiy yozib turmaydi);
//   • log 1 MB dan oshsa aylantiriladi: Kotib.log → Kotib.1.log → Kotib.2.log.
// Windows'dagi egizagi — `win/core/util.cpp` (logWrite) va `config.h`.

import Foundation

enum LogSiyosati {
    static let diagnostikaMuddati: TimeInterval = 24 * 3600
    static let chegaraBayt: Int64 = 1_048_576
    /// Asosiy fayl + nechta eski nusxa saqlanadi.
    static let eskiNusxalar = 2

    /// Diagnostika rejimi hali amaldami. `tugash` — unix soniya yoki nil.
    static func diagnostikaFaolmi(tugash: Double?, hozir: Double) -> Bool {
        guard let tugash else { return false }
        // Soat orqaga surilgan boʻlsa ham muddat 24 soatdan uzaymasin.
        return hozir < tugash && tugash - hozir <= diagnostikaMuddati
    }

    /// Transkript haqida log yozuvi: diagnostika yoqilmagan boʻlsa — faqat uzunlik.
    static func matnYozuvi(_ matn: String, diagnostika: Bool) -> String {
        diagnostika ? "'\(matn)' len=\(matn.count)" : "len=\(matn.count) (matn yozilmaydi)"
    }

    /// Aylantirish: qaysi fayl qaysiga koʻchadi, BAJARISH TARTIBIDA
    /// (eng eskisi birinchi ustidan yoziladi). `Kotib.log` uchun:
    /// [Kotib.1.log → Kotib.2.log, Kotib.log → Kotib.1.log].
    static func aylantirishRejasi(_ asosiy: URL) -> [(URL, URL)] {
        let papka = asosiy.deletingLastPathComponent()
        let nom = asosiy.deletingPathExtension().lastPathComponent
        let kengaytma = asosiy.pathExtension
        func nusxa(_ i: Int) -> URL {
            i == 0 ? asosiy : papka.appendingPathComponent("\(nom).\(i).\(kengaytma)")
        }
        return (0..<eskiNusxalar).reversed().map { (nusxa($0), nusxa($0 + 1)) }
    }
}
