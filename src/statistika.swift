// Anonim foydalanish statistikasi: kunlik «ping» va har transkripsiya/tarjima
// oʻlchovi.
//
// 1.2.0 gacha shu fayl yangilanishni ham tekshirardi (ping javobidagi versiya →
// banner). Endi yangilanish — Sparkle (`yangilovchi.swift`), va ikkalasi kodda
// ATAYLAB ajratilgan (barqarorlik G2): biri yiqilsa ikkinchisi ishlayveradi.
// Ping javobidagi versiya maydonlari faqat 1.1.0 lar uchun (koʻprik) qoldi.
//
// ---------------------------------------------------------------------------
// MAXFIYLIK. Bu faylni oʻzgartirishdan oldin oʻqing. Bu chegara maxfiylik
// siyosati (PRIVACY.md) va serverdagi izoh (statistika/src/index.js) bilan
// MOS boʻlishi SHART.
//
// Yuboriladi:  tasodifiy oʻrnatma ID, platforma ("mac"), ilova va macOS
//              versiyasi, qurilma muhiti (arxitektura, CPU/GPU modeli, RAM,
//              yadro soni), va har transkripsiya/tarjima uchun OʻLCHOVLAR:
//              ovoz uzunligi, ishlov vaqti, backend (metal/cpu), natija.
//              Taxminiy joylashuv (mamlakat/shahar) serverda IP'dan aniqlanadi.
// YUBORILMAYDI — hech qachon: ovoz, transkripsiya/tarjima MATNI, diktovka
//              tarixi, fayl nomlari, mikrofon nomi, email, hisob maʼlumotlari.
//              Foydalanuvchi ID'si YOʻQ — faqat oʻrnatma ID'si, u ilova
//              birinchi ochilganda tasodifiy yasaladi va shaxs bilan
//              bogʻlanmaydi; qayta oʻrnatishda yangisi paydo boʻladi.
//
// Statistika DOIM yoqiq — oʻchirish tugmasi yoʻq. Uning yagona huquqiy asosi
// — oʻrnatishda koʻrsatilgan foydalanish shartlari va maxfiylik siyosatidagi
// OCHIQ eʼlon. Shu sabab bu izoh doim haqiqatni aytishi SHART.
// ---------------------------------------------------------------------------

import Foundation
import Metal  // GPU modelini oʻqish uchun (MTLCreateSystemDefaultDevice)

enum Statistika {

    // Server manzili. Cloudflare Worker — kodi `statistika/` papkasida.
    private static let baza = "https://stat.mirqobilov.com"

    // MARK: Oʻrnatma ID

    /// Tasodifiy 32-belgili hex. Bir marta yasaladi va oʻzgarmaydi.
    /// Bu qurilma identifikatori EMAS: ilova oʻchirilib qayta oʻrnatilsa
    /// yangisi paydo boʻladi va eskisi bilan hech qanday aloqasi qolmaydi.
    private static var ornatmaID: String {
        if let bor = UserDefaults.standard.string(forKey: "ornatmaID"), bor.count == 32 {
            return bor
        }
        let yangi = (0..<16).map { _ in String(format: "%02x", UInt8.random(in: 0...255)) }.joined()
        UserDefaults.standard.set(yangi, forKey: "ornatmaID")
        return yangi
    }

    // MARK: Kunlik ping

    /// Ilova ishga tushganda chaqiriladi. Kunda bir martadan koʻp soʻrov
    /// yuborilmaydi.
    static func pingYubor() {
        let hozir = Date().timeIntervalSince1970
        let oxirgi = UserDefaults.standard.double(forKey: "oxirgiTekshiruv")
        // 20 soat, 24 emas: aks holda har kuni bir xil vaqtda ochadigan
        // foydalanuvchi chegaradan bir necha daqiqa oldin qolib, kun oshib
        // ketardi va statistikada kun tashlab ketardi.
        guard hozir - oxirgi > 20 * 3600 else { return }
        UserDefaults.standard.set(hozir, forKey: "oxirgiTekshiruv")

        guard let url = URL(string: baza + "/v1/ping") else { return }
        var soʻrov = URLRequest(url: url)
        soʻrov.httpMethod = "POST"
        soʻrov.setValue("application/json", forHTTPHeaderField: "Content-Type")
        soʻrov.timeoutInterval = 10

        let tana: [String: Any] = [
            "id": ornatmaID,
            "platforma": "mac",
            "versiya": joriyVersiya,
            "os": osVersiyasi,
            "arx": arxitektura,
            "cpu": cpuModeli,
            "gpu": gpuModeli,
            "ram_gb": ramGb,
            "yadro": ProcessInfo.processInfo.processorCount
        ]
        soʻrov.httpBody = try? JSONSerialization.data(withJSONObject: tana)
        // Javob kerak emas: undagi versiya maydonlari faqat 1.1.0 uchun.
        URLSession.shared.dataTask(with: soʻrov).resume()
    }

