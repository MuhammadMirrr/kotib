// Ed25519 imzo tekshiruvi — avto-yangilanish manifesti va fayllari uchun.
//
// Nega Ed25519 va nega CryptoKit: yangilanishlar Sparkle'ning `sign_update`
// vositasi bilan imzolanadi va u RFC 8032 Ed25519 (SHA-512) beradi. CryptoKit
// — tizimning oʻz kutubxonasi, tashqi bogʻliqlik kerak emas. Windows tomonida
// xuddi shu ishni Monocypher qiladi (`win/core/imzo.cpp`); ikkalasi bitta
// holatlar jadvalidan sinaladi (`tests/umumiy/yangilanish_holatlari.def`).
//
// Bu fayl faqat Foundation va CryptoKit'ni import qiladi — `src/test.sh` uni
// kompilyatsiya qila oladi: CryptoKit sof hisob (fayl, tarmoq, Keychain yoʻq).

import CryptoKit
import Foundation

enum Imzo {
    /// `imzo` — `xabar` ning `ochiqKalit` ga mos haqiqiy Ed25519 imzosimi.
    ///
    /// Uzunligi notoʻgʻri kalit yoki imzo — shunchaki `false`, istisno yoʻq:
    /// qiymatlar tarmoqdan keladi va buzuq javob ilovani yiqitmasligi kerak.
    static func ed25519Tekshir(ochiqKalit: Data, xabar: Data, imzo: Data) -> Bool {
        guard ochiqKalit.count == 32, imzo.count == 64,
            let kalit = try? Curve25519.Signing.PublicKey(rawRepresentation: ochiqKalit)
        else { return false }
        return kalit.isValidSignature(imzo, for: xabar)
    }
}
