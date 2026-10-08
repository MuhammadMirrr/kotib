// media_decode.swift RubaiLog.write ni chaqiradi; ilovada u log.swift da
// (AppKit bilan) yashaydi. Sinov uchun — faqat stderr ga.
import Foundation
enum RubaiLog {
    static func write(_ msg: String) {
        FileHandle.standardError.write(("[rubai] " + msg + "\n").data(using: .utf8)!)
    }
}
