// Asosiy oynaning qismlari: yangilanish banneri, tab konteyneri, sozlamalar
// sheet'i (pastki «Donat qilish» / «Tayyor» qatori bilan) va tepaga tekislovchi klip.

import AppKit

/// `SozlamalarVC` qatʼiy freym asosidagi (Auto Layout emas) kontentni
/// `NSScrollView` ichiga soladi. Standart `NSClipView` flip qilinmagan boʻlgani
/// uchun kontent klipdan baland boʻlmaganda PASTKI chap burchakka mahkamlanadi.
/// Bu klip buni tuzatadi:
///   - `isFlipped = true` — AppKit bounds'ni hujjatning YUQORISIDAN hisoblaydi,
///     shu bilan kontent har doim tepada qoladi (hujjatning oʻzi flip
///     QILINMAYDI — uning ichidagi pastdan-yuqoriga freym arifmetikasi
///     buzilmaydi).
///   - `constrainBoundsRect(_:)` — hujjat klipdan tor boʻlsa, gorizontal
///     markazlashtiradi.
final class TepagaTekislovchiKlipView: NSClipView {
    override var isFlipped: Bool { true }

    override func constrainBoundsRect(_ proposedBounds: NSRect) -> NSRect {
        var rect = super.constrainBoundsRect(proposedBounds)
        guard let doc = documentView, doc.frame.width < rect.width else { return rect }
        rect.origin.x = (doc.frame.width - rect.width) / 2
        return rect
    }
}

/// Oyna tepasidagi ingichka xabar chizigʻi mazmuni.
struct BannerXabar {
    let matn: String
    /// Tugma yozuvi; nil — tugmasiz (masalan, «Kotib 1.2.1 ga yangilandi»).
    let tugma: String?
    let amal: (() -> Void)?
    /// Yopilgach shu kalit eslab qolinadi — aynan shu xabar qayta chiqmaydi.
    /// nil — ✕ yoʻq: majburiy yangilanish banneri yopilmaydi (S9), u holat
    /// oʻzgarganda `AsosiyOyna.yangilanishYoq` bilan yoʻqoladi.
    let kalit: String?
    /// Chaproqdagi qoʻshimcha tugma («Saytdan yuklab olish»).
    var ikkinchi: (nom: String, amal: () -> Void)? = nil
}

/// Yangilanish haqidagi ingichka chiziq: «Kotib X ga yangilandi» (S5), keyinroq
/// majburiy yangilanish ogohlantirishi (S9).
///
/// Oyna chrome'ining bir qismi — shuning uchun tab ichida emas, `KonteynerVC`
/// da turadi va uchala tabda ham koʻrinadi. Modal oyna ataylab EMAS: yangilanish
/// shoshilinch ish emas va foydalanuvchining ishini toʻxtatishga haqqi yoʻq.
final class YangilanishBanneri: NSView {

    var onYopildi: (() -> Void)?
    private var xabar: BannerXabar?
    private var tugmaView: NSView!
    private var ikkinchiView: NSView!
    private var yopishTugma: NSButton!
    private let yozuv = U.yozuv("", 13, .regular, U.bannerMatn)

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = U.bannerFon.cgColor

        let chiziq = NSView()
        chiziq.wantsLayer = true
        chiziq.layer?.backgroundColor = U.bannerChet.cgColor

