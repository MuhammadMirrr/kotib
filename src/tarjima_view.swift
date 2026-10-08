// Kotib — «Tarjima» tabi.
//
// Dizayn: yuqorida ikkita til maydoni va ⇄ tugmasi; ostida manba katagi,
// natija katagi; pastda jarayon koʻrsatkichi, «Nusxa olish» va «Tarjima».
//
// Til maydonlari — `NSComboBox`, chunki model 202 tilni biladi va oddiy
// ochiluvchi roʻyxatda ularni topib boʻlmaydi. Foydalanuvchi til nomini yoza
// boshlaydi, roʻyxat filtrlanadi (`completes = true`). Bunday boshqaruv
// ilovada allaqachon bor — LLM modellarini tanlashda.
//
// Model 3,1 GB va ilova ichida KELMAYDI. Shuning uchun tab ikki holatda
// boʻlishi mumkin: model bor (ish holati) yoki yoʻq (banner). Holat har safar
// tab koʻrsatilganda qayta tekshiriladi — foydalanuvchi modelni boshqa tabda
// turganda yuklab olgan boʻlishi mumkin.

import AppKit

final class TarjimaVC: NSViewController, NSTextViewDelegate, NSComboBoxDelegate {

    /// Model kerak boʻlganda — `AsosiyOyna` yuklab olishni boshlaydi.
    var onModelKerak: (() -> Void)?

    private let manbaTil = NSComboBox()
    private let maqsadTil = NSComboBox()
    private let manbaView = NSTextView()
    private let natijaView = NSTextView()
    private var tarjimaTugma: FonliTugma!
    private var nusxaTugma: NSButton!
    private let jarayonBar = NSProgressIndicator()
    private let jarayonYozuv = U.yozuv("", 13, .regular, U.matn2)
    private var banner: NSView!
    private var bannerBalandligi: NSLayoutConstraint!
    private var ishHolati = false

    /// Manba katagi boʻsh boʻlganda ustida turadigan yozuv. `NSTextView` da
    /// `NSTextField` dagi kabi placeholder YOʻQ — uni ustiga qoʻyish kerak.
    /// Skroll ichiga emas, ildiz koʻrinishga qoʻshiladi (skroll kesib qoʻyadi).
    private let joyYozuvi = U.yozuv("Matnni shu yerga yozing…", 15, .regular, U.matn3)

    private var manba: Til { tilOl(manbaTil, standart: .uz) }
    private var maqsad: Til { tilOl(maqsadTil, standart: .ru) }

    // MARK: Koʻrinish

