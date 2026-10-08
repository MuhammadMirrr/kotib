// "Audio" tabi: ovozli faylni matnga oʻgirish.
// Dizayn manbasi: Claude Design "Kotib - yangi.dc.html".
//
// Ikki koʻrinish, bir vaqtda faqat bittasi:
//   1. Roʻyxat — katta tashlash zonasi va "Oxirgi fayllar"
//   2. Matn   — "‹ Audio" sarlavhasi, transkript va pastdagi amallar
//
// Oyna qobigʻi yoʻq — asosiy oyna (asosiy_oyna.swift) ichida tab sifatida
// yashaydi. Faollik siyosati va oyna yopilishini AsosiyOyna boshqaradi.
//
// Dizayndan ikkita ATAYLAB chetlanish, ikkalasi ham "koʻrinish soddalashsin,
// lekin imkoniyat yoʻqolmasin" tamoyili boʻyicha:
//   • LLM amallari (tozalash, xulosa…) bitta "Matnni yaxshilash ⌄" menyusiga
//     yigʻildi — dizayndagidek. Ilgari ular oʻng tomonda alohida panel edi.
//   • Tayyor/xom matn almashtirgichi dizaynda umuman yoʻq; u matn maydonining
//     kontekst menyusiga (oʻng tugma) koʻchdi. LLM sozlanmagan foydalanuvchi
//     ham xom matnga yeta olishi kerak.
//
// "Tarjima qilish ⌄" ALOHIDA tugma, "Matnni yaxshilash" menyusi ichida emas:
// yaxshilash API kaliti sozlanmagan boʻlsa butunlay yashiriladi, tarjima esa
// oflayn ishlaydi va kalit talab qilmaydi. U matnni "Tarjima" tabiga oʻtkazadi
// (`onTarjima`) — tarjima hujjatga saqlanmaydi, bu ataylab: tarjima natijasi
// transkriptning oʻzi emas.

import AppKit

final class StudiyaVC: NSViewController {

    private var drop: DropView!

    // 1-koʻrinish: roʻyxat
    private let royxatKorinish = NSView()
    /// Roʻyxat boʻsh boʻlganda uning oʻrnida turadigan yozuv — Yozish tabidagi
    /// bilan bir xil naqsh (`diktovka_view.swift`).
    private var bosYozuv: NSTextField!
    private var jadval: NSTableView!
    private var hujjatlar: [Hujjat] = []

    // 2-koʻrinish: matn
    private let matnKorinish = NSView()
    private var faylNomi: NSTextField!
    var matnView: NSTextView!
    var yaxshilashTugma: FonliTugma!
    var tarjimaTugma: FonliTugma!

    /// Transkriptni «Tarjima» tabiga uzatadi. `Til` nil — foydalanuvchi tilni
    /// oʻsha tabda oʻzi tanlaydi («Boshqa til…»).
    var onTarjima: ((String, Til?) -> Void)?

    // Pastki progress paneli (ikkala koʻrinish ustida)
    private let progressKonteyner = NSView()
    var progressBar: NSProgressIndicator!
    var holatYozuvi: NSTextField!
    private var bekorTugma: NSButton!

    var joriy: Hujjat?
    private var ish: TranskripsiyaIshi?
    /// Matn maydonida hozir koʻrinayotgan narsa: matn turi yoki LLM natijasi.
    private var korinayotgan: String = MatnTuri.chiroyli.rawValue

    /// Har bir LLM oqimini oʻzi boshlangan hujjatga bogʻlaydi. `amalniBajar`
    /// oqim boshida oshiradi va oʻsha qiymatni yopib oladi (closure capture);
    /// `hujjatniOch`/`turniAlmashtir` esa koʻrinish almashganda oshiradi — shu
    /// bilan eski oqimning keyingi delta'lari yangi koʻrinishga yozilmaydi.
    var oqimToken = 0

    // MARK: Koʻrsatish

    override func viewWillAppear() {
        super.viewWillAppear()
        kutubxonaniYangila()
        // LLM sozlamasi Sozlamalar sheet'ida oʻzgargan boʻlishi mumkin
        // (llmSozlamaSaqlandi bildirishnomasi ham yangilaydi, lekin Audio tabi
        // koʻrinmayotgan boʻlsa u kelmagan boʻlishi mumkin).
        tugmalarniYangila()
    }

