// Kotib — tarjima modeli papkasi joyidami va toʻliqmi.
//
// Yarim yuklangan papka bilan ishga tushish CTranslate2 ichida tushunarsiz
// xato beradi, shuning uchun papka faqat BARCHA fayllar boʻlgandagina "tayyor"
// hisoblanadi. Fayllarning boʻsh emasligi ham tekshiriladi: uzilib qolgan
// koʻchirish yoki arxiv ochish 0 baytli fayl qoldirishi mumkin.
//
// Nega `yollar.swift` da emas: u faqat yoʻllarni beradi (uning masʼuliyati
// shu). Papka toʻliqligini tekshirish — boshqa ish, va u yuklab oluvchi bilan
// UI oʻrtasida boʻlinadi.
//
// Foundation'dan boshqa hech narsa import qilmaydi — test.sh buni qamraydi.

import Foundation

enum TarjimaModel {

    /// CTranslate2 papkasidagi zarur fayllar.
    ///
    /// `tokenizer.json` ATAYLAB yoʻq: u HuggingFace tokenizatori uchun (17 MB),
    /// biz esa `sentencepiece.bpe.model` ni toʻgʻridan-toʻgʻri ishlatamiz
    /// (`tarjima_bridge.cpp`). Uni yuklab olmaslik arxivni 17 MB yengillatadi.
    static let kerakliFayllar = [
        "model.bin",
        "shared_vocabulary.json",
        "sentencepiece.bpe.model",
        "config.json"
    ]

    /// `.tar.gz` arxivining aniq hajmi — yuklab olish koʻrsatkichi uchun.
    static let taxminiyBayt: Int64 = 3_114_748_195

    /// Ochilgan papkaning hajmi. Arxivdan KATTA — int8 ogʻirliklar deyarli
    /// siqilmaydi (3,37 GB → 3,11 GB, atigi 8%).
    static let ochilganBayt: Int64 = 3_366_822_263

    /// Yuklash uchun kerakli boʻsh joy: arxiv + ochilgan nusxa.
    /// Ochilgandan keyin arxiv oʻchiriladi, lekin oʻrtada ikkalasi ham turadi.
    static var kerakliJoy: Int64 { taxminiyBayt + ochilganBayt }

    /// Papka turgan diskda `kerak` bayt boʻsh joy bormi.
    ///
    /// Nega kerak: busiz toʻla diskda yuklash 3 GB dan keyin qulardi va
    /// foydalanuvchi sababini bilmasdi. Boʻsh joyni aniqlab boʻlmasa
    /// toʻsmaymiz — noaniqlik tufayli ishlayotgan narsani buzmaslik kerak.
    static func yetarliJoyBormi(_ papka: URL, kerak: Int64) -> Bool {
        guard
            let qiymatlar = try? papka.resourceValues(
                forKeys: [.volumeAvailableCapacityForImportantUsageKey]),
            let bosh = qiymatlar.volumeAvailableCapacityForImportantUsage
        else { return true }
        return bosh >= kerak
    }

    /// ~/Library/Application Support/Kotib/tarjima-model
    static var papka: URL { Yollar.tarjimaModeli }

    static var tayyor: Bool { tayyormi(papka) }

    /// `papka` parametri sinov uchun: testlar vaqtinchalik papka beradi.
    static func tayyormi(_ papka: URL, fm: FileManager = .default) -> Bool {
        for f in kerakliFayllar {
            let u = papka.appendingPathComponent(f)
            guard fm.fileExists(atPath: u.path) else { return false }
            let hajm = (try? fm.attributesOfItem(atPath: u.path)[.size] as? NSNumber)??.int64Value ?? 0
            guard hajm > 0 else { return false }
        }
        return true
    }
}
