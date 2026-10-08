// Mikrofon yozuvi — 16 kHz mono float32. Mikrofon fon navbatida va vaqt
// chegarasi bilan ochiladi (CoreAudio qotib qolsa ham ilova tirik qoladi).

import AVFoundation
import AudioToolbox
import CoreAudio
import Foundation

// MARK: - Mikrofon (16kHz mono float32)

final class Recorder {
    /// `start()` natijasi. Bool yetarli emas: "javob yoʻq" holatini oddiy
    /// xatodan ajratish kerak — foydalanuvchiga koʻrsatiladigan xabar ham,
    /// logdagi izoh ham boshqacha boʻladi.
    enum Natija: CustomStringConvertible {
        case ok, ruxsatYoq, javobYoq
        case xato(OchishXatosi)
        var description: String {
            switch self {
            case .ok: return "ok"
            case .ruxsatYoq: return "ruxsatYoq"
            case .javobYoq: return "javobYoq"
            case .xato(let x): return "xato (\(x.log))"
            }
        }
    }

    /// Mikrofon nega ochilmadi. Ilgari logda faqat «record start: xato»
    /// qolardi va sababini (qurilma yoʻqmi, CoreAudio rad etdimi) bilib
    /// boʻlmasdi (D7). Endi sabab logga ham, overlay'ga ham chiqadi.
    enum OchishXatosi: Error {
        /// Kirish formati 0 Hz — mikrofon ulanmagan yoki boshqa dastur band qilgan.
        case formatYoq
        /// Qurilma formatidan 16 kHz ga oʻgiruvchi yaratilmadi.
        case konvertorYoq(String)
        /// `AVAudioEngine.start()` xatosi (NSError kodi).
        case ishgaTushmadi(Int)

        var log: String {
            switch self {
            case .formatYoq: return "kirish formati 0 Hz (mikrofon ulanmagan yoki band)"
            case .konvertorYoq(let f): return "AVAudioConverter yaratilmadi (\(f))"
            case .ishgaTushmadi(let k): return "engine.start() xatosi, kod \(k)"
            }
        }
        var xabar: String {
            switch self {
            case .formatYoq: return "Mikrofon topilmadi"
            case .konvertorYoq: return "Mikrofon formati mos emas"
            case .ishgaTushmadi(let k): return "Mikrofon ochilmadi (kod \(k))"
            }
        }
    }

    /// Ochilgan bitta engine va uning kirish tuguni. Yopish aynan SHU nusxaga
    /// tegadi — `self.engine` ga emas (D3).
    private struct Ochilgan {
        let engine: AVAudioEngine
        let input: AVAudioInputNode
        func yop() { input.removeTap(onBus: 0); engine.stop() }
    }

    /// Mikrofon ochilishi shuncha kutiladi. CoreAudio odatda millisekundlarda
    /// javob beradi — bundan uzoq kutish qotib qolgan degani.
    private static let ochishTimeout: TimeInterval = 3.0

    /// Qabul qilingan (yozayotgan) engine. FAQAT asosiy oqimda oʻqiladi va
    /// yoziladi: `boshla` uni javob asosiy oqimga yetgandagina oʻrnatadi.
    private var engine: AVAudioEngine?
    private var input: AVAudioInputNode?
    private let target = AVAudioFormat(
        commonFormat: .pcmFormatFloat32,
        sampleRate: 16000, channels: 1, interleaved: false)!
    private var samples: [Float] = []
    private let lock = NSLock()
    /// Bufer egasi — qaysi ochish urinishining tap'i `samples` ga yoza oladi
    /// (`lock` ostida). Har `och()` uni oʻziga oladi: kech ochilgan eski engine
    /// hali ishlayotgan boʻlsa ham, uning namunalari yangi yozuvga aralashmaydi.
    private var buferEgasi = 0
    /// Ochish urinishlari sanogʻi. Faqat `ochishQ` da (serial) oʻzgaradi.
    private var urinish = 0

