// Foydalanuvchi clipboard'ini diktovka paytida saqlab, keyin aslidek tiklash.
//
// Nega kerak: «tez» kiritish usuli matnni clipboard + ⌘V orqali qoʻyadi.
// 1.1.0 gacha transkript clipboard'ga eski qiymat olinishidan OLDIN yozilardi
// (`finishTranscription`), shuning uchun «tiklash» transkriptning oʻzini
// qaytarardi — foydalanuvchining clipboard'i HAR diktovkada yoʻqolardi; ustiga
// faqat matn tiklanardi, rasm, fayl yoki boy matn butunlay ketardi
// (barqarorlik spec'i, C1).
//
// Endi: avval barcha elementlarning barcha turlari nusxalanadi → transkript
// vaqtinchalik yoziladi va «oʻtkinchi/yashirin» deb belgilanadi (clipboard
// menejerlari uni tarixga olmaydi — matn begona roʻyxatlarda
// qolmaydi) → ⌘V → clipboard'ni boshqa hech kim oʻzgartirmagan
// boʻlsa, toʻliq tiklanadi.
//
// Windows'dagi egizagi — `win/ui/inserter.cpp` (S13, C2).

import AppKit

enum Clipboard {

    /// Clipboard'ning toʻliq nusxasi: har element uchun (tur, maʼlumot) roʻyxati.
    struct Nusxa {
        fileprivate let elementlar: [[(NSPasteboard.PasteboardType, Data)]]
        var boshmi: Bool { elementlar.isEmpty }
        /// Logga — faqat soni, mazmun emas.
        var tavsif: String {
            "\(elementlar.count) element, \(elementlar.reduce(0) { $0 + $1.count }) tur"
        }
    }

    /// nspasteboard.org kelishuvi: clipboard menejerlari (Maccy, Paste,
    /// Alfred…) bu turlar bor elementni tarixga yozmaydi.
    static let oʻtkinchiTur = NSPasteboard.PasteboardType("org.nspasteboard.TransientType")
    static let yashirinTur = NSPasteboard.PasteboardType("org.nspasteboard.ConcealedType")

    /// Hozirgi mazmunning toʻliq nusxasi. `data(forType:)` kechiktirilgan
    /// (promised) turlarni shu yerda olib qoʻyadi — keyinroq manba ilova
    /// yopilgan boʻlishi mumkin.
    static func nusxaOl(_ pb: NSPasteboard) -> Nusxa {
        let elementlar = (pb.pasteboardItems ?? []).map { element in
            element.types.compactMap { tur in element.data(forType: tur).map { (tur, $0) } }
        }.filter { !$0.isEmpty }
        return Nusxa(elementlar: elementlar)
    }

    /// Matnni vaqtincha yozadi va shu yozuvning `changeCount` ini qaytaradi —
    /// tiklashda «bizdan keyin hech kim yozmaganmi» degan tekshiruv uchun.
    @discardableResult
    static func vaqtinchaYoz(_ matn: String, _ pb: NSPasteboard) -> Int {
        pb.clearContents()
        let element = NSPasteboardItem()
        element.setString(matn, forType: .string)
        element.setData(Data(), forType: oʻtkinchiTur)
        element.setData(Data(), forType: yashirinTur)
        pb.writeObjects([element])
        return pb.changeCount
    }

    /// Nusxani aslidek qaytaradi — lekin faqat clipboard'ni `bizniki` dan
    /// keyin hech kim oʻzgartirmagan boʻlsa. Foydalanuvchi shu orada biror
    /// narsa nusxalagan boʻlsa, uning yangi tanlovi ustidan yozilmaydi.
    /// Qaytaradi: tiklandimi.
    @discardableResult
    static func tikla(_ nusxa: Nusxa, _ pb: NSPasteboard, bizniki: Int) -> Bool {
        guard pb.changeCount == bizniki else { return false }
        pb.clearContents()
        guard !nusxa.boshmi else { return true }  // avval boʻsh edi — boʻsh qoladi
        let elementlar = nusxa.elementlar.map { turlar -> NSPasteboardItem in
            let e = NSPasteboardItem()
            for (tur, maʼlumot) in turlar { e.setData(maʼlumot, forType: tur) }
            return e
        }
        return pb.writeObjects(elementlar)
    }
}
