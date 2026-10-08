// Kotib — ilovaning asosiy menyu satri.
//
// NEGA BU FAYL BOR: macOS'da ⌘C, ⌘V, ⌘A, ⌘Z sehrli emas — ular NSMenuItem'ning
// `keyEquivalent` maydoni orqali ishlaydi. Kotib LSUIElement (menyu satri
// ilovasi) sifatida boshlangani uchun `NSApp.mainMenu` umuman qurilmagan edi:
// natijada oyna ichidagi hech bir matn maydonida standart qisqartmalar
// ishlamasdi. Status-bar menyusi (`ilova.swift`) buning oʻrnini bosmaydi —
// undagi `keyEquivalent` faqat menyu ochiq turganda tekshiriladi.
//
// `target = nil` ATAYLAB: shunda AppKit amalni javobgarlik zanjiri (responder
// chain) boʻylab yuboradi va qaysi matn maydoni faol boʻlsa, oʻsha bajaradi.
// Aniq nishon berilsa, qisqartma faqat oʻsha obyekt uchun ishlab qolardi.
//
// Menyu satri faqat aktivlik siyosati `.regular` boʻlganda koʻrinadi (yaʼni
// oyna ochiq paytda). `.accessory` rejimda u shunchaki yashirin turadi —
// shuning uchun menyuni bir marta, ishga tushishda qurish yetarli.

import AppKit

/// Nomi oʻzgarmaydigan menyu bandi.
///
/// `NSUndoManager` `undo:`/`redo:` bandlarining nomini har validatsiyada oʻzi
/// qayta yozadi («Undo», «Undo Typing» …). Ilovada lokalizatsiya yoʻq, shuning
/// uchun u inglizcha tushadi va qolgan hammasi oʻzbekcha boʻlgan menyuda yalt
/// etib koʻrinadi. Bu sinf birinchi nomdan keyin oʻzgartirishni qabul qilmaydi.
private final class QatiyNomliBand: NSMenuItem {
    private var qulflangan = false

    override var title: String {
        get { super.title }
        set { if !qulflangan { super.title = newValue } }
    }

    func qulfla() { qulflangan = true }
}

enum Menyu {

    /// Asosiy menyuni quradi va `NSApp.mainMenu` ga oʻrnatadi.
    /// `applicationDidFinishLaunching` dan bir marta chaqiriladi.
    static func qur() {
        let bosh = NSMenu()
        bosh.addItem(ilova())
        bosh.addItem(fayl())
        bosh.addItem(tahrir())
        bosh.addItem(korinish())
        bosh.addItem(oyna())
        NSApp.mainMenu = bosh
    }

    // MARK: Yordamchi

    private static func band(
        _ nom: String,
        _ amal: Selector?,
        _ tugma: String = "",
        _ modifikator: NSEvent.ModifierFlags = .command,
        nishon: AnyObject? = nil
    ) -> NSMenuItem {
        let i = NSMenuItem(title: nom, action: amal, keyEquivalent: tugma)
        if !tugma.isEmpty { i.keyEquivalentModifierMask = modifikator }
        i.target = nishon
        return i
    }

    /// `band` bilan bir xil, faqat nomi qulflanadi (`QatiyNomliBand` ga qarang).
    private static func qatiy(
        _ nom: String,
        _ amal: Selector?,
        _ tugma: String,
        _ modifikator: NSEvent.ModifierFlags = .command
    ) -> NSMenuItem {
        let i = QatiyNomliBand(title: nom, action: amal, keyEquivalent: tugma)
        i.keyEquivalentModifierMask = modifikator
        i.target = nil
        i.qulfla()
        return i
    }

    private static func bolim(_ nom: String, _ menyu: NSMenu) -> NSMenuItem {
        let b = NSMenuItem(title: nom, action: nil, keyEquivalent: "")
        b.submenu = menyu
        return b
    }

    // MARK: Menyular