    // MARK: Qurish

    override func loadView() {
        drop = DropView(frame: .zero)
        drop.wantsLayer = true
        drop.layer?.backgroundColor = U.oq.cgColor
        drop.onDrop = { [weak self] url in self?.faylniQabulQil(url) }

        qurRoyxat()
        qurMatn()
        qurProgress()

        for x in [royxatKorinish, matnKorinish, progressKonteyner] {
            x.translatesAutoresizingMaskIntoConstraints = false
            drop.addSubview(x)
        }

        NSLayoutConstraint.activate([
            royxatKorinish.topAnchor.constraint(equalTo: drop.topAnchor),
            royxatKorinish.leadingAnchor.constraint(equalTo: drop.leadingAnchor),
            royxatKorinish.trailingAnchor.constraint(equalTo: drop.trailingAnchor),
            royxatKorinish.bottomAnchor.constraint(equalTo: progressKonteyner.topAnchor),

            matnKorinish.topAnchor.constraint(equalTo: drop.topAnchor),
            matnKorinish.leadingAnchor.constraint(equalTo: drop.leadingAnchor),
            matnKorinish.trailingAnchor.constraint(equalTo: drop.trailingAnchor),
            matnKorinish.bottomAnchor.constraint(equalTo: progressKonteyner.topAnchor),

            progressKonteyner.leadingAnchor.constraint(equalTo: drop.leadingAnchor),
            progressKonteyner.trailingAnchor.constraint(equalTo: drop.trailingAnchor),
            progressKonteyner.bottomAnchor.constraint(equalTo: drop.bottomAnchor)
        ])

        amallarniUlash()
        progressniKorsat(false)
        matnniKorsat(false)

        view = drop
    }

    /// Roʻyxat ↔ matn almashuvi. Progress paneli ikkalasidan ham tashqarida.
    private func matnniKorsat(_ v: Bool) {
        matnKorinish.isHidden = !v
        royxatKorinish.isHidden = v
    }

    // MARK: 1-koʻrinish — roʻyxat

    private func qurRoyxat() {
        let zona = TashlashZonasi()
        zona.onBos = { [weak self] in self?.faylTanla() }

        let sarlavha = U.bolimSarlavhasi("Oxirgi fayllar")

        bosYozuv = U.yozuv("Hali birorta fayl oʻgirilmagan", 15, .regular, U.matn3)
        bosYozuv.alignment = .center
        bosYozuv.isHidden = true

        jadval = NSTableView()
        jadval.headerView = nil
        jadval.rowHeight = 60
        jadval.intercellSpacing = NSSize(width: 0, height: 8)
        jadval.backgroundColor = .clear
        jadval.selectionHighlightStyle = .none
        jadval.dataSource = self
        jadval.delegate = self
        jadval.addTableColumn(NSTableColumn(identifier: .init("hujjat")))

        let scroll = NSScrollView()
        scroll.documentView = jadval
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.borderType = .noBorder

        for x in [zona, sarlavha, scroll, bosYozuv] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            royxatKorinish.addSubview(x)
        }