    override func loadView() {
        let v = NSView()
        v.wantsLayer = true
        v.layer?.backgroundColor = U.oq.cgColor

        // Tillar qatori — 202 band, shuning uchun yozib qidiriladi.
        let nomlar = Til.hammasi.map(\.nom)
        for (c, kalit, standart) in [
            (manbaTil, "tr.manba", Til.uz),
            (maqsadTil, "tr.maqsad", Til.ru)
        ] {
            c.removeAllItems()
            c.addItems(withObjectValues: nomlar)
            c.completes = true  // yozilgan harflar boʻyicha toʻldiradi
            c.numberOfVisibleItems = 12
            c.isEditable = true
            c.font = U.f(14)
            c.delegate = self
            c.widthAnchor.constraint(equalToConstant: 200).isActive = true
            tilniQoy(c, saqlangan(kalit, standart))
        }

        let almashtir = U.ikkilamchiTugma(
            "⇄", target: self, action: #selector(tillarniAlmashtir),
            balandlik: 30, shrift: 16)

        let tillarQatori = NSStackView(views: [manbaTil, almashtir, maqsadTil])
        tillarQatori.orientation = .horizontal
        tillarQatori.spacing = 10
        tillarQatori.alignment = .centerY

        // Kataklar
        let manbaScroll = matnMaydoni(manbaView, oqishUchun: false)
        let natijaScroll = matnMaydoni(natijaView, oqishUchun: true)
        manbaView.delegate = self

        // Tugmalar
        nusxaTugma = U.ikkilamchiTugma("Nusxa olish", target: self, action: #selector(nusxaOl))
        nusxaTugma.isEnabled = false
        // `U.asosiyTugma` doim `FonliTugma` yasaydi — matnni «Bekor qilish» ga
        // almashtirish uchun aynan shu tur kerak (`matnniAlmashtir`).
        tarjimaTugma =
            (U.asosiyTugma(
                "Tarjima", target: self,
                action: #selector(tarjimaBosildi)) as! FonliTugma)

        jarayonBar.style = .bar
        jarayonBar.isIndeterminate = false
        jarayonBar.minValue = 0
        jarayonBar.maxValue = 1
        jarayonBar.isHidden = true

        let bosliq = NSView()
        bosliq.setContentHuggingPriority(.defaultLow, for: .horizontal)
        let tugmalar = NSStackView(views: [jarayonYozuv, bosliq, nusxaTugma, tarjimaTugma])
        tugmalar.orientation = .horizontal
        tugmalar.spacing = 12

        banner = modelBanneri()
        bannerBalandligi = banner.heightAnchor.constraint(equalToConstant: 0)

        // Joy-yozuv ILDIZ koʻrinishga qoʻshiladi, `NSScrollView` ichiga emas:
        // skroll oʻz ichidagi begona koʻrinishlarni kesib qoʻyadi.
        for x in [
            banner, tillarQatori, manbaScroll, natijaScroll, jarayonBar,
            tugmalar, joyYozuvi
        ] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            v.addSubview(x)
        }

        NSLayoutConstraint.activate([
            banner.topAnchor.constraint(equalTo: v.topAnchor, constant: 24),
            banner.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            banner.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),

            tillarQatori.topAnchor.constraint(equalTo: banner.bottomAnchor, constant: 16),
            tillarQatori.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),

            manbaScroll.topAnchor.constraint(equalTo: tillarQatori.bottomAnchor, constant: 14),
            manbaScroll.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            manbaScroll.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),

            natijaScroll.topAnchor.constraint(equalTo: manbaScroll.bottomAnchor, constant: 14),
            natijaScroll.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            natijaScroll.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),
            natijaScroll.heightAnchor.constraint(equalTo: manbaScroll.heightAnchor),

            joyYozuvi.topAnchor.constraint(equalTo: manbaScroll.topAnchor, constant: 12),
            joyYozuvi.leadingAnchor.constraint(equalTo: manbaScroll.leadingAnchor, constant: 15),

            jarayonBar.topAnchor.constraint(equalTo: natijaScroll.bottomAnchor, constant: 12),
            jarayonBar.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            jarayonBar.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),
            jarayonBar.heightAnchor.constraint(equalToConstant: 6),

