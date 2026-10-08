// Kotib — tarjima modelini yuklab olish va ochish.
//
// Model 3,1 GB va ilova ichida kelmaydi. Yuklash `ModelDownloader` bilan bir
// xil naqshda: `.part` fayl va HTTP Range bilan davom ettirish — sekin yoki
// uzilib turadigan internetda bu shart.
//
// Ochish tartibi MUHIM: arxiv avval vaqtinchalik papkaga ochiladi va faqat
// `TarjimaModel.tayyormi` toʻgʻri deganda asl joyiga koʻchiriladi. Aks holda
// uzilib qolgan ochish yarim papka qoldiradi va ilova uni «tayyor» deb
// oʻqiydi — CTranslate2 esa tushunarsiz xato bilan quladi.
//
// Butunlik tekshiruvi: `.tar.gz` ning oʻzida gzip CRC32 bor, shuning uchun
// buzilgan yoki yarim fayl `tar -xzf` da noldan boshqa kod bilan tugaydi.
// Alohida SHA-256 fayli qoʻshilmaydi — u yana bitta CDN obyekti va yana
// bitta kesh tuzogʻi boʻlardi.

import AppKit

final class TarjimaYuklovchi: NSObject, URLSessionDataDelegate {

    /// CDN'dagi arxiv. Prefiks HECH QACHON qayta ishlatilmaydi: obyektlarda
    /// `Cache-Control: immutable` va Cloudflare eski nusxani bir yil ushlaydi.
    static let url = URL(string: "https://cdn.mirqobilov.com/dl/tarjima/v2/nllb-200-3.3B-int8.tar.gz")!

    private var session: URLSession!
    private var task: URLSessionDataTask?
    private var handle: FileHandle?
    private var olingan: Int64 = 0
    private var jami: Int64 = TarjimaModel.taxminiyBayt

    private var oyna: NSWindow?
    private var bar: NSProgressIndicator!
    private var holat: NSTextField!
    private var tugma: NSButton!
    private var yopTugma: NSButton!
    private var tugadi: ((Bool) -> Void)?
    private var xatoBerdi = false

    private var arxiv: URL { Yollar.qollab.appendingPathComponent("tarjima-model.tar.gz") }
    private var qism: URL { arxiv.appendingPathExtension("part") }
    private var vaqtinchalik: URL {
        Yollar.qollab.appendingPathComponent("tarjima-model.yangi", isDirectory: true)
    }

    private static let fmt: ByteCountFormatter = {
        let f = ByteCountFormatter()
        f.allowedUnits = [.useMB, .useGB]
        f.countStyle = .file
        return f
    }()

    func boshla(ustida ota: NSWindow, tugadi: @escaping (Bool) -> Void) {
        self.tugadi = tugadi
        oynaniQur()
        guard let oyna else { tugadi(false); return }
        ota.beginSheet(oyna)
        yuklashniBoshla()
    }

    // MARK: Yuklash

    private func yuklashniBoshla() {
        xatoBerdi = false
        tugma.title = "Bekor qilish"
        yopTugma.isHidden = true
        holat.textColor = .secondaryLabelColor
        holat.stringValue = "Ulanmoqda…"
        bar.isIndeterminate = true
        bar.startAnimation(nil)

        let fm = FileManager.default
        try? fm.createDirectory(at: Yollar.qollab, withIntermediateDirectories: true)

        // Joyni OLDINDAN tekshiramiz. Busiz toʻla diskda yuklash 3 GB dan
        // keyin qulardi va foydalanuvchi sababini bilmasdi. Yarim yoʻlda
        // qolgan `.part` fayl ham hisobga olinadi — u allaqachon diskda.
        let qismHajmi = (try? fm.attributesOfItem(atPath: qism.path)[.size] as? NSNumber)??.int64Value ?? 0
        guard
            TarjimaModel.yetarliJoyBormi(
                Yollar.qollab,
                kerak: TarjimaModel.kerakliJoy - qismHajmi)
        else {
            // Yuqoriga yaxlitlanadi: 6,48 GB ni «6 GB» deb aytish foydalanuvchini
            // chalgʻitadi — u joy boʻshatib qaytadi va yana toʻsiladi.
            let kerakGB = (TarjimaModel.kerakliJoy + 999_999_999) / 1_000_000_000
            return xatoKorsat("Diskda joy yetarli emas — kamida \(kerakGB) GB kerak.")
        }

        var ofset: Int64 = 0
        if qismHajmi > 0 { ofset = qismHajmi } else { try? fm.removeItem(at: qism) }
        if !fm.fileExists(atPath: qism.path) { fm.createFile(atPath: qism.path, contents: nil) }
        guard let h = try? FileHandle(forWritingTo: qism) else {
            return xatoKorsat("Faylni ochib boʻlmadi. Diskda joy borligini tekshiring.")
        }
        handle = h
        _ = try? h.seekToEnd()
        olingan = ofset

        var req = URLRequest(url: Self.url)
        req.timeoutInterval = 60
        if ofset > 0 { req.setValue("bytes=\(ofset)-", forHTTPHeaderField: "Range") }
        RubaiLog.write("tarjima modeli: yuklash boshlandi (ofset \(ofset))")

        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForResource = 3600 * 4
        cfg.waitsForConnectivity = true
        session = URLSession(configuration: cfg, delegate: self, delegateQueue: nil)
        task = session.dataTask(with: req)
        task?.resume()
    }

