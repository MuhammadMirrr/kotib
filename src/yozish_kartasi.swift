// Yozish tabining katta karta koʻrinishi — tinch va yozilyapti holatlari bitta
// view'da (ovoz toʻlqini, pulsatsiya, taymer) — va uning yordamchilari.

import AppKit

// MARK: - Yozish kartasi

/// Katta bosiladigan karta. Ikki holat bitta koʻrinishda: tinch (koʻk mikrofon
/// + ⌃⌥D klavishalari) va yozilyapti (qizil, pulsatsiya, ovoz toʻlqini, taymer).
///
/// Nega ikkita alohida view emas: kartaning oʻlchami, joylashuvi va bosilish
/// sohasi ikkala holatda bir xil. Ikki view boʻlsa ular orasidagi farq
/// sezilarli "sakrash" berardi va konstreyntlar ikki nusxada saqlanardi.
final class YozishKartasi: NSView {

    var onBos: (() -> Void)?

    var hotkeyMatni: String = "⌃⌥D" { didSet { klavishalarniQur() } }

    var yozilyapti: Bool = false {
        didSet {
            guard yozilyapti != oldValue else { return }
            koʻrinishniYangila()
        }
    }

    private let doira = NSView()
    private let pulsQatlam = CALayer()
    private let mikrofon = NSImageView()
    private let toxtatBelgi = NSView()
    private let sarlavha = U.yozuv("Bosing va gapiring", 20, .semibold)
    private let izoh = U.yozuv("Matn siz turgan joyga oʻzi yoziladi", 14, .regular, U.matn2)
    private let ongTaraf = NSStackView()
    private let klavishalar = NSStackView()
    private let tolqin = TolqinView()
    private let taymer = U.yozuv("0:00", 17, .semibold, U.qizil)
    private var boshlanganVaqt: Date?
    private var taymerTimer: Timer?
    private var hover = false

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        layer?.cornerRadius = U.radiusKarta
        layer?.cornerCurve = .continuous
        layer?.borderWidth = 1

        doira.translatesAutoresizingMaskIntoConstraints = false
        doira.wantsLayer = true
        doira.layer?.cornerRadius = 30
        doira.layer?.shadowOpacity = 1
        doira.layer?.shadowOffset = CGSize(width: 0, height: -6)
        doira.layer?.shadowRadius = 7

        // Pulsatsiya doiraning ORTIDA turadi va tashqariga oʻsadi.
        pulsQatlam.frame = CGRect(x: 0, y: 0, width: 60, height: 60)
        pulsQatlam.cornerRadius = 30
        pulsQatlam.isHidden = true
        doira.layer?.insertSublayer(pulsQatlam, at: 0)

        mikrofon.image = NSImage(systemSymbolName: "mic.fill", accessibilityDescription: "Mikrofon")
        mikrofon.symbolConfiguration = .init(pointSize: 26, weight: .medium)
        mikrofon.contentTintColor = .white
        mikrofon.translatesAutoresizingMaskIntoConstraints = false

        toxtatBelgi.translatesAutoresizingMaskIntoConstraints = false
        toxtatBelgi.wantsLayer = true
        toxtatBelgi.layer?.backgroundColor = NSColor.white.cgColor
        toxtatBelgi.layer?.cornerRadius = 5
        toxtatBelgi.isHidden = true

        doira.addSubview(mikrofon)
        doira.addSubview(toxtatBelgi)

        let matnlar = NSStackView(views: [sarlavha, izoh])
        matnlar.orientation = .vertical
        matnlar.alignment = .leading
        matnlar.spacing = 3

        taymer.font = NSFont.monospacedDigitSystemFont(ofSize: 17, weight: .semibold)
        ongTaraf.orientation = .horizontal
        ongTaraf.alignment = .centerY
        ongTaraf.spacing = 14
        klavishalar.orientation = .horizontal
        klavishalar.spacing = 5

