// `log_siyosati.swift` testlari — logga shaxsiy matn tushmasligi (G1).
import Foundation

func logSiyosatiTestlari() {
    testQosh("log — diagnostika muddati") {
        let h = 1_000_000.0
        tekshir("yoqilmagan", !LogSiyosati.diagnostikaFaolmi(tugash: nil, hozir: h))
        tekshir("muddat ichida", LogSiyosati.diagnostikaFaolmi(tugash: h + 3600, hozir: h))
        tekshir("muddat tugadi", !LogSiyosati.diagnostikaFaolmi(tugash: h, hozir: h))
        tekshir("tugagandan keyin", !LogSiyosati.diagnostikaFaolmi(tugash: h - 1, hozir: h))
        tekshir(
            "soat orqaga surilsa ham 24 soatdan uzaymaydi",
            !LogSiyosati.diagnostikaFaolmi(tugash: h + 2 * 86400, hozir: h))
    }
    testQosh("log — matn faqat diagnostikada") {
        let matn = "uydagi shaxsiy suhbat"
        let oddiy = LogSiyosati.matnYozuvi(matn, diagnostika: false)
        tekshir("standart: matn yoʻq", !oddiy.contains("shaxsiy"))
        tekshir("standart: uzunlik bor", oddiy.contains("len=\(matn.count)"))
        tekshir("diagnostika: matn bor", LogSiyosati.matnYozuvi(matn, diagnostika: true).contains(matn))
    }
    testQosh("log — aylantirish rejasi") {
        let a = URL(fileURLWithPath: "/L/Kotib.log")
        let r = LogSiyosati.aylantirishRejasi(a).map { ($0.0.lastPathComponent, $0.1.lastPathComponent) }
        tengmi(
            "tartib: eng eskisi birinchi", r.map { "\($0.0)→\($0.1)" },
            ["Kotib.1.log→Kotib.2.log", "Kotib.log→Kotib.1.log"])
    }
}
