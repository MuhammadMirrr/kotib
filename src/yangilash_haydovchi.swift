// Sparkle'ning oʻzbekcha foydalanuvchi qismi (`SPUUserDriver`).
//
// Sparkle'ning standart oynasi oʻzbekcha emas. Avtomatik rejimda (fon
// tekshiruvi) bu haydovchi JIM: hech narsa koʻrsatmaydi, yuklash va oʻrnatish
// `yangilovchi.swift` siyosati boʻyicha boʻsh paytda boʻladi. Oyna faqat
// foydalanuvchi oʻzi «Hozir tekshirish» ni bosganda chiqadi: tekshirilmoqda →
// topildi → yuklanmoqda (foiz) → tayyor → oʻrnatilmoqda; xato va «yangilanish yoʻq».
//
// LSUIElement ilova: oyna `orderFrontRegardless` bilan koʻrsatiladi
// (`AGENTS.md` → «LSUIElement window trap»).

import AppKit
import Sparkle

final class YangilashHaydovchi: NSObject, SPUUserDriver {

    // MARK: Oyna

    private var oyna: NSWindow?
    private let sarlavha = U.yozuv("", 15, .semibold)
    private let matn = U.yozuv("", 13, .regular, U.matn2)
    private let progress = NSProgressIndicator()
    private var asosiyTugma: FonliTugma!
    private var ikkinchiTugma: FonliTugma!
    private var asosiyAmal: (() -> Void)?
    private var ikkinchiAmal: (() -> Void)?

    /// Joriy sessiyani foydalanuvchi oʻzi boshlagan — UI koʻrinadi. Fon
    /// tekshiruvida false: hech narsa chiqmaydi.
    private var korinadi = false
    private var kutilganHajm: UInt64 = 0
    private var olinganHajm: UInt64 = 0

