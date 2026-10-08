// Diktovka tugmasi: sozlama (UserDefaults), Carbon modifikatorlari va tizim
// boʻylab roʻyxatdan oʻtkazish (`RegisterEventHotKey` → `.rubaiHotkey`).

import AppKit
import Carbon.HIToolbox

// MARK: - Hotkey sozlamasi (UserDefaults'da saqlanadi)

struct HotKeyConfig {
    var keyCode: UInt32  // virtual key code (masalan kVK_ANSI_D)
    var carbonModifiers: UInt32  // controlKey | optionKey | ...
    var label: String  // ekranda koʻrsatiladigan tugma nomi, masalan "D"

    static let `default` = HotKeyConfig(
        keyCode: UInt32(kVK_ANSI_D),
        carbonModifiers: UInt32(controlKey | optionKey),
        label: "D")

    // Ekranda koʻrinishi, masalan "⌃⌥D"
    var displayString: String {
        var s = ""
        if carbonModifiers & UInt32(controlKey) != 0 { s += "⌃" }
        if carbonModifiers & UInt32(optionKey) != 0 { s += "⌥" }
        if carbonModifiers & UInt32(shiftKey) != 0 { s += "⇧" }
        if carbonModifiers & UInt32(cmdKey) != 0 { s += "⌘" }
        return s + label
    }

    /// ⌃, ⌥ yoki ⌘ dan kamida bittasi bormi. Yolgʻiz ⇧ yetmaydi: ⇧+harf —
    /// oddiy bosh harf, u global tugma boʻlib qolsa hech bir ilovada shu katta
    /// harfni yozib boʻlmasdi (F5).
    var modifikatorYetarli: Bool {
        carbonModifiers & UInt32(controlKey | optionKey | cmdKey) != 0
    }
}

enum HotKeyStore {
    private static let d = UserDefaults.standard
    static func load() -> HotKeyConfig {
        guard d.object(forKey: "hk.keyCode") != nil else { return .default }
        let c = HotKeyConfig(
            keyCode: UInt32(d.integer(forKey: "hk.keyCode")),
            carbonModifiers: UInt32(d.integer(forKey: "hk.mods")),
            label: d.string(forKey: "hk.label") ?? "?")
        // 1.2.0 gacha ⇧+tugma ham qabul qilinardi — shunday saqlangan tugma
        // standartga qaytariladi (F5).
        return c.modifikatorYetarli ? c : .default
    }
    static func save(_ c: HotKeyConfig) {
        d.set(Int(c.keyCode), forKey: "hk.keyCode")
        d.set(Int(c.carbonModifiers), forKey: "hk.mods")
        d.set(c.label, forKey: "hk.label")
    }
}

// NSEvent modifier'larini Carbon maskasiga oʻgirish
func carbonModifiers(from flags: NSEvent.ModifierFlags) -> UInt32 {
    var m: UInt32 = 0
    if flags.contains(.control) { m |= UInt32(controlKey) }
    if flags.contains(.option) { m |= UInt32(optionKey) }
    if flags.contains(.shift) { m |= UInt32(shiftKey) }
    if flags.contains(.command) { m |= UInt32(cmdKey) }
    return m
}

// keyDown hodisasidan tugmaning koʻrinadigan nomini olish
func keyLabel(for event: NSEvent) -> String {
    let special: [UInt16: String] = [
        UInt16(kVK_Space): "Space", UInt16(kVK_Return): "↩", UInt16(kVK_Tab): "⇥",
        UInt16(kVK_Escape): "⎋", UInt16(kVK_Delete): "⌫", UInt16(kVK_ForwardDelete): "⌦",
        UInt16(kVK_LeftArrow): "←", UInt16(kVK_RightArrow): "→",
        UInt16(kVK_UpArrow): "↑", UInt16(kVK_DownArrow): "↓",
        UInt16(kVK_Home): "↖", UInt16(kVK_End): "↘", UInt16(kVK_PageUp): "⇞", UInt16(kVK_PageDown): "⇟",
        UInt16(kVK_F1): "F1", UInt16(kVK_F2): "F2", UInt16(kVK_F3): "F3", UInt16(kVK_F4): "F4",
        UInt16(kVK_F5): "F5", UInt16(kVK_F6): "F6", UInt16(kVK_F7): "F7", UInt16(kVK_F8): "F8",
        UInt16(kVK_F9): "F9", UInt16(kVK_F10): "F10", UInt16(kVK_F11): "F11", UInt16(kVK_F12): "F12"
    ]
    if let s = special[event.keyCode] { return s }
    if let c = event.charactersIgnoringModifiers, !c.isEmpty,
        c.rangeOfCharacter(from: .controlCharacters) == nil
    {
        return c.uppercased()
    }
    return "Key\(event.keyCode)"
}

// MARK: - Global hotkey (Carbon) -> Notification

extension Notification.Name {
    static let rubaiHotkey = Notification.Name("rubaiHotkey")
}

private func hotkeyHandler(_ next: EventHandlerCallRef?, _ event: EventRef?, _ ud: UnsafeMutableRawPointer?) -> OSStatus
{
    NotificationCenter.default.post(name: .rubaiHotkey, object: nil)
    return noErr
}

final class HotKey {
    private var ref: EventHotKeyRef?
    private var installed = false

    // Hodisa qabul qiluvchini bir marta oʻrnatib, hozirgi sozlamani qoʻllaydi
    func install() {
        if !installed {
            var spec = EventTypeSpec(eventClass: OSType(kEventClassKeyboard), eventKind: UInt32(kEventHotKeyPressed))
            let s1 = InstallEventHandler(GetApplicationEventTarget(), hotkeyHandler, 1, &spec, nil, nil)
            NSLog("[rubai] hotkey handler oʻrnatildi: \(s1) (0 = OK)")
            installed = true
        }
        apply(HotKeyStore.load())
    }

    private var joriy: HotKeyConfig?

    /// Eski tugmani bekor qilib, yangisini roʻyxatdan oʻtkazadi. Oʻtmasa —
    /// eskisi qaytariladi va false: ilgari xato faqat NSLog'ga tushardi,
    /// sozlamalar esa yangi tugmani koʻrsatib turardi, diktovka esa
    /// umuman tugmasiz qolardi (F5).
    @discardableResult
    func apply(_ c: HotKeyConfig) -> Bool {
        if let r = ref { UnregisterEventHotKey(r); ref = nil }
        let s = royxatdanOtkaz(c)
        RubaiLog.write("hotkey \(c.displayString): \(s) (0 = OK)")
        if s == noErr { joriy = c; return true }
        if let eski = joriy { _ = royxatdanOtkaz(eski) }
        return false
    }

    private func royxatdanOtkaz(_ c: HotKeyConfig) -> OSStatus {
        let id = EventHotKeyID(signature: OSType(0x52535454), id: 1)  // 'RSTT'
        return RegisterEventHotKey(
            c.keyCode, c.carbonModifiers, id,
            GetApplicationEventTarget(), 0, &ref)
    }
}
