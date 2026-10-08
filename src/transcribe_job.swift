// Bitta faylni matnga oʻgirish ishi: dekodlash → whisper → saqlash.
// Progress: har bir boʻlak butun ish barining OʻZ ulushiga (slice) ega —
// shu ulush ichida dekodlash 0–10%ni, transkripsiya 10–100%ni egallaydi.
// Shu tarzda koʻp boʻlakli ishda ham progress bar bir tekis, orqaga
// qaytmasdan oʻsadi (bitta boʻlakli faylda bitta ulush — butun bar).

import Foundation

/// 90 daqiqadan uzun fayllar boʻlaklanadi — 1 soat ≈ 230 MB RAM.
private let bolaklashChegarasi: Double = 90 * 60
private let bolakUzunligi: Double = 30 * 60
/// Boʻlak chegarasini shu oynada jimlik boʻyicha tanlaymiz.
private let chegaraOynasi: Double = 30

/// Ish tugaganda (muvaffaqiyat, xato yoki bekor qilishdan qatʼiy nazar)
/// joʻnatiladi. AppDelegate shuni tinglab, idle-unload taymerini qayta
/// ishga tushiradi — aks holda, agar ilova diktovka qilmasdan toʻgʻridan
/// toʻgʻri studiya ishini boshlagan boʻlsa, taymer umuman oʻrnatilmagan
/// boʻlib qoladi va model ish tugagach ham RAM'da abadiy qolib ketadi.
extension Notification.Name { static let studiyaIshTugadi = Notification.Name("studiyaIshTugadi") }

final class TranskripsiyaIshi {

    private static let holatLock = NSLock()
    private static var _ishlayapti = false
    /// Bir vaqtda faqat bitta ish. Diktovka (⌃⌥D) ham shu bayroqni tekshiradi.
    static var ishlayapti: Bool {
        holatLock.lock(); defer { holatLock.unlock() }; return _ishlayapti
    }
    /// Tekshirish va oʻrnatishni bitta qulf ostida birlashtiradi — ular
    /// orasida poyga holati (race) boʻlmasin. Muvaffaqiyatli boʻlsa (ish
    /// "olindi") true qaytaradi va shu zahoti `band`ni ham shu qulf ostida
    /// oʻrnatadi.
    private static func ishNiOlish() -> Bool {
        holatLock.lock(); defer { holatLock.unlock() }
        guard !_ishlayapti else { return false }
        _ishlayapti = true
        Whisper.shared.band = true
        return true
    }
    private static func holatOrnat(_ v: Bool) {
        holatLock.lock(); _ishlayapti = v; holatLock.unlock()
    }

    private let url: URL
    private let bekorLock = NSLock()
    private var _bekor = false

    var onProgress: ((Double, String) -> Void)?  // 0…1, holat matni
    var onTayyor: ((Hujjat) -> Void)?
    var onXato: ((String) -> Void)?

    init(url: URL) { self.url = url }

    func bekorQil() { bekorLock.lock(); _bekor = true; bekorLock.unlock() }
    private var bekorMi: Bool {
        bekorLock.lock(); defer { bekorLock.unlock() }; return _bekor
    }

    func boshla() {
        guard TranskripsiyaIshi.ishNiOlish() else {
            DispatchQueue.main.async {
                self.onXato?("Boshqa fayl ustida ish ketmoqda. Tugashini kuting.")
            }
            return
        }
        // `self` shu yerda ATAYLAB kuchli ushlanadi: ish oʻzini oʻzi tirik
        // saqlashi kerak. `[weak self]` boʻlganda chaqiruvchi obʼyektni
        // darhol qoʻyib yuborsa (yoki navbat hali boshlanmagan boʻlsa),
        // `ishla()` HECH QACHON ishga tushmaydi — lekin bayroqlar yuqorida
        // allaqachon `true` qilib qoʻyilgan boʻladi va hech kim ularni
        // tozalamaydi: model butun jarayon davomida RAM'da qulflanib qoladi.
        DispatchQueue.global(qos: .userInitiated).async {
            self.ishla()
        }
    }