    private static func ilova() -> NSMenuItem {
        let m = NSMenu(title: "Kotib")
        m.addItem(band("Kotib haqida", #selector(NSApplication.orderFrontStandardAboutPanel(_:))))
        m.addItem(.separator())
        m.addItem(
            band(
                "Sozlamalar…", #selector(AsosiyOyna.menyuSozlamalar), ",",
                nishon: AsosiyOyna.birgalik))
        m.addItem(.separator())

        // Tizim «Xizmatlar» menyusi: matn tanlanganda boshqa ilovalarga uzatish.
        let xizmatlar = NSMenu(title: "Xizmatlar")
        m.addItem(bolim("Xizmatlar", xizmatlar))
        NSApp.servicesMenu = xizmatlar

        m.addItem(.separator())
        m.addItem(band("Kotibni yashirish", #selector(NSApplication.hide(_:)), "h"))
        m.addItem(
            band(
                "Boshqalarni yashirish",
                #selector(NSApplication.hideOtherApplications(_:)), "h", [.command, .option]))
        m.addItem(band("Hammasini koʻrsatish", #selector(NSApplication.unhideAllApplications(_:))))
        m.addItem(.separator())
        m.addItem(band("Kotibdan chiqish", #selector(NSApplication.terminate(_:)), "q"))
        return bolim("Kotib", m)
    }

    private static func fayl() -> NSMenuItem {
        let m = NSMenu(title: "Fayl")
        m.addItem(
            band(
                "Ochish…", #selector(AsosiyOyna.menyuFaylOch), "o",
                nishon: AsosiyOyna.birgalik))
        m.addItem(.separator())
        m.addItem(band("Oynani yopish", #selector(NSWindow.performClose(_:)), "w"))
        return bolim("Fayl", m)
    }

    private static func tahrir() -> NSMenuItem {
        let m = NSMenu(title: "Tahrir")
        // undo:/redo: NSResponder'da eʼlon qilinmagan — ular UndoManager'ga
        // javobgarlik zanjiri orqali boradi, shuning uchun selektor qoʻlda.
        m.addItem(qatiy("Orqaga qaytarish", Selector(("undo:")), "z"))
        m.addItem(qatiy("Qaytarish", Selector(("redo:")), "z", [.command, .shift]))
        m.addItem(.separator())
        m.addItem(band("Kesish", #selector(NSText.cut(_:)), "x"))
        m.addItem(band("Nusxa olish", #selector(NSText.copy(_:)), "c"))
        m.addItem(band("Qoʻyish", #selector(NSText.paste(_:)), "v"))
        m.addItem(
            band(
                "Uslubsiz qoʻyish", #selector(NSTextView.pasteAsPlainText(_:)), "v",
                [.command, .option, .shift]))
        m.addItem(band("Oʻchirish", #selector(NSText.delete(_:))))
        m.addItem(.separator())
        m.addItem(band("Hammasini tanlash", #selector(NSText.selectAll(_:)), "a"))
        return bolim("Tahrir", m)
    }

    private static func korinish() -> NSMenuItem {
        let m = NSMenu(title: "Koʻrinish")
        for b in Bolim.allCases {
            let i = band(
                b.nom, #selector(AsosiyOyna.menyuBolim(_:)), "\(b.rawValue + 1)",
                nishon: AsosiyOyna.birgalik)
            i.tag = b.rawValue
            m.addItem(i)
        }
        return bolim("Koʻrinish", m)
    }

    private static func oyna() -> NSMenuItem {
        let m = NSMenu(title: "Oyna")
        m.addItem(band("Kichraytirish", #selector(NSWindow.performMiniaturize(_:)), "m"))
        m.addItem(band("Kattalashtirish", #selector(NSWindow.performZoom(_:))))
        m.addItem(.separator())
        m.addItem(
            band(
                "Hammasini oldinga chiqarish",
                #selector(NSApplication.arrangeInFront(_:))))
        NSApp.windowsMenu = m
        return bolim("Oyna", m)
    }
}
