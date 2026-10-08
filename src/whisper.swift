// Whisper yadrosi — model yuklash (nomzodlar boʻyicha), diktovka va Studiya
// transkripsiyasi, RAM'dan boʻshatish. whisper.cpp ustidagi C shim:
// `whisper_bridge.c`. Hamma chaqiruv bitta serial navbatda (`q`).

import Foundation

final class Whisper {
    static let shared = Whisper()
    private var loaded = false
    private let q = DispatchQueue(label: "rubai.whisper")

    /// Modelni yuklaydi (`q` navbatida). Muvaffaqiyatda nil, aks holda xato matni.
    ///
    /// Nomzodlar hajmi bilan tekshirilgan (ModelStore); bittasi yuklanmasa —
    /// masalan fayl hajmi toʻgʻri-yu, ichi buzilgan — keyingisi sinaladi.
    /// Ilgari birinchi (va tekshirilmagan) yoʻl yiqilsa diktovka shu bilan
    /// tugardi (barqarorlik spec'i, A1).
    private func modelniYukla() -> String? {
        if loaded { return nil }
        let nomzodlar = ModelStore.nomzodlar()
        guard !nomzodlar.isEmpty else {
            RubaiLog.write("XATO: model topilmadi (foydalanuvchi, tizim, bundle, eski yoʻl)")
            return "Model fayli topilmadi"
        }
        for yol in nomzodlar {
            let rc = rubai_load(yol)
            if rc == 0 {
                RubaiLog.write("model yuklandi: \(yol), backend=\(String(cString: rubai_backend_name()))")
                loaded = true
                return nil
            }
            RubaiLog.write("XATO: rubai_load(\(yol)) = \(rc): \(String(cString: rubai_last_error()))")
        }
        return "Model yuklanmadi (\(nomzodlar.count) ta nomzod sinaldi)"
    }

    func transcribe(
        _ samples: [Float], done: @escaping (String) -> Void,
        fail: @escaping (String) -> Void = { _ in }
    ) {
        q.async {
            if let xato = self.modelniYukla() {
                DispatchQueue.main.async { fail(xato) }; return
            }
            let threads = Int32(max(4, ProcessInfo.processInfo.activeProcessorCount - 2))
            var text: String?
            samples.withUnsafeBufferPointer { buf in
                if let c = rubai_transcribe(buf.baseAddress, Int32(buf.count), threads) {
                    text = String(cString: c).trimmingCharacters(in: .whitespacesAndNewlines)
                    rubai_free_str(c)
                }
            }
            // NULL — xato, boʻsh satr — «ovoz yoʻq». Ilgari ikkalasi ham ""
            // boʻlib «Ovoz aniqlanmadi» deb koʻrsatilar va ovoz tashlanardi (A3).
            guard let text else {
                RubaiLog.write("XATO: rubai_transcribe: \(String(cString: rubai_last_error()))")
                DispatchQueue.main.async { fail("Matnga oʻgirib boʻlmadi") }
                return
            }
            DispatchQueue.main.async { done(text) }
        }
    }

