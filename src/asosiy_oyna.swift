// Ilovaning yagona asosiy oynasi.
//
// Tuzilishi (dizayn: "Kotib - yangi.dc.html"): yuqorida toolbar — markazda
// ikki bandli segment (Yozish / Fayl), oʻngda ⚙ tugmasi; ostida tanlangan
// tabning koʻrinishi. Sozlamalar ALOHIDA BOʻLIM EMAS — u ⚙ orqali ochiladigan
// modal sheet. Yoʻriqnoma boʻlimi ham yoʻq: uning oʻrnini Yozish tabidagi
// ruxsat banneri egallaydi.
//
// Nega yon panel emas: ilovada faqat ikkita haqiqiy ish rejimi bor (gapirib
// yozish va faylni matnga oʻgirish). Toʻrt bandli manba-roʻyxat ular ustiga
// keraksiz ierarxiya qoʻshardi — foydalanuvchi "qayerdaman" degan savolni
// oʻzicha hal qilishi kerak boʻlardi.
//
// LSUIElement gotcha: ilova menyu satri ilovasi. Oyna ochilganda faollik
// siyosati .regular ga oʻtadi (Dock va ⌘Tab da koʻrinishi uchun), yopilganda
// .accessory ga qaytadi. Koʻrsatishda orderFrontRegardless ishlatiladi —
// makeKeyAndOrderFront LSUIElement ilovalarda ishonchsiz. Ammo u oynani KEY
// qilmaydi, key oyna boʻlmasa esa menyu satridagi ⌘C/⌘W kabi bandlar oʻchiq
// qoladi — shuning uchun keyingi runloop qadamida `makeKey()` chaqiriladi
// (`keyQil()`).
//
// Tab VC'lari birinchi tanlanganda yaratiladi va keshlanadi: FaylVC whisper
// ishining holatini, SozlamalarVC Keychain'dan oʻqilgan qiymatlarni ushlab
// turadi — har safar qayta yaratish ularni yoʻqotadi.

import AppKit

enum Bolim: Int, CaseIterable {
    case yozish, audio, tarjima

    var nom: String {
        switch self {
        case .yozish: return "Yozish"
        case .audio: return "Audio"
        case .tarjima: return "Tarjima"
        }
    }
}

final class AsosiyOyna: NSObject, NSWindowDelegate, NSToolbarDelegate {

    static let birgalik = AsosiyOyna()

    /// Diktovka tugmasi bosilganda — AppDelegate.toggle ga ulanadi.
    var onDiktovka: (() -> Void)?
    /// Sozlamalar saqlanganda yangi hotkey.
    /// Tugmani qoʻllaydi (AppDelegate). Tizim qabul qilmasa — false.
    var onHotkeySaqlandi: ((HotKeyConfig) -> Bool)?

    var hotkeyMatni: String = "⌃⌥D" {
        didSet { yozishVC?.hotkeyMatni = hotkeyMatni }
    }

    /// Yangi versiya topildi. Oyna hali qurilmagan boʻlishi mumkin (ilova
    /// menyu satrida ishlab turibdi) — shuning uchun maʼlumot saqlanadi va
    /// oyna birinchi ochilganda koʻrsatiladi. Aks holda ishga tushishdan
    /// keyin darhol kelgan javob yoʻqolib ketardi.
    private var kutayotganYangilanish: BannerXabar?

    /// Yangilanish xabari (`yangilovchi.swift`). Oyna hali ochilmagan boʻlsa
    /// saqlanadi va ochilganda koʻrsatiladi.
    func yangilanishBor(_ m: BannerXabar) {
        kutayotganYangilanish = m
        konteyner?.yangilanishKorsat(m)
    }

    /// Majburiy yangilanish talabi yoʻqoldi (S9) — banner yashiriladi. Faqat
    /// kalitsiz (yopilmaydigan) banner: «yangilandi» xabariga tegilmaydi.
    func yangilanishYoq() {
        guard let m = kutayotganYangilanish, m.kalit == nil else { return }
        kutayotganYangilanish = nil
        konteyner?.bannerniYashir()
    }

    private var window: NSWindow?
    private var konteyner: KonteynerVC?
    private var segment: NSSegmentedControl!

