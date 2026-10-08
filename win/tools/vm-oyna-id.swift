// UTM'ning VM oynasining raqamini beradi — `screencapture -l` uchun.
//
// Nega kerak: `screencapture` oynani NOM boʻyicha topa olmaydi, faqat raqam
// boʻyicha. Raqamli suratga olish esa oyna orqada tursa ham ishlaydi va
// fokusni oʻgʻirlamaydi — VM'ni koʻrishning yagona xalaqit bermaydigan yoʻli.
//
// Ishlatish: vm-oyna-id "Kotib-Win11-ARM"
import CoreGraphics
import Foundation

let qidiruv = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : ""
let royxat =
    CGWindowListCopyWindowInfo(
        [.optionOnScreenOnly, .excludeDesktopElements],
        kCGNullWindowID) as? [[String: Any]] ?? []
for oyna in royxat {
    let nom = oyna[kCGWindowName as String] as? String ?? ""
    let ega = oyna[kCGWindowOwnerName as String] as? String ?? ""
    guard ega == "UTM", nom == qidiruv else { continue }
    print(oyna[kCGWindowNumber as String] as? Int ?? 0)
    exit(0)
}
exit(1)
