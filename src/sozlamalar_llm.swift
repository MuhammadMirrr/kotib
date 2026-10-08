// Sozlamalar: AI (LLM) provayderi — kalit (Keychain), base URL, model, ulanishni
// tekshirish va model roʻyxatini provayderdan olish. `SozlamalarVC` kengaytmalari.

import AppKit
import Security

extension SozlamalarVC {
    // MARK: LLM

    func llmniYukla() {
        if let p = LLMSozlama.tanlangan,
            let i = provayderlar.firstIndex(where: { $0.id == p.id })
        {
            provayderPopup.selectItem(at: i + 1)  // +1 — "tanlanmagan"
            kalitMaydon.stringValue = Kalitlar.oqi(provayder: p.id)
        } else {
            provayderPopup.selectItem(at: 0)
        }
        baseURLMaydon.stringValue = LLMSozlama.baseURL
        modelCombo.stringValue = LLMSozlama.model
        xosQatorniYangila()
        // Avval keshdan — sozlamalar darrov toʻla koʻrinsin; keyin fonda
        // API'dan yangilanadi.
        modellarniKorsat(LLMSozlama.keshdagiModellar(LLMSozlama.tanlanganID))
        modellarniOl()
    }

    /// Base URL / model faqat "custom" provayderda koʻrinadi.
    private func xosQatorniYangila() {
        let i = provayderPopup.indexOfSelectedItem
        xosQator.isHidden = !(i > 0 && provayderlar[i - 1].id == "custom")
    }

    @objc func provayderOzgardi() {
        let i = provayderPopup.indexOfSelectedItem
        guard i > 0 else {
            LLMSozlama.tanlanganID = ""
            baseURLMaydon.stringValue = ""
            modelCombo.stringValue = ""
            modelCombo.removeAllItems()
            kalitMaydon.stringValue = ""
            eskirganQator.isHidden = true
            modelHolati.stringValue = ""
            xosQatorniYangila()
            NotificationCenter.default.post(name: .llmSozlamaSaqlandi, object: nil)
            return
        }
        let p = provayderlar[i - 1]
        LLMSozlama.provayderniTanla(p.id)
        baseURLMaydon.stringValue = p.baseURL
        modelCombo.stringValue = p.standartModel
        kalitMaydon.stringValue = Kalitlar.oqi(provayder: p.id)
        tekshirNatija.stringValue = ""
        xosQatorniYangila()
        llmniSaqla()
        modellarniKorsat(LLMSozlama.keshdagiModellar(p.id))
        modellarniOl()
    }

    func llmniSaqla() {
        guard baseURLMaydon != nil else { return }
        LLMSozlama.baseURL = baseURLMaydon.stringValue.trimmingCharacters(in: .whitespaces)
        LLMSozlama.model = modelCombo.stringValue.trimmingCharacters(in: .whitespaces)
        if let p = LLMSozlama.tanlangan {
            let rc = Kalitlar.saqla(
                provayder: p.id,
                kalit: kalitMaydon.stringValue.trimmingCharacters(in: .whitespaces))
            // Ilgari xato faqat logga tushardi va foydalanuvchi kalit saqlandi
            // deb oʻylardi — keyin «Matnni yaxshilash» sababsiz yoʻqolardi (F4).
            if rc != errSecSuccess {
                tekshirNatija.stringValue = "⚠️ Kalit Keychain'ga saqlanmadi (kod \(rc)). Qayta kiritib koʻring."
            }
        }
        // Audio tabi shu bildirishnomani tinglab "Matnni yaxshilash" tugmasini
        // koʻrsatadi/yashiradi — kalit kiritilishi bilan darhol.
        NotificationCenter.default.post(name: .llmSozlamaSaqlandi, object: nil)
    }

    // MARK: Modellarni provayderdan olish

    /// Roʻyxatni combo box'ga soladi va saqlangan model hali mavjudligini
    /// tekshiradi. Boʻsh roʻyxat kelsa — combo shunchaki erkin matn maydoni
    /// boʻlib qoladi (provayderda /models yoʻq yoki tarmoq yoʻq).
    private func modellarniKorsat(_ royxat: [String]) {
        let joriy = modelCombo.stringValue
        modelCombo.removeAllItems()
        guard !royxat.isEmpty else { eskirganQator.isHidden = true; return }
        modelCombo.addItems(withObjectValues: royxat)
        // `removeAllItems` matnni ham tozalaydi — foydalanuvchi tanlovini
        // qaytaramiz.
        modelCombo.stringValue = joriy

        // Saqlangan model roʻyxatdan yoʻqolganmi? (Aynan `deepseek-chat` bilan
        // boʻlgani.) Bunday holatda OGOHLANTIRAMIZ, lekin oʻzimiz almashtirmaymiz.
        if !joriy.isEmpty, !royxat.contains(joriy) {
            taklifQilingan = royxat[0]
            eskirganYozuv.stringValue =
                "«\(joriy)» provayder roʻyxatida yoʻq. Mavjud: «\(taklifQilingan)»"
            eskirganTugma.matnniAlmashtir("«\(taklifQilingan)»ga oʻtish")
            eskirganQator.isHidden = false
        } else {
            eskirganQator.isHidden = true
        }
    }