        NSLayoutConstraint.activate([
            zona.topAnchor.constraint(equalTo: royxatKorinish.topAnchor, constant: 28),
            zona.leadingAnchor.constraint(equalTo: royxatKorinish.leadingAnchor, constant: 28),
            zona.trailingAnchor.constraint(equalTo: royxatKorinish.trailingAnchor, constant: -28),
            zona.heightAnchor.constraint(equalToConstant: 236),

            sarlavha.topAnchor.constraint(equalTo: zona.bottomAnchor, constant: 22),
            sarlavha.leadingAnchor.constraint(equalTo: royxatKorinish.leadingAnchor, constant: 28),

            scroll.topAnchor.constraint(equalTo: sarlavha.bottomAnchor, constant: 10),
            scroll.leadingAnchor.constraint(equalTo: royxatKorinish.leadingAnchor, constant: 28),
            scroll.trailingAnchor.constraint(equalTo: royxatKorinish.trailingAnchor, constant: -28),
            scroll.bottomAnchor.constraint(equalTo: royxatKorinish.bottomAnchor, constant: -24),

            bosYozuv.centerXAnchor.constraint(equalTo: scroll.centerXAnchor),
            bosYozuv.topAnchor.constraint(equalTo: scroll.topAnchor, constant: 32)
        ])
    }

    // MARK: 2-koʻrinish — matn

    private func qurMatn() {
        let orqaga = BosiladiganYozuv("‹ Audio", rang: U.kok, size: 16, weight: .medium)
        orqaga.onBos = { [weak self] in self?.royxatgaQayt() }

        faylNomi = U.yozuv("", 16, .semibold)
        faylNomi.lineBreakMode = .byTruncatingMiddle

        let sarlavhaChiziq = chiziq()

        matnView = NSTextView()
        matnView.isEditable = true
        matnView.allowsUndo = true  // ⌘Z uchun shart (sukut boʻyicha oʻchiq)
        matnView.isRichText = false
        matnView.font = U.f(17)
        matnView.textColor = U.matn
        matnView.backgroundColor = U.oq
        matnView.textContainerInset = NSSize(width: 28, height: 24)
        matnView.delegate = self
        matnView.menu = matnMenyusi()
        // Dizayndagi line-height 1.6
        let paragraf = NSMutableParagraphStyle()
        paragraf.lineHeightMultiple = 1.35
        matnView.defaultParagraphStyle = paragraf
        matnView.typingAttributes = [
            .font: U.f(17), .foregroundColor: U.matn,
            .paragraphStyle: paragraf
        ]

        let scroll = NSScrollView()
        scroll.documentView = matnView
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.borderType = .noBorder

        let footerChiziq = chiziq()
        let footer = NSView()
        footer.wantsLayer = true
        footer.layer?.backgroundColor = U.maydonFon.cgColor

        let nusxa = U.asosiyTugma(
            "Nusxa olish", target: self, action: #selector(nusxaOl),
            balandlik: 44, shrift: 16)
        let saqla = U.ikkilamchiTugma(
            "Saqlash…", target: self, action: #selector(eksport),
            balandlik: 44, shrift: 16)
        yaxshilashTugma =
            U.ikkilamchiTugma(
                "Matnni yaxshilash  ⌄", target: self,
                action: #selector(yaxshilashBosildi),
                balandlik: 44, shrift: 16) as? FonliTugma
        tarjimaTugma =
            U.ikkilamchiTugma(
                "Tarjima qilish  ⌄", target: self,
                action: #selector(tarjimaBosildi),
                balandlik: 44, shrift: 16) as? FonliTugma

        for x in [orqaga, faylNomi, sarlavhaChiziq, scroll, footer] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            matnKorinish.addSubview(x)
        }
        for x in [footerChiziq, nusxa, saqla, tarjimaTugma, yaxshilashTugma] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            footer.addSubview(x)
        }

        NSLayoutConstraint.activate([
            orqaga.leadingAnchor.constraint(equalTo: matnKorinish.leadingAnchor, constant: 20),
            orqaga.topAnchor.constraint(equalTo: matnKorinish.topAnchor, constant: 14),

            faylNomi.leadingAnchor.constraint(equalTo: orqaga.trailingAnchor, constant: 12),
            faylNomi.centerYAnchor.constraint(equalTo: orqaga.centerYAnchor),
            faylNomi.trailingAnchor.constraint(
                lessThanOrEqualTo: matnKorinish.trailingAnchor,
                constant: -20),

            sarlavhaChiziq.topAnchor.constraint(equalTo: orqaga.bottomAnchor, constant: 14),
            sarlavhaChiziq.leadingAnchor.constraint(equalTo: matnKorinish.leadingAnchor),
            sarlavhaChiziq.trailingAnchor.constraint(equalTo: matnKorinish.trailingAnchor),

            scroll.topAnchor.constraint(equalTo: sarlavhaChiziq.bottomAnchor),
            scroll.leadingAnchor.constraint(equalTo: matnKorinish.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: matnKorinish.trailingAnchor),
            scroll.bottomAnchor.constraint(equalTo: footer.topAnchor),

            footer.leadingAnchor.constraint(equalTo: matnKorinish.leadingAnchor),
            footer.trailingAnchor.constraint(equalTo: matnKorinish.trailingAnchor),
            footer.bottomAnchor.constraint(equalTo: matnKorinish.bottomAnchor),
            footer.heightAnchor.constraint(equalToConstant: 76),

            footerChiziq.topAnchor.constraint(equalTo: footer.topAnchor),
            footerChiziq.leadingAnchor.constraint(equalTo: footer.leadingAnchor),
            footerChiziq.trailingAnchor.constraint(equalTo: footer.trailingAnchor),

            nusxa.leadingAnchor.constraint(equalTo: footer.leadingAnchor, constant: 20),
            nusxa.centerYAnchor.constraint(equalTo: footer.centerYAnchor),
            saqla.leadingAnchor.constraint(equalTo: nusxa.trailingAnchor, constant: 10),
            saqla.centerYAnchor.constraint(equalTo: footer.centerYAnchor),

            // Chap guruhda: «Matnni yaxshilash» LLM sozlanmaganda yashirinadi,
            // lekin cheklovlari qoladi — tarjima tugmasi oʻngga bogʻlansa,
            // oʻsha holatda oʻng chetda boʻsh joy osilib qolardi.
            tarjimaTugma.leadingAnchor.constraint(equalTo: saqla.trailingAnchor, constant: 10),
            tarjimaTugma.centerYAnchor.constraint(equalTo: footer.centerYAnchor),

            yaxshilashTugma.leadingAnchor.constraint(
                greaterThanOrEqualTo: tarjimaTugma.trailingAnchor, constant: 12),
            yaxshilashTugma.trailingAnchor.constraint(equalTo: footer.trailingAnchor, constant: -20),
            yaxshilashTugma.centerYAnchor.constraint(equalTo: footer.centerYAnchor)
        ])
    }

    private func chiziq() -> NSView {
        let v = NSView()
        v.wantsLayer = true
        v.layer?.backgroundColor = U.ajratgich.cgColor
        v.heightAnchor.constraint(equalToConstant: 1).isActive = true
        return v
    }

    // MARK: Progress

    private func qurProgress() {
        progressKonteyner.wantsLayer = true
        progressKonteyner.layer?.backgroundColor = U.panel.cgColor

        progressBar = NSProgressIndicator()
        progressBar.style = .bar
        progressBar.isIndeterminate = false
        progressBar.minValue = 0; progressBar.maxValue = 1

        holatYozuvi = U.yozuv("", 13, .regular, U.matn2)
        bekorTugma = U.ikkilamchiTugma(
            "Bekor qilish", target: self,
            action: #selector(bekorBosildi),
            balandlik: 30, shrift: 13)

        for x in [progressBar, holatYozuvi, bekorTugma] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            progressKonteyner.addSubview(x)
        }
        progressBalandlik = progressKonteyner.heightAnchor.constraint(equalToConstant: 48)
        NSLayoutConstraint.activate([
            progressBalandlik,
            progressBar.leadingAnchor.constraint(equalTo: progressKonteyner.leadingAnchor, constant: 20),
            progressBar.centerYAnchor.constraint(equalTo: progressKonteyner.centerYAnchor),
            progressBar.widthAnchor.constraint(equalToConstant: 260),

            holatYozuvi.leadingAnchor.constraint(equalTo: progressBar.trailingAnchor, constant: 14),
            holatYozuvi.centerYAnchor.constraint(equalTo: progressKonteyner.centerYAnchor),

            bekorTugma.trailingAnchor.constraint(equalTo: progressKonteyner.trailingAnchor, constant: -20),
            bekorTugma.centerYAnchor.constraint(equalTo: progressKonteyner.centerYAnchor)
        ])
    }

    private var progressBalandlik: NSLayoutConstraint!

    func progressniKorsat(_ v: Bool) {
        progressKonteyner.isHidden = !v
        // Ish ketmayotganda panel joy egallamaydi — dizaynda u umuman yoʻq,
        // faqat ish davomida paydo boʻladi.
        progressBalandlik.constant = v ? 48 : 0
    }

    // MARK: Fayl qabul qilish

    @objc func faylTanla() {
        let p = NSOpenPanel()
        p.title = "Ovozli yoki videofayl tanlang"
        p.allowsMultipleSelection = false
        p.canChooseDirectories = false
        p.allowedContentTypes = [.audio, .movie]
        guard p.runModal() == .OK, let url = p.url else { return }
        faylniQabulQil(url)
    }

    func faylniQabulQil(_ url: URL) {
        guard ModelStore.isReady else {
            ogohlantir("Model topilmadi", "Sozlamalar → Modelni yuklab olish.")
            return
        }
        guard !TranskripsiyaIshi.ishlayapti else {
            ogohlantir("Ish ketmoqda", "Boshqa fayl ustida ish ketmoqda. Tugashini kuting.")
            return
        }
        // Diktovka yozilayotgan yoki matnga oʻgirilayotgan boʻlsa — fayl kutadi.
        // Aks holda whisper navbatida diktovka fayl ortida qolib, matni
        // daqiqalar oʻtib boshqa oynaga tushardi (D2).
        guard !DiktovkaBand.faol else {
            ogohlantir("Diktovka ketmoqda", "Diktovka tugashini kuting, keyin faylni qayta tashlang.")
            return
        }
        // LLM oqimi joriy hujjatga bogʻlangan (oqimToken). Shu payt yangi fayl
        // qabul qilinsa, hujjatniOch() joriyni almashtiradi va oqim tugagach
        // uning natijasi yangi hujjatning matniga qoʻshilib ketishi mumkin edi.
        guard !bandmi else {
            ogohlantir(
                "Amal ketmoqda",
                "Joriy matn ustida amal tugashini kuting, keyin yangi fayl tashlang.")
            return
        }
        progressniKorsat(true)
        progressBar.doubleValue = 0
        holatYozuvi.stringValue = "Tayyorlanmoqda…"

        let i = TranskripsiyaIshi(url: url)
        i.onProgress = { [weak self] p, s in
            self?.progressBar.doubleValue = p
            self?.holatYozuvi.stringValue = "\(s)  \(Int(p * 100))%"
        }
        i.onTayyor = { [weak self] h in
            guard let self else { return }
            self.progressniKorsat(false)
            self.ish = nil
            self.kutubxonaniYangila()
            self.hujjatniOch(h)
            self.transkriptTayyor?(h)
        }
        i.onXato = { [weak self] m in
            self?.progressniKorsat(false)
            self?.ish = nil
            self?.ogohlantir("Xato", m)
        }
        ish = i
        i.boshla()
    }

    /// Transkript tayyor boʻlganda — avtomatik tozalash uchun.
    var transkriptTayyor: ((Hujjat) -> Void)?

    @objc private func bekorBosildi() {
        if let ish {
            ish.bekorQil()
        } else {
            // Transkripsiya ishi yoʻq — demak LLM amali ketmoqda.
            amalTask?.cancel()
        }
        holatYozuvi.stringValue = "Bekor qilinmoqda…"
    }

    // MARK: Kutubxona

    private func kutubxonaniYangila() {
        hujjatlar = HujjatOmbori.royxat()
        jadval.reloadData()
        bosYozuv?.isHidden = !hujjatlar.isEmpty
    }

    func hujjatniOch(_ h: Hujjat) {
        oqimToken &+= 1
        joriy = h
        korinayotgan = MatnTuri.chiroyli.rawValue
        faylNomi.stringValue = h.manbaNomi
        matnniOqi(.chiroyli)
        matnniKorsat(true)
        tugmalarniYangila()
    }

    private func royxatgaQayt() {
        // Oqim ketayotgan boʻlsa toʻxtatmaymiz — u oʻz hujjatiga saqlanadi.
        matnniKorsat(false)
        kutubxonaniYangila()
    }

    // MARK: Matn turi (kontekst menyusi)

    private func matnMenyusi() -> NSMenu {
        let m = NSMenu()
        m.addItem(NSMenuItem(title: "Tayyor matn", action: #selector(chiroyliniKorsat), keyEquivalent: ""))
        m.addItem(NSMenuItem(title: "Asl (tahrirsiz) matn", action: #selector(xomniKorsat), keyEquivalent: ""))
        m.addItem(.separator())
        let nusxa = NSMenuItem(title: "Nusxa olish", action: #selector(nusxaOl), keyEquivalent: "")
        m.addItem(nusxa)
        for item in m.items { item.target = self }
        return m
    }

    @objc private func chiroyliniKorsat() { turniAlmashtir(.chiroyli) }
    @objc private func xomniKorsat() { turniAlmashtir(.xom) }

    private func turniAlmashtir(_ tur: MatnTuri) {
        guard joriy != nil else { return }
        oqimToken &+= 1
        matnniOqi(tur)
    }

    func matnniOqi(_ tur: MatnTuri) {
        guard let h = joriy else { return }
        korinayotgan = tur.rawValue
        matnView.string = HujjatOmbori.matnOqi(id: h.id, tur: tur)
    }

    /// LLM natijasini matn maydonida koʻrsatadi.
    func natijaniKorsat(amal: String, matn: String) {
        korinayotgan = amal
        matnView.string = matn
    }

    /// LLM oqimi kelayotganda qoʻshib boradi.
    /// `string +=` butun hujjatni har delta'da qayta oʻqiydi/yozadi va toʻliq
    /// relayout majburlaydi — minglab delta'da bu O(n²). Buning oʻrniga
    /// textStorage'ga toʻgʻridan-toʻgʻri qoʻshamiz.
    func natijagaQosh(_ bolak: String) {
        let attrs: [NSAttributedString.Key: Any] = [
            .font: matnView.font ?? U.f(17),
            .foregroundColor: U.matn
        ]
        matnView.textStorage?.append(NSAttributedString(string: bolak, attributes: attrs))
        matnView.scrollToEndOfDocument(nil)
    }

    var joriyHujjat: Hujjat? { joriy }
    var joriyMatn: String { matnView.string }

    func ogohlantir(_ sarlavha: String, _ matn: String) {
        let a = NSAlert()
        a.messageText = sarlavha
        a.informativeText = matn
        a.addButton(withTitle: "Yaxshi")
        a.runModal()
    }

    // MARK: LLM amallari holati (amallar — studiya_amallar.swift)

    let amalIshi = AmalIshi()
    var bandmi = false
    /// Joriy LLM amalining Task'i — "Bekor qilish" shuni bekor qiladi.
    var amalTask: Task<Void, Never>?

}

extension StudiyaVC: NSTableViewDataSource, NSTableViewDelegate {
    func numberOfRows(in tableView: NSTableView) -> Int { hujjatlar.count }

    func tableView(_ tv: NSTableView, viewFor col: NSTableColumn?, row: Int) -> NSView? {
        let h = hujjatlar[row]

        let qator = HoverQuti()
        qator.wantsLayer = true
        qator.layer?.backgroundColor = U.panel.cgColor
        qator.layer?.cornerRadius = 10
        qator.layer?.cornerCurve = .continuous
        qator.hoverFon = U.qatorHover
        qator.onBos = { [weak self] in self?.hujjatniOch(h) }

        let nom = U.yozuv(h.manbaNomi, 16)
        nom.lineBreakMode = .byTruncatingMiddle

        let tafsilot = U.yozuv(
            "\(VaqtFormat.davomiylik(h.davomiylik)) · \(VaqtFormat.kun(h.yaratilgan))",
            14, .regular, U.matn2)

        let strelka = U.yozuv("›", 20, .regular, U.matn4)

        for x in [nom, tafsilot, strelka] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            qator.addSubview(x)
        }
        NSLayoutConstraint.activate([
            nom.topAnchor.constraint(equalTo: qator.topAnchor, constant: 12),
            nom.leadingAnchor.constraint(equalTo: qator.leadingAnchor, constant: 16),
            nom.trailingAnchor.constraint(lessThanOrEqualTo: strelka.leadingAnchor, constant: -14),

            tafsilot.topAnchor.constraint(equalTo: nom.bottomAnchor, constant: 2),
            tafsilot.leadingAnchor.constraint(equalTo: nom.leadingAnchor),

            strelka.trailingAnchor.constraint(equalTo: qator.trailingAnchor, constant: -16),
            strelka.centerYAnchor.constraint(equalTo: qator.centerYAnchor)
        ])
        return qator
    }
}

extension StudiyaVC: NSTextViewDelegate {
    /// Tahrirlar avtomatik saqlanadi — faqat matn turlarida (LLM natijalarida emas).
    func textDidChange(_ n: Notification) {
        guard let h = joriy else { return }
        guard let tur = MatnTuri(rawValue: korinayotgan) else { return }
        try? HujjatOmbori.matnSaqla(id: h.id, tur: tur, matn: matnView.string)
    }
}