    /// Segmentlar bilan transkripsiya. `progress` 0…100, asosiy oqimda chaqiriladi.
    /// `bekor` esa fon (ggml hisoblash) oqimlaridan va `q` navbati oqimidan chaqiriladi —
    /// asosiy oqimda EMAS. Shuning uchun u thread-safe, side-effect'siz, idempotent va
    /// TEZ qaytadigan boʻlishi SHART (masalan, faqat bitta atomik bayroq/holatni oʻqisin).
    /// `bekor` true qaytarsa ish toʻxtaydi va `fail("bekor")` chaqiriladi.
    /// DIQQAT: `q` — serial navbat, shuning uchun diktovka bilan avtomatik qulflangan.
    func transcribeSegments(
        _ samples: [Float],
        progress: @escaping (Int) -> Void,
        bekor: @escaping () -> Bool,
        done: @escaping ([Segment]) -> Void,
        fail: @escaping (String) -> Void
    ) {
        q.async {
            if let xato = self.modelniYukla() {
                DispatchQueue.main.async { fail(xato) }; return
            }
            let threads = Int32(max(4, ProcessInfo.processInfo.activeProcessorCount - 2))

            // C callback'lar Swift closure'ni ushlay olmaydi — ularni box orqali
            // uzatamiz. Box `withExtendedLifetime` davomida tirik turadi.
            final class Box {
                let progress: (Int) -> Void
                let bekor: () -> Bool
                init(_ p: @escaping (Int) -> Void, _ b: @escaping () -> Bool) {
                    progress = p; bekor = b
                }
            }
            let box = Box({ p in DispatchQueue.main.async { progress(p) } }, bekor)

            var rc: Int32 = 1
            withExtendedLifetime(box) {
                let ud = Unmanaged.passUnretained(box).toOpaque()
                samples.withUnsafeBufferPointer { buf in
                    rc = rubai_transcribe_segments(
                        buf.baseAddress, Int32(buf.count), threads,
                        { pct, ud in
                            guard let ud = ud else { return }
                            Unmanaged<Box>.fromOpaque(ud).takeUnretainedValue().progress(Int(pct))
                        }, ud,
                        { ud in
                            guard let ud = ud else { return false }
                            return Unmanaged<Box>.fromOpaque(ud).takeUnretainedValue().bekor()
                        }, ud)
                }
            }

            if rc == 2 { DispatchQueue.main.async { fail("bekor") }; return }
            guard rc == 0 else { DispatchQueue.main.async { fail("Transkripsiya xatosi") }; return }

            var segs: [Segment] = []
            let n = Int(rubai_n_segments())
            segs.reserveCapacity(n)
            for i in 0..<n {
                guard let c = rubai_segment_text(Int32(i)) else { continue }
                let t = String(cString: c).trimmingCharacters(in: .whitespaces)
                if t.isEmpty { continue }
                segs.append(
                    Segment(
                        t0: Double(rubai_segment_t0(Int32(i))) / 100.0,
                        t1: Double(rubai_segment_t1(Int32(i))) / 100.0,
                        matn: t))
            }
            DispatchQueue.main.async { done(segs) }
        }
    }

    func unload() { q.async { if self.loaded { rubai_unload(); self.loaded = false } } }

    /// Studiya ishi (`TranskripsiyaIshi`) ketayotganini bildiradi — idle-unload
    /// taymeri modelni RAM'dan boʻshatib yubormasligi uchun.
    ///
    /// EGALIK SHARTNOMASI: `transcribeSegments` bu bayroqni OʻZI oʻrnatmaydi/tozalamaydi
    /// (chunki `q` navbati whisper chaqiruvlari orasida — masalan, koʻp soatlik faylni
    /// boʻlaklarga boʻlib audio dekodlash bosqichida — boʻsh turishi mumkin, va aynan
    /// oʻsha oraliqda idle-taymer oʻtib ketishi mumkin). Bayroqni ish SOHIBI —
    /// `TranskripsiyaIshi` — boshqaradi:
    /// ish boshlanishidan oldin `true`, ish tugagach — muvaffaqiyat, xato yoki bekor
    /// qilishdan qatʼiy nazar, HAR BIR chiqish yoʻlida — `false` qilib qoʻyishi shart.
    /// Bu butun job davomini (whisper chaqiruvlari orasidagi boʻsh oraliqlarni ham)
    /// qamrab olishi kerak, aks holda model job oʻrtasida RAM'dan boʻshatilib ketishi
    /// mumkin.
    private var bandLock = NSLock()
    private var _band = false
    var band: Bool {
        get { bandLock.lock(); defer { bandLock.unlock() }; return _band }
        set { bandLock.lock(); _band = newValue; bandLock.unlock() }
    }
}
