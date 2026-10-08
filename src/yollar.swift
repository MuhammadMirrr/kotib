// Kotib — ilova fayllari turadigan yoʻllar, bitta joyda.
//
// Ilova avval "Audio-Matnga" deb atalgan va maʼlumotlarini
// ~/Library/Application Support/Audio-Matnga/ da saqlagan. Nom "Kotib"ga
// oʻzgargach papka nomi ham oʻzgardi — lekin mavjud foydalanuvchilarning
// diktovka tarixi va Studiya hujjatlari yoʻqolmasligi kerak. Shuning uchun
// `qollab` ga birinchi murojaatda eski papka bir marta yangisiga koʻchiriladi.
//
// Nega bitta enum: ilgari yoʻl uchta faylda (diktovka_tarixi, hujjat,
// model_download) alohida-alohida yasalar edi. Nom oʻzgarganda uchtasidan
// birini unutish — maʼlumot yoʻqotish demak. Endi manba bitta.
//
// Foundation'dan boshqa hech narsa import qilmaydi — test.sh buni qamraydi.

import Foundation

enum Yollar {

    static let papkaNomi = "Kotib"
    static let eskiPapkaNomi = "Audio-Matnga"

    /// ~/Library/Application Support
    private static func appSupport(_ fm: FileManager = .default) -> URL {
        fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
    }

    // `static let` Swift'da dangasa va ip-xavfsiz (bir marta bajariladi), shuning
    // uchun koʻchirish jarayon davomida aynan bir marta ishlaydi — kim birinchi
    // boʻlib yoʻl soʻrasa ham.
    private static let birMartaKochirildi: Bool = kochir(baza: appSupport())

    /// ~/Library/Application Support/Kotib — birinchi murojaatda eski papkani koʻchiradi.
    static var qollab: URL {
        _ = birMartaKochirildi
        return appSupport().appendingPathComponent(papkaNomi, isDirectory: true)
    }

    /// ~/Library/Application Support/Kotib/diktovka-tarixi.json
    static var diktovkaTarixi: URL { qollab.appendingPathComponent("diktovka-tarixi.json") }

    /// ~/Library/Application Support/Kotib/saqlanmagan/ — matnga oʻgirib
    /// boʻlmagan diktovka ovozi (`saqlanmagan.swift`).
    static var saqlanmagan: URL { qollab.appendingPathComponent("saqlanmagan", isDirectory: true) }

    /// ~/Library/Application Support/Kotib/hujjatlar
    static var hujjatlar: URL { qollab.appendingPathComponent("hujjatlar", isDirectory: true) }

    /// ~/Library/Application Support/Kotib/tarjima-model-33b
    ///
    /// CTranslate2 papkasi (model.bin + lugʻat + sentencepiece). `.app` ichida
    /// KELMAYDI — 3,4 GB; foydalanuvchi kerak boʻlganda yuklab oladi.
    /// Papka toʻliqmi — `TarjimaModel.tayyormi` tekshiradi.
    ///
    /// Nega nomda `-33b`: 1.3B dan 3.3B ga oʻtishda papka nomi oʻzgarishi
    /// SHART. Aks holda `TarjimaModel.tayyormi` eski 1.3B papkasini toʻliq deb
    /// oʻqiydi va yangilanish hech qachon boshlanmaydi.
    static var tarjimaModeli: URL {
        qollab.appendingPathComponent("tarjima-model-33b", isDirectory: true)
    }

    /// Eski 1.3B papkasini oʻchiradi — foydalanuvchi diskida 1,38 GB boʻshaydi.
    /// Oʻchirgan boʻlsa `true`. Xato bersa `false`, ilova baribir ishlaydi.
    ///
    /// `baza` parametri sinov uchun: testlar vaqtinchalik papka beradi.
    @discardableResult
    static func eskiTarjimaModeliniOchir(baza: URL, fm: FileManager = .default) -> Bool {
        let eski = baza.appendingPathComponent("tarjima-model", isDirectory: true)
        guard fm.fileExists(atPath: eski.path) else { return false }
        do {
            try fm.removeItem(at: eski)
            return true
        } catch {
            return false
        }
    }

    /// ~/Library/Application Support/Kotib/models/ggml-rubaistt.bin
    ///
    /// Model faylining nomi ATAYLAB oʻzgarmadi: u ichki fayl, foydalanuvchi uni
    /// koʻrmaydi, lekin CDN'dagi nusxa ham shu nom bilan yotibdi (823 MB).
    /// Ilova ichidagi yuklovchi modelni SHU yerga yozadi.
    static var model: URL { qollab.appendingPathComponent("models/ggml-rubaistt.bin") }

    /// ~/Library/Application Support/Kotib/models — foydalanuvchi modellari papkasi.
    static var foydalanuvchiModellari: URL { model.deletingLastPathComponent() }

    /// /Library/Application Support/Kotib/models — 1.2 dan boshlab `.pkg` modelni
    /// SHU yerga qoʻyadi (root, hamma foydalanuvchiga umumiy, faqat oʻqish).
    /// Nega bundle'dan tashqarida: avto-yangilanish (Sparkle) butun bundle'ni
    /// almashtiradi — model ichida boʻlsa har yangilanish 800 MB boʻlardi; bundle
    /// esa foydalanuvchiniki boʻlishi kerak (parolsiz yangilanish). Spec D2.
    static let tizimModellari = URL(
        fileURLWithPath: "/Library/Application Support/\(papkaNomi)/models",
        isDirectory: true)

    /// ~/rubai-stt/models — eski dasturchi yoʻli (setup.sh hali shu yerga qoʻyadi).
    static var eskiModellar: URL {
        FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("rubai-stt/models", isDirectory: true)
    }

    /// ~/Library/Logs/Kotib.log — Console.app shu papkani oʻzi koʻrsatadi.
    /// Ilgari log ~/rubai-stt/dictation.log da edi; eski fayl koʻchirilmaydi
    /// (log — bir martalik maʼlumot), shunchaki ishlatilmay qoladi.
    static var log: URL {
        let papka = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Logs", isDirectory: true)
        try? FileManager.default.createDirectory(at: papka, withIntermediateDirectories: true)
        return papka.appendingPathComponent("Kotib.log")
    }

    /// Eski nomdagi papkani yangisiga koʻchiradi. Koʻchirgan boʻlsa `true`.
    ///
    /// Qoidalar — hech qanday holatda maʼlumot yoʻqolmaydi:
    ///   • yangi papka allaqachon bor  → tegilmaydi (ustiga yozish yoʻq)
    ///   • eski papka yoʻq             → tegilmaydi (boʻsh papka ham yaratilmaydi)
    ///   • koʻchirish xato bersa       → `false`, ilova baribir ishlaydi
    ///
    /// `baza` parametri sinov uchun: testlar vaqtinchalik papka beradi.
    @discardableResult
    static func kochir(baza: URL, fm: FileManager = .default) -> Bool {
        let eski = baza.appendingPathComponent(eskiPapkaNomi, isDirectory: true)
        let yangi = baza.appendingPathComponent(papkaNomi, isDirectory: true)
        guard fm.fileExists(atPath: eski.path), !fm.fileExists(atPath: yangi.path) else {
            return false
        }
        do {
            try fm.moveItem(at: eski, to: yangi)
            return true
        } catch {
            return false
        }
    }
}