    private var yozishVC: DiktovkaVC?
    private var faylVC: StudiyaVC?
    private var tarjimaVC: TarjimaVC?
    private var tarjimaYuklovchi: TarjimaYuklovchi?
    private var sozlamalarVC: SozlamalarVC?
    private var sozlamalarOyna: NSWindow?
    private var hotkeyKonfig: HotKeyConfig?
    /// Oxirgi maʼlum diktovka holati — DiktovkaVC keyinroq (lazy) yaratilsa ham
    /// karta darrov toʻgʻri holatda koʻrsatilishi uchun keshlanadi.
    private var yozilyaptiHolati: Bool = false
    private var joriyBolim: Bolim = .yozish

    /// AppDelegate ishga tushganda joriy hotkey konfiguratsiyasini beradi.
    func hotkeyniOrnat(_ c: HotKeyConfig) {
        hotkeyKonfig = c
        hotkeyMatni = c.displayString
        sozlamalarVC?.update(c)
    }

    // MARK: Koʻrsatish

    func korsat(_ bolim: Bolim = .yozish) {
        if window == nil { qur() }
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
        window?.orderFrontRegardless()
        keyQil()
        tabniTanla(bolim)
    }

    /// Oynani oldinga chiqaradi. Ochiq boʻlsa tab OʻZGARMAYDI — foydalanuvchi
    /// Audio tabida ishlayotganda `.app` ustiga bosgani uni Yozishga uloqtirmasligi
    /// kerak. Oyna hali yoʻq boʻlsa — Yozish bilan ochiladi.
    func oldingaChiqar() {
        guard window != nil else { korsat(.yozish); return }
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
        window?.orderFrontRegardless()
        keyQil()
    }

    /// `orderFrontRegardless` oynani oldinga chiqaradi, lekin uni KEY QILMAYDI.
    /// Key oyna boʻlmasa, menyu bandlarining javobgarlik zanjiri boʻsh qoladi va
    /// ⌘C, ⌘W kabi standart bandlar oʻchiq turadi (`menyu.swift`). Aktivlashuv
    /// asinxron — shuning uchun key qilish keyingi runloop qadamiga qoldiriladi,
    /// aks holda LSUIElement ilovada u eʼtiborsiz qoladi.
    private func keyQil() {
        DispatchQueue.main.async { [weak self] in
            self?.window?.makeKey()
        }
    }

    func windowWillClose(_ n: Notification) {
        // Tekshiruv Self.faollikSiyosatiniYangila() da — Donat oynasi
        // (settings.swift) ham xuddi shu funksiyani chaqiradi, shunda aktivlik
        // siyosati QAYSI oyna yopilganidan qatʼi nazar bitta joyda saqlanadi.
        Self.faollikSiyosatiniYangila()
    }

    /// Ilovaning "haqiqiy" (titled) oynalaridan birortasi yopilganda chaqiriladi.
    /// Hech biri qolmasa — menyu satri rejimiga (.accessory) qaytamiz. `.titled`
    /// filtri shart: NSApp.windows tarkibida ilovaning status-bar oynasi
    /// (NSStatusBarWindow) ham bor, u butun ilova umri davomida koʻrinuvchan —
    /// filtrlanmasa `ochiq` doim true boʻlib qoladi va Dock ikonkasi hech qachon
    /// yoʻqolmaydi. `DispatchQueue.main.async` — yopilayotgan oyna
    /// `windowWillClose` chaqirilgan payt hali `isVisible` boʻlishi mumkin.
    static func faollikSiyosatiniYangila() {
        DispatchQueue.main.async {
            let ochiq = NSApp.windows.contains { $0.isVisible && $0.styleMask.contains(.titled) }
            if !ochiq { NSApp.setActivationPolicy(.accessory) }
        }
    }

    // MARK: Qurish

    private static let segmentIdent = NSToolbarItem.Identifier("kotib.segment")
    private static let sozlamalarIdent = NSToolbarItem.Identifier("kotib.sozlamalar")