        let yuklash = U.ikkilamchiTugma(
            "Ochish", target: self,
            action: #selector(amalBosildi), balandlik: 26, shrift: 12)
        tugmaView = yuklash
        let ikkinchi = U.ikkilamchiTugma(
            "Sayt", target: self,
            action: #selector(ikkinchiBosildi), balandlik: 26, shrift: 12)
        ikkinchiView = ikkinchi
        let yopish = NSButton(title: "✕", target: self, action: #selector(yopish))
        yopish.isBordered = false
        yopish.font = U.f(13)
        yopish.contentTintColor = U.bannerMatn
        yopishTugma = yopish

        for x in [yozuv, ikkinchi, yuklash, yopish, chiziq] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            addSubview(x)
        }

        NSLayoutConstraint.activate([
            yozuv.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 20),
            yozuv.centerYAnchor.constraint(equalTo: centerYAnchor),
            yozuv.trailingAnchor.constraint(lessThanOrEqualTo: ikkinchi.leadingAnchor, constant: -12),

            ikkinchi.trailingAnchor.constraint(equalTo: yuklash.leadingAnchor, constant: -8),
            ikkinchi.centerYAnchor.constraint(equalTo: centerYAnchor),

            yuklash.trailingAnchor.constraint(equalTo: yopish.leadingAnchor, constant: -10),
            yuklash.centerYAnchor.constraint(equalTo: centerYAnchor),

            yopish.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -14),
            yopish.centerYAnchor.constraint(equalTo: centerYAnchor),
            yopish.widthAnchor.constraint(equalToConstant: 20),

            chiziq.leadingAnchor.constraint(equalTo: leadingAnchor),
            chiziq.trailingAnchor.constraint(equalTo: trailingAnchor),
            chiziq.bottomAnchor.constraint(equalTo: bottomAnchor),
            chiziq.heightAnchor.constraint(equalToConstant: 1)
        ])
    }

    required init?(coder: NSCoder) { fatalError() }

    func korsat(_ x: BannerXabar) {
        xabar = x
        yozuv.stringValue = x.matn
        // Uzun holat matni (majburiy banner) tugmalar ostiga kirmasin: bir
        // qator, oxiri «…», toʻliq matn — sichqoncha ustida.
        yozuv.maximumNumberOfLines = 1
        yozuv.lineBreakMode = .byTruncatingTail
        yozuv.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        yozuv.toolTip = x.matn
        tugmaView.isHidden = x.tugma == nil
        if let t = x.tugma, let b = tugmaView as? FonliTugma { b.matnniAlmashtir(t) }
        ikkinchiView.isHidden = x.ikkinchi == nil
        if let i = x.ikkinchi, let b = ikkinchiView as? FonliTugma { b.matnniAlmashtir(i.nom) }
        yopishTugma.isHidden = x.kalit == nil
    }

    @objc private func amalBosildi() { xabar?.amal?() }
    @objc private func ikkinchiBosildi() { xabar?.ikkinchi?.amal() }

    @objc private func yopish() {
        if let k = xabar?.kalit {
            UserDefaults.standard.set(k, forKey: "yangilanishYopildi")
        }
        onYopildi?()
    }
}

/// Doimiy idish. Faqat child VC almashadi.
final class KonteynerVC: NSViewController {

    private let banner = YangilanishBanneri()
    private var bannerBalandlik: NSLayoutConstraint!

    override func loadView() {
        let v = NSView()
        v.wantsLayer = true
        v.layer?.backgroundColor = U.oq.cgColor
        view = v

        banner.translatesAutoresizingMaskIntoConstraints = false
        banner.isHidden = true
        banner.onYopildi = { [weak self] in self?.bannerniYashir() }
        v.addSubview(banner)

        // Banner yashirilganda balandligi 0 ga tushadi — child koʻrinish butun
        // joyni egallaydi va bannerdan hech qanday iz qolmaydi.
        bannerBalandlik = banner.heightAnchor.constraint(equalToConstant: 0)
        NSLayoutConstraint.activate([
            banner.topAnchor.constraint(equalTo: v.topAnchor),
            banner.leadingAnchor.constraint(equalTo: v.leadingAnchor),
            banner.trailingAnchor.constraint(equalTo: v.trailingAnchor),
            bannerBalandlik
        ])
    }

    /// Yangilanish xabarini koʻrsatadi. Foydalanuvchi aynan shu xabarni
    /// (`kalit`) allaqachon yopgan boʻlsa — koʻrsatilmaydi.
    func yangilanishKorsat(_ m: BannerXabar) {
        if let k = m.kalit, UserDefaults.standard.string(forKey: "yangilanishYopildi") == k { return }
        banner.korsat(m)
        banner.isHidden = false
        bannerBalandlik.constant = 38
    }

    func bannerniYashir() {
        banner.isHidden = true
        bannerBalandlik.constant = 0
    }

    func korsat(_ vc: NSViewController) {
        if children.first === vc { return }
        if let eski = children.first {
            eski.view.removeFromSuperview()
            eski.removeFromParent()
        }
        addChild(vc)
        vc.view.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(vc.view)
        NSLayoutConstraint.activate([
            vc.view.topAnchor.constraint(equalTo: banner.bottomAnchor),
            vc.view.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            vc.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            vc.view.trailingAnchor.constraint(equalTo: view.trailingAnchor)
        ])
    }
}

/// Sozlamalar sheet'ining idishi: yuqorida `SozlamalarVC` kontenti, pastda
/// dizayndagi footer ("Tayyor" tugmasi). Footer ALOHIDA turadi, chunki u sheet
/// chrome'iga tegishli — `SozlamalarVC` esa boʻlim sifatida ham ochilishi
/// mumkin boʻlgan kontentni saqlaydi.
final class SozlamalarSheetVC: NSViewController {
    private let ichki: SozlamalarVC
    var onYop: (() -> Void)?