        for x in [doira, matnlar, ongTaraf] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            addSubview(x)
        }
        matnlar.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        ongTaraf.setContentHuggingPriority(.required, for: .horizontal)

        NSLayoutConstraint.activate([
            doira.widthAnchor.constraint(equalToConstant: 60),
            doira.heightAnchor.constraint(equalToConstant: 60),
            doira.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 20),
            doira.centerYAnchor.constraint(equalTo: centerYAnchor),

            mikrofon.centerXAnchor.constraint(equalTo: doira.centerXAnchor),
            mikrofon.centerYAnchor.constraint(equalTo: doira.centerYAnchor),
            toxtatBelgi.centerXAnchor.constraint(equalTo: doira.centerXAnchor),
            toxtatBelgi.centerYAnchor.constraint(equalTo: doira.centerYAnchor),
            toxtatBelgi.widthAnchor.constraint(equalToConstant: 20),
            toxtatBelgi.heightAnchor.constraint(equalToConstant: 20),

            matnlar.leadingAnchor.constraint(equalTo: doira.trailingAnchor, constant: 18),
            matnlar.centerYAnchor.constraint(equalTo: centerYAnchor),
            matnlar.trailingAnchor.constraint(lessThanOrEqualTo: ongTaraf.leadingAnchor, constant: -18),

            ongTaraf.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -20),
            ongTaraf.centerYAnchor.constraint(equalTo: centerYAnchor),

            heightAnchor.constraint(equalToConstant: 92)
        ])

        klavishalarniQur()
        koʻrinishniYangila()
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    // MARK: Holat

    private func koʻrinishniYangila() {
        let q = yozilyapti
        layer?.backgroundColor = (q ? U.yozishFon : (hover ? U.kartaHover : U.kartaFon)).cgColor
        layer?.borderColor = (q ? U.yozishChet : (hover ? U.kartaHoverChet : U.kartaChet)).cgColor

        doira.layer?.backgroundColor = (q ? U.qizil : U.kok).cgColor
        doira.layer?.shadowColor = (q ? U.qizil : U.kok).cgColor
        doira.layer?.shadowOpacity = 0.3

        mikrofon.isHidden = q
        toxtatBelgi.isHidden = !q

        sarlavha.stringValue = q ? "Eshityapman… gapiring" : "Bosing va gapiring"
        izoh.stringValue = q ? "Tugatgach yana bosing" : "Matn siz turgan joyga oʻzi yoziladi"
        izoh.textColor = q ? U.yozishMatn2 : U.matn2

        ongTaraf.setViews(q ? [tolqin, taymer] : [klavishalar], in: .leading)

        if q {
            pulsQatlam.backgroundColor = U.qizil.cgColor
            pulsQatlam.isHidden = false
            pulsniBoshla()
            tolqin.boshla()
            taymerniBoshla()
        } else {
            pulsQatlam.isHidden = true
            pulsQatlam.removeAllAnimations()
            tolqin.toxtat()
            taymerniToxtat()
        }
    }

    private func klavishalarniQur() {
        klavishalar.setViews(hotkeyMatni.map { klavisha(String($0)) }, in: .leading)
    }

    /// Bitta klavisha "kaltak"i — dizayndagi oq, chegarali kvadratcha.
    private func klavisha(_ belgi: String) -> NSView {
        let quti = NSView()
        quti.wantsLayer = true
        quti.layer?.backgroundColor = U.oq.cgColor
        quti.layer?.cornerRadius = U.radiusKichik
        quti.layer?.cornerCurve = .continuous
        quti.layer?.borderColor = U.rang(0xDCDFE6).cgColor
        quti.layer?.borderWidth = 1
        quti.translatesAutoresizingMaskIntoConstraints = false

        // Harflar (D) qalinroq, modifikator belgilari (⌃⌥) oddiy — dizayndagidek.
        let harfmi = belgi.rangeOfCharacter(from: .letters) != nil
        let t = U.yozuv(belgi, 15, harfmi ? .semibold : .regular, U.rang(0x4A4A4E))
        t.alignment = .center
        t.translatesAutoresizingMaskIntoConstraints = false
        quti.addSubview(t)

        NSLayoutConstraint.activate([
            quti.heightAnchor.constraint(equalToConstant: 30),
            quti.widthAnchor.constraint(greaterThanOrEqualToConstant: 30),
            t.centerXAnchor.constraint(equalTo: quti.centerXAnchor),
            t.centerYAnchor.constraint(equalTo: quti.centerYAnchor),
            t.leadingAnchor.constraint(equalTo: quti.leadingAnchor, constant: 8),
            t.trailingAnchor.constraint(equalTo: quti.trailingAnchor, constant: -8)
        ])
        return quti
    }

    // MARK: Animatsiyalar

    private func pulsniBoshla() {
        let olcham = CABasicAnimation(keyPath: "transform.scale")
        olcham.fromValue = 1.0
        olcham.toValue = 1.5
        let shaffof = CABasicAnimation(keyPath: "opacity")
        shaffof.fromValue = 0.55
        shaffof.toValue = 0.0
        let guruh = CAAnimationGroup()
        guruh.animations = [olcham, shaffof]
        guruh.duration = 1.8
        guruh.repeatCount = .infinity
        guruh.timingFunction = CAMediaTimingFunction(name: .easeOut)
        pulsQatlam.add(guruh, forKey: "puls")
    }

    private func taymerniBoshla() {
        boshlanganVaqt = Date()
        taymer.stringValue = "0:00"
        taymerTimer?.invalidate()
        taymerTimer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
            guard let self, let boshi = self.boshlanganVaqt else { return }
            self.taymer.stringValue = VaqtFormat.taymer(Date().timeIntervalSince(boshi))
        }
    }

    private func taymerniToxtat() {
        taymerTimer?.invalidate()
        taymerTimer = nil
        boshlanganVaqt = nil
    }

    // MARK: Bosish va hover

    override func mouseDown(with event: NSEvent) { onBos?() }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(
            NSTrackingArea(
                rect: bounds,
                options: [.mouseEnteredAndExited, .activeInKeyWindow],
                owner: self))
    }

    override func mouseEntered(with event: NSEvent) { hover = true; koʻrinishniYangila() }
    override func mouseExited(with event: NSEvent) { hover = false; koʻrinishniYangila() }

    override func layout() {
        super.layout()
        pulsQatlam.frame = doira.bounds
    }
}