    private func tugat() {
        TranskripsiyaIshi.holatOrnat(false)
        Whisper.shared.band = false
        // Bildirishnomani asosiy navbatda joʻnatamiz — tinglovchi
        // (AppDelegate) undan `Timer.scheduledTimer` chaqiradi, u esa
        // asosiy run loop'ga bogʻliq.
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: .studiyaIshTugadi, object: nil)
        }
    }

    private func xato(_ m: String) {
        tugat()
        DispatchQueue.main.async { self.onXato?(m) }
    }

    /// Har chaqiruvda koʻrilgan eng yuqori qiymat — progress bar hech qachon
    /// orqaga qaytmasligi uchun shu yerda monoton qilib qisqichlanadi (M1).
    /// Boʻlak chegaralarida ikkita ulush hisoblash usuli (`dekodlashUlushi`/
    /// `transkripsiyaUlushi`) orasida bir necha soniyalik nomuvofiqlik
    /// boʻlishi mumkin — buni manba(lar)ni "toʻgʻrilash" oʻrniga sink'da
    /// qisqichlash ancha soddaroq va ishonchli.
    private var oxirgiProgress: Double = 0

    private func progress(_ p: Double, _ s: String) {
        let m = max(p, oxirgiProgress)
        oxirgiProgress = m
        DispatchQueue.main.async { self.onProgress?(m, s) }
    }

    private func ishla() {
        // Har ish oʻzi yangi `TranskripsiyaIshi` sifatida yaratiladi, lekin
        // shu yerda ANIQ nolga qaytarish — instansiya qayta ishlatilsa ham
        // (masalan kelajakda) ikkinchi ish birinchisining yuqori nuqtasida
        // "qotib qolmasligi" uchun.
        oxirgiProgress = 0
        let malumot: MediaMalumot
        do {
            malumot = try mediaMalumot(url)
        } catch let e as MediaXato {
            xato(e.xabar); return
        } catch {
            xato(MediaXato.oqishXatosi.xabar); return
        }

        let davomiylik = malumot.davomiylik
        RubaiLog.write("studiya: \(url.lastPathComponent), \(Int(davomiylik))s")
        let boshlandi = Date().timeIntervalSinceReferenceDate

        var hammaSegmentlar: [Segment] = []

        if davomiylik <= bolaklashChegarasi {
            guard
                let segs = bolakniQaytaIshla(
                    boshi: 0, oxiri: davomiylik,
                    umumiy: davomiylik, siljish: 0)
            else { return }
            hammaSegmentlar = segs
        } else {
            var boshi: Double = 0
            while true {
                if bekorMi { xato(MediaXato.bekorQilindi.xabar); return }
                // Boʻlakni chegara oynasi bilan birga oʻqiymiz.
                let taxminiyOxir = min(boshi + bolakUzunligi + chegaraOynasi, davomiylik)
                let oxirgiMi = taxminiyOxir >= davomiylik
                guard
                    let (segs, haqiqiyOxir) = bolakniKesibQaytaIshla(
                        boshi: boshi, taxminiyOxir: taxminiyOxir,
                        umumiy: davomiylik, oxirgiMi: oxirgiMi)
                else { return }
                hammaSegmentlar.append(contentsOf: segs)
                // Oxirgi boʻlakdan keyin SHARTSIZ toʻxtaymiz — `oxirgiMi`
                // bayrogʻiga tayanamiz, `boshi < davomiylik` shartiga emas.
                // Haqiqiy dekodlangan oxir (`haqiqiyOxir`, namunalar soniga
                // asoslangan) konteyner eʼlon qilgan `davomiylik`dan (CMTime
                // yaxlitlash, kodek "priming" va h.k. tufayli) deyarli doim
                // biroz farq qiladi. Agar shartga tayansak, oxirgi boʻlakdan
                // keyin deyarli boʻsh oraliq ustida yana bir marta uriniladi,
                // `namunalarniOqi` `.bosh` xatosini tashlaydi va butun
                // (bir necha soatlik boʻlishi mumkin) tayyor transkripsiya
                // notoʻgʻri "Fayl boʻsh" xabari bilan yoʻqqa chiqariladi.
                if oxirgiMi { break }
                boshi = haqiqiyOxir
            }
        }

        guard !hammaSegmentlar.isEmpty else {
            Statistika.amalYubor(
                tur: "stt", ovoz_s: davomiylik,
                ishlov_s: Date().timeIntervalSinceReferenceDate - boshlandi,
                backend: Statistika.sttBackend, natija: "xato:nutq_yoq")
            xato("Faylda nutq aniqlanmadi."); return
        }

        // Studiya (fayl) transkripsiyasi tugadi — anonim oʻlchov (kontentsiz).
        Statistika.amalYubor(
            tur: "stt", ovoz_s: davomiylik,
            ishlov_s: Date().timeIntervalSinceReferenceDate - boshlandi,
            backend: Statistika.sttBackend)

        do {
            let h = try HujjatOmbori.saqla(
                manba: url, davomiylik: davomiylik,
                segmentlar: hammaSegmentlar,
                apostrof: Prefs.apostrof)
            tugat()
            DispatchQueue.main.async { self.onTayyor?(h) }
        } catch {
            xato("Diskka saqlab boʻlmadi. Joy yetarli boʻlmasligi mumkin.")
        }
    }

    /// Bitta oraliqni oʻqib whisper'ga beradi. Segmentlar vaqtlari `siljish` ga suriladi.
    /// `oxiri` shu boʻlakka ajratilgan ulush (slice)ning oxiri sifatida ham
    /// ishlatiladi — bitta boʻlakli faylda bu butun `davomiylik`, demak
    /// ulush = [0, 1].
    private func bolakniQaytaIshla(
        boshi: Double, oxiri: Double,
        umumiy: Double, siljish: Double
    ) -> [Segment]? {
        var namunalar: [Float]
        do {
            namunalar = try namunalarniOqi(
                url, boshi: boshi, oxiri: oxiri,
                progress: { [weak self] u in
                    self?.progress(
                        dekodlashUlushi(u, chunkBoshi: boshi, chunkOxiri: oxiri, umumiy: umumiy),
                        "Audio ajratilmoqda…")
                },
                bekor: { [weak self] in self?.bekorMi ?? true })
        } catch let e as MediaXato {
            xato(e.xabar); return nil
        } catch {
            xato(MediaXato.oqishXatosi.xabar); return nil
        }
        // Joyida, nusxasiz (F7). `var tayyor = namunalar` qilinsa COW baribir
        // nusxa olardi — `namunalar` shu paytda hali tirik.
        kuchaytirJoyida(&namunalar)
        return whisperGaBer(
            namunalar, chunkBoshi: boshi, chunkOxiri: oxiri,
            umumiy: umumiy, siljish: siljish)
    }

    /// Boʻlakni oʻqib, oxiridagi jimlik nuqtasida kesadi.
    /// Qaytaradi: (segmentlar, keyingi boʻlak boshlanadigan vaqt).
    private func bolakniKesibQaytaIshla(
        boshi: Double, taxminiyOxir: Double,
        umumiy: Double,
        oxirgiMi: Bool
    ) -> ([Segment], Double)? {
        var namunalar: [Float]
        do {
            namunalar = try namunalarniOqi(
                url, boshi: boshi, oxiri: taxminiyOxir,
                progress: { [weak self] u in
                    self?.progress(
                        dekodlashUlushi(u, chunkBoshi: boshi, chunkOxiri: taxminiyOxir, umumiy: umumiy),
                        "Audio ajratilmoqda…")
                },
                bekor: { [weak self] in self?.bekorMi ?? true })
        } catch let e as MediaXato {
            xato(e.xabar); return nil
        } catch {
            xato(MediaXato.oqishXatosi.xabar); return nil
        }

        var kesim = namunalar.count
        if !oxirgiMi {
            let oynaBoshi = max(0, namunalar.count - Int(chegaraOynasi) * namunaTezligi)
            kesim = jimlikNuqtasi(namunalar, oynaBoshi: oynaBoshi, oynaOxiri: namunalar.count)
        }
        // Kesim joyida: `Array(namunalar[0..<kesim])` 115 MB lik nusxa olardi,
        // ustiga `kuchaytir` yana bittasini (F7).
        namunalar.removeSubrange(kesim..<namunalar.count)
        kuchaytirJoyida(&namunalar)
        let haqiqiyOxir = boshi + Double(kesim) / Double(namunaTezligi)

        // Progress ulushi (slice) uchun `taxminiyOxir` beramiz — dekodlash
        // bosqichi ham AYNAN shu chegara bilan hisoblangan edi (yuqorida).
        // Haqiqiy kesim nuqtasi (`haqiqiyOxir`) undan biroz kichik boʻlishi
        // mumkin (jimlik chegara oynasi ichida topiladi), lekin bu ikkalasi
        // bir xil ulush ichida qolgani uchun progress hech qachon 30
        // foizlik sakrash bilan orqaga qaytmaydi — eng yomon holatda ham
        // chegaralar orasidagi farq bor-yoʻgʻi bir necha soniyalik ulushga
        // toʻgʻri keladi.
        guard
            let segs = whisperGaBer(
                namunalar, chunkBoshi: boshi, chunkOxiri: taxminiyOxir,
                umumiy: umumiy, siljish: boshi)
        else { return nil }
        return (segs, haqiqiyOxir)
    }

    private func whisperGaBer(
        _ namunalar: [Float], chunkBoshi: Double, chunkOxiri: Double,
        umumiy: Double, siljish: Double
    ) -> [Segment]? {
        let sem = DispatchSemaphore(value: 0)
        var natija: [Segment] = []
        var xatoMatn: String? = nil

        Whisper.shared.transcribeSegments(
            namunalar,
            progress: { [weak self] pct in
                let bolakUlush = Double(pct) / 100.0
                self?.progress(
                    transkripsiyaUlushi(
                        bolakUlush, chunkBoshi: chunkBoshi,
                        chunkOxiri: chunkOxiri, umumiy: umumiy),
                    "Matnga oʻgirilmoqda…")
            },
            bekor: { [weak self] in self?.bekorMi ?? true },
            done: { segs in
                natija = segs; sem.signal()
            },
            fail: { m in
                xatoMatn = m; sem.signal()
            })

        sem.wait()
        if let m = xatoMatn {
            xato(m == "bekor" ? MediaXato.bekorQilindi.xabar : m)
            return nil
        }
        guard siljish != 0 else { return natija }
        return natija.map { Segment(t0: $0.t0 + siljish, t1: $0.t1 + siljish, matn: $0.matn) }
    }
}

