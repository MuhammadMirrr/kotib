// Kotib — til modeli qayerda va (topilmasa) uni yuklab olish.
//
// 1.2 dan boshlab `.pkg` modelni bundle'dan TASHQARIGA qoʻyadi
// (`Yollar.tizimModellari`) — avto-yangilanish bundle'ni almashtiradi va model
// har safar qayta yuklanmasligi kerak. Qidiruv tartibi va tekshiruv —
// `model_tanlov.swift`. Bu yuklovchi faqat hech qayerda model topilmasa
// ishlaydi va uni foydalanuvchi papkasiga (`Yollar.model`) yozadi.
//
// Uzilgan ulanish uchun HTTP Range bilan davom ettirish bor: yarim yuklangan fayl
// ".part" nomi bilan turadi va keyingi urinishda oʻsha joydan davom etadi.

import AppKit

// MARK: - Model qayerda

enum ModelStore {
    /// Yuklanadigan model — `ModelTanlov.joriy` (hajm/sha256 bitta joyda).
    static var expectedBytes: Int64 { ModelTanlov.joriy.hajm }
    static let url = URL(string: "https://cdn.mirqobilov.com/v1.0/ggml-rubaistt.bin")!

    /// Yuklovchi modelni shu yerga yozadi (foydalanuvchi papkasi — admin paroli kerak emas).
    static var downloadedURL: URL { Yollar.model }

    static var partURL: URL { downloadedURL.appendingPathExtension("part") }

    /// Yuklasa boʻladigan modellar, ustuvorlik tartibida — har biri mavjudligi
    /// va HAJMI bilan tekshirilgan (A1). Bundle — CFBundle keshi orqali EMAS,
    /// papka sifatida: kesh oʻchirilgan faylni ham «bor» deb berardi.
    static func nomzodlar() -> [String] {
        var papkalar = [Yollar.foydalanuvchiModellari, Yollar.tizimModellari]
        // 1.1.0 dan oʻtish davri: eski .pkg modelni bundle ichiga qoʻygan.
        if let r = Bundle.main.resourceURL { papkalar.append(r) }
        papkalar.append(Yollar.eskiModellar)
        return ModelTanlov.nomzodlar(papkalar: papkalar, hajmi: ModelTanlov.faylHajmi).map(\.path)
    }

    static func existingPath() -> String? { nomzodlar().first }

    static var isReady: Bool { existingPath() != nil }
}

// MARK: - Yuklovchi

final class ModelDownloader: NSObject, URLSessionDataDelegate {

    enum Failure: LocalizedError {
        case http(Int)
        case shortFile(Int64, Int64)
        case write(String)
        case buzilgan

        var errorDescription: String? {
            switch self {
            case .http(let code):
                return "Server javob bermadi (kod \(code)). Internetni tekshirib, qayta urinib koʻring."
            case .shortFile(let got, let want):
                return "Fayl toʻliq yuklanmadi (\(got) / \(want) bayt). Qayta urinib koʻring."
            case .write(let msg):
                return "Faylga yozib boʻlmadi: \(msg). Diskda joy borligini tekshiring."
            case .buzilgan:
                return "Yuklangan fayl buzilgan — qayta urinib koʻring (boshidan yuklanadi)."
            }
        }
    }

    private var session: URLSession!
    private var task: URLSessionDataTask?
    private var handle: FileHandle?
    private var received: Int64 = 0
    private var total: Int64 = ModelStore.expectedBytes

    // Tezlikni oʻlchash uchun
    private var lastTick = Date()
    private var lastBytes: Int64 = 0
    private var speed: Double = 0

    /// (yuklangan, jami, bayt/sek) — asosiy oqimda chaqiriladi.
    var onProgress: ((Int64, Int64, Double) -> Void)?
    /// Yakun — asosiy oqimda chaqiriladi.
    var onFinish: ((Result<Void, Error>) -> Void)?

