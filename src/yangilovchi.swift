// Avto-yangilanish (macOS) — Sparkle 2 ustida (spec «avto-yangilanish», D1).
//
// Sparkle feed'ni oʻqiydi, EdDSA va kod imzosini tekshiradi, fonda yuklaydi va
// bundle'ni atomar almashtiradi. Bu fayl — faqat SIYOSAT: qachon tekshirish
// (ishga tushgandan 60 s keyin, har 24 soat, uyqudan uygʻonganda), xatoda
// qayta urinish (15 daq → 1 soat → 4 soat) va qachon oʻrnatish — faqat boʻsh
// paytda: yozuv, fayl ishi, tarjima yoʻq, oxirgi diktovkadan ≥ 2 daqiqa.
// Menyu-bar ilova deyarli hech qachon yopilmaydi, shuning uchun Sparkle'ning
// «chiqishda oʻrnatish»i yetmaydi — `immediateInstallationBlock` ishlatiladi.
//
// Majburiy yangilanish (S9): imzolangan siyosat manifesti
// (`/v1/yangilanish/mac.json`) `min_versiya` va muhlatni beradi. Talab diskda
// saqlanadi (`majburiyQaror` — Windows bilan bitta jadvaldan sinalgan): muhlat
// ichida banner va boʻsh paytni kutmasdan oʻrnatish (faqat diktovka tugashini
// kutib), muhlat tugagach — yangi diktovka boshlanmaydi. Yuklashni baribir
// Sparkle qiladi: majburiy reliz feed'da «kritik» deb belgilanadi
// (`reliz.sh mac --min`), kritik element bosqichli tarqatishni chetlab oʻtadi.
// Hech qachon internetga chiqmagan ilova talabni koʻrmaydi — bloklanmaydi.
//
// Sof qarorlar — `yangilanish_siyosat.swift` (testlangan); foydalanuvchiga
// koʻrinadigan qism — `yangilash_haydovchi.swift`.

import AppKit
import Sparkle

final class Yangilovchi: NSObject, SPUUpdaterDelegate {

    static let birgalik = Yangilovchi()

    private let haydovchi = YangilashHaydovchi()
    private var updater: SPUUpdater?
    /// Ketma-ket muvaffaqiyatsiz tekshiruvlar — qayta urinish jadvali uchun.
    private var ketmaKetXato = 0
    private var qaytaTaymer: Timer?
    private var boshlashTaymer: Timer?
    private var soatTaymer: Timer?
    /// Tayyor yangilanishni oʻrnatish bloki (Sparkle beradi) va kutish holati.
    private var ornatishBloki: (() -> Void)?
    private var kutishBoshlandi: Date?
    private var boshPaytTaymer: Timer?
    /// Siyosat manifesti (majburiy talab) — ochiq kalit va soʻrov holati.
    private var ochiqKalit: Data?
    private var siyosatKetyapti = false
    /// Oxirgi siyosat soʻrovi (natijasidan qatʼi nazar) — oflayn Mac soatiga
    /// emas, kuniga bir marta urinsin (majburiy talab boʻlmasa).
    private var siyosatOxirgiUrinish: Date?
    private lazy var sessiya = URLSession(configuration: .ephemeral)
    /// Oxirgi Sparkle xatosi — majburiy bannerda koʻrsatiladi.
    private var oxirgiXato: String?

    private enum K {
        static let oxirgiMuvaffaqiyat = "yangilanish.oxirgiMuvaffaqiyat"
        static let kutilgan = "yangilanish.kutilganVersiya"
        static let kutilganIzoh = "yangilanish.kutilganIzoh"
        static let oxirgiIshlagan = "yangilanish.oxirgiIshlaganVersiya"
        static let siyosatMuvaffaqiyat = "yangilanish.siyosatMuvaffaqiyat"
        static let majburiyMin = "yangilanish.majburiyMin"
        static let majburiyMuhlat = "yangilanish.majburiyMuhlat"
        static let majburiyKorilgan = "yangilanish.majburiyKorilgan"
        /// "sinov" — sinov kanali (faqat qoʻlda: `defaults write … yangilanish.kanal sinov`).
        static let kanal = "yangilanish.kanal"
    }

    /// Bu nusxada avto-yangilanish yoqiqmi (Info.plist'da feed va kalit bor).
    var faolmi: Bool { updater != nil }

