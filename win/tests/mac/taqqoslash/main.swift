// macOS tomonining chiqishi — C++ tomoni bilan bayt-bayt solishtirish uchun.
// Fayl nomi ATAYLAB `main.swift`: Swift yuqori darajadagi kodni faqat shu
// nomdagi faylda ruxsat etadi.
import Foundation

// Korpusdagi har qatorni boʻlaklab, kanonik koʻrinishda chiqaradi.
// C++ tomoni bilan bayt-bayt solishtirish uchun.
let yol = CommandLine.arguments[1]
let matn = try! String(contentsOfFile: yol, encoding: .utf8)
for (i, qator) in matn.split(separator: "\n", omittingEmptySubsequences: false).enumerated() {
    let b = MatnBoluvchi.bol(String(qator))
    print("--- \(i)")
    for (j, satr) in b.enumerated() {
        for bolak in satr {
            switch bolak {
            case .jumla(let m, let q): print("  \(j) J |\(m)|\(q)|")
            case .xom(let s): print("  \(j) X |\(s)|")
            }
        }
    }
}
