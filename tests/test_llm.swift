import Foundation

func llmTestlari() {
    testQosh("OpenAI SSE — matn boʻlagi") {
        let s = #"data: {"choices":[{"delta":{"content":"salom"}}]}"#
        if case .matn(let m) = openaiSSE(s) {
            tengmi("matn", m, "salom")
        } else {
            tekshir("matn hodisasi kutilgandi", false)
        }
    }

    testQosh("OpenAI SSE — tugash") {
        if case .tugadi = openaiSSE("data: [DONE]") {
            tekshir("tugadi", true)
        } else {
            tekshir("tugadi kutilgandi", false)
        }
    }

    testQosh("OpenAI SSE — boʻsh delta oʻtkaziladi") {
        let s = #"data: {"choices":[{"delta":{}}]}"#
        if case .otkaz = openaiSSE(s) { tekshir("otkaz", true) } else { tekshir("otkaz kutilgandi", false) }
    }

    testQosh("OpenAI SSE — data boʻlmagan qator") {
        if case .otkaz = openaiSSE(": keep-alive") {
            tekshir("otkaz", true)
        } else {
            tekshir("otkaz kutilgandi", false)
        }
        if case .otkaz = openaiSSE("") { tekshir("boʻsh — otkaz", true) } else { tekshir("otkaz kutilgandi", false) }
    }

    testQosh("Anthropic SSE — matn boʻlagi") {
        let s = #"data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"salom"}}"#
        if case .matn(let m) = anthropicSSE(s) {
            tengmi("matn", m, "salom")
        } else {
            tekshir("matn hodisasi kutilgandi", false)
        }
    }

    testQosh("Anthropic SSE — tugash") {
        let s = #"data: {"type":"message_stop"}"#
        if case .tugadi = anthropicSSE(s) { tekshir("tugadi", true) } else { tekshir("tugadi kutilgandi", false) }
    }

    testQosh("Anthropic SSE — boshqa hodisalar oʻtkaziladi") {
        let s = #"data: {"type":"ping"}"#
        if case .otkaz = anthropicSSE(s) { tekshir("otkaz", true) } else { tekshir("otkaz kutilgandi", false) }
    }

    testQosh("boʻlaklash — qisqa matn bitta boʻlak") {
        let m = "Birinchi paragraf.\n\nIkkinchi paragraf."
        tengmi("bitta", paragrafBolaklari(m, maxSoz: 100).count, 1)
        tengmi("oʻzgarmadi", paragrafBolaklari(m, maxSoz: 100)[0], m)
    }

    testQosh("boʻlaklash — paragraf chegarasida boʻlinadi") {
        let p1 = String(repeating: "soz ", count: 60).trimmingCharacters(in: .whitespaces)
        let p2 = String(repeating: "gap ", count: 60).trimmingCharacters(in: .whitespaces)
        let bolaklar = paragrafBolaklari("\(p1)\n\n\(p2)", maxSoz: 80)
        tengmi("ikkita boʻlak", bolaklar.count, 2)
        tekshir("birinchi toʻliq", bolaklar[0] == p1)
        tekshir("ikkinchi toʻliq", bolaklar[1] == p2)
    }

    testQosh("boʻlaklash — juda uzun bitta paragraf ham qaytariladi") {
        let p = String(repeating: "soz ", count: 500).trimmingCharacters(in: .whitespaces)
        let bolaklar = paragrafBolaklari(p, maxSoz: 80)
        tengmi("bitta boʻlak", bolaklar.count, 1)
        tekshir("boʻsh emas", !bolaklar[0].isEmpty)
    }

    testQosh("boʻlaklash — boʻsh matn") {
        tengmi("boʻsh massiv", paragrafBolaklari("", maxSoz: 80).count, 0)
    }
}

