// Kotib — sozlamalar va donat oynalari.
//
// Tuzilma va matnlar ataylab Windows versiyasidagi (win/ui/settings_window.cpp,
// win/ui/donate_window.cpp) bilan bir xil — foydalanuvchi ikki platformada bir xil
// narsani koʻrishi kerak.
//
// Bitta ongli farq: Windows'da "Videokartadan foydalanish" — checkbox. macOS'da u
// maʼlumot qatori, chunki Metal Apple Silicon'da doim yoqiq, Intel build'ida esa
// umuman yoʻq — oʻchirib-yoqadigan tugma yolgʻon tugma boʻlar edi.

import AppKit

// MARK: - Matnni kiritish usuli

enum InsertMode: Int, CaseIterable {
    case fast = 0  // clipboard + ⌘V
    case slow = 1  // belgima-belgi yozish

    var label: String {
        switch self {
        case .fast: return "Tez (clipboard orqali qoʻyish) — tavsiya etiladi"
        case .slow: return "Sekin (belgima-belgi yozish) — qoʻyish ishlamasa"
        }
    }
}

// MARK: - Saqlanadigan sozlamalar

enum Prefs {
    private static let d = UserDefaults.standard

    /// nil — tizim standart mikrofoni.
    static var micUID: String? {
        get { d.string(forKey: "mic.uid") }
        set {
            if let v = newValue { d.set(v, forKey: "mic.uid") } else { d.removeObject(forKey: "mic.uid") }
        }
    }

    static var insertMode: InsertMode {
        get { InsertMode(rawValue: d.integer(forKey: "insert.mode")) ?? .fast }
        set { d.set(newValue.rawValue, forKey: "insert.mode") }
    }

    /// Chiqadigan matnda apostrof uslubi. Standart — oʻzbek lotin meʼyori.
    static var apostrof: Apostrof {
        get { UserDefaults.standard.bool(forKey: "oddiyApostrof") ? .oddiy : .standart }
        set { UserDefaults.standard.set(newValue == .oddiy, forKey: "oddiyApostrof") }
    }

    /// «Diagnostika rejimi» — yoqilsa transkript matni ham logga yoziladi va
    /// rejim 24 soatdan keyin oʻzi oʻchadi (log_siyosati.swift). Saqlanadigani —
    /// tugash vaqti (unix soniya).
    static var diagnostika: Bool {
        get {
            let t = UserDefaults.standard.object(forKey: "diagnostikaTugash") as? Double
            return LogSiyosati.diagnostikaFaolmi(tugash: t, hozir: Date().timeIntervalSince1970)
        }
        set {
            if newValue {
                UserDefaults.standard.set(
                    Date().timeIntervalSince1970 + LogSiyosati.diagnostikaMuddati,
                    forKey: "diagnostikaTugash")
            } else {
                UserDefaults.standard.removeObject(forKey: "diagnostikaTugash")
            }
        }
    }
}

/// `saqla()` (SozlamalarVC, sozlamalar_view.swift) LLM sozlamasi saqlangach
/// joʻnatadi — `StudiyaVC` shuni tinglab, Studio tugmalarini darhol yangilaydi (I1).
extension Notification.Name { static let llmSozlamaSaqlandi = Notification.Name("llmSozlamaSaqlandi") }

// MARK: - Donat oynasi

final class DonateWindow: NSObject, NSWindowDelegate {
    private var window: NSWindow?

    private struct Card {
        let name: String
        let pretty: String  // koʻrinadigan koʻrinish
        let plain: String  // nusxalanadigan koʻrinish (probelsiz)
        let color: NSColor
    }

    // Windows versiyasidagi kartalar bilan bir xil.
    private let cards = [
        Card(
            name: "HUMO", pretty: "9860 1606 0855 5431", plain: "9860160608555431",
            color: NSColor(red: 20 / 255, green: 150 / 255, blue: 200 / 255, alpha: 1)),
        Card(
            name: "VISA", pretty: "4231 2000 9261 0830", plain: "4231200092610830",
            color: NSColor(red: 35 / 255, green: 45 / 255, blue: 125 / 255, alpha: 1)),
        Card(
            name: "UZCARD", pretty: "5614 6818 5541 0035", plain: "5614681855410035",
            color: NSColor(red: 30 / 255, green: 150 / 255, blue: 100 / 255, alpha: 1))
    ]

    private var copyButtons: [NSButton] = []
    private var revertTimers: [Timer] = []

    func show() {
        if window == nil { build() }
        resetButtonTitles()
        NSApp.activate(ignoringOtherApps: true)
        window!.center()
        window!.makeKeyAndOrderFront(nil)
        window!.orderFrontRegardless()
    }

    private let W: CGFloat = 420
    // 430: karta egasi bloki 354 da tugaydi, "Yopish" tugmasi 380 dan boshlanadi
    private let H: CGFloat = 430

    private func top(_ d: CGFloat, _ h: CGFloat) -> CGFloat { H - d - h }