    // Yozish holati ikki oqimdan koʻriladi, shuning uchun alohida qulf ostida.
    private let holatLock = NSLock()
    private var _isRecording = false
    var isRecording: Bool {
        holatLock.lock(); defer { holatLock.unlock() }; return _isRecording
    }

    /// Mikrofon FAQAT shu navbatda ochiladi — asosiy oqim hech qachon
    /// CoreAudio'ni kutib turmasligi kerak (pastdagi izohga qarang).
    private let ochishQ = DispatchQueue(label: "com.rubaistt.dictation.recorder")

    func start(_ cb: @escaping (Natija) -> Void) {
        switch AVCaptureDevice.authorizationStatus(for: .audio) {
        case .authorized: boshla(cb)
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .audio) { ok in
                DispatchQueue.main.async {
                    if ok { self.boshla(cb) } else { cb(.ruxsatYoq) }
                }
            }
        default: cb(.ruxsatYoq)
        }
    }

    /// Mikrofonni fon oqimida ochadi va `ochishTimeout` bilan chegaralaydi.
    ///
    /// Nega shunday: `AVAudioEngine.inputNode` CoreAudio HAL'ga SINXRON soʻrov
    /// yuboradi (qurilmalarni sanash — `GetHWFormat` → `GetSubDevices`).
    /// 2026-09-04 da shu soʻrov coreaudiod ichida qotib qolgan va asosiy oqim
    /// bir soatga bloklangan: menyu ham, hotkey ham javob bermay, butun ilova
    /// muzlagan (coreaudiod 66%, ilova 36% CPU). Endi eng yomon holatda ham
    /// foydalanuvchi 3 soniyadan keyin xabar oladi va ilova tirik qoladi.
    private func boshla(_ cb: @escaping (Natija) -> Void) {
        var javobBerildi = false  // faqat asosiy oqimda oʻqiladi/yoziladi

        DispatchQueue.main.asyncAfter(deadline: .now() + Recorder.ochishTimeout) {
            guard !javobBerildi else { return }
            javobBerildi = true
            RubaiLog.write("XATO: mikrofon \(Recorder.ochishTimeout)s ichida ochilmadi — CoreAudio javob bermadi")
            cb(.javobYoq)
        }

        ochishQ.async {
            let natija = self.och()
            DispatchQueue.main.async {
                guard !javobBerildi else {
                    // Timeout eʼlon qilingan, lekin CoreAudio keyinroq uygʻondi.
                    // Aynan SHU urinishning engine'i yopiladi. Ilgari bu yerda
                    // `stop()` chaqirilardi va u oraliqda qayta urinishda ochilgan
                    // YANGI engine'ni toʻxtatardi: UI «yozilmoqda», mikrofon
                    // yopiq (D3).
                    if case .success(let o) = natija {
                        RubaiLog.write("mikrofon kech ochildi — yopildi")
                        self.ochishQ.async { o.yop() }
                    }
                    return
                }
                javobBerildi = true
                switch natija {
                case .success(let o):
                    self.engine = o.engine
                    self.input = o.input
                    self.holatLock.lock(); self._isRecording = true; self.holatLock.unlock()
                    cb(.ok)
                case .failure(let x):
                    RubaiLog.write("XATO: mikrofon ochilmadi — \(x.log)")
                    cb(.xato(x))
                }
            }
        }
    }

    /// Mikrofonni haqiqiy ochish. FAQAT `ochishQ`dan chaqiriladi — bu yerdagi
    /// chaqiruvlar bloklanishi mumkin. `self.engine` ga TEGMAYDI: natijani
    /// qabul qilish-qilmaslikni `boshla` asosiy oqimda hal qiladi.
    private func och() -> Result<Ochilgan, OchishXatosi> {
        urinish += 1
        let mening = urinish
        lock.lock(); samples.removeAll(keepingCapacity: true); buferEgasi = mening; lock.unlock()
        // Har yozishda YANGI engine — login'da/qurilma oʻzgarganda qotib qolgan
        // (jim, "musiqa") holatdan saqlaydi.
        let engine = AVAudioEngine()
        // Havola saqlanadi: `engine.inputNode` getter'i har chaqirilganda
        // HAL soʻrovi yuborishi mumkin, `stop()` esa asosiy oqimda ishlaydi.
        let input = engine.inputNode
        // Sozlamalarda tanlangan mikrofon (boʻlmasa — tizim standarti).
        // Formatni OʻQISHDAN OLDIN qoʻllash shart, aks holda eski qurilma formati qoladi.
        applySelectedInputDevice(to: input)
        let hw = input.outputFormat(forBus: 0)
        guard hw.sampleRate > 0 else { return .failure(.formatYoq) }
        let target = self.target
        guard let conv = AVAudioConverter(from: hw, to: target) else {
            return .failure(.konvertorYoq("\(hw)"))
        }
        input.installTap(onBus: 0, bufferSize: 4096, format: hw) { [weak self] buf, _ in
            guard let self = self else { return }
            let ratio = target.sampleRate / hw.sampleRate
            let cap = AVAudioFrameCount(Double(buf.frameLength) * ratio) + 1024
            guard let out = AVAudioPCMBuffer(pcmFormat: target, frameCapacity: cap) else { return }
            var fed = false; var err: NSError?
            conv.convert(to: out, error: &err) { _, s in
                if fed { s.pointee = .noDataNow; return nil }
                fed = true; s.pointee = .haveData; return buf
            }
            if let ch = out.floatChannelData {
                let n = Int(out.frameLength)
                self.lock.lock()
                if self.buferEgasi == mening {
                    self.samples.append(contentsOf: UnsafeBufferPointer(start: ch[0], count: n))
                }
                self.lock.unlock()
            }
        }
        engine.prepare()
        do { try engine.start() } catch {
            // Tap olib tashlanadi — ilgari xatoda u engine bilan qolib ketardi (D7).
            input.removeTap(onBus: 0)
            return .failure(.ishgaTushmadi((error as NSError).code))
        }
        return .success(Ochilgan(engine: engine, input: input))
    }

    /// Sozlamalardagi mikrofonni AVAudioEngine'ga bogʻlaydi.
    /// Qurilma uzilgan boʻlsa jim tizim standartiga qaytadi — yozish toʻxtamasligi kerak.
    private func applySelectedInputDevice(to input: AVAudioInputNode) {
        guard let uid = Prefs.micUID else { return }
        guard let devID = AudioDevices.deviceID(forUID: uid) else {
            RubaiLog.write("tanlangan mikrofon ulanmagan (uid=\(uid)) — tizim standarti")
            return
        }
        guard let au = input.audioUnit else { return }
        var id = devID
        let st = AudioUnitSetProperty(
            au, kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global, 0,
            &id, UInt32(MemoryLayout<AudioDeviceID>.size))
        if st != noErr {
            RubaiLog.write("mikrofon oʻrnatilmadi (kod \(st)) — tizim standarti")
        }
    }

    /// Yozishni toʻxtatib, toʻplangan namunalarni qaytaradi. Asosiy oqimda
    /// (hotkey yoki `maxYozish` taymeri). Bayroq qulf ostida almashtiriladi —
    /// ikkinchi chaqiruv boʻsh qaytadi va engine ikki marta toʻxtatilmaydi.
    func stop() -> [Float] {
        holatLock.lock()
        guard _isRecording else { holatLock.unlock(); return [] }
        _isRecording = false
        holatLock.unlock()
        // `engine.inputNode` emas, saqlangan havola — getter yana HAL soʻrovi
        // yuborishi mumkin, bu esa yuqoridagi muzlashning aynan sababi edi.
        input?.removeTap(onBus: 0)
        engine?.stop()
        input = nil
        engine = nil
        lock.lock(); let s = samples; lock.unlock()
        return s
    }
}
