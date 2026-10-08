// Matnni fokusdagi ilovaga kiritish: «tez» (clipboard + ⌘V, clipboard
// tiklanadi — `clipboard.swift`) yoki «sekin» (belgima-belgi, fon navbatida).

import AppKit
import ApplicationServices

// MARK: - Matnni faol input'ga kiritish (clipboard + ⌘V)

enum Inserter {
    /// Matnni faol maydonga kiritadi. Usul sozlamalardan olinadi.
    /// Accessibility ruxsati yoʻq boʻlsa false — chaqiruvchi clipboard'ga qaytadi.
    @discardableResult
    static func insert(_ text: String) -> Bool {
        RubaiLog.write("insert: len=\(text.count) mode=\(Prefs.insertMode) AXTrusted=\(AXIsProcessTrusted())")
        guard !text.isEmpty else { RubaiLog.write("insert: matn boʻsh"); return false }
        guard AXIsProcessTrusted() else { RubaiLog.write("insert: Accessibility yoʻq"); return false }
        switch Prefs.insertMode {
        case .fast: return pasteViaClipboard(text)
        case .slow: return typeCharByChar(text)
        }
    }

    /// Sekin kiritish shu navbatda ishlaydi. Belgilar orasida 1,5 ms kutiladi —
    /// 3000 belgida ~5 s, va asosiy oqimda UI shuncha qotardi (D6). Serial:
    /// ketma-ket ikki diktovkaning belgilari bir-biriga aralashmaydi.
    private static let sekinQ = DispatchQueue(
        label: "com.rubaistt.dictation.sekin-kiritish",
        qos: .userInitiated)

    /// "Sekin" usul: har bir belgini Unicode hodisa sifatida yuboradi.
    /// Clipboard'ga qoʻyish ishlamaydigan ilovalar (baʼzi terminal va oʻyinlar) uchun.
    private static func typeCharByChar(_ text: String) -> Bool {
        guard let src = CGEventSource(stateID: .combinedSessionState) else {
            RubaiLog.write("insert(sekin): CGEventSource yaratilmadi"); return false
        }
        sekinQ.async {
            for ch in text {
                var units = Array(String(ch).utf16)
                guard let down = CGEvent(keyboardEventSource: src, virtualKey: 0, keyDown: true),
                    let up = CGEvent(keyboardEventSource: src, virtualKey: 0, keyDown: false)
                else {
                    RubaiLog.write("insert(sekin): hodisa yaratilmadi — toʻxtatildi"); return
                }
                down.keyboardSetUnicodeString(stringLength: units.count, unicodeString: &units)
                up.keyboardSetUnicodeString(stringLength: units.count, unicodeString: &units)
                down.post(tap: .cgAnnotatedSessionEventTap)
                up.post(tap: .cgAnnotatedSessionEventTap)
                // Juda tez yuborilsa ilovalar belgilarni tashlab yuboradi
                usleep(1500)
            }
            RubaiLog.write("insert(sekin): \(text.count) belgi yuborildi")
        }
        return true
    }

    /// "Tez" usul: clipboard + ⌘V. Foydalanuvchining clipboard'i (barcha
    /// turlari bilan) oldin saqlanadi va keyin aslidek tiklanadi — clipboard.swift.
    private static func pasteViaClipboard(_ text: String) -> Bool {
        let pb = NSPasteboard.general
        let nusxa = Clipboard.nusxaOl(pb)
        let bizniki = Clipboard.vaqtinchaYoz(text, pb)
        let src = CGEventSource(stateID: .combinedSessionState)
        let vKey: CGKeyCode = 9  // 'v'
        let down = CGEvent(keyboardEventSource: src, virtualKey: vKey, keyDown: true)
        down?.flags = .maskCommand
        let up = CGEvent(keyboardEventSource: src, virtualKey: vKey, keyDown: false)
        up?.flags = .maskCommand
        down?.post(tap: .cgAnnotatedSessionEventTap)
        up?.post(tap: .cgAnnotatedSessionEventTap)
        RubaiLog.write("⌘V yuborildi (down=\(down != nil) up=\(up != nil)), clipboard nusxasi: \(nusxa.tavsif)")
        // Qabul qiluvchi ilova ⌘V ni qayta ishlab, clipboard'ni oʻqib ulgurishi
        // kerak. 1 s — ogʻir yuklangan Electron ilovalari uchun ham yetarli;
        // shu orada foydalanuvchi oʻzi nusxalasa, `tikla` uning tanlovini buzmaydi.
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
            let tiklandi = Clipboard.tikla(nusxa, pb, bizniki: bizniki)
            RubaiLog.write(tiklandi ? "clipboard tiklandi" : "clipboard tiklanmadi — oraliqda oʻzgargan")
        }
        return true
    }
}