    func start() {
        let fm = FileManager.default
        let dir = ModelStore.downloadedURL.deletingLastPathComponent()
        do {
            try fm.createDirectory(at: dir, withIntermediateDirectories: true)
        } catch {
            return finish(.failure(Failure.write(error.localizedDescription)))
        }

        // Yarim yuklangan fayl bormi — oʻsha joydan davom etamiz
        let part = ModelStore.partURL
        var offset: Int64 = 0
        let partSize = (try? fm.attributesOfItem(atPath: part.path)[.size] as? NSNumber)??.int64Value ?? 0
        if partSize == ModelStore.expectedBytes {
            // Oldingi urinishda toʻliq yuklangan, lekin yakunlanmagan (masalan
            // ilova shu paytda yopilgan). Ilgari bunday fayl oʻchirilib, 823 MB
            // qaytadan yuklanardi (F2) — endi tarmoqsiz tekshirib yakunlanadi.
            RubaiLog.write("model: .part toʻliq — tarmoqsiz yakunlanmoqda")
            DispatchQueue.global(qos: .userInitiated).async { self.yakunla(part: part, got: partSize) }
            return
        }
        if partSize > 0 && partSize < ModelStore.expectedBytes {
            offset = partSize
        } else {
            try? fm.removeItem(at: part)
        }
        if !fm.fileExists(atPath: part.path) {
            fm.createFile(atPath: part.path, contents: nil)
        }
        guard let h = try? FileHandle(forWritingTo: part) else {
            return finish(.failure(Failure.write("'\(part.lastPathComponent)' ochilmadi")))
        }
        handle = h
        _ = try? h.seekToEnd()
        received = offset
        lastBytes = offset
        lastTick = Date()

        var req = URLRequest(url: ModelStore.url)
        req.timeoutInterval = 60
        if offset > 0 {
            req.setValue("bytes=\(offset)-", forHTTPHeaderField: "Range")
            RubaiLog.write("model: \(offset) baytdan davom ettirilmoqda")
        } else {
            RubaiLog.write("model: yuklab olish boshlandi")
        }

        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForResource = 3600 * 4  // sekin internet uchun keng
        cfg.waitsForConnectivity = true
        session = URLSession(configuration: cfg, delegate: self, delegateQueue: nil)
        task = session.dataTask(with: req)
        task?.resume()
    }

    func cancel() {
        onFinish = nil  // kech keladigan javob boshqaruvchi holatini buzmasin
        task?.cancel()
        session?.invalidateAndCancel()
        try? handle?.close()
        handle = nil
        RubaiLog.write("model: yuklab olish bekor qilindi (yarim fayl saqlanib qoldi)")
    }

    // MARK: URLSession delegati

    func urlSession(
        _ s: URLSession, dataTask: URLSessionDataTask,
        didReceive response: URLResponse,
        completionHandler: @escaping (URLSession.ResponseDisposition) -> Void
    ) {
        guard let http = response as? HTTPURLResponse else {
            completionHandler(.allow); return
        }
        // 206 — davom ettirish qabul qilindi. 200 — server Range'ni qoʻllamadi,
        // demak boshidan keladi: yarim faylni tashlab, noldan yozamiz.
        if http.statusCode == 200 && received > 0 {
            RubaiLog.write("model: server Range'ni qoʻllamadi — noldan boshlanadi")
            try? handle?.truncate(atOffset: 0)
            received = 0
            lastBytes = 0
        } else if http.statusCode != 200 && http.statusCode != 206 {
            completionHandler(.cancel)
            finish(.failure(Failure.http(http.statusCode)))
            return
        }

        if http.expectedContentLength > 0 {
            total = received + http.expectedContentLength
        }
        completionHandler(.allow)
    }

    func urlSession(_ s: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
        do {
            try handle?.write(contentsOf: data)
        } catch {
            task?.cancel()
            finish(.failure(Failure.write(error.localizedDescription)))
            return
        }
        received += Int64(data.count)

        // Tezlikni ~0.4s da bir yangilaymiz — UI titramasin
        let now = Date()
        let dt = now.timeIntervalSince(lastTick)
        if dt >= 0.4 {
            speed = Double(received - lastBytes) / dt
            lastTick = now
            lastBytes = received
            let r = received, t = total, sp = speed
            DispatchQueue.main.async { self.onProgress?(r, t, sp) }
        }
    }

    func urlSession(_ s: URLSession, task t: URLSessionTask, didCompleteWithError error: Error?) {
        try? handle?.close()
        handle = nil
        // Sessiya delegatini (bizni) kuchli ushlaydi — boʻshatilmasa yuklovchi
        // abadiy xotirada qolardi (F3).
        s.finishTasksAndInvalidate()

        if let e = error {
            // Bekor qilish — xato emas, foydalanuvchi oʻzi toʻxtatgan
            if (e as NSError).code == NSURLErrorCancelled { return }
            RubaiLog.write("model: tarmoq xatosi — \(e.localizedDescription)")
            finish(.failure(e))
            return
        }

        // Hajm tekshiruvi — yarim yoki buzilgan fayl bilan ishga tushmaymiz
        let part = ModelStore.partURL
        let got = (try? FileManager.default.attributesOfItem(atPath: part.path)[.size] as? NSNumber)??.int64Value ?? 0
        guard got >= ModelStore.expectedBytes else {
            finish(.failure(Failure.shortFile(got, ModelStore.expectedBytes)))
            return
        }
        yakunla(part: part, got: got)
    }

