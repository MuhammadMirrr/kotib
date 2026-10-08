// `model_tanlov.swift` testlari — model qayerdan yuklanadi.
//
// Asosiy sabab (A1): 1.1.0 bundle'dagi yoʻlni fayl borligini tekshirmay
// qaytarardi va whisper «Model yuklanmadi (kod 1)» bilan yiqilardi. Endi
// nomzod faqat mavjud va hajmi aniq mos boʻlsa olinadi.

import CryptoKit
import Foundation

func modelTanlovTestlari() {
    let uy = URL(fileURLWithPath: "/u/Library/Application Support/Kotib/models")
    let tizim = URL(fileURLWithPath: "/Library/Application Support/Kotib/models")
    let bundle = URL(fileURLWithPath: "/Applications/Kotib.app/Contents/Resources")
    let eski = URL(fileURLWithPath: "/u/rubai-stt/models")
    let papkalar = [uy, tizim, bundle, eski]
    let m = ModelTanlov.joriy
    func yol(_ p: URL) -> URL { p.appendingPathComponent(m.fayl) }

    testQosh("model — nomzodlar tartibi va tekshiruv") {
        // Faqat tizimda (yangi .pkg) bor.
        tengmi(
            "faqat tizim", ModelTanlov.nomzodlar(papkalar: papkalar) { $0 == yol(tizim) ? m.hajm : nil },
            [yol(tizim)])
        // Hammasida bor — tartib: foydalanuvchi, tizim, bundle, eski.
        tengmi(
            "ustuvorlik tartibi", ModelTanlov.nomzodlar(papkalar: papkalar) { _ in m.hajm },
            [yol(uy), yol(tizim), yol(bundle), yol(eski)])
        // A1: bundle'dagi model oʻchirilgan (SKIP_MODEL=1 qayta yigʻish) — tushmaydi.
        tengmi(
            "bundle'da fayl yoʻq — olinmaydi",
            ModelTanlov.nomzodlar(papkalar: [bundle, eski]) { $0 == yol(eski) ? m.hajm : nil },
            [yol(eski)])
        // Chala yuklangan yoki buzilgan fayl — hajmi mos emas.
        tengmi(
            "chala fayl — olinmaydi",
            ModelTanlov.nomzodlar(papkalar: papkalar) {
                $0 == yol(uy) ? m.hajm - 1 : ($0 == yol(tizim) ? m.hajm : nil)
            },
            [yol(tizim)])
        tengmi("hech qayerda yoʻq — boʻsh", ModelTanlov.nomzodlar(papkalar: papkalar) { _ in nil }, [])
    }

    testQosh("model — yoʻllar va jadval bir xilligi") {
        tengmi("yuklovchi joriy model nomiga yozadi", Yollar.model.lastPathComponent, m.fayl)
        tengmi(
            "foydalanuvchi modellari = Yollar.model papkasi",
            Yollar.foydalanuvchiModellari, Yollar.model.deletingLastPathComponent())
        tengmi("tizim papkasi", Yollar.tizimModellari.path, "/Library/Application Support/Kotib/models")
        tekshir("eski yoʻl ~/rubai-stt/models", Yollar.eskiModellar.path.hasSuffix("/rubai-stt/models"))

        // Hajm va sha256 — scripts/bogliqliklar.env bilan bir xil boʻlishi SHART:
        // setup.sh va oʻrnatuvchilar shu qiymatlarni tekshiradi.
        let env = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
            .appendingPathComponent("../scripts/bogliqliklar.env").standardizedFileURL
        let matn = (try? String(contentsOf: env, encoding: .utf8)) ?? ""
        func qiymat(_ k: String) -> String? {
            matn.split(separator: "\n").first { $0.hasPrefix(k + "=") }.map { String($0.dropFirst(k.count + 1)) }
        }
        tengmi("hajm = MODEL_HAJM", qiymat("MODEL_HAJM"), String(m.hajm))
        tengmi("sha256 = MODEL_SHA256", qiymat("MODEL_SHA256"), m.sha256)
    }
}

func modelSha256Testlari() {
    testQosh("model — sha256 (oqim bilan)") {
        let yol = FileManager.default.temporaryDirectory.appendingPathComponent("kotib-sha-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: yol) }
        // "abc" — FIPS 180-2 namunasi.
        try? Data("abc".utf8).write(to: yol)
        tengmi(
            "abc", ModelTanlov.faylSha256(yol),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        // 4 MB chegarasidan oshadigan fayl — boʻlaklarga boʻlinsa ham bir xil.
        let katta = Data(repeating: 0x61, count: (4 << 20) + 123)
        try? katta.write(to: yol)
        tengmi(
            "4 MB + 123 bayt", ModelTanlov.faylSha256(yol),
            SHA256.hash(data: katta).map { String(format: "%02x", $0) }.joined())
        tengmi("yoʻq fayl", ModelTanlov.faylSha256(yol.appendingPathExtension("yoq")), nil)
    }
}