    func urlSession(
        _ s: URLSession, dataTask: URLSessionDataTask,
        didReceive response: URLResponse,
        completionHandler: @escaping (URLSession.ResponseDisposition) -> Void
    ) {
        guard let http = response as? HTTPURLResponse else { completionHandler(.allow); return }
        // 416 — soʻralgan joy fayl oxiridan keyin: `.part` allaqachon toʻliq
        // (yuklash tugab, ochishdan oldin uzilgan). Ilgari bu holatda har urinish
        // «kod 416» bilan tugardi (F2). Endi ochishga oʻtamiz — arxiv buzuq
        // boʻlsa `tar` rad etadi, `.part` oʻchadi va keyingi urinish toza boshlanadi.
        if http.statusCode == 416 && olingan > 0 {
            completionHandler(.cancel)
            try? handle?.close()
            handle = nil
            RubaiLog.write("tarjima modeli: .part toʻliq (416) — ochishga oʻtiladi")
            DispatchQueue.main.async {
                self.holat.stringValue = "Ochilmoqda…"
                self.bar.isIndeterminate = true
                self.bar.startAnimation(nil)
            }
            DispatchQueue.global(qos: .userInitiated).async { self.ochish() }
            return
        }
        // 206 — davom ettirish qabul qilindi. 200 — server Range'ni qoʻllamadi,
        // demak boshidan keladi: yarim faylni tashlab, noldan yozamiz.
        if http.statusCode == 200 && olingan > 0 {
            RubaiLog.write("tarjima modeli: server Range'ni qoʻllamadi — noldan boshlanadi")
            try? handle?.truncate(atOffset: 0)
            olingan = 0
        } else if http.statusCode != 200 && http.statusCode != 206 {
            completionHandler(.cancel)
            DispatchQueue.main.async {
                self.xatoKorsat("Server javob bermadi (kod \(http.statusCode)).")
            }
            return
        }
        if http.expectedContentLength > 0 { jami = olingan + http.expectedContentLength }
        completionHandler(.allow)
    }

    func urlSession(_ s: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
        do {
            try handle?.write(contentsOf: data)
        } catch {
            task?.cancel()
            DispatchQueue.main.async { self.xatoKorsat("Faylga yozib boʻlmadi.") }
            return
        }
        olingan += Int64(data.count)
        let got = olingan, tot = jami
        DispatchQueue.main.async {
            self.bar.isIndeterminate = false
            self.bar.doubleValue = tot > 0 ? Double(got) / Double(tot) : 0
            self.holat.stringValue = "\(Self.fmt.string(fromByteCount: got)) / \(Self.fmt.string(fromByteCount: tot))"
        }
    }

    func urlSession(_ s: URLSession, task t: URLSessionTask, didCompleteWithError error: Error?) {
        try? handle?.close()
        handle = nil
        s.finishTasksAndInvalidate()  // delegat (biz) xotirada qolib ketmasin (F3)
        if let e = error {
            // Bekor qilish — xato emas, foydalanuvchi oʻzi toʻxtatgan.
            if (e as NSError).code == NSURLErrorCancelled { return }
            RubaiLog.write("tarjima modeli: tarmoq xatosi")
            DispatchQueue.main.async { self.xatoKorsat(e.localizedDescription) }
            return
        }
        DispatchQueue.main.async {
            self.holat.stringValue = "Ochilmoqda…"
            self.bar.isIndeterminate = true
            self.bar.startAnimation(nil)
        }
        DispatchQueue.global(qos: .userInitiated).async { self.ochish() }
    }

    // MARK: Ochish

