// Audio/video faylni whisper kutadigan 16 kHz mono float32 ga oʻgiradi.
// ffmpeg ISHLATILMAYDI — faqat AVFoundation. Qoʻllab-quvvatlanmaydigan
// formatlar (mkv, webm, ogg) uchun tushunarli xato beriladi.

import Foundation
import AVFoundation

enum MediaXato: Error {
    case ochilmadi(String)  // kengaytma
    case audioYoliYoq
    case bosh
    case oqishXatosi
    case bekorQilindi
}

extension MediaXato {
    /// Foydalanuvchiga koʻrsatiladigan xabar — muammoni VA yechimni aytadi.
    var xabar: String {
        switch self {
        case .ochilmadi(let kengaytma):
            return "«.\(kengaytma)» fayllari qoʻllab-quvvatlanmaydi. "
                + "mp4, mov, m4a, mp3, wav yoki flac faylini tanlang."
        case .audioYoliYoq:
            return "Bu faylda ovoz yoʻli topilmadi."
        case .bosh:
            return "Fayl boʻsh."
        case .oqishXatosi:
            return "Faylni oʻqib boʻlmadi — u buzilgan boʻlishi mumkin."
        case .bekorQilindi:
            return "Bekor qilindi."
        }
    }
}

struct MediaMalumot {
    let davomiylik: Double  // soniya
}

/// `kutib` natijasi uchun quti: Task yozadi, semafordan keyin oqim oʻqiydi.
private final class NatijaQutisi<T>: @unchecked Sendable {
    var natija: Result<T, Error>?
}

/// Async ishni joriy (fon) oqimda tugashini kutib, natijasini qaytaradi.
/// AVFoundation macOS 13 dan beri xossalarni faqat async `load(...)` bilan
/// beradi, transkripsiya esa oʻz fon oqimida sinxron ishlaydi. Asosiy
/// oqimdan CHAQIRILMAYDI — u bloklanib qoladi.
private func kutib<T>(_ ish: @escaping @Sendable () async throws -> T) throws -> T {
    let quti = NatijaQutisi<T>()
    let semafor = DispatchSemaphore(value: 0)
    Task.detached {
        do { quti.natija = .success(try await ish()) } catch { quti.natija = .failure(error) }
        semafor.signal()
    }
    semafor.wait()
    return try quti.natija!.get()
}

/// Faylning audio yoʻllari. Fayl umuman ochilmasa ham boʻsh roʻyxat qaytadi —
/// chaqiruvchi buni «audio yoʻli yoʻq» deb talqin qiladi.
private func audioYollari(_ asset: AVURLAsset) -> [AVAssetTrack] {
    (try? kutib { try await asset.loadTracks(withMediaType: .audio) }) ?? []
}

/// Faylni ochib, davomiyligini qaytaradi. Audio yoʻli yoʻq boʻlsa xato beradi.
func mediaMalumot(_ url: URL) throws -> MediaMalumot {
    let asset = AVURLAsset(url: url)
    guard !audioYollari(asset).isEmpty else {
        // Fayl umuman ochilmagan boʻlsa ham shu yerga tushadi — kengaytmaga
        // qarab aniqroq xabar tanlaymiz.
        let kengaytma = url.pathExtension.lowercased()
        let qollanmaydigan = ["mkv", "webm", "ogg", "opus", "wma", "amr"]
        if qollanmaydigan.contains(kengaytma) {
            throw MediaXato.ochilmadi(kengaytma)
        }
        throw MediaXato.audioYoliYoq
    }
    guard let vaqt = try? kutib({ try await asset.load(.duration) }) else {
        throw MediaXato.oqishXatosi
    }
    let davomiylik = CMTimeGetSeconds(vaqt)
    guard davomiylik > 0, davomiylik.isFinite else { throw MediaXato.bosh }
    return MediaMalumot(davomiylik: davomiylik)
}