    // MARK: Amal oʻlchovi

    /// Har transkripsiya/tarjima tugagach chaqiriladi. Kontent yuborilmaydi —
    /// faqat oʻlchov: ovoz uzunligi, ishlov vaqti, backend, natija.
    /// `tur` — "stt" yoki "tarjima". `natija` — "ok" yoki "xato:<kod>".
    static func amalYubor(
        tur: String, ovoz_s: Double? = nil, belgi: Int? = nil,
        ishlov_s: Double, backend: String? = nil, natija: String = "ok"
    ) {
        guard let url = URL(string: baza + "/v1/amal") else { return }
        var soʻrov = URLRequest(url: url)
        soʻrov.httpMethod = "POST"
        soʻrov.setValue("application/json", forHTTPHeaderField: "Content-Type")
        soʻrov.timeoutInterval = 10

        var tana: [String: Any] = [
            "id": ornatmaID,
            "platforma": "mac",
            "versiya": joriyVersiya,
            "tur": tur,
            "ishlov_s": ishlov_s,
            "natija": natija
        ]
        if let ovoz_s { tana["ovoz_s"] = ovoz_s }
        if let belgi { tana["belgi"] = belgi }
        if let backend { tana["backend"] = backend }
        soʻrov.httpBody = try? JSONSerialization.data(withJSONObject: tana)
        // Javob kerak emas — jim yuboramiz.
        URLSession.shared.dataTask(with: soʻrov).resume()
    }

    // MARK: Yordamchilar

    static var joriyVersiya: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0.0.0"
    }

    private static var osVersiyasi: String {
        let v = ProcessInfo.processInfo.operatingSystemVersion
        return "macOS \(v.majorVersion).\(v.minorVersion).\(v.patchVersion)"
    }

    // MARK: Qurilma muhiti

    /// Ishlayotgan arxitektura. Universal binarda arm64 boʻlagi Apple Silicon'da,
    /// x86_64 boʻlagi Intel'da (yoki Rosetta'da) ishlaydi — shuning uchun
    /// kompilyatsiya vaqti belgisi ishlayotgan qurilmani toʻgʻri aks ettiradi.
    private static var arxitektura: String {
        #if arch(arm64)
            return "arm64"
        #else
            return "x64"
        #endif
    }

    /// `sysctl` orqali qiymat oʻqish (nol bilan tugaydigan satr).
    private static func sysctlSatr(_ nom: String) -> String {
        var oʻlcham = 0
        guard sysctlbyname(nom, nil, &oʻlcham, nil, 0) == 0, oʻlcham > 0 else { return "" }
        var bufer = [CChar](repeating: 0, count: oʻlcham)
        guard sysctlbyname(nom, &bufer, &oʻlcham, nil, 0) == 0 else { return "" }
        return String(cString: bufer)
    }

    /// Protsessor modeli, masalan "Apple M2 Pro" yoki "Intel Core i7".
    private static var cpuModeli: String {
        let s = sysctlSatr("machdep.cpu.brand_string")
        return s.isEmpty ? "?" : s
    }

    /// GPU modeli. Apple Silicon'da protsessor bilan bir xil chip.
    private static var gpuModeli: String {
        MTLCreateSystemDefaultDevice()?.name ?? "?"
    }

    /// Operativ xotira, gigabaytda (yaxlitlangan).
    private static var ramGb: Int {
        Int((Double(ProcessInfo.processInfo.physicalMemory) / 1_073_741_824.0).rounded())
    }

    /// Transkripsiya backend'i: arm64 boʻlagi Metal (GPU) bilan quriladi,
    /// x86_64 boʻlagi faqat CPU. Bu ishlayotgan boʻlakni toʻgʻri aks ettiradi.
    static var sttBackend: String {
        #if arch(arm64)
            return "metal"
        #else
            return "cpu"
        #endif
    }

}
