// Ilova delegati: menyu satri, diktovka oqimi (tugma → yozuv → whisper →
// kiritish), boʻsh turishda modelni boʻshatish, bitta nusxa qoidasi.
// `DiktovkaBand` — diktovka va Studiya bir-birini bloklaydigan holat.

import AppKit
import ApplicationServices

// MARK: - Diktovka holati (Studiya bilan oʻzaro qulf)

/// Diktovka ketayotganini Studiya bilishi kerak (D2): ilgari yozuv paytida
/// fayl ishi boshlanishi mumkin edi — whisper navbati serial, shuning uchun
/// diktovka matni fayl tugaguncha (daqiqalab) kutib, oxirida foydalanuvchi
/// allaqachon oʻtib ketgan boshqa oynaga tushardi. Teskari yoʻnalish
/// (fayl ketayotganda diktovka) `TranskripsiyaIshi.ishlayapti` bilan yopilgan.
/// Faqat asosiy oqimda.
enum DiktovkaBand {
    /// Mikrofon ochilyapti yoki yozilyapti.
    static var mikrofon = false {
        didSet { oxirgiFaollik = Date() }
    }
    /// Whisper'ga berilgan, natijasi hali kelmagan diktovkalar.
    static var transkripsiya = 0 {
        didSet { oxirgiFaollik = Date() }
    }
    static var faol: Bool { mikrofon || transkripsiya > 0 }
    /// Oxirgi diktovka faolligi — avto-yangilanish shundan keyin kamida
    /// 2 daqiqa kutadi (`YangilanishSiyosati.ornatishMumkinmi`).
    static var oxirgiFaollik: Date?
}

