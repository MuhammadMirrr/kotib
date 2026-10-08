// Login'da ishga tushirish — ilova ichidagi LaunchAgent (`SMAppService.agent`),
// `--autostart` argumenti bilan (AGENTS.md → «Login item / launch behaviour»).

import Foundation
import ServiceManagement

// MARK: - Login'da ishga tushirish (SMAppService)

// Login'da ilova `.app` ichidagi LaunchAgent orqali ishga tushadi — u ilovani
// `--autostart` argumenti bilan chaqiradi. Nega aynan shunday: macOS'da "ilovani
// foydalanuvchi ochdimi yoki login ochdimi" degan savolga javob beradigan
// hujjatlashtirilgan API YOʻQ (NSApplicationLaunchIsDefaultLaunchKey saqlangan
// holat tiklanganda ham NO boʻladi; XPC_SERVICE_NAME ikkala holatda bir xil).
// Argument esa ikkiga boʻlmaydigan belgi: bor — login, yoʻq — foydalanuvchi.
// Ilgari ishlatilgan SMAppService.mainApp ilovani argumentsiz ishga tushirardi,
// shuning uchun oynani ochish/ochmaslikni hal qilib boʻlmasdi.
enum LoginItem {

    static let agentPlist = "com.rubaistt.dictation.autostart.plist"

    @available(macOS 13.0, *)
    private static var xizmat: SMAppService { SMAppService.agent(plistName: agentPlist) }

    static var isEnabled: Bool {
        guard #available(macOS 13.0, *) else { return false }
        return xizmat.status == .enabled
    }

    @discardableResult
    static func set(_ on: Bool) -> Bool {
        guard #available(macOS 13.0, *) else { return false }
        // Eski roʻyxatdan oʻtish har doim olib tashlanadi — aks holda login'da
        // ikkita nusxa yonadi (biri argumentli, biri argumentsiz).
        try? SMAppService.mainApp.unregister()
        do {
            if on {
                if xizmat.status != .enabled { try xizmat.register() }
            } else {
                if xizmat.status == .enabled { try xizmat.unregister() }
            }
            return true
        } catch {
            RubaiLog.write("login item xato: \(error.localizedDescription)")
            return false
        }
    }

    /// Eski versiyadan yangilangan foydalanuvchilarni `mainApp` roʻyxatidan
    /// LaunchAgent'ga oʻtkazadi. Ilova har ishga tushganda chaqiriladi, lekin
    /// ish faqat eski roʻyxat hali faol boʻlsa bajariladi.
    static func eskiRoyxatdanKochir() {
        guard #available(macOS 13.0, *) else { return }
        guard SMAppService.mainApp.status == .enabled else { return }
        RubaiLog.write("login item: mainApp'dan LaunchAgent'ga koʻchirilmoqda")
        set(true)
    }
}
