// Audio ustidagi sof yordamchi funksiyalar. Faqat Foundation —
// testlarda alohida kompilyatsiya qilinadi.

import Foundation

/// Whisper kutadigan namuna tezligi.
let namunaTezligi = 16000

/// Berilgan oynada eng past energiyali nuqtani topadi — uzun faylni
/// boʻlaklarga boʻlganda soʻz oʻrtasidan kesib qoʻymaslik uchun.
/// Qaytaradi: massiv boshidan namunalardagi indeks.
func jimlikNuqtasi(_ namunalar: [Float], oynaBoshi: Int, oynaOxiri: Int) -> Int {
    let boshi = max(0, oynaBoshi)
    let oxiri = min(namunalar.count, oynaOxiri)
    let oynaUzunligi = namunaTezligi / 2  // 0.5 s
    guard oxiri - boshi > oynaUzunligi * 2 else { return oxiri }

    var engYaxshiIndeks = oxiri
    var engPastEnergiya = Float.greatestFiniteMagnitude
    // 0.1 s qadam bilan siljitamiz — aniqlik yetarli, hisob arzon.
    let qadam = namunaTezligi / 10

    var i = boshi
    while i + oynaUzunligi <= oxiri {
        var yigindi: Float = 0
        var j = i
        while j < i + oynaUzunligi {
            yigindi += namunalar[j] * namunalar[j]
            j += 8  // har 8-namuna — taxminiy energiya, 8x tezroq
        }
        if yigindi < engPastEnergiya {
            engPastEnergiya = yigindi
            engYaxshiIndeks = i + oynaUzunligi / 2
        }
        i += qadam
    }
    return engYaxshiIndeks
}

/// Past signalni whisper yaxshi eshitadigan darajaga koʻtaradi.
/// Diktovka (`ilova.swift`) va Studiya (`transcribe_job.swift`) ikkalasi
/// ham shu bitta funksiyadan foydalanadi.
func kuchaytir(_ raw: [Float]) -> [Float] {
    var nusxa = raw
    kuchaytirJoyida(&nusxa)
    return nusxa
}

/// `kuchaytir` ning joyida ishlaydigan varianti — ikkinchi nusxa yaratmaydi.
///
/// Studiyada 90 daqiqalik fayl ~345 MB float32; `map` bilan nusxa olinganda
/// choʻqqi ~0,7 GB boʻlardi (barqarorlik spec'i, F7). Natija `kuchaytir` bilan
/// bitma-bit bir xil (test tekshiradi).
func kuchaytirJoyida(_ s: inout [Float]) {
    guard !s.isEmpty else { return }
    var peak: Float = 0
    for x in s { peak = max(peak, abs(x)) }
    guard peak > 0.0001 else { return }
    let gain = min(0.95 / peak, 40)
    s.withUnsafeMutableBufferPointer { b in
        for i in b.indices { b[i] *= gain }
    }
}
