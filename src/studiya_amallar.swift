// Studiya: matn ustidagi LLM amallari («Matnni yaxshilash ⌄», avtomatik
// tozalash, bekor qilish) va eksport (Saqlash…, Nusxa olish). `StudiyaVC` kengaytmasi.

import AppKit

extension StudiyaVC {
    // MARK: LLM amallari

    func amallarniUlash() {
        transkriptTayyor = { [weak self] h in self?.avtomatikTozalash(h) }
        // Sozlamalar sheet'ida LLM kaliti saqlanganda tugma darhol yoqilishi
        // uchun. Kuzatuvchi olib tashlanmaydi: AsosiyOyna StudiyaVC'ni butun
        // jarayon umri davomida keshlab turadi (hech qachon deinit boʻlmaydi),
        // selektor asosidagi kuzatuvchilar esa macOS 10.11'dan beri
        // "zeroing weak" — obyekt yoʻq qilinganda ham xavfsiz.
        NotificationCenter.default.addObserver(
            self, selector: #selector(llmSozlamaYangilandi),
            name: .llmSozlamaSaqlandi, object: nil)
        tugmalarniYangila()
    }

    @objc private func llmSozlamaYangilandi() { tugmalarniYangila() }

    func tugmalarniYangila() {
        // Dizayn: tugma LLM sozlanmagan boʻlsa umuman koʻrinmaydi.
        yaxshilashTugma?.isHidden = !LLMSozlama.sozlanganmi
        yaxshilashTugma?.isEnabled = joriy != nil && !bandmi
        // Tarjima oflayn ishlaydi — LLM sozlamasiga bogʻliq emas, shuning uchun
        // hech qachon yashirilmaydi.
        tarjimaTugma?.isEnabled = joriy != nil && !bandmi
    }

