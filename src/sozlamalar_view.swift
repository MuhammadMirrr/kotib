// Sozlamalar sheet'i: diktovka tugmasi, mikrofon, avtoishga tushish, ruxsatlar
// va yigʻiladigan "Qoʻshimcha sozlamalar".
// Dizayn manbasi: Claude Design "Kotib - yangi.dc.html".
//
// MUHIM XATTI-HARAKAT OʻZGARISHI: sozlamalar endi DARHOL qoʻllanadi.
// Ilgari "qoralama → Saqlash/Qaytarish" modeli edi; dizaynda esa faqat "Tayyor"
// tugmasi bor va u shunchaki oynani yopadi. Bu macOS'ning oʻz System Settings
// xatti-harakati bilan bir xil: foydalanuvchi tugmani bosishi bilan oʻzgarish
// kuchga kiradi va "saqladimmi yoki yoʻqmi" degan savol umuman tugʻilmaydi.
//
// Joylashuv Auto Layout'da (ilgari 460×700 qatʼiy freym edi) — sheet kengligi
// 560 va "Qoʻshimcha sozlamalar" yigʻilib-ochilganda balandlik oʻzgaradi,
// qatʼiy freym bunga yaramaydi.

import AppKit
import AVFoundation
import Carbon.HIToolbox
import Security

final class SozlamalarVC: NSViewController {

    /// Saqlangach chaqiriladi — AsosiyOyna yangi tugmani roʻyxatdan oʻtkazadi.
    /// Yangi tugmani qoʻllaydi; tizim uni roʻyxatdan oʻtkazmasa — false.
    var onSave: ((HotKeyConfig) -> Bool)?
    /// Sheet sifatida ochilganda "Tayyor" bosilishi — `AsosiyOyna` ulaydi.
    var onYop: (() -> Void)?

    private var monitor: Any?
    private var recording = false
    private var joriyHotkey: HotKeyConfig

    private var hotkeyChip: NSTextField!
    private var hotkeyTugma: FonliTugma!
    private var hotkeyIzoh: NSTextField!
    private var micPopup: NSPopUpButton!
    private var micWarning: NSTextField!
    private var avtoSwitch: NSSwitch!
    private var yangilanishIzoh: NSTextField!

    private var mikBelgi: NSTextField!
    private var mikYozuv: NSTextField!
    private var axBelgi: NSTextField!
    private var axTugma: FonliTugma!
    private var ruxsatTimer: Timer?

    var qoshimchaLink: BosiladiganYozuv!
    var qoshimcha: NSStackView!
    var qoshimchaOchiq = false

    var modePopup: NSPopUpButton!
    var apostrofSwitch: NSSwitch!
    var diagnostikaSwitch: NSSwitch!
    var provayderPopup: NSPopUpButton!
    var kalitMaydon: NSSecureTextField!
    var baseURLMaydon: NSTextField!
    var modelCombo: NSComboBox!
    var modelHolati: NSTextField!
    var eskirganQator: NSStackView!
    var eskirganYozuv: NSTextField!
    var eskirganTugma: FonliTugma!
    var taklifQilingan: String = ""
    var modelTask: Task<Void, Never>?
    var xosQator: NSStackView!
    var tekshirTugma: FonliTugma!
    var tekshirNatija: NSTextField!

    /// Roʻyxatdagi tartib: [0] = tizim standarti, keyin qurilmalar.
    private var micList: [MicDevice] = []

    private let donate = DonateWindow()

    init(_ c: HotKeyConfig) {
        joriyHotkey = c
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) ishlatilmaydi") }

    /// Tashqaridan tugma oʻzgarsa sinxronlash uchun.
    func update(_ c: HotKeyConfig) {
        joriyHotkey = c
        hotkeyniYangila()
    }

    // MARK: Hayot sikli