    /// sha256 tekshiruvi va atomik yakun. Fon oqimida chaqiriladi (~2 s).
    private func yakunla(part: URL, got: Int64) {
        // Hajm toʻgʻri boʻlsa ham ichi buzilgan boʻlishi mumkin (disk, tarmoq
        // proksisi). Buzilgan model whisper'da «Model yuklanmadi» boʻlib
        // chiqardi — endi bu yerda tutiladi va .part oʻchiriladi.
        guard ModelTanlov.faylSha256(part) == ModelTanlov.joriy.sha256 else {
            try? FileManager.default.removeItem(at: part)
            RubaiLog.write("model: sha256 mos emas — .part oʻchirildi")
            finish(.failure(Failure.buzilgan))
            return
        }

        // Atomik yakun: toʻliq fayl boʻlgandagina asl nomga oʻtadi
        do {
            let dest = ModelStore.downloadedURL
            try? FileManager.default.removeItem(at: dest)
            try FileManager.default.moveItem(at: part, to: dest)
            RubaiLog.write("model: yuklab olindi (\(got) bayt) → \(dest.path)")
            finish(.success(()))
        } catch {
            finish(.failure(Failure.write(error.localizedDescription)))
        }
    }

    private func finish(_ r: Result<Void, Error>) {
        DispatchQueue.main.async {
            self.onFinish?(r); self.onFinish = nil
        }
    }
}

// MARK: - Yuklab olish oynasi

final class ModelDownloadWindow: NSObject {
    private var window: NSWindow?
    private var bar: NSProgressIndicator!
    private var statusLabel: NSTextField!
    private var noteLabel: NSTextField!
    private var actionButton: NSButton!  // "Bekor qilish" yoki "Qayta urinish"
    private var downloader: ModelDownloader?
    private var failed = false
    /// Yuklash ketayotganini belgilaydi. Busiz show() har chaqirilganda ikkinchi
    /// yuklovchi ishga tushib, ikkalasi bitta .part fayliga yozadi va model buziladi.
    private var isDownloading = false

    /// Model tayyor boʻlganda chaqiriladi (onboarding shundan keyin boshlanadi).
    var onReady: (() -> Void)?

    private static let fmt: ByteCountFormatter = {
        let f = ByteCountFormatter()
        f.allowedUnits = [.useMB, .useGB]
        f.countStyle = .file
        return f
    }()

    func show() {
        if window == nil { build() }
        presentWindow()
        // Allaqachon yuklanayotgan boʻlsa — faqat oynani koʻrsatamiz, qayta boshlamaymiz
        if !isDownloading && !failed { beginDownload() }
    }

    /// Oynani ekranga chiqaradi.
    ///
    /// Ilova LSUIElement (menyu satri ilovasi) boʻlgani uchun makeKeyAndOrderFront
    /// yolgʻiz oʻzi yetarli emas — ishga tushish paytida oyna ekranga umuman
    /// chiqmaydi. orderFrontRegardless() ilova faol boʻlmasa ham majburan koʻrsatadi;
    /// loyihadagi Overlay ham xuddi shu sababdan shuni ishlatadi.
    private func presentWindow() {
        guard let w = window else { return }
        NSApp.activate(ignoringOtherApps: true)
        w.center()
        w.makeKeyAndOrderFront(nil)
        w.orderFrontRegardless()
    }

    private let W: CGFloat = 460
    private let H: CGFloat = 250