    private func qur() {
        konteyner = KonteynerVC()

        segment = NSSegmentedControl(
            labels: Bolim.allCases.map(\.nom),
            trackingMode: .selectOne,
            target: self, action: #selector(segmentOzgardi))
        segment.selectedSegment = 0
        segment.font = U.f(13, .semibold)
        // Uch band 84pt da sigʻmaydi — "Tarjima" kesilib qoladi.
        for i in Bolim.allCases.indices { segment.setWidth(92, forSegment: i) }

        let w = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 900, height: 640),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered, defer: false)
        w.title = "Kotib"
        w.titleVisibility = .hidden
        w.minSize = NSSize(width: 820, height: 560)
        w.contentViewController = konteyner
        // Oyna qurilishidan oldin kelgan yangilanish xabarini koʻrsatamiz.
        if let m = kutayotganYangilanish { konteyner?.yangilanishKorsat(m) }
        // Dizayn faqat yorugʻ rejada chizilgan va ranglar brendning oʻzi —
        // tizim qorongʻi rejasiga ergashsa koʻrinish buziladi.
        w.appearance = NSAppearance(named: .aqua)
        w.center()
        w.delegate = self
        w.isReleasedWhenClosed = false

        let toolbar = NSToolbar(identifier: "kotib.toolbar")
        toolbar.delegate = self
        toolbar.displayMode = .iconOnly
        toolbar.centeredItemIdentifier = Self.segmentIdent
        w.toolbar = toolbar
        w.toolbarStyle = .unified