/// Provayderning /models javobini ajratish. Bu funksiya butun "modellarni
/// avtomatik aniqlash" imkoniyatining poydevori — u yiqilsa foydalanuvchi
/// modelini tanlay olmaydi, shuning uchun har bir chekka holat qamralgan.
func modelRoyxatiTestlari() {

    func d(_ s: String) -> Data { Data(s.utf8) }

    testQosh("modellar — OpenAI/DeepSeek shakli") {
        let javob = #"{"object":"list","data":[{"id":"deepseek-v4-flash"},{"id":"deepseek-v4-pro"}]}"#
        tengmi(
            "ikkita model", modellarniAjrat(d(javob)),
            ["deepseek-v4-flash", "deepseek-v4-pro"])
    }

    testQosh("modellar — Anthropic shakli (qoʻshimcha maydonlar bilan)") {
        let javob = #"{"data":[{"type":"model","id":"claude-haiku-4-5","display_name":"Haiku"}],"has_more":false}"#
        tengmi("id olinadi", modellarniAjrat(d(javob)), ["claude-haiku-4-5"])
    }

    testQosh("modellar — tartib SAQLANADI") {
        // OpenRouter roʻyxatni "yangisi birinchi" tartibida beradi — uni
        // alifbo boʻyicha qayta saralash foydalanuvchiga zarar qiladi.
        let javob = #"{"data":[{"id":"zeta"},{"id":"alpha"},{"id":"beta"}]}"#
        tengmi("kelgan tartibda", modellarniAjrat(d(javob)), ["zeta", "alpha", "beta"])
    }

    testQosh("modellar — takrorlanganlar tashlanadi") {
        let javob = #"{"data":[{"id":"a"},{"id":"a"},{"id":"b"}]}"#
        tengmi("bir marta", modellarniAjrat(d(javob)), ["a", "b"])
    }

    testQosh("modellar — chat boʻlmaganlari filtrlanadi") {
        let javob = #"""
            {"data":[{"id":"gpt-5-mini"},{"id":"text-embedding-3-small"},{"id":"whisper-1"},
                     {"id":"tts-1-hd"},{"id":"dall-e-3"},{"id":"omni-moderation-latest"}]}
            """#
        tengmi("faqat chat modeli qoladi", modellarniAjrat(d(javob)), ["gpt-5-mini"])
    }

    testQosh("modellar — filtr HAMMASINI olib tashlasa, roʻyxat qaytariladi") {
        // Filtr — evristika. U butun roʻyxatni yeb qoʻysa, foydalanuvchini
        // boʻsh roʻyxat bilan qoldirgandan koʻra filtrsiz koʻrsatgan afzal.
        let javob = #"{"data":[{"id":"text-embedding-3-small"},{"id":"whisper-1"}]}"#
        tengmi(
            "filtrsiz qaytadi", modellarniAjrat(d(javob)),
            ["text-embedding-3-small", "whisper-1"])
    }

    testQosh("modellar — id boʻlmagan yozuv oʻtkazib yuboriladi") {
        let javob = #"{"data":[{"id":"a"},{"object":"model"},{"id":""},{"id":"b"}]}"#
        tengmi("faqat haqiqiy id'lar", modellarniAjrat(d(javob)), ["a", "b"])
    }

    testQosh("modellar — buzuq javob boʻsh roʻyxat beradi, xato otmaydi") {
        tengmi("buzuq JSON", modellarniAjrat(d("<html>502</html>")), [])
        tengmi("data yoʻq", modellarniAjrat(d(#"{"object":"list"}"#)), [])
        tengmi("data massiv emas", modellarniAjrat(d(#"{"data":"salom"}"#)), [])
        tengmi("boʻsh", modellarniAjrat(Data()), [])
    }
}

/// Soʻrov tanasi. Eng muhimi — provayderga XOS parametrlar faqat oʻsha
/// provayderga ketishi kerak: DeepSeek'ning `thinking` maydonini OpenAI yoki
/// Groq'ga yuborsak, ular 400 qaytarishi mumkin.
func soravTanasiTestlari() {

    func p(_ id: String) -> Provayder { provayderlar.first { $0.id == id }! }

    testQosh("tana — umumiy maydonlar (OpenAI-mos)") {
        let t = soravTanasi(provayder: p("openai"), model: "m", system: "s", user: "u")
        tengmi("model", t["model"] as? String, "m")
        tekshir("stream yoqilgan", t["stream"] as? Bool == true)
        let xabarlar = t["messages"] as? [[String: String]]
        tengmi("ikkita xabar", xabarlar?.count, 2)
        tengmi("system", xabarlar?[0]["content"], "s")
        tengmi("user", xabarlar?[1]["content"], "u")
    }

    testQosh("tana — Anthropic boshqacha shaklda") {
        let t = soravTanasi(provayder: p("anthropic"), model: "m", system: "s", user: "u")
        tengmi("system alohida maydonda", t["system"] as? String, "s")
        tekshir("max_tokens bor", t["max_tokens"] != nil)
        tengmi("faqat bitta xabar", (t["messages"] as? [[String: String]])?.count, 1)
    }

    testQosh("tana — DeepSeek'da fikrlash OʻCHIRILGAN") {
        // DeepSeek "thinking" ni standart holatda YOQIB yuboradi (effort: high).
        // Bizning ishimiz — transkriptni tozalash; fikrlash zanjiri sifatga
        // deyarli qoʻshmaydi, lekin javobni sekinlashtiradi va chiqish
        // tokenlarini (yaʼni pulni) koʻpaytiradi.
        let t = soravTanasi(provayder: p("deepseek"), model: "m", system: "s", user: "u")
        let fikr = t["thinking"] as? [String: String]
        tengmi("thinking.type = disabled", fikr?["type"], "disabled")
    }

    testQosh("tana — boshqa provayderlarda 'thinking' YOʻQ") {
        for id in ["openai", "groq", "google", "openrouter", "anthropic", "custom"] {
            let t = soravTanasi(provayder: p(id), model: "m", system: "s", user: "u")
            tekshir("\(id): thinking yuborilmaydi", t["thinking"] == nil)
        }
    }
}