    private func build() {
        let w = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: W, height: H),
            styleMask: [.titled], backing: .buffered, defer: false)
        w.title = "Kotib"
        w.isReleasedWhenClosed = false
        let v = NSView(frame: NSRect(x: 0, y: 0, width: W, height: H))

        let icon = NSImageView(frame: NSRect(x: (W - 56) / 2, y: H - 78, width: 56, height: 56))
        icon.image = NSApp.applicationIconImage
        icon.imageScaling = .scaleProportionallyUpOrDown
        v.addSubview(icon)

        let title = NSTextField(labelWithString: "Til modeli yuklab olinmoqda")
        title.font = .systemFont(ofSize: 16, weight: .semibold)
        title.alignment = .center
        title.frame = NSRect(x: 20, y: H - 108, width: W - 40, height: 22)
        v.addSubview(title)

        noteLabel = NSTextField(
            wrappingLabelWithString:
                "Bu faqat bir marta bajariladi. Model ~785 MB — internet tezligiga qarab "
                + "5–20 daqiqa olishi mumkin. Ulanish uzilsa, oʻsha joydan davom etadi.")
        noteLabel.font = .systemFont(ofSize: 11)
        noteLabel.textColor = .secondaryLabelColor
        noteLabel.alignment = .center
        noteLabel.frame = NSRect(x: 30, y: H - 150, width: W - 60, height: 34)
        v.addSubview(noteLabel)

        bar = NSProgressIndicator(frame: NSRect(x: 30, y: 84, width: W - 60, height: 12))
        bar.isIndeterminate = true
        bar.minValue = 0
        bar.maxValue = 1
        bar.style = .bar
        v.addSubview(bar)

        statusLabel = NSTextField(labelWithString: "Ulanmoqda…")
        statusLabel.font = .systemFont(ofSize: 11)
        statusLabel.textColor = .secondaryLabelColor
        statusLabel.frame = NSRect(x: 30, y: 62, width: W - 60, height: 16)
        v.addSubview(statusLabel)

        actionButton = NSButton(title: "Bekor qilish", target: self, action: #selector(actionPressed))
        actionButton.bezelStyle = .rounded
        actionButton.frame = NSRect(x: W - 30 - 130, y: 20, width: 130, height: 30)
        v.addSubview(actionButton)

        w.contentView = v
        window = w
    }

    private func beginDownload() {
        guard !isDownloading else { return }
        isDownloading = true
        failed = false
        actionButton.title = "Bekor qilish"
        statusLabel.textColor = .secondaryLabelColor
        statusLabel.stringValue = "Ulanmoqda…"
        bar.isIndeterminate = true
        bar.startAnimation(nil)

        let d = ModelDownloader()
        d.onProgress = { [weak self] got, total, speed in
            guard let self = self else { return }
            self.bar.isIndeterminate = false
            self.bar.doubleValue = total > 0 ? Double(got) / Double(total) : 0
            let g = Self.fmt.string(fromByteCount: got)
            let t = Self.fmt.string(fromByteCount: total)
            let s = Self.fmt.string(fromByteCount: Int64(speed))
            self.statusLabel.stringValue = "\(g) / \(t)  ·  \(s)/s"
        }
        d.onFinish = { [weak self] result in
            guard let self = self else { return }
            self.isDownloading = false
            switch result {
            case .success:
                self.bar.doubleValue = 1
                self.statusLabel.stringValue = "Tayyor!"
                self.window?.close()
                self.onReady?()
            case .failure(let e):
                self.showFailure(e)
            }
        }
        downloader = d
        d.start()
    }

    private func showFailure(_ e: Error) {
        failed = true
        bar.stopAnimation(nil)
        bar.isIndeterminate = false
        statusLabel.textColor = .systemRed
        statusLabel.stringValue = e.localizedDescription
        noteLabel.stringValue =
            "Yuklangan qism saqlanib qoldi — qayta urinilganda "
            + "oʻsha joydan davom etadi."
        actionButton.title = "Qayta urinish"
    }

    @objc private func actionPressed() {
        if failed {
            beginDownload()
            return
        }
        // Bekor qilish — ilova modelsiz ishlay olmaydi, shuning uchun ogohlantiramiz
        let a = NSAlert()
        a.messageText = "Yuklab olish toʻxtatilsinmi?"
        a.informativeText =
            "Modelsiz diktovka ishlamaydi. Yuklangan qism saqlanadi — "
            + "keyinroq menyu satridagi 🎙 ikonka orqali davom ettirishingiz mumkin."
        a.alertStyle = .warning
        a.addButton(withTitle: "Davom ettirish")
        a.addButton(withTitle: "Toʻxtatish")
        if a.runModal() == .alertSecondButtonReturn {
            downloader?.cancel()
            // Holat toʻliq tiklanadi: ilgari `isDownloading` true qolib, keyingi
            // `show()` yuklashni davom ettirmasdi (F1). `.part` saqlanadi.
            downloader = nil
            isDownloading = false
            window?.close()
        }
    }
}