    /// Oxirgi MUVAFFAQIYATLI tekshiruv (feed oʻqildi). Spec: vaqt belgisi faqat
    /// muvaffaqiyatli javobdan keyin saqlanadi — tarmoqsiz urinish uni yangilamaydi.
    var oxirgiMuvaffaqiyat: Date? {
        let t = UserDefaults.standard.double(forKey: K.oxirgiMuvaffaqiyat)
        return t > 0 ? Date(timeIntervalSince1970: t) : nil
    }

    // MARK: Ishga tushirish

    func boshla() {
        yangilanganBolsaXabarBer()

        // Info.plist kalitlari `build.sh` da. Feed yoki ochiq kalit boʻlmasa
        // (eski build, qoʻlda yigʻilgan nusxa) — Sparkle ishga tushirilmaydi.
        let info = Bundle.main.infoDictionary ?? [:]
        guard info["SUFeedURL"] != nil, let kalit64 = info["SUPublicEDKey"] as? String else {
            RubaiLog.write("yangilanish: Info.plist'da feed yoki kalit yoʻq — oʻchiq")
            return
        }
        let u = SPUUpdater(
            hostBundle: .main, applicationBundle: .main, userDriver: haydovchi, delegate: self)
        do {
            try u.start()
        } catch {
            RubaiLog.write("yangilanish: Sparkle ishga tushmadi — \(error.localizedDescription)")
            return
        }
        updater = u
        if let k = Data(base64Encoded: kalit64), k.count == 32 { ochiqKalit = k }

        // Birinchi tekshiruv — 60 s keyin (tarmoq tayyor boʻlsin) va faqat
        // oxirgi muvaffaqiyatli tekshiruvdan 24 soat oʻtgan boʻlsa: tez-tez
        // qayta ochiladigan ilova serverni bekorga bezovta qilmasin. Majburiy
        // talab diskda turgan boʻlsa — tezroq va banner darhol.
        let majburiy = majburiyHolat != .hech
        boshlashTaymer = Timer.scheduledTimer(withTimeInterval: majburiy ? 10 : 60, repeats: false) {
            [weak self] _ in
            self?.kerakBolsaTekshir(sabab: "ishga tushish")
        }
        // Soatlik: siyosat manifestining 24 soatlik jadvali va majburiy
        // bannerdagi «N soatdan keyin». Sparkle oʻz feed jadvalini oʻzi yuritadi.
        soatTaymer = Timer.scheduledTimer(withTimeInterval: 3600, repeats: true) { [weak self] _ in
            guard let self else { return }
            self.siyosatniKerakBolsaTekshir()
            // Majburiy talab bor va tayyor emas — soatda bir Sparkle tekshiruvi.
            if self.majburiyHolat != .hech { self.majburiyniQolla(tekshir: true) }
        }
        if majburiy { majburiyniQolla(tekshir: true) }
        NSWorkspace.shared.notificationCenter.addObserver(
            self, selector: #selector(uygondi), name: NSWorkspace.didWakeNotification, object: nil)
    }

    @objc private func uygondi() { kerakBolsaTekshir(sabab: "uygʻonish") }

    private func kerakBolsaTekshir(sabab: String) {
        siyosatniKerakBolsaTekshir()
        guard let u = updater, u.canCheckForUpdates, !u.sessionInProgress else { return }
        guard
            YangilanishSiyosati.uygʻonishdaTekshirish(
                oxirgiMuvaffaqiyat: oxirgiMuvaffaqiyat, hozir: Date())
        else { return }
        RubaiLog.write("yangilanish: tekshiruv (\(sabab))")
        u.checkForUpdatesInBackground()
    }

    /// Sozlamalardagi «Hozir tekshirish». Natija oʻzbekcha oynada.
    func hozirTekshir() {
        guard let u = updater else {
            haydovchi.malumotKorsat(
                sarlavha: "Avto-yangilanish oʻchiq",
                matn: "Bu nusxa qoʻlda yigʻilgan — unda yangilanish manzili yoʻq.")
            return
        }
        siyosatniTekshir()
        if u.canCheckForUpdates { u.checkForUpdates() } else { haydovchi.oldingaChiqar() }
    }

    // MARK: Siyosat manifesti — majburiy talab (S9)

    private func siyosatniKerakBolsaTekshir() {
        // Talab bor va yangilanish hali tayyor emas — 24 soat kutilmaydi.
        let majburiy = majburiyHolat != .hech && ornatishBloki == nil
        let t = UserDefaults.standard.double(forKey: K.siyosatMuvaffaqiyat)
        let muvaffaqiyat = t > 0 ? Date(timeIntervalSince1970: t) : nil
        // 24 soat — oxirgi muvaffaqiyatdan yoki oxirgi urinishdan (qaysi biri keyin).
        let oxirgi = [muvaffaqiyat, siyosatOxirgiUrinish].compactMap { $0 }.max()
        guard majburiy || YangilanishSiyosati.uygʻonishdaTekshirish(oxirgiMuvaffaqiyat: oxirgi, hozir: Date())
        else { return }
        siyosatniTekshir()
    }

    private func siyosatniTekshir() {
        guard !siyosatKetyapti, let kalit = ochiqKalit else { return }
        siyosatKetyapti = true
        siyosatOxirgiUrinish = Date()
        // Sinov kanali — oʻsha server va kalit, alohida KV yozuvi.
        let nom = UserDefaults.standard.string(forKey: K.kanal) == "sinov" ? "mac-sinov" : "mac"
        let url = URL(string: "https://stat.mirqobilov.com/v1/yangilanish/\(nom).json")!
        let soʻrov = URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 15)
        sessiya.dataTask(with: soʻrov) { [weak self] data, javob, xato in
            let holat = (javob as? HTTPURLResponse)?.statusCode ?? 0
            DispatchQueue.main.async {
                self?.siyosatKeldi(holat: holat, data: data, xato: xato, kalit: kalit)
            }
        }.resume()
    }