    private func build() {
        let w = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: W, height: H),
            styleMask: [.titled, .closable], backing: .buffered, defer: false)
        w.title = "Donat qilish"
        w.isReleasedWhenClosed = false
        w.delegate = self
        let v = NSView(frame: NSRect(x: 0, y: 0, width: W, height: H))

        let title = NSTextField(labelWithString: "Ilova bepul")
        title.font = .systemFont(ofSize: 18, weight: .bold)
        title.frame = NSRect(x: 24, y: top(22, 24), width: W - 48, height: 24)
        v.addSubview(title)

        let sub = NSTextField(
            wrappingLabelWithString:
                "Reklama yoʻq, obuna yoʻq. Agar foydali boʻlsa va imkoningiz boʻlsa, "
                + "qoʻllab-quvvatlashingiz mumkin — majburiy emas.")
        sub.font = .systemFont(ofSize: 12)
        sub.textColor = .secondaryLabelColor
        sub.frame = NSRect(x: 24, y: top(52, 36), width: W - 48, height: 36)
        v.addSubview(sub)

        // Kartalar
        var yTop: CGFloat = 104
        for (i, c) in cards.enumerated() {
            v.addSubview(cardBadge(c, y: top(yTop, 22)))

            let num = NSTextField(labelWithString: c.pretty)
            num.font = .monospacedSystemFont(ofSize: 15, weight: .medium)
            num.frame = NSRect(x: 24, y: top(yTop + 26, 20), width: 220, height: 20)
            v.addSubview(num)

            let btn = NSButton(title: "Nusxalash", target: self, action: #selector(copyCard(_:)))
            btn.bezelStyle = .rounded
            btn.tag = i
            btn.frame = NSRect(x: W - 24 - 110, y: top(yTop + 24, 26), width: 110, height: 26)
            v.addSubview(btn)
            copyButtons.append(btn)

            yTop += 66
        }

        // Karta egasi
        let sep = NSBox(frame: NSRect(x: 24, y: top(yTop + 4, 1), width: W - 48, height: 1))
        sep.boxType = .separator
        v.addSubview(sep)

        let ownerLabel = NSTextField(labelWithString: "Karta egasi")
        ownerLabel.font = .systemFont(ofSize: 11)
        ownerLabel.textColor = .secondaryLabelColor
        ownerLabel.frame = NSRect(x: 24, y: top(yTop + 18, 14), width: 200, height: 14)
        v.addSubview(ownerLabel)

        let owner = NSTextField(labelWithString: "Muhammad Mirkabilov")
        owner.font = .systemFont(ofSize: 14, weight: .medium)
        owner.frame = NSRect(x: 24, y: top(yTop + 36, 20), width: W - 48, height: 20)
        v.addSubview(owner)

        let close = NSButton(title: "Yopish", target: self, action: #selector(closeWindow))
        close.bezelStyle = .rounded
        close.keyEquivalent = "\r"
        close.frame = NSRect(x: W - 24 - 100, y: 20, width: 100, height: 30)
        v.addSubview(close)

        w.contentView = v
        window = w
    }

    /// Rangli toʻrtburchak ichida karta tizimi nomi.
    private func cardBadge(_ c: Card, y: CGFloat) -> NSView {
        let box = NSView(frame: NSRect(x: 24, y: y, width: 84, height: 22))
        box.wantsLayer = true
        box.layer?.backgroundColor = c.color.cgColor
        box.layer?.cornerRadius = 5

        let l = NSTextField(labelWithString: c.name)
        l.font = .systemFont(ofSize: 11, weight: .bold)
        l.textColor = .white
        l.alignment = .center
        l.frame = NSRect(x: 0, y: 3, width: 84, height: 16)
        box.addSubview(l)
        return box
    }

    @objc private func copyCard(_ sender: NSButton) {
        let c = cards[sender.tag]
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(c.plain, forType: .string)
        sender.title = "Nusxalandi ✓"
        let t = Timer.scheduledTimer(withTimeInterval: 1.8, repeats: false) { [weak sender] _ in
            sender?.title = "Nusxalash"
        }
        revertTimers.append(t)
    }

    private func resetButtonTitles() {
        revertTimers.forEach { $0.invalidate() }
        revertTimers.removeAll()
        copyButtons.forEach { $0.title = "Nusxalash" }
    }

    @objc private func closeWindow() { window?.close() }

    /// Sarlavha satridagi qizil yopish tugmasi ("Yopish" tugmasi emas, u
    /// closeWindow() ni chaqiradi — ikkalasi ham oxir-oqibat -close() orqali
    /// shu delegatga yetib keladi) bosilganda ham aktivlik siyosati
    /// tekshiruvi ishlashi uchun. Aks holda asosiy oyna avval yopilib, Donat
    /// ochiq qolsa, keyin Donat yopilganda hech kim policy'ni .accessory'ga
    /// qaytarmas edi — ilova Dock'da abadiy qolib ketardi.
    func windowWillClose(_ n: Notification) {
        AsosiyOyna.faollikSiyosatiniYangila()
    }
}
