// Kotib — dizayn tokenlari va umumiy koʻrinish yordamchilari.
//
// Manba: Claude Design loyihasidagi "Kotib - yangi.dc.html". Dizaynda ranglar,
// radiuslar va shrift oʻlchamlari oʻnlab joyda takrorlanadi; ular shu yerda
// BIR MARTA aniqlanadi, boshqa fayllar faqat shu nomlarga murojaat qiladi.
// Yangi rang kerak boʻlsa — avval shu yerga qoʻshiladi, view faylida emas.
//
// Ilova YORUGʻ rejaga qotirilgan (`AsosiyOyna.qur`da `appearance = .aqua`) —
// dizaynda faqat yorugʻ variant chizilgan va ranglar brendning oʻzi. Shu sababli
// bu yerda dinamik (light/dark) NSColor'lar ATAYLAB ishlatilmagan: ular tizim
// rejimiga ergashib, dizaynni buzgan boʻlardi.
//
// Shrift: tizim shrifti (SF Pro) — ATAYLAB. Dizayn "IBM Plex Sans" ni soʻraydi,
// lekin uning oʻz fallback zanjiri `-apple-system` bilan davom etadi, Windows
// esa Segoe UI ishlatadi: ikkala platforma ham oʻz tizim shriftida. Plex'ni
// faqat Mac'ga qoʻshish shu tenglikni buzadi (qaror: 2026-09-25).

import AppKit

enum U {

    // MARK: Ranglar

    static let oq = rang(0xFFFFFF)
    static let panel = rang(0xF7F7F9)  // toolbar / yumshoq fon
    static let ajratgich = rang(0xECECF0)  // ichki ajratuvchi chiziqlar
    static let qatorChiziq = rang(0xEDEEF1)  // roʻyxat qatori chegarasi
    static let qatorHover = rang(0xF7F8FA)

    static let kok = rang(0x0A6CFF)  // asosiy amal rangi
    static let kokBosilgan = rang(0x0850C0)
    static let qizil = rang(0xE5484D)  // yozib olish holati
    static let yashil = rang(0x1F7A4D)  // "ruxsat berilgan" belgisi

    static let matn = rang(0x1D1D1F)
    static let matn2 = rang(0x8A8A90)  // ikkilamchi
    static let matn3 = rang(0x9A9AA0)  // vaqt, uchlamchi
    static let matn4 = rang(0xB0B0B6)  // eng och (chevron, ikonka)

    static let kartaFon = rang(0xF5F7FA)  // "Bosing va gapiring" kartasi
    static let kartaChet = rang(0xE3E6EC)
    static let kartaHover = rang(0xEFF4FF)
    static let kartaHoverChet = rang(0xC7DBFF)

    static let yozishFon = rang(0xFFF3F3)  // yozib olinayotgandagi karta
    static let yozishChet = rang(0xF3CCCE)
    static let yozishMatn2 = rang(0x8A6A6C)

    static let bannerFon = rang(0xFFF6E0)  // ruxsat banneri
    static let bannerChet = rang(0xF0D79A)
    static let bannerMatn = rang(0x5C4708)

    static let tugmaChet = rang(0xD6D6DB)  // ikkilamchi tugma chegarasi
    static let maydonFon = rang(0xFAFAFB)  // drop zona / footer foni
    static let punktir = rang(0xC9C9D0)  // drop zona punktiri
    static let yumshoqFon = rang(0xF0F0F3)  // hotkey "chip" foni

    // MARK: Oʻlchamlar

    static let radiusKarta: CGFloat = 16
    static let radiusQator: CGFloat = 11
    static let radiusTugma: CGFloat = 9
    static let radiusKichik: CGFloat = 7

    // MARK: Shriftlar

    static func f(_ size: CGFloat, _ w: NSFont.Weight = .regular) -> NSFont {
        .systemFont(ofSize: size, weight: w)
    }

    // MARK: Yordamchilar

    static func rang(_ hex: UInt32) -> NSColor {
        NSColor(
            srgbRed: CGFloat((hex >> 16) & 0xFF) / 255,
            green: CGFloat((hex >> 8) & 0xFF) / 255,
            blue: CGFloat(hex & 0xFF) / 255,
            alpha: 1)
    }

    /// Asosiy (koʻk, toʻldirilgan) tugma.
    static func asosiyTugma(
        _ nom: String, target: AnyObject?, action: Selector?,
        balandlik: CGFloat = 40, shrift: CGFloat = 15
    ) -> NSButton {
        let b = FonliTugma(title: nom, target: target, action: action)
        b.sozla(
            fon: kok, bosilganFon: kokBosilgan, matnRangi: .white,
            balandlik: balandlik, shrift: f(shrift, .semibold))
        return b
    }

    /// Ikkilamchi (oq, chegarali) tugma.
    static func ikkilamchiTugma(
        _ nom: String, target: AnyObject?, action: Selector?,
        balandlik: CGFloat = 38, shrift: CGFloat = 15,
        matnRangi: NSColor = matn
    ) -> NSButton {
        let b = FonliTugma(title: nom, target: target, action: action)
        b.sozla(
            fon: oq, bosilganFon: qatorHover, matnRangi: matnRangi,
            balandlik: balandlik, shrift: f(shrift, .medium), chet: tugmaChet)
        return b
    }

