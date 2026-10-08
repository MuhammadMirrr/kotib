// Server javobini (imzolangan siyosat manifesti) ILOVA BILAN AYNAN BIR XIL
// kod orqali tekshiradi: `src/yangilanish_siyosat.swift` va `src/imzo.swift`
// shu vositaga toʻgʻridan-toʻgʻri kompilyatsiya qilinadi (`scripts/reliz.sh`).
// Shunday qilib reliz skripti «toʻgʻri» deb KV'ga yozgan javobni ilova ham
// albatta qabul qiladi — ikkita alohida tekshiruvchi bir-biridan ajralib
// qolmaydi.
//
//   manifest-tekshir <javob.json> <mac|win> <ochiq-kalit-base64>
//
// Muvaffaqiyatda stdout'ga (tab bilan ajratilgan):
//   versiya <v> / min <v> / muhlat <soat> / foiz <n> / izoh <matn>
//   fayl <arx> <url> <hajm> <sha256>        (faqat win, har arxitektura uchun)
// Rad etilsa — stderr'ga sabab, chiqish kodi 1.
import Foundation

let a = CommandLine.arguments
guard a.count == 4 else {
    FileHandle.standardError.write(
        "ishlatish: manifest-tekshir <javob.json> <mac|win> <ochiq-kalit-base64>\n".data(using: .utf8)!)
    exit(2)
}
guard let javob = FileManager.default.contents(atPath: a[1]) else {
    FileHandle.standardError.write("oʻqilmadi: \(a[1])\n".data(using: .utf8)!)
    exit(2)
}
guard let kalit = Data(base64Encoded: a[3]), kalit.count == 32 else {
    FileHandle.standardError.write("ochiq kalit notoʻgʻri (32 baytli base64 kerak)\n".data(using: .utf8)!)
    exit(2)
}
guard let m = YangilanishSiyosati.javobniTekshir(javob, ochiqKalit: kalit, platforma: a[2]) else {
    FileHandle.standardError.write("RAD: imzo yoki manifest notoʻgʻri (ilova ham qabul qilmaydi)\n".data(using: .utf8)!)
    exit(1)
}
print("versiya\t\(m.versiya)")
print("min\t\(m.minVersiya)")
print("muhlat\t\(m.muhlatSoat)")
print("foiz\t\(m.foiz)")
print("izoh\t\(m.izoh)")
for arx in m.fayllar.keys.sorted() {
    let f = m.fayllar[arx]!
    print("fayl\t\(arx)\t\(f.url)\t\(f.hajm)\t\(f.sha256)")
}