    override func viewWillAppear() {
        super.viewWillAppear()
        stopRecording()
        holatniYukla()
        ruxsatniYangila()
        yangilanishIzohiniYangila()
        // Ruxsat ilovadan TASHQARIDA beriladi va hech qanday bildirishnoma
        // kelmaydi — sheet ochiq turganida davriy tekshiramiz.
        ruxsatTimer?.invalidate()
        ruxsatTimer = Timer.scheduledTimer(withTimeInterval: 1.5, repeats: true) { [weak self] _ in
            self?.ruxsatniYangila()
        }
    }

    override func viewDidDisappear() {
        super.viewDidDisappear()
        stopRecording()  // hotkey yozish monitorini qoldirmaymiz
        llmniSaqla()  // maydonda tahrir qolgan boʻlsa — yozib qoʻyamiz
        ruxsatTimer?.invalidate()
        ruxsatTimer = nil
        modelTask?.cancel()  // sheet yopilgach tarmoq soʻrovini kutmaymiz
        modelTask = nil
    }

    private func holatniYukla() {
        reloadMics()
        modePopup.selectItem(at: Prefs.insertMode.rawValue)
        avtoSwitch.state = LoginItem.isEnabled ? .on : .off
        apostrofSwitch.state = Prefs.apostrof == .oddiy ? .on : .off
        diagnostikaSwitch.state = Prefs.diagnostika ? .on : .off
        llmniYukla()
        hotkeyniYangila()
    }

    // MARK: Qurish