    private func siyosatKeldi(holat: Int, data: Data?, xato: Error?, kalit: Data) {
        siyosatKetyapti = false
        let d = UserDefaults.standard
        let hozir = Date().timeIntervalSince1970
        switch holat {
        case 404:
            // Server bor, siyosat yoʻq — majburiy talab ham yoʻq (kill switch).
            d.set(hozir, forKey: K.siyosatMuvaffaqiyat)
            talabniTozala()
        case 200:
            guard let data, let m = YangilanishSiyosati.javobniTekshir(data, ochiqKalit: kalit, platforma: "mac")
            else {
                RubaiLog.write("yangilanish: siyosat imzosi yoki tarkibi notoʻgʻri — eʼtiborsiz qoldirildi")
                return
            }
            d.set(hozir, forKey: K.siyosatMuvaffaqiyat)
            if !m.minVersiya.isEmpty, YangilanishSiyosati.taqqosla(Statistika.joriyVersiya, m.minVersiya) < 0 {
                // Muhlat BIRINCHI koʻrilgan paytdan; yangi talab (boshqa min) — qaytadan.
                if d.string(forKey: K.majburiyMin) != m.minVersiya || d.double(forKey: K.majburiyKorilgan) <= 0 {
                    d.set(hozir, forKey: K.majburiyKorilgan)
                    RubaiLog.write("yangilanish: majburiy — Kotib \(m.minVersiya), muhlat \(m.muhlatSoat) soat")
                }
                d.set(m.minVersiya, forKey: K.majburiyMin)
                d.set(m.muhlatSoat, forKey: K.majburiyMuhlat)
            } else {
                talabniTozala()
            }
        default:
            let sabab = holat == 0 ? (xato?.localizedDescription ?? "tarmoq") : "HTTP \(holat)"
            RubaiLog.write("yangilanish: siyosat olinmadi — \(sabab)")
        }
        majburiyniQolla(tekshir: true)
    }

    private func talabniTozala() {
        let d = UserDefaults.standard
        guard d.string(forKey: K.majburiyMin) != nil else { return }
        RubaiLog.write("yangilanish: majburiy talab olib tashlandi")
        d.removeObject(forKey: K.majburiyMin)
        d.removeObject(forKey: K.majburiyMuhlat)
        d.removeObject(forKey: K.majburiyKorilgan)
    }

