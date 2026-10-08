// "Yozish" tabi: ruxsat banneri, katta yozish kartasi va tarix roʻyxati.
// Dizayn manbasi: Claude Design "Kotib - yangi.dc.html".
//
// MUHIM: bu koʻrinish diktovka mantigʻini boshqarmaydi — u faqat AppDelegate'ga
// "toggle" signalini uzatadi va bildirishnomalar orqali holatni koʻrsatadi.
// Matn baribir fokusdagi ilovaga ⌘V bilan tushadi; tarix — qoʻshimcha qulaylik.
//
// Dizayndan bitta ATAYLAB chetlanish: dizayn tarix qatorida faqat "Nusxa"
// tugmasini koʻrsatadi, yaʼni yozuvni oʻchirish va tarixni tozalash yoʻqoladi.
// Bu maʼlumotni boshqarish imkoniyati — uni olib tashlash mumkin emas, shuning
// uchun ikkalasi ham qatorning kontekst menyusiga (oʻng tugma) koʻchirildi:
// koʻrinish dizayndagidek toza qoladi, imkoniyat esa saqlanadi.

import AppKit
import AVFoundation

final class DiktovkaVC: NSViewController {

    var onDiktovka: (() -> Void)?

    var hotkeyMatni: String = "⌃⌥D" {
        didSet { karta?.hotkeyMatni = hotkeyMatni }
    }

    private var banner: RuxsatBanneri!
    private var bannerBalandlik: NSLayoutConstraint!
    private var bannerTepa: NSLayoutConstraint!
    private var karta: YozishKartasi!
    private var jadval: NSTableView!
    private var bosYozuv: NSTextField!
    private var yozuvlar: [DiktovkaYozuvi] = []
    private var ruxsatTimer: Timer?

    override func loadView() {
        let v = NSView()
        v.wantsLayer = true
        v.layer?.backgroundColor = U.oq.cgColor

        banner = RuxsatBanneri()
        banner.onRuxsat = { [weak self] in self?.ruxsatSora() }

        karta = YozishKartasi()
        karta.hotkeyMatni = hotkeyMatni
        karta.onBos = { [weak self] in self?.onDiktovka?() }

        let sarlavha = U.bolimSarlavhasi("Oxirgi yozuvlar")

        jadval = NSTableView()
        jadval.headerView = nil
        jadval.rowHeight = 64
        jadval.intercellSpacing = NSSize(width: 0, height: 8)
        jadval.backgroundColor = .clear
        jadval.selectionHighlightStyle = .none
        jadval.dataSource = self
        jadval.delegate = self
        jadval.menu = tarixMenyusi()
        jadval.addTableColumn(NSTableColumn(identifier: .init("yozuv")))

        let scroll = NSScrollView()
        scroll.documentView = jadval
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.borderType = .noBorder
        scroll.automaticallyAdjustsContentInsets = false

        bosYozuv = U.yozuv("Hali hech narsa yozilmagan", 15, .regular, U.matn3)
        bosYozuv.alignment = .center
        bosYozuv.isHidden = true

        for x in [banner, karta, sarlavha, scroll, bosYozuv] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            v.addSubview(x)
        }

        // Banner yashirilganda balandligi 0 ga tushadi va tepadagi joyni ham
        // boʻshatadi — shuning uchun uning ostidagi masofa bannerning oʻzida emas,
        // shu yerda hisoblanadi.
        bannerBalandlik = banner.heightAnchor.constraint(equalToConstant: 0)
        bannerTepa = banner.topAnchor.constraint(equalTo: v.topAnchor, constant: 16)

        NSLayoutConstraint.activate([
            bannerTepa,
            banner.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 20),
            banner.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -20),