    override func loadView() {
        let kontent = NSStackView()
        kontent.orientation = .vertical
        kontent.alignment = .leading
        kontent.spacing = 22
        kontent.edgeInsets = NSEdgeInsets(top: 22, left: 24, bottom: 18, right: 24)
        kontent.translatesAutoresizingMaskIntoConstraints = false

        kontent.addArrangedSubview(U.yozuv("Sozlamalar", 22, .bold))
        kontent.addArrangedSubview(qurHotkey())
        kontent.addArrangedSubview(qurMikrofon())
        kontent.addArrangedSubview(qurAvtoIshga())
        kontent.addArrangedSubview(qurYangilanish())
        kontent.addArrangedSubview(qurRuxsatQutisi())

        qoshimchaLink = BosiladiganYozuv(
            "Qoʻshimcha sozlamalar  ⌄", rang: U.kok,
            size: 16, weight: .medium)
        qoshimchaLink.onBos = { [weak self] in self?.qoshimchaniAlmashtir() }
        kontent.addArrangedSubview(qoshimchaLink)

        qoshimcha = qurQoshimcha()
        qoshimcha.isHidden = true
        kontent.addArrangedSubview(qoshimcha)

        // Har bir qator sheet kengligiga tortilsin — aks holda NSStackView ularni
        // eng keng elementga qarab tekislaydi va oʻng chekka "sakrab" turadi.
        for qator in kontent.arrangedSubviews where qator is NSStackView || qator is NSView {
            qator.widthAnchor.constraint(
                equalTo: kontent.widthAnchor,
                constant: -48
            ).isActive = true
        }

        let scroll = NSScrollView()
        scroll.contentView = TepagaTekislovchiKlipView()
        scroll.documentView = kontent
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.borderType = .noBorder
        // Hujjat kengligi klip kengligiga bogʻlanadi — gorizontal skroll chiqmaydi.
        kontent.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor).isActive = true
        view = scroll
    }

    // MARK: Diktovka tugmasi

    private func qurHotkey() -> NSView {
        let sarlavha = U.yozuv("Diktovka tugmasi", 17)
        let izoh = U.yozuv("Istalgan ilovada shu tugmani bosing", 14, .regular, U.matn2)
        let matnlar = NSStackView(views: [sarlavha, izoh])
        matnlar.orientation = .vertical
        matnlar.alignment = .leading
        matnlar.spacing = 2

        hotkeyChip = U.yozuv(joriyHotkey.displayString, 18, .semibold)
        hotkeyChip.alignment = .center
        let chipQuti = NSView()
        chipQuti.wantsLayer = true
        chipQuti.layer?.backgroundColor = U.yumshoqFon.cgColor
        chipQuti.layer?.cornerRadius = 8
        chipQuti.layer?.cornerCurve = .continuous
        hotkeyChip.translatesAutoresizingMaskIntoConstraints = false
        chipQuti.addSubview(hotkeyChip)
        NSLayoutConstraint.activate([
            hotkeyChip.topAnchor.constraint(equalTo: chipQuti.topAnchor, constant: 8),
            hotkeyChip.bottomAnchor.constraint(equalTo: chipQuti.bottomAnchor, constant: -8),
            hotkeyChip.leadingAnchor.constraint(equalTo: chipQuti.leadingAnchor, constant: 14),
            hotkeyChip.trailingAnchor.constraint(equalTo: chipQuti.trailingAnchor, constant: -14),
            chipQuti.widthAnchor.constraint(greaterThanOrEqualToConstant: 76)
        ])

        hotkeyTugma =
            U.ikkilamchiTugma(
                "Oʻzgartirish", target: self,
                action: #selector(toggleRecord),
                balandlik: 40) as? FonliTugma

        let qator = NSStackView(views: [matnlar, chipQuti, hotkeyTugma])
        qator.orientation = .horizontal
        qator.alignment = .centerY
        qator.spacing = 16
        matnlar.setContentHuggingPriority(.defaultLow, for: .horizontal)
        chipQuti.setContentHuggingPriority(.required, for: .horizontal)

        hotkeyIzoh = U.yozuv("", 13, .regular, U.matn2)
        hotkeyIzoh.lineBreakMode = .byWordWrapping
        hotkeyIzoh.maximumNumberOfLines = 2
        hotkeyIzoh.preferredMaxLayoutWidth = 512
        hotkeyIzoh.isHidden = true

        let ustun = NSStackView(views: [qator, hotkeyIzoh])
        ustun.orientation = .vertical
        ustun.alignment = .leading
        ustun.spacing = 6
        qator.widthAnchor.constraint(equalTo: ustun.widthAnchor).isActive = true
        return ustun
    }

    private func hotkeyniYangila() {
        guard hotkeyChip != nil else { return }
        hotkeyChip.stringValue = recording ? "Bosing…" : joriyHotkey.displayString
        hotkeyTugma.matnniAlmashtir(recording ? "Bekor" : "Oʻzgartirish")
        hotkeyIzoh.isHidden = !recording
        hotkeyIzoh.stringValue = "⌃, ⌥ yoki ⌘ (⇧ qoʻshsa boʻladi) + tugma. Bekor qilish: ⎋"
        hotkeyIzoh.textColor = U.matn2
    }

    @objc private func toggleRecord() {
        if recording { stopRecording() } else { startRecording() }
        hotkeyniYangila()
    }

    private func startRecording() {
        recording = true
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown]) { [weak self] event in
            guard let self else { return event }
            if event.keyCode == UInt16(kVK_Escape) {
                self.stopRecording(); self.hotkeyniYangila(); return nil
            }
            let yangi = HotKeyConfig(
                keyCode: UInt32(event.keyCode),
                carbonModifiers: carbonModifiers(from: event.modifierFlags),
                label: keyLabel(for: event))
            // Modifikatorsiz tugma global qotib qoladi, yolgʻiz ⇧ esa bosh
            // harflarni yeb qoʻyadi — ikkalasiga ham ruxsat yoʻq (F5).
            guard yangi.modifikatorYetarli else {
                self.hotkeyIzoh.stringValue = "⌃, ⌥ yoki ⌘ dan kamida bittasi kerak — yolgʻiz ⇧ yetmaydi."
                self.hotkeyIzoh.textColor = U.qizil
                return nil
            }
            self.stopRecording()
            // Darhol qoʻllanadi — "Saqlash" tugmasi yoʻq. Saqlash tizim tugmani
            // qabul qilgandan keyin (AppDelegate.applyHotKey).
            if self.onSave?(yangi) == true {
                self.joriyHotkey = yangi
                self.hotkeyniYangila()
                RubaiLog.write("sozlamalar: tugma \(yangi.displayString)")
            } else {
                self.hotkeyniYangila()
                self.hotkeyIzoh.isHidden = false
                self.hotkeyIzoh.stringValue =
                    "\(yangi.displayString) roʻyxatdan oʻtmadi — boshqa ilova band qilgan boʻlishi mumkin. Eski tugma ishlayapti."
                self.hotkeyIzoh.textColor = U.qizil
                RubaiLog.write("sozlamalar: tugma \(yangi.displayString) roʻyxatdan oʻtmadi")
            }
            return nil
        }
    }

    private func stopRecording() {
        recording = false
        if let m = monitor { NSEvent.removeMonitor(m); monitor = nil }
    }

    // MARK: Mikrofon

    private func qurMikrofon() -> NSView {
        micPopup = NSPopUpButton()
        micPopup.target = self
        micPopup.action = #selector(micChanged)
        micPopup.translatesAutoresizingMaskIntoConstraints = false
        micPopup.widthAnchor.constraint(equalToConstant: 280).isActive = true

        let qator = NSStackView(views: [U.yozuv("Mikrofon", 17), micPopup])
        qator.orientation = .horizontal
        qator.alignment = .centerY
        qator.spacing = 16
        qator.setHuggingPriority(.defaultLow, for: .horizontal)

        micWarning = U.yozuv("", 13, .regular, U.matn2)
        micWarning.lineBreakMode = .byWordWrapping
        micWarning.maximumNumberOfLines = 3
        micWarning.preferredMaxLayoutWidth = 512
        micWarning.isHidden = true

        let ustun = NSStackView(views: [qator, micWarning])
        ustun.orientation = .vertical
        ustun.alignment = .leading
        ustun.spacing = 6
        qator.widthAnchor.constraint(equalTo: ustun.widthAnchor).isActive = true
        micWarning.widthAnchor.constraint(equalTo: ustun.widthAnchor).isActive = true
        return ustun
    }

    private func reloadMics() {
        micList = AudioDevices.inputDevices()
        micPopup.removeAllItems()
        micPopup.addItem(withTitle: "(tizim standarti)")
        for d in micList { micPopup.addItem(withTitle: d.listLabel) }

        // Saqlangan qurilma hali ulanganmi?
        if let uid = Prefs.micUID, let idx = micList.firstIndex(where: { $0.uid == uid }) {
            micPopup.selectItem(at: idx + 1)
        } else {
            micPopup.selectItem(at: 0)
            if Prefs.micUID != nil {
                // Qurilma uzilgan — jim standartga qaytamiz, lekin logga yozamiz
                RubaiLog.write("sozlamalar: saqlangan mikrofon topilmadi, standartga qaytildi")
            }
        }
        micOgohlantirishi()
    }

    @objc private func micChanged() {
        let idx = micPopup.indexOfSelectedItem
        Prefs.micUID = idx <= 0 ? nil : micList[idx - 1].uid
        micOgohlantirishi()
    }

    private func micOgohlantirishi() {
        let idx = micPopup.indexOfSelectedItem
        var matn = ""
        var ogohmi = false
        if idx <= 0 {
            if let def = AudioDevices.defaultInput() {
                let w = def.kind.warning
                matn = "Tizim standarti: \(def.name)" + (w.isEmpty ? "" : "\n\(w)")
                ogohmi = !w.isEmpty
            } else {
                matn = "Mikrofon topilmadi. Qurilma ulanganini tekshiring."
                ogohmi = true
            }
        } else {
            matn = micList[idx - 1].kind.warning
            ogohmi = !matn.isEmpty
        }
        micWarning.stringValue = matn
        micWarning.textColor = ogohmi ? U.qizil : U.matn2
        micWarning.isHidden = matn.isEmpty
    }

    // MARK: Avtomatik ishga tushish

    private func qurAvtoIshga() -> NSView {
        avtoSwitch = NSSwitch()
        avtoSwitch.target = self
        avtoSwitch.action = #selector(avtoOzgardi)
        let qator = NSStackView(views: [U.yozuv("Kompyuter yonganda ishga tushsin", 17), avtoSwitch])
        qator.orientation = .horizontal
        qator.alignment = .centerY
        qator.spacing = 16
        return qator
    }

    @objc private func avtoOzgardi() {
        let xohlangan = avtoSwitch.state == .on
        guard xohlangan != LoginItem.isEnabled else { return }
        if !LoginItem.set(xohlangan) {
            avtoSwitch.state = LoginItem.isEnabled ? .on : .off
            ogohlantir(
                "Avtomatik ishga tushirish sozlanmadi",
                "macOS bu amalni rad etdi. System Settings → General → Login Items "
                    + "boʻlimida qoʻlda yoqishingiz mumkin.")
        }
    }

    // MARK: Yangilanishlar

    /// Avto-yangilanish (`yangilovchi.swift`). Yoqish/oʻchirish tugmasi YOʻQ —
    /// spec: yangilanish doim avtomatik; bu yerda faqat holat va qoʻlda tekshirish.
    private func qurYangilanish() -> NSView {
        yangilanishIzoh = U.yozuv("", 13, .regular, U.matn2)
        let chap = NSStackView(views: [U.yozuv("Yangilanishlar", 17), yangilanishIzoh])
        chap.orientation = .vertical
        chap.alignment = .leading
        chap.spacing = 2
        let tugma = U.ikkilamchiTugma(
            "Hozir tekshirish", target: self, action: #selector(yangilanishniTekshir),
            balandlik: 32, shrift: 13)
        let qator = NSStackView(views: [chap, tugma])
        qator.orientation = .horizontal
        qator.alignment = .centerY
        qator.spacing = 16
        return qator
    }

    private func yangilanishIzohiniYangila() {
        let y = Yangilovchi.birgalik
        guard y.faolmi else {
            yangilanishIzoh.stringValue = "Bu nusxada avto-yangilanish oʻchiq"
            return
        }
        let oxirgi = y.oxirgiMuvaffaqiyat.map { "oxirgi tekshiruv: \(VaqtFormat.nisbiy($0))" }
        yangilanishIzoh.stringValue = "Avtomatik oʻrnatiladi · " + (oxirgi ?? "hali tekshirilmagan")
    }

    @objc private func yangilanishniTekshir() { Yangilovchi.birgalik.hozirTekshir() }

    // MARK: Ruxsat holati

    private func qurRuxsatQutisi() -> NSView {
        let quti = NSView()
        quti.wantsLayer = true
        quti.layer?.backgroundColor = U.panel.cgColor
        quti.layer?.cornerRadius = 10
        quti.layer?.cornerCurve = .continuous

        mikBelgi = U.yozuv("○", 17, .regular, U.matn4)
        mikYozuv = U.yozuv("Mikrofon ruxsati", 16)
        let mikQator = NSStackView(views: [mikBelgi, mikYozuv])
        mikQator.orientation = .horizontal
        mikQator.alignment = .centerY
        mikQator.spacing = 10

        axBelgi = U.yozuv("○", 17, .regular, U.matn4)
        let axYozuv = U.yozuv("Matn yozish ruxsati", 16)
        axTugma =
            U.ikkilamchiTugma(
                "Berish", target: self, action: #selector(axniOch),
                balandlik: 36, shrift: 15) as? FonliTugma
        let axQator = NSStackView(views: [axBelgi, axYozuv, NSView(), axTugma])
        axQator.orientation = .horizontal
        axQator.alignment = .centerY
        axQator.spacing = 10
        axYozuv.setContentHuggingPriority(.defaultLow, for: .horizontal)

        let ustun = NSStackView(views: [mikQator, axQator])
        ustun.orientation = .vertical
        ustun.alignment = .leading
        ustun.spacing = 10
        ustun.translatesAutoresizingMaskIntoConstraints = false
        quti.addSubview(ustun)
        NSLayoutConstraint.activate([
            ustun.topAnchor.constraint(equalTo: quti.topAnchor, constant: 14),
            ustun.bottomAnchor.constraint(equalTo: quti.bottomAnchor, constant: -14),
            ustun.leadingAnchor.constraint(equalTo: quti.leadingAnchor, constant: 16),
            ustun.trailingAnchor.constraint(equalTo: quti.trailingAnchor, constant: -16),
            axQator.widthAnchor.constraint(equalTo: ustun.widthAnchor)
        ])
        return quti
    }

    private func ruxsatniYangila() {
        let mikOK = AVCaptureDevice.authorizationStatus(for: .audio) == .authorized
        let axOK = AXIsProcessTrusted()
        mikBelgi.stringValue = mikOK ? "✓" : "○"
        mikBelgi.textColor = mikOK ? U.yashil : U.matn4
        mikYozuv.stringValue = mikOK ? "Mikrofon ruxsati berilgan" : "Mikrofon ruxsati"
        axBelgi.stringValue = axOK ? "✓" : "○"
        axBelgi.textColor = axOK ? U.yashil : U.matn4
        axTugma.isHidden = axOK
    }

    @objc private func axniOch() {
        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(opts)
        NSWorkspace.shared.open(
            URL(
                string:
                    "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!)
    }

    // MARK: Xizmat amallari

    @objc func openDonate() { donate.show() }

    @objc func logniOch() {
        let path = Yollar.log.path
        if !FileManager.default.fileExists(atPath: path) {
            FileManager.default.createFile(atPath: path, contents: Data())
        }
        NSWorkspace.shared.open(URL(fileURLWithPath: path))
    }

    /// Ilova ichidagi `Litsenziyalar.txt` (`scripts/litsenziyalar.sh` yasaydi):
    /// Kotib, uchinchi tomon dasturlari va modellar litsenziyalari — rubaiSTT
    /// (Apache-2.0) va NLLB-200 (CC-BY-NC-4.0) atribusiyasi shu yerda.
    @objc func litsenziyalarniOch() {
        guard let url = Bundle.main.url(forResource: "Litsenziyalar", withExtension: "txt") else {
            RubaiLog.write("XATO: Litsenziyalar.txt bundle'da yoʻq")
            return
        }
        NSWorkspace.shared.open(url)
    }

    /// Ilovani oʻchirish. Toʻliq oʻchirish (paket kvitansiyasi, TCC ruxsatlari,
    /// /Applications) admin huquqini talab qiladi, shuning uchun ilova buni
    /// OʻZI BAJARMAYDI — skript `.app` ichida keladi va buyruq nusxalanadi.
    /// Foydalanuvchi nima bajarilishini koʻrib turib oʻzi tasdiqlaydi.
    @objc func ilovaniOchir() {
        let skript = Bundle.main.path(forResource: "uninstall", ofType: "sh")
        let buyruq = "sudo \"\(skript ?? "/Applications/Kotib.app/Contents/Resources/uninstall.sh")\""

        let a = NSAlert()
        a.messageText = "Kotib'ni oʻchirish"
        a.informativeText =
            "Ilova, sozlamalar, ruxsatlar va til modeli oʻchiriladi. Diktovka tarixi va "
            + "Studiya hujjatlari ham ketadi.\n\nOʻchirish administrator parolini talab qiladi, "
            + "shuning uchun uni Terminal'da bajarasiz. Quyidagi buyruq nusxalanadi:\n\n\(buyruq)"
        a.addButton(withTitle: "Buyruqni nusxalash")
        a.addButton(withTitle: "Bekor qilish")
        guard a.runModal() == .alertFirstButtonReturn else { return }
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(buyruq, forType: .string)
        NSWorkspace.shared.open(URL(fileURLWithPath: "/System/Applications/Utilities/Terminal.app"))
    }

    private func ogohlantir(_ sarlavha: String, _ matn: String) {
        let a = NSAlert()
        a.messageText = sarlavha
        a.informativeText = matn
        a.alertStyle = .warning
        a.addButton(withTitle: "OK")
        a.runModal()
    }
}