    /// "Matnni yaxshilash ⌄" — dizayndagi yagona LLM boshqaruvi. Barcha amallar
    /// shu menyudan chiqadi (ilgari ular oʻng tomonda alohida panel edi).
    @objc func yaxshilashBosildi() {
        let m = NSMenu()
        for amal in amallar {
            let item = NSMenuItem(title: amal.nom, action: #selector(amalTanlandi(_:)), keyEquivalent: "")
            item.representedObject = amal.id
            item.target = self
            m.addItem(item)
        }
        m.addItem(.separator())
        let erkin = NSMenuItem(title: "Oʻz soʻrovim…", action: #selector(erkinBosildi), keyEquivalent: "")
        erkin.target = self
        m.addItem(erkin)
        // Menyu tugmaning TEPASIDA ochiladi — tugma oynaning pastki chetida
        // turibdi, pastga ochilsa ekrandan chiqib ketardi.
        m.popUp(
            positioning: nil,
            at: NSPoint(x: 0, y: -6),
            in: yaxshilashTugma)
    }

    /// «Tarjima qilish ⌄» — tillar menyusi. Tepada «Tarjima» tabida oxirgi
    /// ishlatilgan til (`tr.maqsad`), keyin eng koʻp soʻraladigan uchtasi,
    /// oxirida toʻliq 202 tillik tanlagichga oʻtish.
    @objc func tarjimaBosildi() {
        let m = NSMenu()
        var korsatilgan: [Til] = []

        func qosh(_ t: Til) {
            // Oʻzbekchaga tarjima qilishning maʼnosi yoʻq — manba oʻzbekcha.
            guard t != .uz, !korsatilgan.contains(t) else { return }
            korsatilgan.append(t)
            let i = NSMenuItem(title: t.nom, action: #selector(tilTanlandi(_:)), keyEquivalent: "")
            i.representedObject = t.nllb
            i.target = self
            m.addItem(i)
        }

        if let oxirgi = UserDefaults.standard.string(forKey: "tr.maqsad"),
            let t = Til.topilsin(oxirgi)
        {
            qosh(t)
        }
        qosh(.ru)
        qosh(.en)
        if let turk = Til.topilsin("tur_Latn") { qosh(turk) }

        m.addItem(.separator())
        let boshqa = NSMenuItem(title: "Boshqa til…", action: #selector(boshqaTil), keyEquivalent: "")
        boshqa.target = self
        m.addItem(boshqa)

        // Yaxshilash menyusi kabi tepaga ochiladi — tugma oynaning pastida.
        m.popUp(positioning: nil, at: NSPoint(x: 0, y: -6), in: tarjimaTugma)
    }

    @objc private func tilTanlandi(_ sender: NSMenuItem) {
        guard let kod = sender.representedObject as? String,
            let til = Til.topilsin(kod)
        else { return }
        tarjimagaUzat(til)
    }

    /// Til koʻrsatilmaydi: matn «Tarjima» tabiga qoʻyiladi, foydalanuvchi
    /// oʻsha yerdagi qidiruvli roʻyxatdan tilni oʻzi tanlaydi. Menyuga 202 ta
    /// band solishdan koʻra shu maʼqul — tanlagich allaqachon oʻsha yerda.
    @objc private func boshqaTil() { tarjimagaUzat(nil) }

    private func tarjimagaUzat(_ til: Til?) {
        // `NSTextView.string` ichki bufer ustidan dangasa koʻrinish qaytaradi —
        // nusxa majburlanadi (`tarjima_view.swift` dagi ⇄ xatosiga qarang).
        let matn = String(matnView.string[...])
        guard !matn.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return }
        onTarjima?(matn, til)
    }

    @objc private func amalTanlandi(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? String,
            let amal = amallar.first(where: { $0.id == id })
        else { return }
        amalniBajar(amal)
    }

    @objc private func erkinBosildi() {
        let a = NSAlert()
        a.messageText = "Oʻz soʻrovingiz"
        a.informativeText = "Matn ustida nima qilish kerakligini yozing."
        let maydon = NSTextField(frame: NSRect(x: 0, y: 0, width: 320, height: 24))
        maydon.placeholderString = "masalan: inglizchaga tarjima qil"
        a.accessoryView = maydon
        a.addButton(withTitle: "Bajarish")
        a.addButton(withTitle: "Bekor")
        guard a.runModal() == .alertFirstButtonReturn else { return }
        let korsatma = maydon.stringValue.trimmingCharacters(in: .whitespaces)
        guard !korsatma.isEmpty else { return }
        amalniBajar(erkinAmal(korsatma))
    }

    private func amalniBajar(_ amal: Amal) {
        // Qayta kirishdan himoya: qoʻlda bosishda tugma allaqachon oʻchirilgan,
        // lekin avtomatikTozalash bu himoyani chetlab oʻtib bevosita shu yerga
        // keladi — masalan foydalanuvchi bitta amal ishlab turganda yangi fayl
        // tashlab, uni transkripsiya qilib boʻlsa.
        guard !bandmi else { return }
        guard let h = joriy else { return }
        let manba = HujjatOmbori.matnOqi(id: h.id, tur: .chiroyli)
        bandmi = true
        // Bu oqimni AYNAN shu hujjatga bogʻlaymiz. `hujjatniOch`/`turniAlmashtir`
        // koʻrinish almashganda `oqimToken`ni oshiradi — quyidagi `token` oʻsha
        // paytdagi qiymatni closure orqali "muzlatib" saqlaydi, shuning uchun
        // keyinroq kelgan delta/onTayyor/onXato eski koʻrinishni yangi hujjat
        // ustiga yozib qoʻymaydi.
        oqimToken &+= 1
        let token = oqimToken
        let docID = h.id
        tugmalarniYangila()
        progressniKorsat(true)
        progressBar.isIndeterminate = true
        progressBar.startAnimation(nil)
        holatYozuvi.stringValue = "\(amal.nom)…"
        natijaniKorsat(amal: amal.id, matn: "")

        amalTask = amalIshi.bajar(
            amal: amal, matn: manba,
            onDelta: { [weak self] d in
                guard let self, self.oqimToken == token, self.joriy?.id == docID else { return }
                self.natijagaQosh(d)
            },
            onTayyor: { [weak self] natija in
                guard let self else { return }
                // Natija OʻZ hujjatiga saqlanadi — foydalanuvchi boshqa hujjatga
                // oʻtib ketgan boʻlsa ham (tokenga qaramasdan).
                try? HujjatOmbori.natijaSaqla(id: docID, amal: amal.id, matn: natija)
                self.ishTugadi()
            },
            onXato: { [weak self] m in
                guard let self else { return }
                self.ishTugadi()
                // Faqat shu oqim hali "joriy" boʻlsa xato oynasini koʻrsatamiz —
                // aks holda foydalanuvchi allaqachon boshqa hujjatga qarab
                // turganida uning ustiga eski amalning xatosi chiqib ketardi.
                guard self.oqimToken == token, self.joriy?.id == docID else { return }
                // Amal boshida matn maydoni boʻshatilgan edi (natija shu yerga
                // oqib tushishi kerak edi). Amal yiqilsa uni SHU HOLDA qoldirib
                // boʻlmaydi: transkript diskda turibdi, lekin foydalanuvchi boʻsh
                // ekranni koʻradi va matnini yoʻqotdim deb oʻylaydi. Tayyor
                // matnni qaytaramiz, keyin xatoni aytamiz.
                self.matnniOqi(.chiroyli)
                self.ogohlantir("Xato", m)
            })
    }

    private func ishTugadi() {
        bandmi = false
        amalTask = nil
        progressBar.stopAnimation(nil)
        progressBar.isIndeterminate = false
        progressniKorsat(false)
        tugmalarniYangila()
    }

    /// Transkript tayyor boʻlgach, kalit bor boʻlsa avtomatik tozalaymiz.
    private func avtomatikTozalash(_ h: Hujjat) {
        guard LLMSozlama.sozlanganmi else { return }
        guard let tozalash = amallar.first(where: { $0.id == "tozalash" }) else { return }
        amalniBajar(tozalash)
    }

    // MARK: Eksport

    @objc func eksport() {
        guard joriy != nil else { return }
        let p = NSSavePanel()
        p.title = "Matnni saqlash"
        p.nameFieldStringValue =
            ((joriy?.manbaNomi ?? "matn") as NSString).deletingPathExtension + ".txt"
        p.allowedContentTypes = [.plainText]
        guard p.runModal() == .OK, let url = p.url else { return }
        try? matnView.string.write(to: url, atomically: true, encoding: .utf8)
    }

    @objc func nusxaOl() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(matnView.string, forType: .string)
    }
}