    init(_ ichki: SozlamalarVC) {
        self.ichki = ichki
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    override func loadView() {
        let v = NSView()
        v.wantsLayer = true
        v.layer?.backgroundColor = U.oq.cgColor

        addChild(ichki)
        let kontent = ichki.view

        let footer = NSView()
        footer.wantsLayer = true
        footer.layer?.backgroundColor = U.maydonFon.cgColor

        let chiziq = NSView()
        chiziq.wantsLayer = true
        chiziq.layer?.backgroundColor = U.ajratgich.cgColor

        let tayyor = U.asosiyTugma(
            "Tayyor", target: self, action: #selector(tayyorBosildi),
            balandlik: 42, shrift: 16)
        let donat = U.ikkilamchiTugma(
            "♥  Donat qilish", target: ichki,
            action: #selector(SozlamalarVC.openDonate),
            balandlik: 42, shrift: 15, matnRangi: U.qizil)

        for x in [kontent, footer] { x.translatesAutoresizingMaskIntoConstraints = false; v.addSubview(x) }
        for x in [chiziq, tayyor, donat] { x.translatesAutoresizingMaskIntoConstraints = false; footer.addSubview(x) }

        // Sheet oʻlchami. Kenglik dizayndagidek qatʼiy 560. Balandlik esa
        // KONTENTGA qarab oʻsadi, lekin dizayndagi 588 dan oshmaydi ("Qoʻshimcha
        // sozlamalar" ochilganda ortigʻi skroll boʻladi).
        //
        // Bu cheklovlar SHART: `SozlamalarVC` koʻrinishi — NSScrollView, uning
        // hujjati esa Auto Layout stack. Bunday skrollning oʻz "intrinsic"
        // balandligi YOʻQ, shuning uchun cheklovsiz u 0 ga tushadi va sheet
        // faqat footer'dan iborat boʻlib qoladi.
        let balandlikIstagi = kontent.heightAnchor.constraint(
            equalTo: (ichki.view as? NSScrollView)?.documentView?.heightAnchor ?? kontent.heightAnchor)
        // 750: kontentga teng boʻlishga intiladi, lekin quyidagi `<= 514`
        // (majburiy) undan ustun. Yozuvlar endi ezilmaydi (kotib_uslub.swift'da
        // vertikal qarshilik .required), shuning uchun bu istak kontentni
        // siqmaydi — u faqat sheet'ni kontent boʻyicha oʻstiradi.
        balandlikIstagi.priority = .defaultHigh

        NSLayoutConstraint.activate([
            v.widthAnchor.constraint(equalToConstant: 560),
            balandlikIstagi,
            kontent.heightAnchor.constraint(greaterThanOrEqualToConstant: 240),
            kontent.heightAnchor.constraint(lessThanOrEqualToConstant: 514),

            kontent.topAnchor.constraint(equalTo: v.topAnchor),
            kontent.leadingAnchor.constraint(equalTo: v.leadingAnchor),
            kontent.trailingAnchor.constraint(equalTo: v.trailingAnchor),
            kontent.bottomAnchor.constraint(equalTo: footer.topAnchor),

            footer.leadingAnchor.constraint(equalTo: v.leadingAnchor),
            footer.trailingAnchor.constraint(equalTo: v.trailingAnchor),
            footer.bottomAnchor.constraint(equalTo: v.bottomAnchor),
            footer.heightAnchor.constraint(equalToConstant: 74),

            chiziq.topAnchor.constraint(equalTo: footer.topAnchor),
            chiziq.leadingAnchor.constraint(equalTo: footer.leadingAnchor),
            chiziq.trailingAnchor.constraint(equalTo: footer.trailingAnchor),
            chiziq.heightAnchor.constraint(equalToConstant: 1),

            tayyor.trailingAnchor.constraint(equalTo: footer.trailingAnchor, constant: -24),
            tayyor.centerYAnchor.constraint(equalTo: footer.centerYAnchor),

            donat.leadingAnchor.constraint(equalTo: footer.leadingAnchor, constant: 24),
            donat.centerYAnchor.constraint(equalTo: footer.centerYAnchor)
        ])
        view = v
    }

    @objc private func tayyorBosildi() { onYop?() }

    /// Escape — sheet'ni yopadi (macOS'da odatiy kutilgan xatti-harakat).
    override func cancelOperation(_ sender: Any?) { onYop?() }
}
