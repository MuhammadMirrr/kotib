// Studiya tabining kichik koʻrinishlari: fayl tashlash qabul qiluvchi qatlam,
// dizayndagi punktirli tashlash zonasi, hujjat ikonkasi, bosiladigan yozuv.

import AppKit

/// Butun tab ustida fayl tashlashni qabul qiladi.
final class DropView: NSView {
    var onDrop: ((URL) -> Void)?
    private var faolmi = false

    override init(frame: NSRect) {
        super.init(frame: frame)
        registerForDraggedTypes([.fileURL])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    private func birinchiURL(_ sender: NSDraggingInfo) -> URL? {
        let opts: [NSPasteboard.ReadingOptionKey: Any] = [.urlReadingFileURLsOnly: true]
        guard
            let urls = sender.draggingPasteboard.readObjects(
                forClasses: [NSURL.self], options: opts) as? [URL]
        else { return nil }
        return urls.first
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation {
        guard birinchiURL(sender) != nil else { return [] }
        faolmi = true; needsDisplay = true
        return .copy
    }

    override func draggingExited(_ sender: NSDraggingInfo?) {
        faolmi = false; needsDisplay = true
    }

    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        faolmi = false; needsDisplay = true
        guard let url = birinchiURL(sender) else { return false }
        onDrop?(url)
        return true
    }

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        guard faolmi else { return }
        U.kok.withAlphaComponent(0.08).setFill()
        bounds.fill()
        let yol = NSBezierPath(
            roundedRect: bounds.insetBy(dx: 8, dy: 8),
            xRadius: 12, yRadius: 12)
        yol.lineWidth = 3
        U.kok.setStroke()
        yol.stroke()
    }
}

// MARK: - Tashlash zonasi

/// Dizayndagi katta punktir ramka: hujjat ikonkasi, sarlavha va "Fayl tanlash".
/// Butun zona bosiladi — foydalanuvchi tugmani izlab oʻtirmasin.
final class TashlashZonasi: PunktirQuti {
    var onBos: (() -> Void)?

    init() {
        super.init(fon: U.maydonFon, radius: 14, chet: U.punktir)

        let ikonka = HujjatIkonkasi()
        let sarlavha = U.yozuv("Ovozli faylni shu yerga tashlang", 20, .semibold)
        let tugma = U.asosiyTugma(
            "Fayl tanlash", target: self, action: #selector(bosildi),
            balandlik: 46, shrift: 16)

        let stack = NSStackView(views: [ikonka, sarlavha, tugma])
        stack.orientation = .vertical
        stack.alignment = .centerX
        stack.spacing = 16
        stack.translatesAutoresizingMaskIntoConstraints = false
        addSubview(stack)
        NSLayoutConstraint.activate([
            stack.centerXAnchor.constraint(equalTo: centerXAnchor),
            stack.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    @objc private func bosildi() { onBos?() }
    override func mouseDown(with event: NSEvent) { onBos?() }
}

/// 46×58 hujjat ikonkasi — ichida uchta matn chizigʻi (uchinchisi kaltaroq).
/// SF Symbol emas: dizayndagi shakl aniq, ramkasi ingichka va nisbatlari oʻziga xos.
final class HujjatIkonkasi: NSView {
    init() {
        super.init(frame: .zero)
        wantsLayer = true
        translatesAutoresizingMaskIntoConstraints = false
        layer?.borderColor = U.matn4.cgColor
        layer?.borderWidth = 2
        layer?.cornerRadius = 7
        layer?.cornerCurve = .continuous

        var oldingi: NSView?
        for (i, kenglikNisbati) in [1.0, 1.0, 0.6].enumerated() {
            let chiziq = NSView()
            chiziq.wantsLayer = true
            chiziq.layer?.backgroundColor = U.rang(0xC4C4CA).cgColor
            chiziq.layer?.cornerRadius = 1
            chiziq.translatesAutoresizingMaskIntoConstraints = false
            addSubview(chiziq)
            NSLayoutConstraint.activate([
                chiziq.heightAnchor.constraint(equalToConstant: 2),
                chiziq.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 10),
                chiziq.widthAnchor.constraint(
                    equalTo: widthAnchor,
                    multiplier: kenglikNisbati * 26.0 / 46.0),
                oldingi == nil
                    ? chiziq.topAnchor.constraint(equalTo: topAnchor, constant: 18)
                    : chiziq.topAnchor.constraint(equalTo: oldingi!.bottomAnchor, constant: 7)
            ])
            oldingi = chiziq
            _ = i
        }

        NSLayoutConstraint.activate([
            widthAnchor.constraint(equalToConstant: 46),
            heightAnchor.constraint(equalToConstant: 58)
        ])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }
}

/// Bosiladigan matn ("‹ Audio"). NSButton emas: dizaynda u oddiy koʻk yozuv,
/// hech qanday ramka yoki fon yoʻq.
final class BosiladiganYozuv: NSTextField {
    var onBos: (() -> Void)?

    init(_ matn: String, rang: NSColor, size: CGFloat, weight: NSFont.Weight) {
        super.init(frame: .zero)
        stringValue = matn
        font = U.f(size, weight)
        textColor = rang
        isEditable = false
        isBordered = false
        isSelectable = false
        drawsBackground = false
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    override func mouseDown(with event: NSEvent) { onBos?() }

    // Accessibility'da TUGMA: aks holda VoiceOver uni oddiy matn deb oʻqiydi va
    // «Qoʻshimcha sozlamalar» kabi havolani bosib boʻlmasdi (S23 sinovida topildi).
    override func accessibilityRole() -> NSAccessibility.Role? { .button }
    override func accessibilityLabel() -> String? { stringValue }
    override func accessibilityPerformPress() -> Bool {
        onBos?()
        return true
    }

    override func resetCursorRects() {
        addCursorRect(bounds, cursor: .pointingHand)
    }
}
