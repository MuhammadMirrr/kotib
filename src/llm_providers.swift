// API kalitlarini macOS Keychain'da saqlaydi va LLM sozlamalarini boshqaradi.
// Kalit HECH QACHON log'ga yozilmaydi.

import Foundation
import Security

private let servis = "com.rubaistt.dictation.llm"

enum Kalitlar {

    static func oqi(provayder: String) -> String {
        let sorov: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: servis,
            kSecAttrAccount as String: provayder,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        var natija: CFTypeRef?
        guard SecItemCopyMatching(sorov as CFDictionary, &natija) == errSecSuccess,
            let d = natija as? Data, let s = String(data: d, encoding: .utf8)
        else {
            return ""
        }
        return s
    }

    /// Kalitni saqlaydi. Mavjud yozuv OʻCHIRILMAYDI, ustiga yoziladi.
    /// Natija — Keychain kodi (`errSecSuccess` — saqlandi); chaqiruvchi xatoni
    /// foydalanuvchiga koʻrsatadi (F4).
    ///
    /// Ilgari bu "avval oʻchir, keyin qoʻsh" edi va u ikki holatda buzilardi:
    ///   • yozuv boshqa jarayon (masalan `security` buyrugʻi) yaratgan boʻlsa,
    ///     oʻchirish oʻtmasdi va `SecItemAdd` errSecDuplicateItem (-25299)
    ///     qaytarardi — yangi kalit SAQLANMAY, eskisi qolib ketardi;
    ///   • oʻchirish bilan qoʻshish orasida ilova toʻxtasa, kalit yoʻqolardi.
    @discardableResult
    static func saqla(provayder: String, kalit: String) -> OSStatus {
        guard !kalit.isEmpty else { return ochir(provayder: provayder) }
        let qidiruv: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: servis,
            kSecAttrAccount as String: provayder
        ]
        let yangi: [String: Any] = [kSecValueData as String: Data(kalit.utf8)]

        let yangilash = SecItemUpdate(qidiruv as CFDictionary, yangi as CFDictionary)
        if yangilash == errSecSuccess { return errSecSuccess }
        guard yangilash == errSecItemNotFound else {
            // Kalitning OʻZI hech qachon logga tushmaydi — faqat kod.
            RubaiLog.write("keychain: yangilash xatosi, kod \(yangilash)")
            return yangilash
        }
        var yozuv = qidiruv
        yozuv[kSecValueData as String] = Data(kalit.utf8)
        yozuv[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
        let rc = SecItemAdd(yozuv as CFDictionary, nil)
        if rc != errSecSuccess {
            RubaiLog.write("keychain: saqlash xatosi, kod \(rc)")
        }
        return rc
    }

    /// Yozuv boʻlmasa ham muvaffaqiyat — oʻchirilishi kerak boʻlgan narsa yoʻq.
    @discardableResult
    static func ochir(provayder: String) -> OSStatus {
        let sorov: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: servis,
            kSecAttrAccount as String: provayder
        ]
        let rc = SecItemDelete(sorov as CFDictionary)
        if rc != errSecSuccess && rc != errSecItemNotFound {
            RubaiLog.write("keychain: oʻchirish xatosi, kod \(rc)")
            return rc
        }
        return errSecSuccess
    }
}

enum LLMSozlama {
    private static let d = UserDefaults.standard

    /// Tanlangan provayder identifikatori. Boʻsh — sozlanmagan.
    static var tanlanganID: String {
        get { d.string(forKey: "llm.provayder") ?? "" }
        set { d.set(newValue, forKey: "llm.provayder") }
    }

    static var tanlangan: Provayder? {
        provayderlar.first { $0.id == tanlanganID }
    }

    /// Custom provayder uchun — foydalanuvchi kiritgan URL.
    static var baseURL: String {
        get {
            let saqlangan = d.string(forKey: "llm.baseURL") ?? ""
            if !saqlangan.isEmpty { return saqlangan }
            return tanlangan?.baseURL ?? ""
        }
        set { d.set(newValue, forKey: "llm.baseURL") }
    }

    static var model: String {
        get {
            let saqlangan = d.string(forKey: "llm.model") ?? ""
            if !saqlangan.isEmpty { return saqlangan }
            return tanlangan?.standartModel ?? ""
        }
        set { d.set(newValue, forKey: "llm.model") }
    }

    /// LLM ishlatishga tayyormi.
    static var sozlanganmi: Bool {
        guard let p = tanlangan else { return false }
        return !Kalitlar.oqi(provayder: p.id).isEmpty
            && !baseURL.isEmpty && !model.isEmpty
    }

    static var joriyKalit: String {
        guard let p = tanlangan else { return "" }
        return Kalitlar.oqi(provayder: p.id)
    }

    // MARK: Model roʻyxati keshi
    //
    // Provayderdan olingan roʻyxat shu yerda saqlanadi, shunda Sozlamalar har
    // ochilganda tarmoqni kutib oʻtirmaydi: avval kesh koʻrsatiladi, yangilanish
    // esa fonda ketadi. Kesh MUDDATSIZ — u faqat qulaylik uchun; haqiqat manbai
    // doim API'ning oʻzi va u har ochilishda soʻraladi.

    static func modellarniSaqla(_ provayderID: String, _ royxat: [String]) {
        guard !provayderID.isEmpty else { return }
        d.set(royxat, forKey: "llm.modellar." + provayderID)
    }

    static func keshdagiModellar(_ provayderID: String) -> [String] {
        guard !provayderID.isEmpty else { return [] }
        return d.stringArray(forKey: "llm.modellar." + provayderID) ?? []
    }

    /// Provayder almashganda URL va modelni yangi presetga qaytaradi.
    static func provayderniTanla(_ id: String) {
        tanlanganID = id
        guard let p = provayderlar.first(where: { $0.id == id }) else { return }
        d.set(p.baseURL, forKey: "llm.baseURL")
        d.set(p.standartModel, forKey: "llm.model")
    }
}