/// Boʻlakning [chunkBoshi, chunkOxiri] oraligʻi butun fayl (`umumiy`)ga
/// nisbatan qaysi ulushni (slice) egallashini hisoblaydi, soʻng shu ulush
/// ichida dekodlash bosqichiga ajratilgan BIRINCHI 10%ni `u` (0…1, shu
/// oʻqishning oʻz ichidagi ulushi) boʻyicha toʻldiradi.
private func dekodlashUlushi(_ u: Double, chunkBoshi: Double, chunkOxiri: Double, umumiy: Double) -> Double {
    guard umumiy > 0 else { return 0 }
    let ulushBoshi = chunkBoshi / umumiy
    let ulushKengligi = (chunkOxiri - chunkBoshi) / umumiy
    return ulushBoshi + u * 0.10 * ulushKengligi
}

/// Xuddi shu ulushning QOLGAN 90%ini transkripsiya bosqichiga ajratadi.
private func transkripsiyaUlushi(_ bolakUlush: Double, chunkBoshi: Double, chunkOxiri: Double, umumiy: Double) -> Double
{
    guard umumiy > 0 else { return 0 }
    let ulushBoshi = chunkBoshi / umumiy
    let ulushKengligi = (chunkOxiri - chunkBoshi) / umumiy
    return ulushBoshi + 0.10 * ulushKengligi + bolakUlush * 0.90 * ulushKengligi
}