    /// Diskdagi talabdan: `.hech`, `.ornat` (muhlat ichida) yoki `.blokla`.
    var majburiyHolat: YangilanishSiyosati.Qaror {
        let d = UserDefaults.standard
        let k = d.double(forKey: K.majburiyKorilgan)
        return YangilanishSiyosati.majburiyQaror(
            joriy: Statistika.joriyVersiya, min: d.string(forKey: K.majburiyMin) ?? "",
            muhlatSoat: d.object(forKey: K.majburiyMuhlat) as? Int ?? 72,
            koʻrilgan: k > 0 ? Int64(k) : nil, hozir: Int64(Date().timeIntervalSince1970))
    }

    /// Muhlat tugagan — yangi diktovka boshlanmaydi (`ilova.swift`).
    var bloklanganmi: Bool { majburiyHolat == .blokla }

    /// Banner va Sparkle'ni majburiy holatga moslaydi. `tekshir` — Sparkle
    /// tekshiruvini boshlash: FAQAT soatlik taymer, siyosat natijasi, ishga
    /// tushish va «Qayta urinish» dan. Sparkle sikli tugaganda (`didFinishUpdateCycle`)
    /// faqat banner yangilanadi — aks holda oflayn yoki «yangilanish yoʻq»
    /// javobida sikl tugashi bilan yangisi boshlanib, toʻxtovsiz halqa boʻlardi.
    private func majburiyniQolla(tekshir: Bool = false) {
        let q = majburiyHolat
        guard q != .hech else {
            AsosiyOyna.birgalik.yangilanishYoq()
            return
        }
        if tekshir, ornatishBloki == nil, let u = updater, u.canCheckForUpdates, !u.sessionInProgress {
            RubaiLog.write("yangilanish: majburiy — darhol tekshiruv")
            u.checkForUpdatesInBackground()
        }
        AsosiyOyna.birgalik.yangilanishBor(majburiyBanner(q))
    }

    private func majburiyBanner(_ q: YangilanishSiyosati.Qaror) -> BannerXabar {
        let d = UserDefaults.standard
        let min = d.string(forKey: K.majburiyMin) ?? ""
        let holat: String
        var tugma: String?
        var amal: (() -> Void)?
        if ornatishBloki != nil {
            holat = "tayyor, diktovka tugagach oʻrnatiladi"
            tugma = "Hozir oʻrnatish"
            amal = { [weak self] in self?.hozirOrnat() }
        } else if updater?.sessionInProgress == true {
            holat = "yuklanmoqda…"
        } else {
            holat = oxirgiXato.map { "yuklab boʻlmadi (\($0))" } ?? "yuklab olinadi"
            tugma = "Qayta urinish"
            amal = { [weak self] in self?.majburiyQaytaUrin() }
        }
        if q == .blokla {
            // «Saytdan yuklab olish» — FAQAT avtomatik yuklash muvaffaqiyatsiz
            // boʻlganda: yuklash ketayotganda u odamni «demak saytdan oʻzim
            // yuklashim kerak» deb oʻylatadi (egasi, 2026-10-08). Xatoda esa
            // boshqa chiqish yoʻli yoʻq. Sayt manzili QATTIQ yozilgan —
            // serverdan kelgan URL ochilmaydi.
            let xato = ornatishBloki == nil && updater?.sessionInProgress != true && oxirgiXato != nil
            return BannerXabar(
                matn: "Yangilanish majburiy — diktovka toʻxtatildi. Kotib \(min): \(holat).",
                tugma: tugma, amal: amal, kalit: nil,
                ikkinchi: xato
                    ? ("Saytdan yuklab olish", { NSWorkspace.shared.open(URL(string: "https://uzb.mirqobilov.com")!) })
                    : nil)
        }
        let muhlat = d.object(forKey: K.majburiyMuhlat) as? Int ?? 72
        let otgan = max(0, Date().timeIntervalSince1970 - d.double(forKey: K.majburiyKorilgan))
        let qoldi = max(1, Int((Double(muhlat) * 3600 - otgan + 3599) / 3600))
        return BannerXabar(
            matn: "Muhim yangilanish: Kotib \(min) — \(holat). \(qoldi) soatdan keyin diktovka toʻxtaydi.",
            tugma: tugma, amal: amal, kalit: nil)
    }

    private func majburiyQaytaUrin() {
        oxirgiXato = nil
        siyosatniTekshir()
        majburiyniQolla(tekshir: true)
    }