            tugmalar.topAnchor.constraint(equalTo: jarayonBar.bottomAnchor, constant: 12),
            tugmalar.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            tugmalar.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),
            tugmalar.bottomAnchor.constraint(equalTo: v.bottomAnchor, constant: -24)
        ])

        view = v
        modelHolatiniYangila()
    }

    /// Model bor/yoʻqligiga qarab bannerni koʻrsatadi yoki yashiradi.
    /// Har safar tab koʻrsatilganda chaqiriladi.
    /// Boshqa tabdan matn bilan kirish nuqtasi — «Audio» tabidagi
    /// «Tarjima qilish ⌄» shu yerga keladi.
    ///
    /// `til` berilsa maqsad tili oʻrnatiladi va tarjima darhol boshlanadi;
    /// berilmasa («Boshqa til…») matn faqat qoʻyiladi va foydalanuvchi tilni
    /// oʻzi tanlaydi. Manba tili doim oʻzbekcha: transkript whisper'ning
    /// oʻzbek modelidan keladi.
    func matnniQabulQil(_ matn: String, maqsad til: Til?) {
        // `Tarjimon` bir vaqtda bitta ishni koʻtaradi (`band` bayrogʻi bor,
        // lekin ikkinchi chaqiruvni toʻsmaydi). Shuning uchun ish ketayotgan
        // boʻlsa uni bekor qilamiz va YANGISINI BOSHLAMAYMIZ — eski ishning
        // callback'i keyinroq kelib, yangisining holatini buzib qoʻyardi.
        let bandEdi = ishHolati
        if bandEdi { Tarjimon.shared.bekorQil() }

        manbaView.string = matn
        natijaView.string = ""
        nusxaTugma.isEnabled = false
        tilniQoy(manbaTil, .uz)
        if let til { tilniQoy(maqsadTil, til) }
        tilOzgardi()
        kiritishHolatiniYangila()

        guard !bandEdi, til != nil, TarjimaModel.tayyor else { return }
        tarjimaBosildi()
    }

    func modelHolatiniYangila() {
        let bor = TarjimaModel.tayyor
        banner.isHidden = bor
        // Yashiringan koʻrinish ham Auto Layout'da joy egallaydi — balandlik
        // ataylab nolga tushiriladi.
        bannerBalandligi.isActive = bor
        manbaTil.isEnabled = bor
        maqsadTil.isEnabled = bor
        manbaView.isEditable = bor
        kiritishHolatiniYangila()
    }

    // MARK: Delegatlar

    func textDidChange(_ n: Notification) { kiritishHolatiniYangila() }

    /// Til tanlovi oʻzgarganda. Bildirishnoma yuborilganda tanlov hali
    /// qoʻllanmagan boʻladi, shuning uchun keyingi siklga qoldiriladi.
    func comboBoxSelectionDidChange(_ n: Notification) {
        DispatchQueue.main.async { self.tilOzgardi() }
    }

    /// Til nomi qoʻlda yozib tugatilganda ham tanlov saqlanadi.
    func controlTextDidEndEditing(_ n: Notification) { tilOzgardi() }

    /// Manba boʻsh boʻlsa «Tarjima» tugmasi oʻchiq turadi va joy-yozuv koʻrinadi.
    private func kiritishHolatiniYangila() {
        let bosh = manbaView.string.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        joyYozuvi.isHidden = !bosh || !TarjimaModel.tayyor
        if !ishHolati { tarjimaTugma.isEnabled = !bosh && TarjimaModel.tayyor }
    }

    // MARK: Amallar

    private func tilOzgardi() {
        UserDefaults.standard.set(manba.nllb, forKey: "tr.manba")
        UserDefaults.standard.set(maqsad.nllb, forKey: "tr.maqsad")
    }

    /// Tillarni ham, kataklardagi matnni ham almashtiradi — foydalanuvchi
    /// odatda tarjimani koʻrib, teskari yoʻnalishda davom ettirmoqchi boʻladi.
    @objc private func tillarniAlmashtir() {
        let eskiManba = manba, eskiMaqsad = maqsad
        tilniQoy(manbaTil, eskiMaqsad)
        tilniQoy(maqsadTil, eskiManba)
        if !natijaView.string.isEmpty {
            // `NSTextView.string` ichki bufer ustidan DANGASA koʻrinish qaytaradi:
            // birinchi katakka yozilishi bilan saqlab qoʻyilgan oʻzgaruvchi ham
            // oʻzgarib ketadi va ikkala katakda bir xil matn qoladi.
            // `String(...[...])` yangi bufer yasab, nusxani kafolatlaydi.
            let manbaMatn = String(manbaView.string[...])
            let natijaMatn = String(natijaView.string[...])
            manbaView.string = natijaMatn
            natijaView.string = manbaMatn
        }
        tilOzgardi()
        kiritishHolatiniYangila()
    }

    @objc private func nusxaOl() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(natijaView.string, forType: .string)
    }

    @objc private func tarjimaBosildi() {
        if ishHolati {
            Tarjimon.shared.bekorQil()
            return
        }
        guard !manbaView.string.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        guard TarjimaModel.tayyor else { onModelKerak?(); return }

        ishniBoshla()
        Tarjimon.shared.tarjimaQil(
            matn: manbaView.string, manba: manba, maqsad: maqsad,
            jarayon: { [weak self] bajarilgan, jami in
                self?.jarayonBar.doubleValue = Double(bajarilgan) / Double(jami)
                self?.jarayonYozuv.stringValue = "\(bajarilgan) / \(jami) jumla"
            },
            tugadi: { [weak self] natija in
                guard let self else { return }
                self.ishniTugat()
                switch natija {
                case .success(let matn):
                    self.natijaView.string = matn
                    self.nusxaTugma.isEnabled = !matn.isEmpty
                case .failure(let xato):
                    self.jarayonYozuv.stringValue = xato.localizedDescription
                }
            })
    }

    private func ishniBoshla() {
        ishHolati = true
        tarjimaTugma.matnniAlmashtir("Bekor qilish")
        tarjimaTugma.isEnabled = true  // bekor qilish uchun yoqiq qoladi
        jarayonBar.isHidden = false
        jarayonBar.doubleValue = 0
        jarayonYozuv.stringValue = "Tayyorlanmoqda…"
        natijaView.string = ""
        nusxaTugma.isEnabled = false
    }

    private func ishniTugat() {
        ishHolati = false
        tarjimaTugma.matnniAlmashtir("Tarjima")
        jarayonBar.isHidden = true
        jarayonYozuv.stringValue = ""
        kiritishHolatiniYangila()
    }

    // MARK: Yordamchilar

    /// Combo box'dagi tanlov → Til.
    ///
    /// KOʻRINAYOTGAN MATN ustun, tanlangan indeks emas. Foydalanuvchi til nomini
    /// yozganda `indexOfSelectedItem` ESKI tanlovda qolib ketadi — indeksga
    /// ishonilsa, ilova yozilgan tilni eʼtiborsiz qoldirib eskisiga tarjima
    /// qiladi (qoʻlda sinovda aynan shunday boʻlgan: oʻzbek→turk soʻralib,
    /// rus→oʻzbek bajarilgan).
    private func tilOl(_ c: NSComboBox, standart: Til) -> Til {
        if let t = Til.hammasi.first(where: { $0.nom == c.stringValue }) { return t }
        let i = c.indexOfSelectedItem
        if i >= 0, i < Til.hammasi.count { return Til.hammasi[i] }
        return standart
    }

    private func tilniQoy(_ c: NSComboBox, _ t: Til) {
        if let i = Til.hammasi.firstIndex(of: t) { c.selectItem(at: i) }
        c.stringValue = t.nom
    }

    private func saqlangan(_ kalit: String, _ standart: Til) -> Til {
        guard let s = UserDefaults.standard.string(forKey: kalit),
            let t = Til.topilsin(s)
        else { return standart }
        return t
    }

    private func matnMaydoni(_ tv: NSTextView, oqishUchun: Bool) -> NSScrollView {
        let s = NSScrollView()
        s.hasVerticalScroller = true
        s.borderType = .noBorder
        s.drawsBackground = true
        s.backgroundColor = oqishUchun ? U.maydonFon : U.oq
        s.wantsLayer = true
        s.layer?.cornerRadius = U.radiusKichik
        s.layer?.cornerCurve = .continuous
        s.layer?.borderColor = U.tugmaChet.cgColor
        s.layer?.borderWidth = 1
        tv.isEditable = !oqishUchun
        // NSTextView'da undo sukut boʻyicha OʻCHIQ — bu satrsiz ⌘Z ishlamaydi.
        tv.allowsUndo = !oqishUchun
        tv.isRichText = false
        tv.font = U.f(15)
        tv.textColor = U.matn
        tv.drawsBackground = false
        tv.textContainerInset = NSSize(width: 10, height: 10)
        tv.isVerticallyResizable = true
        tv.autoresizingMask = [.width]
        s.documentView = tv
        s.heightAnchor.constraint(greaterThanOrEqualToConstant: 150).isActive = true
        return s
    }

    /// Model yoʻq boʻlgandagi banner — `RuxsatBanneri` uslubida.
    private func modelBanneri() -> NSView {
        let v = NSView()
        v.wantsLayer = true
        v.layer?.backgroundColor = U.bannerFon.cgColor
        v.layer?.cornerRadius = 10
        v.layer?.cornerCurve = .continuous
        v.layer?.borderColor = U.bannerChet.cgColor
        v.layer?.borderWidth = 1

        let ikonka = U.yozuv("⬇️", 20)
        let matn = U.yozuv(
            "Tarjima uchun model kerak (3,1 GB). Bir marta yuklanadi, keyin internetsiz ishlaydi.",
            15, .regular, U.bannerMatn)
        matn.lineBreakMode = .byWordWrapping
        matn.maximumNumberOfLines = 2
        matn.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        let tugma = U.asosiyTugma(
            "Yuklab olish", target: self, action: #selector(modelniYukla),
            balandlik: 40, shrift: 15)
        tugma.setContentHuggingPriority(.required, for: .horizontal)

        for x in [ikonka, matn, tugma] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            v.addSubview(x)
        }
        NSLayoutConstraint.activate([
            ikonka.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 16),
            ikonka.centerYAnchor.constraint(equalTo: v.centerYAnchor),
            matn.leadingAnchor.constraint(equalTo: ikonka.trailingAnchor, constant: 14),
            matn.centerYAnchor.constraint(equalTo: v.centerYAnchor),
            matn.trailingAnchor.constraint(equalTo: tugma.leadingAnchor, constant: -14),
            tugma.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -16),
            tugma.centerYAnchor.constraint(equalTo: v.centerYAnchor)
        ])
        // Prioriteti majburiydan past: banner yashiringanda balandlik 0 ga
        // tushiriladi va bu cheklov unga yoʻl berishi kerak.
        let engKam = v.heightAnchor.constraint(greaterThanOrEqualToConstant: 68)
        engKam.priority = .defaultHigh
        engKam.isActive = true
        return v
    }

    @objc private func modelniYukla() { onModelKerak?() }
}
