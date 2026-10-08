// Studio amallari va ularning prompt'lari.
// Prompt'lar oʻzbek tilida — model javobni ham oʻzbekcha berishi uchun.

import Foundation

struct Amal {
    let id: String
    let nom: String
    /// true — natija butun matndan yigʻiladi (map-reduce kerak).
    /// false — matnni oʻzgartiradi (boʻlaklar ketma-ket ulanadi).
    /// DIQQAT: maydonlar tartibi quyidagi Amal(...) chaqiruvlari bilan mos —
    /// Swift memberwise init argumentlarni AYNAN shu tartibda kutadi.
    let yiguvchi: Bool
    let system: String
}

/// Har bir prompt'ga qoʻshiladigan umumiy qoidalar.
private let umumiyQoida = """
    Sen oʻzbek tilidagi nutq transkripti ustida ishlaysan.

    Qatʼiy qoidalar:
    - Javobni FAQAT oʻzbek lotin alifbosida yoz. Rus, turk yoki ingliz tiliga oʻtma.
    - «oʻ» va «gʻ» harflarini toʻgʻri yoz.
    - Maʼnoni OʻZGARTIRMA. Matnda boʻlmagan maʼlumotni QOʻSHMA.
    - Faqat natijani qaytar. «Mana natija», «Albatta» kabi muqaddima yozma.
    """

let amallar: [Amal] = [
    Amal(
        id: "tozalash", nom: "Tozalash", yiguvchi: false,
        system: """
            \(umumiyQoida)

            Vazifang: transkriptni oʻqishga qulay holga keltirish.
            - Tinish belgilarini toʻgʻri qoʻy.
            - Maʼnoli paragraflarga boʻl.
            - Ogʻzaki axlatni olib tashla: «e-e», «ha shunday», «anavi», «yaʼni»,
              takrorlangan soʻzlar, toʻxtab qolishlar.
            - Yarim aytilgan jumlalarni toʻliq jumlaga aylantir.
            - Raqamlarni raqam bilan yoz: «yigirma besh» → «25».
            """),

    Amal(
        id: "xulosa", nom: "Qisqacha xulosa", yiguvchi: true,
        system: """
            \(umumiyQoida)

            Vazifang: matnning 3–5 jumlalik xulosasini yoz.
            """),

    Amal(
        id: "asosiy_fikrlar", nom: "Asosiy fikrlar", yiguvchi: true,
        system: """
            \(umumiyQoida)

            Vazifang: asosiy fikrlarni belgili roʻyxat («- » bilan) shaklida yoz.
            """),

    Amal(
        id: "bayonnoma", nom: "Bayonnoma", yiguvchi: true,
        system: """
            \(umumiyQoida)

            Vazifang: yigʻilish bayonnomasini tuz. Uchta boʻlim:
            «Muhokama qilinganlar», «Qarorlar», «Topshiriqlar».
            Matnda boʻlmagan boʻlimni boʻsh qoldir, oʻylab topma.
            """),

    Amal(
        id: "maqola", nom: "Maqola", yiguvchi: false,
        system: """
            \(umumiyQoida)

            Vazifang: matnni maqola yoki bloq posti shakliga keltir.
            Sarlavha va kichik sarlavhalar qoʻy, paragraflarga boʻl.
            """),

    Amal(
        id: "savol_javob", nom: "Savol-javob", yiguvchi: true,
        system: """
            \(umumiyQoida)

            Vazifang: matn asosida savol-javob (FAQ) tuz.
            Har bir savol «**S:** », javob «**J:** » bilan boshlansin.
            """),

    Amal(
        id: "qisqartirish", nom: "Qisqartirish", yiguvchi: false,
        system: """
            \(umumiyQoida)

            Vazifang: matnni taxminan ikki barobar qisqartir.
            Barcha muhim maʼlumot saqlanib qolsin.
            """),

    Amal(
        id: "uzaytirish", nom: "Uzaytirish", yiguvchi: false,
        system: """
            \(umumiyQoida)

            Vazifang: matnni batafsilroq yozib chiq — jumlalarni toʻliqroq,
            fikrlarni ochiqroq qil. Yangi FAKT qoʻshma.
            """)
]

/// Erkin soʻrov — foydalanuvchi oʻz koʻrsatmasini yozadi.
func erkinAmal(_ korsatma: String) -> Amal {
    Amal(
        id: "erkin", nom: "Oʻz soʻrovim", yiguvchi: false,
        system: """
            \(umumiyQoida)

            Foydalanuvchi koʻrsatmasi: \(korsatma)
            """)
}

