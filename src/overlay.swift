// Suzuvchi koʻrsatkich — «Yozilmoqda…», «Matnga oʻgirilmoqda…» va qisqa
// ogohlantirishlar. Fokusni oʻgʻirlamaydigan panel.

import AppKit

// MARK: - Suzuvchi overlay oyna (fokusni oʻgʻirlamaydi)

final class Overlay {
    private var panel: NSPanel?
    private let W: CGFloat = 320
    private let H: CGFloat = 56
    /// Har koʻrsatish yangi avlod. Kechiktirilgan yashirish faqat oʻzi
    /// koʻrsatgan xabarni yopadi (D5): ilgari koʻr `asyncAfter { hide() }`
    /// oraliqda boshlangan yangi yozuvning «Yozilmoqda…» belgisini oʻchirardi.
    private var avlod = 0
    /// Vaqtinchalik xabar tugagach qaytiladigan matn — yozuv davom etayotgan
    /// boʻlsa uning «Yozilmoqda…» belgisi. nil — panel yashiriladi.
    var asosiy: String?

    /// Qaytgan qiymat — `yashir` uchun token.
    @discardableResult
    func show(_ text: String, recording: Bool) -> Int {
        // Eni matnga qarab 320…560 pt: «⚠️ … — ovoz saqlandi …» kabi xabarlar
        // qatʼiy 320 da oxiri kesilib, eng muhim qismi koʻrinmasdi.
        let shrift = NSFont.systemFont(ofSize: 14, weight: .semibold)
        let matnEni = ceil((text as NSString).size(withAttributes: [.font: shrift]).width)
        let W = max(self.W, min(560, matnEni + 70))
        let v = NSVisualEffectView(frame: NSRect(x: 0, y: 0, width: W, height: H))
        v.material = .hudWindow; v.blendingMode = .behindWindow; v.state = .active
        v.wantsLayer = true; v.layer?.cornerRadius = 14; v.layer?.masksToBounds = true
        let ic = labelField(
            frame: NSRect(x: 16, y: (H - 26) / 2, width: 26, height: 26),
            fontSize: 20, alignment: .center)
        ic.stringValue = recording ? "🔴" : "✍️"
        let lb = labelField(
            frame: NSRect(x: 50, y: (H - 18) / 2, width: W - 66, height: 18),
            fontSize: 14)
        lb.stringValue = text
        v.addSubview(ic); v.addSubview(lb)
        return showPanel(v, height: H)
    }

    /// Xabarni `soniya` davomida koʻrsatadi. Oraliqda boshqa xabar chiqsa,
    /// u yopilmaydi.
    func vaqtincha(_ text: String, soniya: TimeInterval) {
        let t = show(text, recording: false)
        DispatchQueue.main.asyncAfter(deadline: .now() + soniya) { [weak self] in self?.yashir(t) }
    }

    /// `token` hali ekrandagi xabarniki boʻlsa — yopadi (yozuv davom etsa
    /// uning belgisiga qaytadi); oraliqda boshqa xabar chiqqan boʻlsa — hech narsa.
    func yashir(_ token: Int) {
        guard token == avlod else { return }
        if let a = asosiy { show(a, recording: true) } else { panel?.orderOut(nil) }
    }

    @discardableResult
    private func showPanel(_ content: NSView, height: CGFloat) -> Int {
        if panel == nil {
            let p = NSPanel(
                contentRect: NSRect(x: 0, y: 0, width: W, height: height),
                styleMask: [.nonactivatingPanel, .borderless],
                backing: .buffered, defer: false)
            p.level = .floating; p.isFloatingPanel = true; p.hidesOnDeactivate = false
            p.backgroundColor = .clear; p.isOpaque = false; p.hasShadow = true
            panel = p
        }
        panel!.contentView = content
        panel!.setContentSize(content.frame.size)
        if let scr = NSScreen.main {
            let f = scr.visibleFrame
            panel!.setFrameOrigin(NSPoint(x: f.midX - content.frame.width / 2, y: f.minY + 140))
        }
        panel!.orderFrontRegardless()
        avlod += 1
        return avlod
    }

    private func labelField(frame: NSRect, fontSize: CGFloat, alignment: NSTextAlignment = .left) -> NSTextField {
        let f = NSTextField(labelWithString: "")
        f.font = .systemFont(ofSize: fontSize, weight: .semibold)
        f.isBezeled = false; f.isEditable = false; f.drawsBackground = false
        f.alignment = alignment; f.textColor = .secondaryLabelColor
        f.frame = frame
        return f
    }
}