    /// Bosh harfli boʻlim sarlavhasi — "OXIRGI YOZUVLAR".
    static func bolimSarlavhasi(_ nom: String) -> NSTextField {
        let t = NSTextField(labelWithString: nom.uppercased())
        t.font = f(13, .semibold)
        t.textColor = matn2
        // Dizayndagi letter-spacing:0.04em
        t.attributedStringValue = NSAttributedString(
            string: nom.uppercased(),
            attributes: [
                .font: f(13, .semibold), .foregroundColor: matn2,
                .kern: 13 * 0.04
            ])
        return t
    }

    static func yozuv(
        _ matn: String, _ size: CGFloat, _ w: NSFont.Weight = .regular,
        _ rang: NSColor = U.matn
    ) -> NSTextField {
        let t = NSTextField(labelWithString: matn)
        t.font = f(size, w)
        t.textColor = rang
        // Skroll ichidagi kontent balandligi cheklanganda Auto Layout yetishmagan
        // joyni eng past vertikal qarshilikli elementlardan oladi. Standart 750
        // yetarli emas: sozlamalar sheet'ida yozuvlar 22pt oʻrniga 2pt boʻlib
        // ezilib qolgandi. Matn hech qachon ezilmasligi kerak — kerak boʻlsa
        // skroll paydo boʻlsin.
        t.setContentCompressionResistancePriority(.required, for: .vertical)
        return t
    }
}

/// Punktir chegarali quti (Audio tabidagi tashlash zonasi). Chegara `CAShapeLayer`
/// bilan chiziladi, shuning uchun oʻlcham oʻzgarganda yoʻlni qayta hisoblash kerak.
class PunktirQuti: NSView {
    private let shape = CAShapeLayer()
    private let radius: CGFloat

    init(fon: NSColor, radius: CGFloat, chet: NSColor) {
        self.radius = radius
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = fon.cgColor
        layer?.cornerRadius = radius
        layer?.cornerCurve = .continuous
        shape.fillColor = nil
        shape.strokeColor = chet.cgColor
        shape.lineWidth = 2
        shape.lineDashPattern = [6, 5]
        layer?.addSublayer(shape)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    override func layout() {
        super.layout()
        shape.frame = bounds
        shape.path = CGPath(
            roundedRect: bounds.insetBy(dx: 1, dy: 1),
            cornerWidth: radius, cornerHeight: radius, transform: nil)
    }
}

/// Fon rangini oʻzi chizadigan tugma.
///
/// Nega standart `NSButton` emas: dizaynda tugmalar toʻldirilgan koʻk yoki oq
/// fonli, aniq balandlikda va radiusda. `bezelStyle` bilan bunga erishib
/// boʻlmaydi — macOS oʻz uslubini majburlaydi va rang faqat matnga tegadi.
final class FonliTugma: NSButton {
    private var fon: NSColor = .clear
    private var bosilganFon: NSColor = .clear
    private var chet: NSColor?
    private var balandlik: CGFloat = 38
    private var yon: CGFloat = 18

    func sozla(
        fon: NSColor, bosilganFon: NSColor, matnRangi: NSColor,
        balandlik: CGFloat, shrift: NSFont, chet: NSColor? = nil,
        yon: CGFloat = 18
    ) {
        self.fon = fon
        self.bosilganFon = bosilganFon
        self.chet = chet
        self.balandlik = balandlik
        self.yon = yon
        isBordered = false
        wantsLayer = true
        layer?.cornerRadius = U.radiusTugma
        layer?.cornerCurve = .continuous
        layer?.backgroundColor = fon.cgColor
        if let chet {
            layer?.borderColor = chet.cgColor
            layer?.borderWidth = 1
        }
        attributedTitle = NSAttributedString(
            string: title, attributes: [.font: shrift, .foregroundColor: matnRangi])
        heightAnchor.constraint(equalToConstant: balandlik).isActive = true
    }

    /// Matnni almashtirganda rang va shriftni saqlab qoladi (`title` ni toʻgʻridan
    /// toʻgʻri yozish `attributedTitle` ni bekor qiladi).
    func matnniAlmashtir(_ yangi: String) {
        let eski = attributedTitle
        guard eski.length > 0 else { title = yangi; return }
        let atr = eski.attributes(at: 0, effectiveRange: nil)
        attributedTitle = NSAttributedString(string: yangi, attributes: atr)
    }

    override var intrinsicContentSize: NSSize {
        var s = super.intrinsicContentSize
        s.width += yon * 2
        s.height = balandlik
        return s
    }

    override func mouseDown(with event: NSEvent) {
        layer?.backgroundColor = bosilganFon.cgColor
        super.mouseDown(with: event)
        layer?.backgroundColor = fon.cgColor
    }
}