    private func oynaniQur() -> NSWindow {
        if let oyna { return oyna }
        let w = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 440, height: 170),
            styleMask: [.titled, .closable], backing: .buffered, defer: false)
        w.title = "Kotib — yangilanish"
        w.appearance = NSAppearance(named: .aqua)
        w.isReleasedWhenClosed = false
        w.level = .floating

        matn.lineBreakMode = .byWordWrapping
        matn.maximumNumberOfLines = 4
        matn.preferredMaxLayoutWidth = 392
        progress.minValue = 0
        progress.maxValue = 1
        progress.style = .bar
        asosiyTugma =
            U.asosiyTugma(
                "OK", target: self, action: #selector(asosiyBosildi),
                balandlik: 32, shrift: 13) as? FonliTugma
        ikkinchiTugma =
            U.ikkilamchiTugma(
                "Bekor", target: self, action: #selector(ikkinchiBosildi),
                balandlik: 32, shrift: 13) as? FonliTugma

        let tugmalar = NSStackView(views: [NSView(), ikkinchiTugma, asosiyTugma])
        tugmalar.orientation = .horizontal
        tugmalar.spacing = 10
        let ustun = NSStackView(views: [sarlavha, matn, progress, tugmalar])
        ustun.orientation = .vertical
        ustun.alignment = .leading
        ustun.spacing = 10
        ustun.edgeInsets = NSEdgeInsets(top: 20, left: 24, bottom: 18, right: 24)
        ustun.translatesAutoresizingMaskIntoConstraints = false
        let fon = NSView()
        fon.addSubview(ustun)
        NSLayoutConstraint.activate([
            ustun.topAnchor.constraint(equalTo: fon.topAnchor),
            ustun.leadingAnchor.constraint(equalTo: fon.leadingAnchor),
            ustun.trailingAnchor.constraint(equalTo: fon.trailingAnchor),
            ustun.bottomAnchor.constraint(equalTo: fon.bottomAnchor),
            progress.widthAnchor.constraint(equalTo: ustun.widthAnchor, constant: -48),
            tugmalar.widthAnchor.constraint(equalTo: ustun.widthAnchor, constant: -48)
        ])
        w.contentView = fon
        oyna = w
        return w
    }

    /// Oynani bir holatga keltiradi. `foiz`: nil — progress yoʻq, -1 — noaniq.
    private func holat(
        _ s: String, _ m: String, foiz: Double? = nil,
        asosiy: (String, () -> Void)? = nil, ikkinchi: (String, () -> Void)? = nil
    ) {
        let w = oynaniQur()
        sarlavha.stringValue = s
        matn.stringValue = m
        matn.isHidden = m.isEmpty
        progress.isHidden = foiz == nil
        if let foiz {
            progress.isIndeterminate = foiz < 0
            if foiz < 0 {
                progress.startAnimation(nil)
            } else {
                progress.stopAnimation(nil)
                progress.doubleValue = foiz
            }
        }
        asosiyTugma.isHidden = asosiy == nil
        if let asosiy { asosiyTugma.matnniAlmashtir(asosiy.0) }
        asosiyAmal = asosiy?.1
        ikkinchiTugma.isHidden = ikkinchi == nil
        if let ikkinchi { ikkinchiTugma.matnniAlmashtir(ikkinchi.0) }
        ikkinchiAmal = ikkinchi?.1
        if !w.isVisible { w.center() }
        oldingaChiqar()
    }

    func oldingaChiqar() {
        guard let w = oyna else { return }
        w.orderFrontRegardless()
        NSApp.activate(ignoringOtherApps: true)
        DispatchQueue.main.async { w.makeKey() }
    }

    private func yop() {
        oyna?.orderOut(nil)
        progress.stopAnimation(nil)
        asosiyAmal = nil
        ikkinchiAmal = nil
    }

    @objc private func asosiyBosildi() { asosiyAmal?() }
    @objc private func ikkinchiBosildi() { ikkinchiAmal?() }

    /// Sparkle'dan tashqari maʼlumot (masalan «avto-yangilanish oʻchiq»).
    func malumotKorsat(sarlavha s: String, matn m: String) {
        holat(s, m, asosiy: ("OK", { [weak self] in self?.yop() }))
    }

    // MARK: SPUUserDriver

    /// Ruxsat soʻralmaydi: Info.plist'da `SUEnableAutomaticChecks=YES` (spec
    /// D1). Shunga qaramay chaqirilsa — avtomatik tekshiruv, profilsiz.
    func show(
        _ request: SPUUpdatePermissionRequest,
        reply: @escaping (SUUpdatePermissionResponse) -> Void
    ) {
        reply(SUUpdatePermissionResponse(automaticUpdateChecks: true, sendSystemProfile: false))
    }

    func showUserInitiatedUpdateCheck(cancellation: @escaping () -> Void) {
        korinadi = true
        holat(
            "Yangilanish tekshirilmoqda…", "", foiz: -1,
            ikkinchi: (
                "Bekor",
                { [weak self] in
                    cancellation(); self?.yop()
                }
            ))
    }

    func showUpdateFound(
        with appcastItem: SUAppcastItem, state: SPUUserUpdateState,
        reply: @escaping (SPUUserUpdateChoice) -> Void
    ) {
        guard state.userInitiated || korinadi else {
            // Fon tekshiruvi: foydalanuvchini bezovta qilmaymiz.
            reply(appcastItem.isInformationOnlyUpdate ? .dismiss : .install)
            return
        }
        korinadi = true
        let v = appcastItem.displayVersionString
        let izoh = Yangilovchi.qisqaIzoh(appcastItem)
        if appcastItem.isInformationOnlyUpdate, let url = appcastItem.infoURL {
            holat(
                "Kotib \(v) chiqdi", izoh,
                asosiy: (
                    "Saytni ochish",
                    { [weak self] in
                        NSWorkspace.shared.open(url); reply(.dismiss); self?.yop()
                    }
                ),
                ikkinchi: (
                    "Keyinroq",
                    { [weak self] in
                        reply(.dismiss); self?.yop()
                    }
                ))
            return
        }
        holat(
            "Kotib \(v) mavjud", izoh.isEmpty ? "Siz \(Statistika.joriyVersiya) dasiz." : izoh,
            asosiy: ("Yuklab olish", { reply(.install) }),
            ikkinchi: (
                "Keyinroq",
                { [weak self] in
                    reply(.dismiss); self?.yop()
                }
            ))
    }

    func showUpdateReleaseNotes(with downloadData: SPUDownloadData) {}

    func showUpdateReleaseNotesFailedToDownloadWithError(_ error: Error) {}

    func showUpdateNotFoundWithError(_ error: Error, acknowledgement: @escaping () -> Void) {
        guard korinadi else { acknowledgement(); return }
        holat(
            "Siz eng oxirgi versiyadasiz", "Kotib \(Statistika.joriyVersiya)",
            asosiy: (
                "OK",
                { [weak self] in
                    acknowledgement(); self?.yop()
                }
            ))
    }

    func showUpdaterError(_ error: Error, acknowledgement: @escaping () -> Void) {
        RubaiLog.write("yangilanish: Sparkle xatosi — \(error.localizedDescription)")
        guard korinadi else { acknowledgement(); return }
        holat(
            "Yangilanishni tekshirib boʻlmadi",
            "Internet ulanishini tekshirib, keyinroq qayta urinib koʻring. (\(error.localizedDescription))",
            asosiy: (
                "OK",
                { [weak self] in
                    acknowledgement(); self?.yop()
                }
            ))
    }

    func showDownloadInitiated(cancellation: @escaping () -> Void) {
        kutilganHajm = 0
        olinganHajm = 0
        guard korinadi else { return }
        holat(
            "Yuklanmoqda…", "", foiz: -1,
            ikkinchi: (
                "Bekor",
                { [weak self] in
                    cancellation(); self?.yop()
                }
            ))
    }

    func showDownloadDidReceiveExpectedContentLength(_ expectedContentLength: UInt64) {
        kutilganHajm = expectedContentLength
    }

    func showDownloadDidReceiveData(ofLength length: UInt64) {
        olinganHajm += length
        guard korinadi, kutilganHajm > 0 else { return }
        let f = min(1, Double(olinganHajm) / Double(kutilganHajm))
        progress.isIndeterminate = false
        progress.stopAnimation(nil)
        progress.doubleValue = f
        sarlavha.stringValue = "Yuklanmoqda… \(Int(f * 100)) %"
    }

    func showDownloadDidStartExtractingUpdate() {
        guard korinadi else { return }
        holat("Tayyorlanmoqda…", "", foiz: -1)
    }

    func showExtractionReceivedProgress(_ progress: Double) {
        guard korinadi else { return }
        self.progress.isIndeterminate = false
        self.progress.doubleValue = progress
    }

    func showReady(toInstallAndRelaunch reply: @escaping (SPUUserUpdateChoice) -> Void) {
        // Fon rejimida — `immediateInstallationBlock` orqali boʻsh paytda.
        guard korinadi else { reply(.dismiss); return }
        holat(
            "Oʻrnatishga tayyor", "Kotib yopiladi va yangi versiya bilan qayta ochiladi.",
            asosiy: (
                "Oʻrnatish va qayta ochish",
                { [weak self] in
                    // Yozuv ketayotgan boʻlsa — yoʻqolmasin: boʻsh paytga qoldiramiz.
                    if DiktovkaBand.faol {
                        reply(.dismiss)
                        self?.holat(
                            "Diktovka tugagach oʻrnatiladi", "",
                            asosiy: ("OK", { [weak self] in self?.yop() }))
                        return
                    }
                    Yangilovchi.ochiqOynalarniYop()
                    reply(.install)
                }
            ),
            ikkinchi: (
                "Keyinroq",
                { [weak self] in
                    reply(.dismiss); self?.yop()
                }
            ))
    }

    func showInstallingUpdate(
        withApplicationTerminated applicationTerminated: Bool,
        retryTerminatingApplication: @escaping () -> Void
    ) {
        // Ilova yopilmagan boʻlsa (ochiq sheet yoki modal oyna «quit» ni bekor
        // qiladi — «App termination blocked by modal sheet»), Sparkle qayta
        // urinmaydi va «Oʻrnatilmoqda…» abadiy turardi (1.2.1 relizi, egasining
        // Mac'i). Foydalanuvchi oʻrnatishni oʻzi tanlagan — oynalarni yopib,
        // yopishni qayta soʻraymiz.
        if !applicationTerminated {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
                Yangilovchi.ochiqOynalarniYop()
                retryTerminatingApplication()
            }
        }
        guard korinadi else { return }
        holat("Oʻrnatilmoqda…", "", foiz: -1)
    }

    func showUpdateInstalledAndRelaunched(_ relaunched: Bool, acknowledgement: @escaping () -> Void) {
        acknowledgement()
        yop()
    }

    func showUpdateInFocus() { oldingaChiqar() }

    func dismissUpdateInstallation() {
        korinadi = false
        yop()
    }
}