/// Bitta boʻlakka beriladigan maksimal soʻz soni.
private let bolakSozLimiti = 3000

final class AmalIshi {
    private let mijoz = LLMMijoz()

    /// Amalni bajaradi. Uzun matn boʻlaklanadi. Qaytarilgan `Task`ni chaqiruvchi
    /// "Bekor qilish" uchun `.cancel()` qilishi mumkin (I6) — `URLSession.bytes`
    /// bekor qilishni hurmat qiladi, shu bilan oqim toʻxtaydi.
    @discardableResult
    func bajar(
        amal: Amal, matn: String,
        onDelta: @escaping (String) -> Void,
        onTayyor: @escaping (String) -> Void,
        onXato: @escaping (String) -> Void
    ) -> Task<Void, Never>? {
        guard let p = LLMSozlama.tanlangan, LLMSozlama.sozlanganmi else {
            onXato(LLMXato.kalitYoq.xabar); return nil
        }
        let bolaklar = paragrafBolaklari(matn, maxSoz: bolakSozLimiti)
        guard !bolaklar.isEmpty else { onXato("Matn boʻsh."); return nil }

        return Task {
            do {
                let natija: String
                if bolaklar.count == 1 {
                    natija = try await bitta(p, amal, bolaklar[0], onDelta)
                } else if amal.yiguvchi {
                    natija = try await yig(p, amal, bolaklar, onDelta)
                } else {
                    natija = try await ketmaKet(p, amal, bolaklar, onDelta)
                }
                await MainActor.run { onTayyor(natija) }
            } catch {
                // Bekor qilingan boʻlsa (Task.cancel()) buni alohida ushlaymiz —
                // aks holda foydalanuvchi oʻzi bosgan "Bekor qilish" tugmasi
                // uchun "Tarmoqqa ulanib boʻlmadi" kabi chalgʻituvchi xato
                // koʻrsatilar edi.
                let xabar: String
                if Task.isCancelled {
                    xabar = "Bekor qilindi."
                } else if let e = error as? LLMXato {
                    xabar = e.xabar
                } else {
                    xabar = LLMXato.tarmoq.xabar
                }
                await MainActor.run { onXato(xabar) }
            }
        }
    }

    private func bitta(
        _ p: Provayder, _ amal: Amal, _ matn: String,
        _ onDelta: @escaping (String) -> Void
    ) async throws -> String {
        var yigilgan = ""
        try await mijoz.oqim(
            provayder: p, baseURL: LLMSozlama.baseURL,
            model: LLMSozlama.model, kalit: LLMSozlama.joriyKalit,
            system: amal.system, user: matn,
            onDelta: { d in
                yigilgan += d
                // DispatchQueue.main.async — FIFO kafolatlanadi;
                // mustaqil yaratilgan Task'lar orasida Swift
                // hech qanday tartib kafolatini bermaydi (M9).
                DispatchQueue.main.async { onDelta(d) }
            })
        return yigilgan
    }

    /// Oʻzgartiruvchi amallar: har bir boʻlak alohida, natijalar ulanadi.
    private func ketmaKet(
        _ p: Provayder, _ amal: Amal, _ bolaklar: [String],
        _ onDelta: @escaping (String) -> Void
    ) async throws -> String {
        var natijalar: [String] = []
        for (i, b) in bolaklar.enumerated() {
            if i > 0 { DispatchQueue.main.async { onDelta("\n\n") } }
            natijalar.append(try await bitta(p, amal, b, onDelta))
        }
        return natijalar.joined(separator: "\n\n")
    }

    /// Yigʻuvchi amallar: har bir boʻlak qisqartiriladi, soʻng yakuniy soʻrov.
    private func yig(
        _ p: Provayder, _ amal: Amal, _ bolaklar: [String],
        _ onDelta: @escaping (String) -> Void
    ) async throws -> String {
        let oraliqAmal = Amal(
            id: "oraliq", nom: "", yiguvchi: false,
            system: """
                \(umumiyQoida)

                Vazifang: matnning asosiy mazmunini qisqacha yozib ber.
                Bu keyingi bosqichda boshqa qismlar bilan birlashtiriladi.
                """)
        var oraliq: [String] = []
        for b in bolaklar {
            oraliq.append(try await bitta(p, oraliqAmal, b, { _ in }))
        }
        return try await bitta(p, amal, oraliq.joined(separator: "\n\n"), onDelta)
    }
}