    private func ochish() {
        let fm = FileManager.default
        do {
            try? fm.removeItem(at: arxiv)
            try fm.moveItem(at: qism, to: arxiv)

            try? fm.removeItem(at: vaqtinchalik)
            try fm.createDirectory(at: vaqtinchalik, withIntermediateDirectories: true)

            let p = Process()
            p.executableURL = URL(fileURLWithPath: "/usr/bin/tar")
            p.arguments = ["-xzf", arxiv.path, "-C", vaqtinchalik.path, "--strip-components", "1"]
            try p.run()
            p.waitUntilExit()

            guard p.terminationStatus == 0, TarjimaModel.tayyormi(vaqtinchalik) else {
                try? fm.removeItem(at: vaqtinchalik)
                try? fm.removeItem(at: arxiv)
                RubaiLog.write("tarjima modeli: arxiv buzilgan (tar kodi \(p.terminationStatus))")
                DispatchQueue.main.async { self.xatoKorsat("Arxiv buzilgan. Qayta yuklab koʻring.") }
                return
            }

            try? fm.removeItem(at: Yollar.tarjimaModeli)
            try fm.moveItem(at: vaqtinchalik, to: Yollar.tarjimaModeli)
            try? fm.removeItem(at: arxiv)

            // Eski 1.3B papkasi FAQAT shu yerda oʻchiriladi — yangi model
            // joyiga oʻtgandan keyin. Oldinroq oʻchirilsa, yuklash yarmida
            // uzilgan foydalanuvchi ishlaydigan modelsiz qolardi.
            if Yollar.eskiTarjimaModeliniOchir(baza: Yollar.qollab) {
                RubaiLog.write("tarjima modeli: eski 1.3B papkasi oʻchirildi")
            }
            RubaiLog.write("tarjima modeli: tayyor")
            DispatchQueue.main.async { self.yakunla(true) }
        } catch {
            RubaiLog.write("tarjima modeli: ochib boʻlmadi")
            DispatchQueue.main.async { self.xatoKorsat("Arxivni ochib boʻlmadi.") }
        }
    }

    // MARK: Oyna

    private func oynaniQur() {
        let W: CGFloat = 460, H: CGFloat = 210
        let w = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: W, height: H),
            styleMask: [.titled], backing: .buffered, defer: false)
        w.title = "Tarjima modeli"
        w.isReleasedWhenClosed = false
        w.appearance = NSAppearance(named: .aqua)
        let v = NSView(frame: NSRect(x: 0, y: 0, width: W, height: H))

        let sarlavha = NSTextField(labelWithString: "Tarjima modeli yuklab olinmoqda")
        sarlavha.font = .systemFont(ofSize: 16, weight: .semibold)
        sarlavha.alignment = .center
        sarlavha.frame = NSRect(x: 20, y: H - 60, width: W - 40, height: 22)
        v.addSubview(sarlavha)

        let izoh = NSTextField(
            wrappingLabelWithString:
                "Bu faqat bir marta bajariladi. Model ~3,4 GB. Ulanish uzilsa, oʻsha joydan davom etadi.")
        izoh.font = .systemFont(ofSize: 11)
        izoh.textColor = .secondaryLabelColor
        izoh.alignment = .center
        izoh.frame = NSRect(x: 30, y: H - 104, width: W - 60, height: 34)
        v.addSubview(izoh)

        bar = NSProgressIndicator(frame: NSRect(x: 30, y: 80, width: W - 60, height: 12))
        bar.style = .bar
        bar.minValue = 0
        bar.maxValue = 1
        bar.isIndeterminate = true
        v.addSubview(bar)

        holat = NSTextField(labelWithString: "Ulanmoqda…")
        holat.font = .systemFont(ofSize: 11)
        holat.textColor = .secondaryLabelColor
        holat.frame = NSRect(x: 30, y: 58, width: W - 60, height: 16)
        v.addSubview(holat)

        tugma = NSButton(title: "Bekor qilish", target: self, action: #selector(tugmaBosildi))
        tugma.bezelStyle = .rounded
        tugma.frame = NSRect(x: W - 30 - 130, y: 18, width: 130, height: 30)
        v.addSubview(tugma)

        // Xatodan keyin asosiy tugma "Qayta urinish" ga aylanadi — busiz sheet'ni
        // yopishning yoʻli qolmaydi va foydalanuvchi tiqilib qoladi.
        yopTugma = NSButton(title: "Yopish", target: self, action: #selector(yopBosildi))
        yopTugma.bezelStyle = .rounded
        yopTugma.frame = NSRect(x: W - 30 - 130 - 10 - 100, y: 18, width: 100, height: 30)
        yopTugma.isHidden = true
        v.addSubview(yopTugma)

        w.contentView = v
        oyna = w
    }

    private func xatoKorsat(_ matn: String) {
        xatoBerdi = true
        bar.stopAnimation(nil)
        bar.isIndeterminate = false
        holat.textColor = .systemRed
        holat.stringValue = matn
        tugma.title = "Qayta urinish"
        yopTugma.isHidden = false
    }

    /// Xatodan keyin sheet'ni yopadi. Yarim yuklangan `.part` fayl saqlanadi —
    /// keyingi urinishda oʻsha joydan davom etadi.
    @objc private func yopBosildi() {
        task?.cancel()
        session?.invalidateAndCancel()
        try? handle?.close()
        handle = nil
        yakunla(false)
    }

    @objc private func tugmaBosildi() {
        if xatoBerdi { yuklashniBoshla(); return }
        task?.cancel()
        session?.invalidateAndCancel()
        try? handle?.close()
        handle = nil
        RubaiLog.write("tarjima modeli: bekor qilindi (yarim fayl saqlanib qoldi)")
        yakunla(false)
    }

    private func yakunla(_ muvaffaqiyat: Bool) {
        if let w = oyna, let ota = w.sheetParent { ota.endSheet(w) }
        oyna = nil
        tugadi?(muvaffaqiyat)
        tugadi = nil
    }
}
