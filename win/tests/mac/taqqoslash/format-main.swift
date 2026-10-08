// macOS tomonining matn formatlash chiqishi — C++ tomoni bilan solishtirish
// uchun. Fayl nomi ATAYLAB `main.swift` boʻlishi shart emas: bu fayl
// `taqqosla.sh` da alohida yigʻiladi va u yerda nomi oʻzgartiriladi.
import Foundation

let yol = CommandLine.arguments[1]
let matn = try! String(contentsOfFile: yol, encoding: .utf8)

var hujjatlar: [[Segment]] = []
var joriy: [Segment] = []
for qator in matn.split(separator: "\n", omittingEmptySubsequences: false) {
    let s = String(qator)
    if s.hasPrefix("#") { continue }
    if s.trimmingCharacters(in: .whitespaces).isEmpty {
        if !joriy.isEmpty { hujjatlar.append(joriy); joriy = [] }
        continue
    }
    let bolaklar = s.split(separator: " ", maxSplits: 2, omittingEmptySubsequences: false)
    guard bolaklar.count >= 2 else { continue }
    let t0 = Double(bolaklar[0]) ?? 0
    let t1 = Double(bolaklar[1]) ?? 0
    let m = bolaklar.count > 2 ? String(bolaklar[2]) : ""
    joriy.append(Segment(t0: t0, t1: t1, matn: m))
}
if !joriy.isEmpty { hujjatlar.append(joriy) }

for (i, segs) in hujjatlar.enumerated() {
    print("=== hujjat \(i)")
    print("--- xom")
    print(xomMatn(segs))
    print("--- chiroyli (standart apostrof)")
    print(chiroyliMatn(segs, apostrof: .standart))
    print("--- chiroyli (oddiy apostrof)")
    print(chiroyliMatn(segs, apostrof: .oddiy))
    print("--- diktovka (standart apostrof)")
    print(matnniTayyorla(xomMatn(segs), apostrof: .standart))
    print("--- diktovka (oddiy apostrof)")
    print(matnniTayyorla(xomMatn(segs), apostrof: .oddiy))
}