    /// Tayyor yangilanishni hozir oʻrnatish (majburiy banner tugmasi). Yozuv
    /// ketayotgan boʻlsa — kutadi: ovoz yoʻqolmasin.
    private func hozirOrnat() {
        guard let blok = ornatishBloki else { return }
        if DiktovkaBand.faol {
            (NSApp.delegate as? AppDelegate)?.overlay.vaqtincha("Diktovka tugagach oʻrnatiladi", soniya: 3)
            return
        }
        // Foydalanuvchi oʻzi soʻradi — ochiq sheet/modal oyna yopilishni bekor qilmasin.
        Yangilovchi.ochiqOynalarniYop()
        ornatishBloki = nil
        kutishBoshlandi = nil
        boshPaytTaymer?.invalidate()
        RubaiLog.write("yangilanish: foydalanuvchi soʻradi — oʻrnatilmoqda va qayta ochiladi")
        blok()
    }

    // MARK: SPUUpdaterDelegate — tekshiruv natijasi

    /// Sinov kanali (`defaults write com.rubaistt.dictation yangilanish.kanal sinov`):
    /// Sparkle ham sinov feed'ini oʻqisin — aks holda sinov siyosatidagi
    /// `min_versiya` ga yetib boʻlmas va sinovchi bloklanib qolardi. Oʻsha server,
    /// oʻsha imzo kaliti (`SURequireSignedFeed`).
    func feedURLString(for updater: SPUUpdater) -> String? {
        UserDefaults.standard.string(forKey: K.kanal) == "sinov"
            ? "https://stat.mirqobilov.com/v1/appcast/mac-sinov.xml" : nil
    }

    func updater(_ updater: SPUUpdater, didFinishLoading appcast: SUAppcast) {
        UserDefaults.standard.set(Date().timeIntervalSince1970, forKey: K.oxirgiMuvaffaqiyat)
    }

    func updater(
        _ updater: SPUUpdater, didFinishUpdateCycleFor updateCheck: SPUUpdateCheck, error: Error?
    ) {
        let e = error as NSError?
        let yangilanishYoq =
            e.map { $0.domain == SUSparkleErrorDomain && $0.code == Int(SUError.noUpdateError.rawValue) } ?? false
        let bekor =
            e.map { $0.domain == SUSparkleErrorDomain && $0.code == Int(SUError.installationCanceledError.rawValue) }
            ?? false
        defer { if majburiyHolat != .hech { majburiyniQolla() } }
        guard let e, !yangilanishYoq, !bekor else {
            ketmaKetXato = 0
            oxirgiXato = nil
            qaytaTaymer?.invalidate()
            return
        }
        RubaiLog.write("yangilanish: xato (\(e.domain) \(e.code)) — \(e.localizedDescription)")
        oxirgiXato = e.localizedDescription
        guard let kechikish = YangilanishSiyosati.qaytaUrinishKechikishi(ketmaKetXato) else {
            ketmaKetXato = 0  // urinishlar tugadi — odatdagi 24 soatlik jadval
            return
        }
        ketmaKetXato += 1
        qaytaTaymer?.invalidate()
        qaytaTaymer = Timer.scheduledTimer(withTimeInterval: kechikish, repeats: false) { [weak self] _ in
            guard let u = self?.updater, u.canCheckForUpdates, !u.sessionInProgress else { return }
            RubaiLog.write("yangilanish: qayta urinish")
            u.checkForUpdatesInBackground()
        }
    }

    // MARK: SPUUpdaterDelegate — oʻrnatish

    /// Yangilanish yuklandi va tekshirildi. `true` qaytarib Sparkle'ga «oʻzimiz
    /// aytamiz» deymiz va blokni boʻsh paytda chaqiramiz.
    func updater(
        _ updater: SPUUpdater, willInstallUpdateOnQuit item: SUAppcastItem,
        immediateInstallationBlock immediateInstallHandler: @escaping () -> Void
    ) -> Bool {
        ornatishBloki = immediateInstallHandler
        if kutishBoshlandi == nil { kutishBoshlandi = Date() }
        // Qayta ochilgach «Kotib X ga yangilandi» xabari uchun.
        UserDefaults.standard.set(item.displayVersionString, forKey: K.kutilgan)
        UserDefaults.standard.set(Self.qisqaIzoh(item), forKey: K.kutilganIzoh)
        RubaiLog.write("yangilanish: \(item.displayVersionString) tayyor — boʻsh paytda oʻrnatiladi")
        boshPaytniKut()
        if majburiyHolat != .hech { majburiyniQolla() }
        return true
    }

