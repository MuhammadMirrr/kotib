// Matnga oʻgirib boʻlmagan diktovka ovozi (barqarorlik A2): WAV'ga saqlash,
// menyudagi «Qayta urinish» va avtomatik bir martalik urinish. Sof qismi
// (WAV, nom, 20 ta / 7 kun) — `saqlanmagan.swift`.

import AppKit

extension AppDelegate {
    /// Matnga oʻgirib boʻlmagan ovozni WAV qilib saqlaydi — 1.1.0 gacha u
    /// shu yerda tashlanardi. Yozish fon oqimida (10 daqiqa ≈ 19 MB).
    func ovozniSaqla(_ samples: [Float], sabab: String) {
        DispatchQueue.global(qos: .userInitiated).async {
            let url = SaqlanmaganOvoz.saqla(samples)
            DispatchQueue.main.async {
                if let url {
                    RubaiLog.write("ovoz saqlandi: \(url.lastPathComponent) (\(samples.count) namuna)")
                    self.overlay.vaqtincha("⚠️ \(sabab) — ovoz saqlandi, menyuda «Qayta urinish»", soniya: 6)
                } else {
                    RubaiLog.write("XATO: ovozni saqlab boʻlmadi")
                    self.overlay.vaqtincha("⚠️ \(sabab)", soniya: 3.5)
                }
            }
        }
    }

    @objc func qaytaUrinBosildi() { saqlanganlarniOgir(qolda: true) }

    /// Model ishlagani maʼlum boʻlgach (muvaffaqiyatli diktovka, model yuklab
    /// olindi) saqlangan ovozlar bir marta oʻzi sinaladi. Foydalanuvchi shu
    /// payt diktovka qilayotgan yoki fayl ishi ketayotgan boʻlsa — kutiladi:
    /// whisper navbati serial, qayta urinish uning diktovkasini kechiktirmasin.
    func avtoQaytaUrin() {
        guard !DiktovkaBand.faol, !TranskripsiyaIshi.ishlayapti else { return }
        saqlanganlarniOgir(qolda: false)
    }

    /// Saqlangan ovozlarni eskisidan boshlab ketma-ket matnga oʻgiradi.
    /// Natija tarixga yoziladi (asl yozilgan sana bilan). Qoʻlda bosilganda
    /// clipboard'ga ham qoʻyiladi — lekin HECH QACHON fokusdagi ilovaga
    /// kiritilmaydi: u kutilmagan paytda, boshqa oynaga tushib qolardi.
    func saqlanganlarniOgir(qolda: Bool) {
        guard !qaytaIshlanyapti else { return }
        let navbat = SaqlanmaganOvoz.royxat().filter { qolda || !avtoUrinilgan.contains($0.lastPathComponent) }
        guard !navbat.isEmpty else {
            if qolda { overlay.vaqtincha("Saqlangan ovoz yoʻq", soniya: 2) }
            return
        }
        qaytaIshlanyapti = true
        RubaiLog.write("saqlangan ovoz: \(navbat.count) ta qayta urinish (\(qolda ? "qoʻlda" : "avtomatik"))")
        if qolda { overlay.show("Saqlangan ovoz matnga oʻgirilmoqda…", recording: false) }
        var matnlar: [String] = []
        var xato: String?

        func tugadi() {
            qaytaIshlanyapti = false
            scheduleIdleUnload()
            if !matnlar.isEmpty {
                NotificationCenter.default.post(name: .diktovkaTarixYangilandi, object: nil)
            }
            RubaiLog.write("saqlangan ovoz: \(matnlar.count) ta matnga oʻgirildi\(xato.map { ", xato: \($0)" } ?? "")")
            if qolda {
                if !matnlar.isEmpty {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(matnlar.joined(separator: "\n\n"), forType: .string)
                    overlay.vaqtincha(
                        "✓ \(matnlar.count) ta ovoz matnga oʻgirildi — clipboard'da (⌘V) va tarixda", soniya: 5)
                } else if let xato {
                    overlay.vaqtincha("⚠️ \(xato) — ovoz saqlanib qoldi", soniya: 4)
                } else {
                    overlay.vaqtincha("Saqlangan ovozda nutq topilmadi", soniya: 3)
                }
            } else if !matnlar.isEmpty {
                overlay.vaqtincha("✓ Saqlangan \(matnlar.count) ta ovoz matnga oʻgirildi — Kotib oynasida", soniya: 4)
            }
        }

        func keyingi(_ i: Int) {
            // Avtomatik rejimda foydalanuvchi diktovkani boshlasa — toʻxtaymiz;
            // qolganlari keyingi muvaffaqiyatli diktovkadan keyin sinaladi.
            guard i < navbat.count, qolda || !DiktovkaBand.faol else { tugadi(); return }
            let url = navbat[i]
            avtoUrinilgan.insert(url.lastPathComponent)
            DispatchQueue.global(qos: .userInitiated).async {
                let s = (try? Data(contentsOf: url)).flatMap { SaqlanmaganOvoz.namunalar($0) }
                DispatchQueue.main.async {
                    guard let s else {
                        RubaiLog.write("XATO: saqlangan ovoz oʻqilmadi: \(url.lastPathComponent)")
                        keyingi(i + 1)
                        return
                    }
                    Whisper.shared.transcribe(kuchaytir(s)) { xom in
                        let matn = matnniTayyorla(xom, apostrof: Prefs.apostrof)
                        if !matn.isEmpty {
                            let sana = SaqlanmaganOvoz.sana(nomdan: url.lastPathComponent) ?? Date()
                            let yozuv = DiktovkaYozuvi(matn: matn, davomiylik: Double(s.count) / 16000, sana: sana)
                            if !DiktovkaTarixi.birgalik.qoshish(yozuv) { RubaiLog.write("tarixga yozib boʻlmadi") }
                            matnlar.append(matn)
                        }
                        // Boʻsh natija — ovozda nutq yoʻq edi; saqlashdan maʼno yoʻq.
                        try? FileManager.default.removeItem(at: url)
                        keyingi(i + 1)
                    } fail: { msg in
                        // Model hali ham ishlamayapti — qolganini sinash befoyda.
                        xato = msg
                        tugadi()
                    }
                }
            }
        }
        keyingi(0)
    }
}

extension AppDelegate: NSMenuDelegate {
    /// Status menyusi ochilayotganda — saqlangan ovoz bormi, nechta.
    func menuNeedsUpdate(_ menu: NSMenu) {
        let n = SaqlanmaganOvoz.royxat().count
        qaytaItem.isHidden = n == 0
        qaytaItem.title = "Saqlangan ovozni matnga oʻgirish (\(n))"
    }
}