            karta.topAnchor.constraint(equalTo: banner.bottomAnchor, constant: 22),
            karta.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            karta.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),

            sarlavha.topAnchor.constraint(equalTo: karta.bottomAnchor, constant: 26),
            sarlavha.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),

            scroll.topAnchor.constraint(equalTo: sarlavha.bottomAnchor, constant: 12),
            scroll.leadingAnchor.constraint(equalTo: v.leadingAnchor, constant: 24),
            scroll.trailingAnchor.constraint(equalTo: v.trailingAnchor, constant: -24),
            scroll.bottomAnchor.constraint(equalTo: v.bottomAnchor, constant: -24),

            bosYozuv.centerXAnchor.constraint(equalTo: scroll.centerXAnchor),
            bosYozuv.topAnchor.constraint(equalTo: scroll.topAnchor, constant: 32)
        ])

        view = v
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        // Kuzatuvchi hech qachon olib tashlanmaydi: AsosiyOyna DiktovkaVC'ni butun
        // jarayon umri davomida keshlab turadi (hech qachon deinit boʻlmaydi),
        // selektor asosidagi kuzatuvchilar esa macOS 10.11'dan beri "zeroing weak".
        NotificationCenter.default.addObserver(
            self, selector: #selector(tarixYangilandi),
            name: .diktovkaTarixYangilandi, object: nil)
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        yangila()
        ruxsatniYangila()
        // Ruxsat ilovadan TASHQARIDA (System Settings'da) beriladi — hech qanday
        // bildirishnoma kelmaydi, shuning uchun oyna koʻrinib turganida davriy
        // tekshiramiz. Oyna yopilganda timer toʻxtaydi.
        ruxsatTimer?.invalidate()
        ruxsatTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            self?.ruxsatniYangila()
        }
    }

    override func viewDidDisappear() {
        super.viewDidDisappear()
        ruxsatTimer?.invalidate()
        ruxsatTimer = nil
    }

    func holatniOzgartir(yozilyapti: Bool) {
        karta?.yozilyapti = yozilyapti
    }

    // MARK: Ruxsatlar

    private func ruxsatniYangila() {
        let mikrofon = AVCaptureDevice.authorizationStatus(for: .audio) == .authorized
        let matnYozish = AXIsProcessTrusted()
        let kerak = !(mikrofon && matnYozish)
        guard banner.isHidden == kerak else { return }  // oʻzgarmagan boʻlsa tegmaymiz
        banner.isHidden = !kerak
        bannerBalandlik.isActive = !kerak
        // Yashiringanda tepadagi masofa ham yigʻiladi — aks holda karta dizaynda
        // koʻrsatilganidan 16pt pastda turadi.
        bannerTepa.constant = kerak ? 16 : 0
    }

    /// Ikkala ruxsatni ketma-ket soʻraydi. Mikrofon — tizim dialogi orqali;
    /// Accessibility'ni esa dasturiy berib boʻlmaydi, shuning uchun tizim
    /// soʻrovi koʻrsatiladi va System Settings ochiladi.
    private func ruxsatSora() {
        if AVCaptureDevice.authorizationStatus(for: .audio) != .authorized {
            AVCaptureDevice.requestAccess(for: .audio) { [weak self] _ in
                DispatchQueue.main.async {
                    self?.ruxsatniYangila()
                    self?.accessibilitySora()
                }
            }
            return
        }
        accessibilitySora()
    }

    private func accessibilitySora() {
        guard !AXIsProcessTrusted() else { return }
        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(opts)
        NSWorkspace.shared.open(
            URL(
                string:
                    "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
    }

    // MARK: Tarix

    @objc private func tarixYangilandi() {
        DispatchQueue.main.async { [weak self] in self?.yangila() }
    }

    private func yangila() {
        yozuvlar = DiktovkaTarixi.birgalik.oqi()
        jadval?.reloadData()
        bosYozuv?.isHidden = !yozuvlar.isEmpty
    }

    /// Kontekst menyusi — dizaynda koʻrinmaydigan, lekin zarur amallar.
    private func tarixMenyusi() -> NSMenu {
        let m = NSMenu()
        m.addItem(NSMenuItem(title: "Nusxa olish", action: #selector(menyuNusxa), keyEquivalent: ""))
        m.addItem(NSMenuItem(title: "Oʻchirish", action: #selector(menyuOchir), keyEquivalent: ""))
        m.addItem(.separator())
        m.addItem(NSMenuItem(title: "Tarixni tozalash…", action: #selector(menyuTozala), keyEquivalent: ""))
        for item in m.items { item.target = self }
        return m
    }

    /// Kontekst menyusi bosilgan qator. `clickedRow` menyu yopilgandan keyin ham
    /// saqlanadi, shuning uchun uni bevosita oʻqish xavfsiz.
    private var menyuQatori: Int { jadval.clickedRow }

    private func nusxaOl(_ matn: String) {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(matn, forType: .string)
    }

    @objc private func menyuNusxa() {
        guard yozuvlar.indices.contains(menyuQatori) else { return }
        nusxaOl(yozuvlar[menyuQatori].matn)
    }

    @objc private func menyuOchir() {
        guard yozuvlar.indices.contains(menyuQatori) else { return }
        DiktovkaTarixi.birgalik.ochir(id: yozuvlar[menyuQatori].id)
        yangila()
    }

    @objc private func menyuTozala() {
        let ogoh = NSAlert()
        ogoh.messageText = "Tarixni tozalash"
        ogoh.informativeText = "Barcha yozuvlar oʻchiriladi. Bu amalni qaytarib boʻlmaydi."
        ogoh.addButton(withTitle: "Tozalash")
        ogoh.addButton(withTitle: "Bekor qilish")
        guard ogoh.runModal() == .alertFirstButtonReturn else { return }
        DiktovkaTarixi.birgalik.tozala()
        yangila()
    }

    @objc private func nusxaBosildi(_ sender: FonliTugma) {
        guard yozuvlar.indices.contains(sender.tag) else { return }
        nusxaOl(yozuvlar[sender.tag].matn)
        sender.matnniAlmashtir("Nusxa olindi")
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak sender] in
            sender?.matnniAlmashtir("Nusxa")
        }
    }
}

extension DiktovkaVC: NSTableViewDataSource, NSTableViewDelegate {
    func numberOfRows(in tableView: NSTableView) -> Int { yozuvlar.count }

    func tableView(_ tv: NSTableView, viewFor col: NSTableColumn?, row: Int) -> NSView? {
        let y = yozuvlar[row]

        let qator = HoverQuti()
        qator.wantsLayer = true
        qator.layer?.cornerRadius = U.radiusQator
        qator.layer?.cornerCurve = .continuous
        qator.layer?.borderColor = U.qatorChiziq.cgColor
        qator.layer?.borderWidth = 1
        qator.hoverFon = U.qatorHover

        let matn = U.yozuv(y.matn, 16)
        matn.lineBreakMode = .byTruncatingTail
        matn.maximumNumberOfLines = 1
        matn.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        let tafsilot = U.yozuv(VaqtFormat.nisbiy(y.sana), 13, .regular, U.matn3)

        let nusxa = U.ikkilamchiTugma("Nusxa", target: self, action: #selector(nusxaBosildi(_:)))
        nusxa.tag = row
        nusxa.setContentHuggingPriority(.required, for: .horizontal)

        for x in [matn, tafsilot, nusxa] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            qator.addSubview(x)
        }

        NSLayoutConstraint.activate([
            matn.topAnchor.constraint(equalTo: qator.topAnchor, constant: 13),
            matn.leadingAnchor.constraint(equalTo: qator.leadingAnchor, constant: 15),
            matn.trailingAnchor.constraint(equalTo: nusxa.leadingAnchor, constant: -14),

            tafsilot.topAnchor.constraint(equalTo: matn.bottomAnchor, constant: 3),
            tafsilot.leadingAnchor.constraint(equalTo: matn.leadingAnchor),

            nusxa.trailingAnchor.constraint(equalTo: qator.trailingAnchor, constant: -15),
            nusxa.centerYAnchor.constraint(equalTo: qator.centerYAnchor)
        ])
        return qator
    }
}

// MARK: - Ruxsat banneri

/// Sariq ogohlantirish paneli: mikrofon yoki matn yozish ruxsati yoʻqligini
/// aytadi. Ilgari bu alohida "Yoʻriqnoma" boʻlimi edi — foydalanuvchi u yerga
/// oʻzi borishi kerak edi. Endi u aynan ruxsat kerak boʻlgan joyda turadi va
/// ruxsatlar berilishi bilan yoʻqoladi.
final class RuxsatBanneri: NSView {
    var onRuxsat: (() -> Void)?

    init() {
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = U.bannerFon.cgColor
        layer?.cornerRadius = 10
        layer?.cornerCurve = .continuous
        layer?.borderColor = U.bannerChet.cgColor
        layer?.borderWidth = 1

        let ikonka = U.yozuv("⚠️", 20)
        let matn = U.yozuv(
            "Mikrofon va matn yozish uchun ruxsat kerak. Bir marta beriladi.",
            15, .regular, U.bannerMatn)
        matn.lineBreakMode = .byWordWrapping
        matn.maximumNumberOfLines = 2
        matn.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)

        let tugma = U.asosiyTugma(
            "Ruxsat berish", target: self, action: #selector(bosildi),
            balandlik: 40, shrift: 15)
        tugma.setContentHuggingPriority(.required, for: .horizontal)

        for x in [ikonka, matn, tugma] as [NSView] {
            x.translatesAutoresizingMaskIntoConstraints = false
            addSubview(x)
        }
        NSLayoutConstraint.activate([
            ikonka.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 16),
            ikonka.centerYAnchor.constraint(equalTo: centerYAnchor),

            matn.leadingAnchor.constraint(equalTo: ikonka.trailingAnchor, constant: 14),
            matn.centerYAnchor.constraint(equalTo: centerYAnchor),
            matn.trailingAnchor.constraint(equalTo: tugma.leadingAnchor, constant: -14),

            tugma.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -16),
            tugma.centerYAnchor.constraint(equalTo: centerYAnchor)

        ])
        // Prioriteti majburiydan past: banner yashiringanda `DiktovkaVC` uni 0 ga
        // tushiradi va bu cheklov unga yoʻl berishi kerak.
        let engKam = heightAnchor.constraint(greaterThanOrEqualToConstant: 68)
        engKam.priority = .defaultHigh
        engKam.isActive = true
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    @objc private func bosildi() { onRuxsat?() }
}