    private var bandmi: Bool {
        DiktovkaBand.faol || TranskripsiyaIshi.ishlayapti || Tarjimon.shared.band || modalOchiq
    }

    /// Ochiq sheet yoki modal oyna (NSAlert, Saqlash paneli) ilovani yopishni
    /// BEKOR qiladi: Sparkle'ning «quit» buyrugʻi «App termination blocked by
    /// modal sheet» bilan rad etiladi va oʻrnatish qayta urinilmaydi — majburiy
    /// banner abadiy «yuklanmoqda…» da qolardi (S9 sinovi, 2026-10-08).
    /// Shuning uchun oʻrnatish ular yopilguncha kutadi.
    /// Ochiq sheet va modal oynalarni yopadi — FAQAT foydalanuvchi oʻrnatishni
    /// oʻzi soʻraganda (fon yoʻli ularni kutadi, odamning ishiga tegmaydi).
    static func ochiqOynalarniYop() {
        if NSApp.modalWindow != nil { NSApp.abortModal() }
        for w in NSApp.windows {
            if let sheet = w.attachedSheet { w.endSheet(sheet) }
        }
    }

    private var modalOchiq: Bool {
        NSApp.modalWindow != nil || NSApp.windows.contains { $0.attachedSheet != nil }
    }

    private func boshPaytniKut() {
        boshPaytTaymer?.invalidate()
        boshPaytTaymer = Timer.scheduledTimer(withTimeInterval: 30, repeats: true) { [weak self] t in
            guard let self, let boshlandi = self.kutishBoshlandi, self.ornatishBloki != nil else {
                t.invalidate()
                return
            }
            // Majburiy yangilanish boʻsh paytni kutmaydi — faqat diktovka tugashini.
            let mumkin =
                self.majburiyHolat != .hech
                ? !DiktovkaBand.faol && !self.modalOchiq
                : YangilanishSiyosati.ornatishMumkinmi(
                    band: self.bandmi, oxirgiFaollik: DiktovkaBand.oxirgiFaollik,
                    kutishBoshlandi: boshlandi, hozir: Date())
            guard mumkin else { return }
            t.invalidate()
            let blok = self.ornatishBloki
            self.ornatishBloki = nil
            self.kutishBoshlandi = nil
            RubaiLog.write("yangilanish: boʻsh payt — oʻrnatilmoqda va qayta ochiladi")
            blok?()
        }
    }

    // MARK: Yangilangandan keyin

    /// Oldingi ishga tushishdan beri versiya oʻzgargan va u aynan biz kutgan
    /// yangilanish boʻlsa — xabar: qisqa overlay va oynadagi banner.
    private func yangilanganBolsaXabarBer() {
        let d = UserDefaults.standard
        let joriy = Statistika.joriyVersiya
        let oldingi = d.string(forKey: K.oxirgiIshlagan)
        d.set(joriy, forKey: K.oxirgiIshlagan)
        guard let oldingi, oldingi != joriy, d.string(forKey: K.kutilgan) == joriy else { return }
        let izoh = d.string(forKey: K.kutilganIzoh) ?? ""
        d.removeObject(forKey: K.kutilgan)
        d.removeObject(forKey: K.kutilganIzoh)
        RubaiLog.write("yangilanish: \(oldingi) → \(joriy)")
        let matn = "Kotib \(joriy) ga yangilandi"
        (NSApp.delegate as? AppDelegate)?.overlay.vaqtincha("✓ \(matn)", soniya: 4)
        AsosiyOyna.birgalik.yangilanishBor(
            BannerXabar(
                matn: izoh.isEmpty ? matn : "\(matn) — \(izoh)",
                tugma: nil, amal: nil, kalit: "yangilandi-\(joriy)"))
    }

    /// Appcast'dagi izoh → bir qatorli qisqa matn. Izoh Markdown
    /// (`CHANGELOG.md` boʻlimi, `reliz.sh mac`) yoki HTML boʻlishi mumkin:
    /// teglar, `###` sarlavhalar va roʻyxat belgilari olib tashlanadi, bandlar
    /// «; » bilan qoʻshiladi.
    static func qisqaIzoh(_ item: SUAppcastItem) -> String {
        YangilanishSiyosati.qisqaIzoh(item.itemDescription ?? item.title ?? "")
    }
}