        window = w
        tabniKorsat(.yozish)
    }

    // MARK: NSToolbarDelegate

    func toolbarAllowedItemIdentifiers(_ t: NSToolbar) -> [NSToolbarItem.Identifier] {
        [Self.segmentIdent, .flexibleSpace, Self.sozlamalarIdent]
    }

    func toolbarDefaultItemIdentifiers(_ t: NSToolbar) -> [NSToolbarItem.Identifier] {
        [Self.segmentIdent, .flexibleSpace, Self.sozlamalarIdent]
    }

    func toolbar(
        _ t: NSToolbar, itemForItemIdentifier id: NSToolbarItem.Identifier,
        willBeInsertedIntoToolbar flag: Bool
    ) -> NSToolbarItem? {
        switch id {
        case Self.segmentIdent:
            let item = NSToolbarItem(itemIdentifier: id)
            item.view = segment
            item.label = "Boʻlim"
            return item
        case Self.sozlamalarIdent:
            let item = NSToolbarItem(itemIdentifier: id)
            let b = NSButton(
                image: NSImage(
                    systemSymbolName: "gearshape",
                    accessibilityDescription: "Sozlamalar")!,
                target: self, action: #selector(sozlamalarniOch))
            b.bezelStyle = .texturedRounded
            b.isBordered = false
            b.contentTintColor = U.matn
            item.view = b
            item.label = "Sozlamalar"
            item.toolTip = "Sozlamalar"
            return item
        default:
            return nil
        }
    }

    // MARK: Tablar

    @objc private func segmentOzgardi() {
        guard let bolim = Bolim(rawValue: segment.selectedSegment) else { return }
        tabniKorsat(bolim)
    }

    private func tabniTanla(_ bolim: Bolim) {
        segment.selectedSegment = bolim.rawValue
        tabniKorsat(bolim)
    }

    private func tabniKorsat(_ bolim: Bolim) {
        joriyBolim = bolim
        switch bolim {
        case .yozish:
            let yangiYaratildi = yozishVC == nil
            if yangiYaratildi {
                let vc = DiktovkaVC()
                vc.hotkeyMatni = hotkeyMatni
                vc.onDiktovka = { [weak self] in self?.onDiktovka?() }
                yozishVC = vc
            }
            konteyner?.korsat(yozishVC!)
            // holatniOzgartir view ierarxiyasiga bevosita yozadi — .view (va u
            // orqali loadView) allaqachon yuklangan boʻlishi kerak, shu sababli
            // konteyner.korsat'dan KEYIN chaqiriladi.
            if yangiYaratildi { yozishVC?.holatniOzgartir(yozilyapti: yozilyaptiHolati) }
        case .audio:
            konteyner?.korsat(faylniOl())
        case .tarjima:
            let vc = tarjimaniOl()
            konteyner?.korsat(vc)
            // Model boshqa tabda turganda yuklab olingan boʻlishi mumkin —
            // holat har koʻrsatishda qayta tekshiriladi.
            vc.modelHolatiniYangila()
        }
    }

    private func faylniOl() -> StudiyaVC {
        if let s = faylVC { return s }
        let s = StudiyaVC()
        s.onTarjima = { [weak self] matn, til in self?.tarjimagaMatn(matn, maqsad: til) }
        faylVC = s
        return s
    }

    /// «Audio» tabidagi transkriptni «Tarjima» tabiga oʻtkazadi.
    /// `til` nil boʻlsa tarjima boshlanmaydi — foydalanuvchi tilni oʻsha yerda
    /// tanlaydi. Model yoʻq boʻlsa tab oʻz bannerini koʻrsatadi, shuning uchun
    /// bu yerda alohida tekshiruv shart emas.
    func tarjimagaMatn(_ matn: String, maqsad til: Til?) {
        korsat(.tarjima)
        tarjimaniOl().matnniQabulQil(matn, maqsad: til)
    }

    private func tarjimaniOl() -> TarjimaVC {
        if let t = tarjimaVC { return t }
        let t = TarjimaVC()
        t.onModelKerak = { [weak self] in self?.tarjimaModeliniYukla() }
        tarjimaVC = t
        return t
    }

    /// Tarjima modelini yuklab olish sheet'i. Bir vaqtda faqat bitta yuklovchi —
    /// aks holda ikkalasi bitta `.part` fayliga yozadi va arxiv buziladi.
    private func tarjimaModeliniYukla() {
        guard let window, tarjimaYuklovchi == nil else { return }
        let y = TarjimaYuklovchi()
        tarjimaYuklovchi = y
        y.boshla(ustida: window) { [weak self] muvaffaqiyat in
            self?.tarjimaYuklovchi = nil
            if muvaffaqiyat { self?.tarjimaVC?.modelHolatiniYangila() }
        }
    }

    // MARK: Menyu satri amallari
    //
    // Bu uchtasi `menyu.swift` dagi bandlar uchun. Ular oynasiz ham chaqirilishi
    // mumkin (ilova `.accessory` rejimda, oyna hali qurilmagan) — shuning uchun
    // har biri avval oynani koʻrsatadi.

    @objc func menyuSozlamalar() {
        if window == nil { korsat(.yozish) }
        sozlamalarniOch()
    }

    @objc func menyuFaylOch() {
        korsat(.audio)
        faylniOl().faylTanla()
    }

    @objc func menyuBolim(_ sender: NSMenuItem) {
        guard let b = Bolim(rawValue: sender.tag) else { return }
        korsat(b)
    }

    // MARK: Sozlamalar sheet

    @objc private func sozlamalarniOch() {
        guard let window else { return }
        if sozlamalarVC == nil {
            let vc = SozlamalarVC(hotkeyKonfig ?? HotKeyStore.load())
            vc.onSave = { [weak self] cfg in
                guard let self, self.onHotkeySaqlandi?(cfg) == true else { return false }
                self.hotkeyKonfig = cfg
                self.hotkeyMatni = cfg.displayString
                return true
            }
            vc.onYop = { [weak self] in self?.sozlamalarniYop() }
            sozlamalarVC = vc
        }
        if sozlamalarOyna == nil {
            let idish = SozlamalarSheetVC(sozlamalarVC!)
            idish.onYop = { [weak self] in self?.sozlamalarniYop() }
            let s = NSWindow(
                contentRect: NSRect(x: 0, y: 0, width: 560, height: 588),
                styleMask: [.titled, .fullSizeContentView],
                backing: .buffered, defer: false)
            s.contentViewController = idish
            s.titleVisibility = .hidden
            s.titlebarAppearsTransparent = true
            s.appearance = NSAppearance(named: .aqua)
            s.isReleasedWhenClosed = false
            sozlamalarOyna = s
        }
        guard let sheet = sozlamalarOyna, sheet.sheetParent == nil else { return }
        window.beginSheet(sheet)
    }

    private func sozlamalarniYop() {
        guard let window, let sheet = sozlamalarOyna, sheet.sheetParent != nil else { return }
        window.endSheet(sheet)
    }

    /// Finder "Open With" yoki fayl tashlanganda — oynani Audio tabida ochib
    /// faylni beradi.
    func studiyagaFayl(_ url: URL) {
        korsat(.audio)
        faylniOl().faylniQabulQil(url)
    }

    /// AppDelegate yozish holati oʻzgarganda chaqiradi.
    func diktovkaHolati(yozilyapti: Bool) {
        yozilyaptiHolati = yozilyapti
        yozishVC?.holatniOzgartir(yozilyapti: yozilyapti)
    }
}