/// Yozib olinayotganda koʻrinadigan ovoz toʻlqini — oltita ustuncha, har biri
/// oʻz kechikishi bilan pulsatsiyalanadi. Bu MIKROFON DARAJASI EMAS, dizayndagi
/// dekorativ ishora: "ilova eshityapti".
final class TolqinView: NSView {
    private static let balandliklar: [CGFloat] = [14, 26, 34, 22, 30, 16]
    private var ustunlar: [CALayer] = []

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        translatesAutoresizingMaskIntoConstraints = false
        for b in Self.balandliklar {
            let l = CALayer()
            l.backgroundColor = U.qizil.cgColor
            l.cornerRadius = 2
            l.bounds = CGRect(x: 0, y: 0, width: 4, height: b)
            layer?.addSublayer(l)
            ustunlar.append(l)
        }
        NSLayoutConstraint.activate([
            widthAnchor.constraint(equalToConstant: CGFloat(Self.balandliklar.count) * 8 - 4),
            heightAnchor.constraint(equalToConstant: 34)
        ])
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    override func layout() {
        super.layout()
        // CALayer joylashuvini oʻzimiz beramiz — animatsiya transform orqali
        // ishlagani uchun ular Auto Layout'ga qatnashmaydi.
        for (i, l) in ustunlar.enumerated() {
            l.position = CGPoint(x: CGFloat(i) * 8 + 2, y: bounds.midY)
        }
    }

    func boshla() {
        for (i, l) in ustunlar.enumerated() {
            let a = CABasicAnimation(keyPath: "transform.scale.y")
            a.fromValue = 0.3
            a.toValue = 1.0
            a.duration = 0.45
            a.autoreverses = true
            a.repeatCount = .infinity
            a.timingFunction = CAMediaTimingFunction(name: .easeInEaseOut)
            a.beginTime = CACurrentMediaTime() + Double(i) * 0.12
            l.add(a, forKey: "tolqin")
        }
    }

    func toxtat() { ustunlar.forEach { $0.removeAllAnimations() } }
}

/// Sichqoncha ustiga kelganda fonini oʻzgartiradigan quti (roʻyxat qatorlari).
///
/// `onBos` berilsa — butun qator bosiladigan boʻladi. Bu `NSTableView`ning
/// `target`/`action` mexanizmidan ataylab afzal koʻrilgan: dizaynda qatorning
/// oʻzi (oxiridagi `›` bilan) bitta katta tugma, tanlash holati esa umuman
/// koʻrsatilmaydi (`selectionHighlightStyle = .none`) — demak "tanlangan qator"
/// tushunchasiga bogʻlanishning maʼnosi yoʻq.
final class HoverQuti: NSView {
    var hoverFon: NSColor = .clear
    var onBos: (() -> Void)?

    override func mouseDown(with event: NSEvent) {
        guard let onBos else { super.mouseDown(with: event); return }
        onBos()
    }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        trackingAreas.forEach(removeTrackingArea)
        addTrackingArea(
            NSTrackingArea(
                rect: bounds,
                options: [.mouseEnteredAndExited, .activeInKeyWindow],
                owner: self))
    }

    override func mouseEntered(with event: NSEvent) {
        layer?.backgroundColor = hoverFon.cgColor
    }

    override func mouseExited(with event: NSEvent) {
        layer?.backgroundColor = NSColor.clear.cgColor
    }

    // MARK: Accessibility
    //
    // Bosiladigan qator oddiy `NSView` — AppKit uni Accessibility'ga tugma
    // sifatida koʻrsatmaydi va `AXPress` amali ham boʻlmaydi. Yaʼni VoiceOver
    // bilan ishlaydigan odam «Oxirgi fayllar» dagi transkriptni ham, diktovka
    // tarixidagi yozuvni ham umuman ocholmasdi. Bosish mantigʻi
    // `mouseDown` da boʻlgani uchun uni shu yerdan takrorlaymiz.

    override func isAccessibilityElement() -> Bool { onBos != nil }

    override func accessibilityRole() -> NSAccessibility.Role? {
        onBos != nil ? .button : super.accessibilityRole()
    }

    /// Nom — ichidagi birinchi yozuv (fayl nomi yoki diktovka matni). Yozuvlar
    /// stack view'lar ichida boʻlishi mumkin, shuning uchun chuqur qidiriladi.
    override func accessibilityLabel() -> String? {
        func qidir(_ v: NSView) -> String? {
            for k in v.subviews {
                if let tf = k as? NSTextField, !tf.stringValue.isEmpty { return tf.stringValue }
                if let topilgan = qidir(k) { return topilgan }
            }
            return nil
        }
        return qidir(self)
    }

    override func accessibilityPerformPress() -> Bool {
        guard let onBos else { return false }
        onBos()
        return true
    }
}
