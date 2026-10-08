// Kotib — ishga tushirish nuqtasi.
//
// Swift'da top-level kod faqat "main.swift" faylida boʻlishi mumkin, shuning uchun
// bu toʻrt qator alohida turadi (loyihada bir nechta .swift fayl bor).

import AppKit

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.run()