/// Faylning `boshi…oxiri` oraligʻini 16 kHz mono float32 qilib oʻqiydi.
/// `progress` 0…1 — shu oraliq ichidagi ulush.
func namunalarniOqi(
    _ url: URL, boshi: Double, oxiri: Double,
    progress: @escaping (Double) -> Void,
    bekor: @escaping () -> Bool
) throws -> [Float] {
    let asset = AVURLAsset(url: url)
    guard let track = audioYollari(asset).first else {
        throw MediaXato.audioYoliYoq
    }
    guard let reader = try? AVAssetReader(asset: asset) else {
        throw MediaXato.oqishXatosi
    }

    let uzunlik = max(0.001, oxiri - boshi)
    reader.timeRange = CMTimeRange(
        start: CMTime(seconds: boshi, preferredTimescale: 600),
        duration: CMTime(seconds: uzunlik, preferredTimescale: 600))

    let sozlamalar: [String: Any] = [
        AVFormatIDKey: kAudioFormatLinearPCM,
        AVSampleRateKey: namunaTezligi,
        AVNumberOfChannelsKey: 1,
        AVLinearPCMBitDepthKey: 32,
        AVLinearPCMIsFloatKey: true,
        AVLinearPCMIsNonInterleaved: false,
        AVLinearPCMIsBigEndianKey: false
    ]
    let output = AVAssetReaderTrackOutput(track: track, outputSettings: sozlamalar)
    guard reader.canAdd(output) else { throw MediaXato.oqishXatosi }
    reader.add(output)

    var namunalar = [Float]()
    // Oldindan joy ajratamiz — realloc portlashlarining oldini oladi.
    namunalar.reserveCapacity(Int(uzunlik * Double(namunaTezligi)) + namunaTezligi)

    reader.startReading()
    var oxirgiXabar: Double = 0
    // CMBlockBufferGetDataPointer nazariy jihatdan nil qaytarishi mumkin
    // (masalan uzuq-yuluq/non-contiguous bufer). Bunday holatda ovoz sukut
    // saqlab yoʻqolib ketmasin — nechta bufer tashlab yuborilganini sanaymiz
    // va oqish tugagach BITTA umumiy xabar bilan logga yozamiz (M10).
    var tashlabYuborilganBuferlar = 0

    while reader.status == .reading {
        if bekor() { reader.cancelReading(); throw MediaXato.bekorQilindi }
        guard let buf = output.copyNextSampleBuffer() else { break }
        defer { CMSampleBufferInvalidate(buf) }
        guard let bb = CMSampleBufferGetDataBuffer(buf) else { continue }

        var uzunligi = 0
        var ptr: UnsafeMutablePointer<Int8>?
        CMBlockBufferGetDataPointer(
            bb, atOffset: 0, lengthAtOffsetOut: nil,
            totalLengthOut: &uzunligi, dataPointerOut: &ptr)
        if let p = ptr {
            let soni = uzunligi / MemoryLayout<Float>.size
            p.withMemoryRebound(to: Float.self, capacity: soni) { fp in
                namunalar.append(contentsOf: UnsafeBufferPointer(start: fp, count: soni))
            }
        } else {
            tashlabYuborilganBuferlar += 1
        }

        let otgan = Double(namunalar.count) / Double(namunaTezligi)
        let ulush = min(otgan / uzunlik, 1.0)
        if ulush - oxirgiXabar > 0.02 { oxirgiXabar = ulush; progress(ulush) }
    }

    guard reader.status == .completed else {
        if reader.status == .cancelled { throw MediaXato.bekorQilindi }
        throw MediaXato.oqishXatosi
    }
    guard !namunalar.isEmpty else { throw MediaXato.bosh }
    if tashlabYuborilganBuferlar > 0 {
        RubaiLog.write(
            "media_decode: \(tashlabYuborilganBuferlar) ta bufer oʻqib boʻlmadi (dataPointer nil) — ayrim namunalar yoʻqolgan boʻlishi mumkin: \(url.lastPathComponent)"
        )
    }
    progress(1.0)
    return namunalar
}