// MARK: - App

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private let rec = Recorder()
    let overlay = Overlay()

    /// Kotib oynasi foydalanuvchining koʻz oldidami.
    ///
    /// Suzuvchi koʻrsatkich shu payt ORTIQCHA: «Bosing va gapiring» kartasi
    /// allaqachon qizil holatga oʻtib turadi va ekranda ikkita bir xil
    /// xabar chiqadi. Boshqa ilovada ishlanayotganda esa u yagona belgi —
    /// matn oʻsha ilovaga yoziladi va Kotib koʻrinmaydi.
    private var oynaOldindami: Bool {
        NSApp.isActive
            && NSApp.windows.contains {
                $0.isVisible && $0.styleMask.contains(.titled)
            }
    }
    private let hotkey = HotKey()
    private var idleTimer: Timer?
    private var hkConfig = HotKeyStore.load()

    /// Ikki toggle orasidagi eng kichik oraliq. Hotkey ikki marta ishlab
    /// ketganda yozuv toʻxtagach darhol qaytadan boshlanib ketardi.
    private static let toggleOynasi: TimeInterval = 0.4
    /// Yozish shuncha davom etsa — oʻzi toʻxtaydi. Diktovka uchun bu juda koʻp
    /// (odatdagi yozuvlar bir daqiqagacha), lekin eʼtibordan chetda qolgan
    /// yozuvni cheklaydi: 2026-09-04 da bitta yozuv 32 daqiqa davom etib,
    /// 7952 belgilik keraksiz matn qoʻyib yuborgan.
    private static let maxYozish: TimeInterval = 600

    private var oxirgiToggle: TimeInterval = 0
    /// Mikrofon ochilishi kutilyaptimi — shu payt yangi bosish eʼtiborsiz
    /// qoldiriladi, aks holda ikkita engine parallel ochilib ketardi.
    private var boshlanyapti = false
    private var boshlashVaqti: TimeInterval = 0
    /// `boshlanyapti` shundan uzoq turib qolsa — bayroq majburan ochiladi.
    /// Mikrofon ruxsati soʻralganda tizim oynasi javobsiz qolishi mumkin,
    /// oʻshanda `start` callback'i umuman kelmaydi va aks holda hotkey
    /// butunlay ishlamay qolardi.
    private static let boshlashChegarasi: TimeInterval = 10
    private var maxYozishTimer: Timer?
    /// Yozuv boshlangan payt — «Juda qisqa» ni «ovoz kelmadi» dan ajratadi (A7).
    private var yozishBoshlandi: TimeInterval = 0
    private var dictateItem: NSMenuItem!
    /// «Saqlangan ovozni matnga oʻgirish (N)» — saqlangan ovoz boʻlsagina koʻrinadi (A2).
    var qaytaItem: NSMenuItem!
    var qaytaIshlanyapti = false
    /// Avtomatik qayta urinish har fayl uchun bir marta (shu ishga tushish davomida).
    var avtoUrinilgan = Set<String>()
    private lazy var modelDownload: ModelDownloadWindow = {
        let m = ModelDownloadWindow()
        m.onReady = { [weak self] in
            self?.startOnboardingIfNeeded()
            self?.avtoQaytaUrin()
        }
        return m
    }()

    /// Ikkinchi nusxa ishga tushmasligi uchun qalqon. launchd (login'dagi
    /// LaunchAgent) LaunchServices'ni chetlab oʻtadi — u ilova allaqachon ishlab
    /// turganini bilmaydi va yangi nusxa koʻtaradi. Natijada menyu satrida ikkita
    /// ikonka chiqadi, hotkey ikki marta ishlaydi, model RAM'ga ikki marta yuklanadi.
    ///
    /// Qoida: kim eskiroq boʻlsa — oʻsha qoladi. Ikki nusxa bir vaqtda koʻtarilsa
    /// ham qaror bir xil chiqadi (ikkalasi ham chiqib ketmaydi).
    private func ortiqchaNusxaChiqsin() -> Bool {
        guard let id = Bundle.main.bundleIdentifier else { return false }
        let meniki = ProcessInfo.processInfo.processIdentifier
        let ozSanam = NSRunningApplication.current.launchDate ?? Date()
        let eskiroq = NSRunningApplication.runningApplications(withBundleIdentifier: id)
            .first {
                $0.processIdentifier != meniki
                    && ($0.launchDate ?? .distantPast) < ozSanam
            }
        guard let eskiroq else { return false }

        RubaiLog.write("ikkinchi nusxa — chiqamiz (ishlab turgani: pid=\(eskiroq.processIdentifier))")
        // Foydalanuvchi ilovani oʻzi ochgan boʻlsa, ishlab turgan nusxa oldinga chiqsin —
        // aks holda bosgan odam uchun "hech narsa boʻlmadi"ga oʻxshaydi.
        if !Self.avtomatikIshgaTushdi { eskiroq.activate() }
        NSApp.terminate(nil)
        return true
    }

    func applicationDidFinishLaunching(_ n: Notification) {
        if ortiqchaNusxaChiqsin() { return }
        RubaiLog.write(ishgaTushishQatori())
        // whisper/ggml xabarlari (yuklash xatosining haqiqiy sababi) logga (A5).
        rubai_set_log { matn in
            guard let matn else { return }
            let m = String(cString: matn).trimmingCharacters(in: .whitespacesAndNewlines)
            if !m.isEmpty { RubaiLog.write("whisper: \(m)") }
        }
        // Silero VAD (S12) — bundle ichida; har model yuklanishida VAD ham
        // yuklanadi. Yoʻq boʻlsa diktovka 1.1 dagi kabi boʻlaksiz ishlaydi.
        if let vad = Bundle.main.path(forResource: "ggml-silero-v6.2.0", ofType: "bin") {
            rubai_set_vad_path(vad)
        } else {
            RubaiLog.write("VAD modeli bundle'da yoʻq — ovoz boʻlaklarga boʻlinmaydi")
        }
        DispatchQueue.global(qos: .utility).async { SaqlanmaganOvoz.tozala() }
        NSApp.setActivationPolicy(.accessory)  // menu-bar (Dock'da yoʻq)

        // Asosiy menyu satri — ⌘C/⌘V/⌘A/⌘Z shu yerdan keladi (`menyu.swift`).
        // `.accessory` da yashirin turadi, oyna ochilib siyosat `.regular`
        // boʻlganda koʻrinadi.
        Menyu.qur()

        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        statusItem.button?.image = NSImage(systemSymbolName: "mic.fill", accessibilityDescription: "Kotib")
        let menu = NSMenu()
        dictateItem = NSMenuItem(
            title: "Diktovka  (\(hkConfig.displayString))", action: #selector(toggle), keyEquivalent: "")
        menu.addItem(dictateItem)
        qaytaItem = NSMenuItem(title: "", action: #selector(qaytaUrinBosildi), keyEquivalent: "")
        qaytaItem.isHidden = true
        menu.addItem(qaytaItem)
        menu.addItem(NSMenuItem(title: "Oynani ochish", action: #selector(oynaniOch), keyEquivalent: ""))
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Chiqish", action: #selector(quit), keyEquivalent: "q"))
        menu.delegate = self
        statusItem.menu = menu

        AsosiyOyna.birgalik.hotkeyniOrnat(hkConfig)
        AsosiyOyna.birgalik.onDiktovka = { [weak self] in self?.toggle() }
        AsosiyOyna.birgalik.onHotkeySaqlandi = { [weak self] cfg in self?.applyHotKey(cfg) ?? false }

        hotkey.install()
        NotificationCenter.default.addObserver(self, selector: #selector(toggle), name: .rubaiHotkey, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(studiyaIshTugadiKeldi),
            name: .studiyaIshTugadi, object: nil)

        LoginItem.eskiRoyxatdanKochir()

        // Anonim statistika (`statistika.swift`) va avto-yangilanish
        // (`yangilovchi.swift`, Sparkle) — kodda ATAYLAB alohida (G2).
        Statistika.pingYubor()
        Yangilovchi.birgalik.boshla()

        // Model yoʻq boʻlsa — avval uni yuklaymiz, onboarding keyin.
        if ModelStore.isReady {
            startOnboardingIfNeeded()
        } else {
            modelDownload.show()
        }
    }

    /// Login'da ilova LaunchAgent tomonidan shu argument bilan chaqiriladi.
    /// Boshqa hech kim uni bermaydi — Finder'dan ochilsa ham, `open` bilan ham yoʻq.
    private static var avtomatikIshgaTushdi: Bool {
        CommandLine.arguments.contains("--autostart")
    }

    /// Ishga tushishda oyna koʻrsatiladimi — uch holat:
    ///   • birinchi marta       → Yozish tabi (ruxsat banneri oʻsha yerda —
    ///                            alohida yoʻriqnoma yoʻq) va avtostart yoqiladi
    ///   • login'da avtomatik   → oyna YOʻQ, faqat menyu satri
    ///   • foydalanuvchi ochdi  → Diktovka oynasi
    private func startOnboardingIfNeeded() {
        let d = UserDefaults.standard
        if !d.bool(forKey: "didOnboard") {
            d.set(true, forKey: "didOnboard")
            LoginItem.set(true)  // standart: login'da yonsin
            AsosiyOyna.birgalik.korsat(.yozish)
            return
        }
        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(opts)
        if !Self.avtomatikIshgaTushdi {
            AsosiyOyna.birgalik.oldingaChiqar()
        }
    }

    /// Ilova ishlab turganda `.app` ustiga bosilsa (yoki Dock ikonkasi bosilsa)
    /// macOS shuni chaqiradi. Ilgari bu yoʻq edi — shuning uchun ilova
    /// ishlayotganda `.app` bosilsa HECH NARSA boʻlmasdi va foydalanuvchi oynani
    /// faqat menyu satridan topa olardi.
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        AsosiyOyna.birgalik.oldingaChiqar()
        return true
    }

    // Yangi hotkey'ni qoʻllab, saqlab, menyuni yangilaydi
    /// Tugmani tizimda roʻyxatdan oʻtkazadi va faqat muvaffaqiyatda saqlaydi.
    /// Oʻtmasa eski tugma ishlashda davom etadi, sozlamalar xabar koʻrsatadi (F5).
    private func applyHotKey(_ cfg: HotKeyConfig) -> Bool {
        guard hotkey.apply(cfg) else { return false }
        hkConfig = cfg
        HotKeyStore.save(cfg)
        dictateItem.title = "Diktovka  (\(cfg.displayString))"
        return true
    }

    @objc private func oynaniOch() {
        AsosiyOyna.birgalik.korsat(.yozish)
    }

    func application(_ sender: NSApplication, openFile filename: String) -> Bool {
        guard ModelStore.isReady else { modelDownload.show(); return true }
        AsosiyOyna.birgalik.studiyagaFayl(URL(fileURLWithPath: filename))
        return true
    }

    func application(_ sender: NSApplication, openFiles filenames: [String]) {
        guard ModelStore.isReady else { modelDownload.show(); return }
        // Bir vaqtda bitta ish — birinchisini olamiz, qolganini aytamiz.
        if let birinchi = filenames.first {
            AsosiyOyna.birgalik.studiyagaFayl(URL(fileURLWithPath: birinchi))
        }
        if filenames.count > 1 {
            RubaiLog.write("studiya: \(filenames.count) fayl tashlandi, birinchisi olindi")
        }
        sender.reply(toOpenOrPrint: .success)
    }

    /// `belgi` — «Matnga oʻgirilmoqda…» xabarining tokeni: oraliqda yangi
    /// yozuv boshlangan boʻlsa, uning belgisi yopilmaydi (D5).
    private func finishTranscription(_ xom: String, samples: [Float], belgi: Int) {
        // Yagona tayyorlash qadami — log, tarix va kiritish shu natijani oladi
        // (oʻ/gʻ/ʼ va «Oddiy apostrof» sozlamasi; text_format.swift).
        let text = matnniTayyorla(xom, apostrof: Prefs.apostrof)
        let secs = Double(samples.count) / 16000.0
        // Matn faqat «Diagnostika rejimi» yoqiq boʻlsa yoziladi (log_siyosati.swift).
        RubaiLog.write(
            "natija: \(LogSiyosati.matnYozuvi(text, diagnostika: Prefs.diagnostika)) audio=\(String(format: "%.1f", secs))s"
        )

        if text.isEmpty {
            let msg =
                samples.count < 8000
                ? "Juda qisqa — kamida 1 soniya gapiring"
                : "Ovoz aniqlanmadi — balandroq gapiring"
            overlay.vaqtincha("⚠️ \(msg)", soniya: 3.5)
            scheduleIdleUnload()
            return
        }

        // Clipboard'ga bu yerda YOZILMAYDI: 1.1.0 gacha shu ikki qator
        // foydalanuvchining clipboard'ini har diktovkada (hatto clipboard
        // ishlatmaydigan «sekin» rejimda ham) yoʻq qilardi. «Tez» rejim uni
        // oʻzi saqlab-tiklaydi (Clipboard); kiritib boʻlmasagina matn
        // clipboard'da qoldiriladi — foydalanuvchi ⌘V bilan qoʻyadi.
        if Inserter.insert(text) {
            overlay.yashir(belgi)
        } else {
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(text, forType: .string)
            let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
            _ = AXIsProcessTrustedWithOptions(opts)
            overlay.vaqtincha("Matn clipboard'da — ⌘V bosing (Accessibility kerak)", soniya: 4.0)
        }

        // Tarixga yozamiz. Xato boʻlsa faqat logga tushadi — foydalanuvchining matni
        // allaqachon fokusdagi ilovaga tushgan, tarix nosozligi asosiy funksiyani
        // buzmasligi kerak.
        let yozuv = DiktovkaYozuvi(matn: text, davomiylik: secs)
        if !DiktovkaTarixi.birgalik.qoshish(yozuv) {
            RubaiLog.write("tarixga yozib boʻlmadi")
        }
        NotificationCenter.default.post(name: .diktovkaTarixYangilandi, object: nil)

        scheduleIdleUnload()
        // Model ishlayapti — oldin saqlangan ovoz boʻlsa, bir oz kutib sinab koʻramiz.
        DispatchQueue.main.asyncAfter(deadline: .now() + 3) { [weak self] in self?.avtoQaytaUrin() }
    }

    @objc private func toggle() {
        // Takroriy hodisadan himoya: bitta bosishda hotkey ikki marta ishlasa,
        // yozuv toʻxtagach darhol qaytadan boshlanib ketardi.
        let hozir = Date().timeIntervalSinceReferenceDate
        let oraliq = hozir - oxirgiToggle
        if oraliq < AppDelegate.toggleOynasi {
            RubaiLog.write(String(format: "toggle oʻtkazib yuborildi (takroriy, %.3fs)", oraliq))
            return
        }
        oxirgiToggle = hozir

        if boshlanyapti {
            if hozir - boshlashVaqti < AppDelegate.boshlashChegarasi {
                RubaiLog.write("toggle oʻtkazib yuborildi (mikrofon hali ochilyapti)")
                return
            }
            RubaiLog.write("mikrofon ochish javobi kelmadi — bayroq ochildi, qayta urinilyapti")
            boshlanyapti = false
        }

        RubaiLog.write("toggle fired, isRecording=\(rec.isRecording)")
        if rec.isRecording {
            // Toʻxtatish HAR DOIM mumkin. Model tekshiruvi ilgari shu yerdan oldin
            // turardi: yozuv paytida model fayli yoʻqolsa, toʻxtatish bosilganda
            // mikrofon yopilmas, ovoz saqlanmas va model qayta yuklanib ketardi
            // (S23 macOS sinovi). Endi model yoʻq boʻlsa transkripsiya xato beradi
            // va `ovozniSaqla` ovozni WAV qilib qoldiradi (A2) — Windows bilan bir xil.
            yozishniToxtat()
        } else {
            // Model yoʻq boʻlsa yozuv BOSHLANMAYDI — yuklash oynasi ochiladi (A4).
            guard ModelStore.isReady else { modelDownload.show(); return }
            yozishniBoshla()
        }
    }

    /// Yozishni toʻxtatib, transkripsiyaga uzatadi. Ikki joydan chaqiriladi:
    /// hotkey (`toggle`) va `maxYozish` taymeri.
    private func yozishniToxtat() {
        maxYozishTimer?.invalidate(); maxYozishTimer = nil
        let samples = rec.stop()
        DiktovkaBand.mikrofon = false
        overlay.asosiy = nil
        AsosiyOyna.birgalik.diktovkaHolati(yozilyapti: rec.isRecording)
        let secs = Double(samples.count) / 16000.0
        // Signal darajasi logga yoziladi: "musiqa" kabi gʻalati natijalarda
        // muammo mikrofonda (jimlik) yoki modelda ekanini shu ajratib beradi.
        var peak: Float = 0
        var sumSquares = 0.0
        for s in samples {
            let a = abs(s)
            if a > peak { peak = a }
            sumSquares += Double(s) * Double(s)
        }
        let rms = samples.isEmpty ? 0 : (sumSquares / Double(samples.count)).squareRoot()
        RubaiLog.write(
            String(
                format: "stopped, samples=%d (%.1fs) peak=%.5f rms=%.5f",
                samples.count, secs, peak, rms))

        // Mikrofon jimlik uzatsa (boshqa dastur uni band qilgan, oʻchirilgan yoki
        // ruxsat amalda ishlamayapti) — modelga bermaymiz. Aks holda whisper boʻsh
        // audioga "musiqa" deb javob beradi va foydalanuvchi buni transkripsiya deb
        // oʻylaydi. Haqiqiy nutqda peak har doim bundan ancha yuqori boʻladi.
        if peak < 0.0005 {
            // Avval uzunlik, keyin signal (A7): tez ikki bosishda namuna umuman
            // boʻlmaydi va ilgari bu ham «Mikrofondan ovoz kelmadi» deyilardi.
            let davom = Date().timeIntervalSinceReferenceDate - yozishBoshlandi
            if davom < 1.0 {
                RubaiLog.write(String(format: "juda qisqa yozuv (%.2fs) — transkripsiya oʻtkazib yuborildi", davom))
                overlay.vaqtincha("Juda qisqa — kamida 1 soniya gapiring", soniya: 2.5)
            } else {
                RubaiLog.write("XATO: mikrofondan ovoz kelmadi (peak=\(peak)) — transkripsiya oʻtkazib yuborildi")
                overlay.vaqtincha("⚠️ Mikrofondan ovoz kelmadi", soniya: 5.0)
            }
            scheduleIdleUnload()
            return
        }
        let belgi = overlay.show("Matnga oʻgirilmoqda…", recording: false)
        DiktovkaBand.transkripsiya += 1
        let prepared = kuchaytir(samples)
        let ovozSoniya = Double(samples.count) / 16000.0
        let boshlandi = Date().timeIntervalSinceReferenceDate
        Whisper.shared.transcribe(prepared) { [weak self] text in
            Statistika.amalYubor(
                tur: "stt", ovoz_s: ovozSoniya,
                ishlov_s: Date().timeIntervalSinceReferenceDate - boshlandi,
                backend: Statistika.sttBackend)
            DiktovkaBand.transkripsiya -= 1
            self?.finishTranscription(text, samples: samples, belgi: belgi)
        } fail: { [weak self] msg in
            DiktovkaBand.transkripsiya -= 1
            Statistika.amalYubor(
                tur: "stt", ovoz_s: ovozSoniya,
                ishlov_s: Date().timeIntervalSinceReferenceDate - boshlandi,
                backend: Statistika.sttBackend, natija: "xato:transkripsiya")
            RubaiLog.write("transcription XATO: \(msg)")
            self?.ovozniSaqla(samples, sabab: msg)
            self?.scheduleIdleUnload()
        }
    }

    /// Logdagi ishga tushish qatori (A6): muammo xabarida «qaysi versiya,
    /// qaysi Mac, qaysi model» degan savol qolmasin.
    private func ishgaTushishQatori() -> String {
        let b = Bundle.main
        let v = b.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "?"
        let build = b.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "?"
        #if arch(arm64)
            let arx = "arm64"
        #else
            let arx = "x86_64"
        #endif
        let os = ProcessInfo.processInfo.operatingSystemVersionString
        let model =
            ModelStore.existingPath().map { yol -> String in
                let hajm = ModelTanlov.faylHajmi(URL(fileURLWithPath: yol)) ?? 0
                return "\(yol) (\(hajm) bayt)"
            } ?? "yoʻq"
        let qanday = AppDelegate.avtomatikIshgaTushdi ? "login" : "qoʻlda"
        let saqlangan = SaqlanmaganOvoz.royxat().count
        return
            "ishga tushdi: Kotib \(v) (\(build)) \(arx), \(os), model: \(model), GPU: \(Statistika.sttBackend), \(qanday)"
            + (saqlangan > 0 ? ", saqlanmagan ovoz: \(saqlangan)" : "")
    }

    private func yozishniBoshla() {
        // Qulf faqat YANGI diktovkani BOSHLASHNI bloklaydi — allaqachon
        // yozilayotgan ovozni toʻxtatish (`yozishniToxtat`) har doim oʻtishi
        // kerak, aks holda mikrofon studiya ishi tugaguncha (soatlab) tirik
        // qolib, bufer cheksiz oʻsadi.
        guard !TranskripsiyaIshi.ishlayapti else {
            overlay.vaqtincha("Fayl ustida ish ketmoqda — kuting", soniya: 2.0)
            return
        }
        // Majburiy yangilanish muhlati tugagan (S9): yangi yozuv boshlanmaydi.
        // Sabab, «Qayta urinish» va «Saytdan yuklab olish» — oynadagi bannerda.
        guard !Yangilovchi.birgalik.bloklanganmi else {
            RubaiLog.write("yozish boshlanmadi: majburiy yangilanish muhlati tugagan")
            overlay.vaqtincha("Yangilanish majburiy — oynada batafsil", soniya: 4)
            AsosiyOyna.birgalik.korsat()
            return
        }
        boshlanyapti = true
        DiktovkaBand.mikrofon = true
        boshlashVaqti = Date().timeIntervalSinceReferenceDate
        rec.start { [weak self] natija in
            guard let self = self else { return }
            self.boshlanyapti = false
            DiktovkaBand.mikrofon = self.rec.isRecording
            RubaiLog.write("record start: \(natija)")
            switch natija {
            case .ok:
                self.yozishBoshlandi = Date().timeIntervalSinceReferenceDate
                if !self.oynaOldindami {
                    let m = "Yozilmoqda… (yana \(self.hkConfig.displayString))"
                    self.overlay.asosiy = m
                    self.overlay.show(m, recording: true)
                }
                self.maxYozishniRejalashtir()
            case .ruxsatYoq:
                self.overlay.vaqtincha("Mikrofonga ruxsat yoʻq", soniya: 1.5)
            case .javobYoq:
                self.overlay.vaqtincha("⚠️ Mikrofon javob bermayapti — qayta urining", soniya: 4.0)
            case .xato(let x):
                self.overlay.vaqtincha("⚠️ \(x.xabar)", soniya: 3.0)
            }
            AsosiyOyna.birgalik.diktovkaHolati(yozilyapti: self.rec.isRecording)
        }
    }

    /// Yozish `maxYozish`dan uzoq davom etsa — oʻzi toʻxtatiladi. Bosilgan
    /// tugma sezilmay qolsa yoki foydalanuvchi toʻxtatishni unutsa, mikrofon
    /// soatlab ochiq qolib, bufer cheksiz oʻsishidan saqlaydi.
    private func maxYozishniRejalashtir() {
        maxYozishTimer?.invalidate()
        maxYozishTimer = Timer.scheduledTimer(
            withTimeInterval: AppDelegate.maxYozish,
            repeats: false
        ) { [weak self] _ in
            guard let self = self, self.rec.isRecording else { return }
            RubaiLog.write("yozish chegarasi (\(Int(AppDelegate.maxYozish))s) — oʻzi toʻxtatildi")
            self.yozishniToxtat()
        }
    }

    /// `TranskripsiyaIshi` (src/transcribe_job.swift) har bir ish tugaganda
    /// (muvaffaqiyat, xato yoki bekor qilishdan qatʼiy nazar) shu bildirishnomani
    /// joʻnatadi. Agar ilova hali diktovka qilmagan boʻlsa, idle-unload taymeri
    /// umuman oʻrnatilmagan boʻladi — shu bildirishnomasiz studiya ishi
    /// tugagach model RAM'da abadiy qolib ketardi.
    @objc private func studiyaIshTugadiKeldi() { scheduleIdleUnload() }

    func scheduleIdleUnload() {
        idleTimer?.invalidate()
        idleTimer = Timer.scheduledTimer(withTimeInterval: 180, repeats: false) { [weak self] _ in
            // 3 daqiqa ishlatilmasa RAM boʻshaydi. Lekin uzoq ish ketayotgan boʻlsa
            // (band == true) unload qilinmaydi — SHUNCHAKI oʻtkazib yuborilmaydi,
            // balki taymer qayta ishga tushiriladi, shu bilan ish tugagach ham
            // unload hali ham amalga oshadi.
            if Whisper.shared.band {
                self?.scheduleIdleUnload()
            } else {
                Whisper.shared.unload()
            }
        }
    }

    // Diqqat: bu yerda ATAYLAB rubai_unload() chaqirilmaydi. Jarayon
    // chiqishida kontekstni boʻshatish hech narsaga foyda keltirmaydi —
    // yadro manzil maydonini baribir qaytarib oladi. Aksincha, uni asosiy
    // oqimda toʻgʻridan-toʻgʻri (q navbatini va Whisper.shared.band'ni
    // chetlab oʻtib) chaqirish, agar shu payt `q`da Studiya ishi
    // whisper_full ichida boʻlsa — use-after-free. Studiya ishi soatlab
    // davom etishi mumkinligi sababli bu oyna endi juda katta.
    @objc private func quit() { NSApp.terminate(nil) }
}