    @objc func taklifgaOt() {
        guard !taklifQilingan.isEmpty else { return }
        modelCombo.stringValue = taklifQilingan
        eskirganQator.isHidden = true
        llmniSaqla()
        RubaiLog.write("sozlamalar: model \(taklifQilingan) ga oʻtildi")
    }

    @objc func modellarniYangila() { modellarniOl(qolda: true) }

    /// Provayderdan model roʻyxatini oladi. Kalit yoʻq boʻlsa jim turadi —
    /// bu xato emas, shunchaki hali sozlanmagan.
    private func modellarniOl(qolda: Bool = false) {
        modelTask?.cancel()
        guard let p = LLMSozlama.tanlangan else { return }
        let kalit = kalitMaydon.stringValue.trimmingCharacters(in: .whitespaces)
        let base = baseURLMaydon.stringValue.trimmingCharacters(in: .whitespaces)
        guard !kalit.isEmpty, !base.isEmpty else {
            if qolda { modelHolati.stringValue = "Avval API kalitni kiriting" }
            return
        }
        modelHolati.stringValue = "Modellar olinmoqda…"
        modelTask = Task { [weak self] in
            do {
                let royxat = try await LLMMijoz().modellar(provayder: p, baseURL: base, kalit: kalit)
                guard !Task.isCancelled else { return }
                await MainActor.run {
                    guard let self else { return }
                    if royxat.isEmpty {
                        self.modelHolati.stringValue =
                            "Bu provayder model roʻyxatini bermaydi — nomni qoʻlda yozing"
                    } else {
                        LLMSozlama.modellarniSaqla(p.id, royxat)
                        self.modellarniKorsat(royxat)
                        self.modelHolati.stringValue = "\(royxat.count) ta model"
                    }
                }
            } catch let e as LLMXato {
                guard !Task.isCancelled else { return }
                await MainActor.run { self?.modelHolati.stringValue = "⚠️ " + e.xabar }
            } catch {
                guard !Task.isCancelled else { return }
                await MainActor.run { self?.modelHolati.stringValue = "⚠️ " + LLMXato.tarmoq.xabar }
            }
        }
    }

    @objc func ulanishniTekshir() {
        llmniSaqla()
        guard let p = LLMSozlama.tanlangan else {
            tekshirNatija.stringValue = "Avval provayderni tanlang"
            return
        }
        tekshirTugma.isEnabled = false
        tekshirNatija.stringValue = "Tekshirilmoqda…"

        Task {
            var javob = ""
            do {
                try await LLMMijoz().oqim(
                    provayder: p, baseURL: LLMSozlama.baseURL,
                    model: LLMSozlama.model, kalit: LLMSozlama.joriyKalit,
                    system: "Faqat «ha» deb javob ber.",
                    user: "Salom",
                    onDelta: { javob += $0 })
                await MainActor.run {
                    tekshirNatija.stringValue =
                        javob.isEmpty
                        ? "⚠️ Javob boʻsh keldi" : "✓ Ulanish ishlayapti"
                    tekshirTugma.isEnabled = true
                }
            } catch let e as LLMXato {
                await MainActor.run {
                    tekshirNatija.stringValue = "⚠️ " + e.xabar
                    tekshirTugma.isEnabled = true
                }
            } catch {
                await MainActor.run {
                    tekshirNatija.stringValue = "⚠️ " + LLMXato.tarmoq.xabar
                    tekshirTugma.isEnabled = true
                }
            }
        }
    }
}

extension SozlamalarVC: NSComboBoxDelegate {
    /// Maydondan chiqilganda darhol saqlaymiz — "Saqlash" tugmasi yoʻq.
    ///
    /// Kalit yoki Base URL oʻzgargan boʻlsa model roʻyxati ham boshqacha
    /// boʻlishi mumkin — shuning uchun uni qayta soʻraymiz.
    func controlTextDidEndEditing(_ obj: Notification) {
        llmniSaqla()
        let manba = obj.object as AnyObject
        if manba === kalitMaydon || manba === baseURLMaydon { modellarniOl() }
    }

    /// Roʻyxatdan model tanlanganda darhol saqlanadi.
    func comboBoxSelectionDidChange(_ notification: Notification) {
        // Tanlov `stringValue` ga keyingi runloop aylanishida tushadi.
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.llmniSaqla()
            self.eskirganQator.isHidden = true
        }
    }
}
