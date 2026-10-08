// LLM provayderlari bilan ishlash. Ikkita adapter:
//  - OpenAI-mos  /chat/completions  (OpenAI, DeepSeek, OpenRouter, Gemini, Groq, custom)
//  - Anthropic native /messages
//
// DIQQAT: bu fayl faqat Foundation import qiladi — u src/test.sh da alohida
// kompilyatsiya qilinadi. AppKit yoki Security bu yerga QOʻSHILMASIN.

import Foundation

enum AdapterTuri: String, Codable {
    case openai
    case anthropic
}

struct Provayder {
    let id: String
    let nom: String
    let baseURL: String
    let standartModel: String
    let adapter: AdapterTuri
    /// Faqat SHU provayder tushunadigan qoʻshimcha soʻrov maydonlari.
    /// Boshqa provayderlarga yuborilmaydi — notanish maydon 400 berishi mumkin.
    let qoshimcha: [String: Any]

    init(
        id: String, nom: String, baseURL: String, standartModel: String,
        adapter: AdapterTuri, qoshimcha: [String: Any] = [:]
    ) {
        self.id = id
        self.nom = nom
        self.baseURL = baseURL
        self.standartModel = standartModel
        self.adapter = adapter
        self.qoshimcha = qoshimcha
    }
}

/// Tayyor presetlar. Model maydoni HAR DOIM tahrirlanadi — preset faqat
/// boshlangʻich qiymat beradi, chunki model roʻyxatlari tez oʻzgaradi.
let provayderlar: [Provayder] = [
    Provayder(
        id: "google", nom: "Google AI Studio",
        baseURL: "https://generativelanguage.googleapis.com/v1beta/openai",
        standartModel: "gemini-2.5-flash", adapter: .openai),
    Provayder(
        id: "openrouter", nom: "OpenRouter",
        baseURL: "https://openrouter.ai/api/v1",
        standartModel: "google/gemini-2.5-flash", adapter: .openai),
    // `thinking` — DeepSeek'ning oʻz maydoni. U standart holatda YOQIQ (effort:
    // high): model javobdan oldin fikrlash zanjirini chiqaradi. Bizning ishimiz
    // — transkriptni tozalash va xulosa qilish; bunga fikrlash zanjiri sifat
    // qoʻshmaydi, lekin javobni sekinlashtiradi va chiqish tokenlarini
    // (yaʼni toʻlovni) sezilarli koʻpaytiradi. Shuning uchun oʻchiramiz.
    // Hujjat: api-docs.deepseek.com/guides/thinking_mode
    Provayder(
        id: "deepseek", nom: "DeepSeek",
        baseURL: "https://api.deepseek.com/v1",
        standartModel: "deepseek-v4-flash", adapter: .openai,
        qoshimcha: ["thinking": ["type": "disabled"]]),
    Provayder(
        id: "openai", nom: "OpenAI",
        baseURL: "https://api.openai.com/v1",
        standartModel: "gpt-5-mini", adapter: .openai),
    Provayder(
        id: "groq", nom: "Groq",
        baseURL: "https://api.groq.com/openai/v1",
        standartModel: "llama-3.3-70b-versatile", adapter: .openai),
    Provayder(
        id: "anthropic", nom: "Anthropic",
        baseURL: "https://api.anthropic.com/v1",
        standartModel: "claude-haiku-4-5", adapter: .anthropic),
    Provayder(
        id: "custom", nom: "Boshqa (custom)",
        baseURL: "", standartModel: "", adapter: .openai)
]

// MARK: - Soʻrov tanasi (sof funksiya)

/// Chat soʻrovining JSON tanasini yasaydi.
///
/// Alohida funksiya, chunki bu yerda provayderlar orasidagi farqlar toʻplanadi
/// (Anthropic `system`ni alohida maydonda kutadi, DeepSeek `thinking`ni
/// oʻchirishni talab qiladi) — va ularning har biri test bilan qotirilgan.
func soravTanasi(
    provayder: Provayder, model: String,
    system: String, user: String
) -> [String: Any] {
    var tana: [String: Any]
    switch provayder.adapter {
    case .openai:
        tana = [
            "model": model,
            "stream": true,
            "messages": [
                ["role": "system", "content": system],
                ["role": "user", "content": user]
            ]
        ]
    case .anthropic:
        tana = [
            "model": model,
            "stream": true,
            "max_tokens": 16000,
            "system": system,
            "messages": [["role": "user", "content": user]]
        ]
    }
    for (kalit, qiymat) in provayder.qoshimcha { tana[kalit] = qiymat }
    return tana
}

// MARK: - Model roʻyxati

/// Chat uchun ishlatib boʻlmaydigan modellarni tanib olish uchun belgilar.
/// ATAYLAB tor: shubha boʻlsa model roʻyxatda QOLADI. Notoʻgʻri yashirilgan
/// model — foydalanuvchi uchun "nega yoʻq?" degan boshi berk savol; ortiqcha
/// koʻringan model esa shunchaki eʼtiborsiz qoldiriladi.
private let chatEmasBelgilar = ["embedding", "whisper", "dall-e", "moderation", "rerank"]

private func chatEmasmi(_ id: String) -> Bool {
    let past = id.lowercased()
    if chatEmasBelgilar.contains(where: past.contains) { return true }
    // "tts" faqat alohida boʻlak sifatida — "tts-1", "gpt-4o-mini-tts".
    // Oddiy `contains("tts")` boshqa nomlarni ham tutib qolishi mumkin.
    return past.split(whereSeparator: { $0 == "-" || $0 == "." || $0 == "/" || $0 == ":" })
        .contains("tts")
}

/// Provayderning `/models` javobidan model identifikatorlarini ajratadi.
///
/// OpenAI-mos va Anthropic javoblari bir xil shaklda — `{"data":[{"id":…}]}` —
/// shuning uchun bitta funksiya ikkalasiga ham yetadi.
///
/// Qoidalar:
///   • kelgan TARTIB saqlanadi (OpenRouter yangisini birinchi beradi);
///   • takrorlanganlar tashlanadi;
///   • chat boʻlmagan modellar filtrlanadi, LEKIN filtr hammasini olib
///     tashlasa — filtrsiz roʻyxat qaytariladi;
///   • buzuq javobda xato OTILMAYDI, boʻsh roʻyxat qaytadi: model roʻyxatini
///     ololmaslik ilovani toʻxtatadigan holat emas, foydalanuvchi nomni
///     qoʻlda yozishi mumkin.
func modellarniAjrat(_ data: Data) -> [String] {
    guard let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
        let royxat = obj["data"] as? [[String: Any]]
    else { return [] }

    var korilgan = Set<String>()
    var hammasi: [String] = []
    for element in royxat {
        guard let id = element["id"] as? String, !id.isEmpty else { continue }
        if korilgan.insert(id).inserted { hammasi.append(id) }
    }
    let filtrlangan = hammasi.filter { !chatEmasmi($0) }
    return filtrlangan.isEmpty ? hammasi : filtrlangan
}

// MARK: - SSE tahlili (sof funksiyalar)

enum SSEHodisa {
    case matn(String)
    case tugadi
    case otkaz
}

private func dataJSON(_ satr: String) -> [String: Any]? {
    guard satr.hasPrefix("data:") else { return nil }
    let tana = satr.dropFirst(5).trimmingCharacters(in: .whitespaces)
    guard !tana.isEmpty, tana != "[DONE]" else { return nil }
    guard let d = tana.data(using: .utf8) else { return nil }
    return (try? JSONSerialization.jsonObject(with: d)) as? [String: Any]
}

func openaiSSE(_ satr: String) -> SSEHodisa {
    guard satr.hasPrefix("data:") else { return .otkaz }
    let tana = satr.dropFirst(5).trimmingCharacters(in: .whitespaces)
    if tana == "[DONE]" { return .tugadi }
    guard let j = dataJSON(satr),
        let choices = j["choices"] as? [[String: Any]],
        let delta = choices.first?["delta"] as? [String: Any],
        let matn = delta["content"] as? String, !matn.isEmpty
    else { return .otkaz }
    return .matn(matn)
}

func anthropicSSE(_ satr: String) -> SSEHodisa {
    guard let j = dataJSON(satr), let tur = j["type"] as? String else { return .otkaz }
    if tur == "message_stop" { return .tugadi }
    guard tur == "content_block_delta",
        let delta = j["delta"] as? [String: Any],
        delta["type"] as? String == "text_delta",
        let matn = delta["text"] as? String, !matn.isEmpty
    else { return .otkaz }
    return .matn(matn)
}

// MARK: - Boʻlaklash

/// Uzun matnni paragraf chegaralarida boʻlaklarga ajratadi.
/// Bitta paragrafning oʻzi limitdan uzun boʻlsa — u yaxlit qoladi
/// (jumla oʻrtasidan kesish sifatni buzadi).
func paragrafBolaklari(_ matn: String, maxSoz: Int) -> [String] {
    let paragraflar = matn.components(separatedBy: "\n\n")
        .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
        .filter { !$0.isEmpty }
    guard !paragraflar.isEmpty else { return [] }

    var bolaklar: [String] = []
    var joriy: [String] = []
    var joriySoz = 0

    for p in paragraflar {
        let soz = p.split(separator: " ").count
        if joriySoz > 0, joriySoz + soz > maxSoz {
            bolaklar.append(joriy.joined(separator: "\n\n"))
            joriy = []; joriySoz = 0
        }
        joriy.append(p)
        joriySoz += soz
    }
    if !joriy.isEmpty { bolaklar.append(joriy.joined(separator: "\n\n")) }
    return bolaklar
}

// MARK: - Xatolar

enum LLMXato: Error {
    case kalitYoq
    case notogriKalit
    case modelTopilmadi(String)
    case limit
    case tarmoq
    case server(Int)
    case javobOqilmadi
}

extension LLMXato {
    var xabar: String {
        switch self {
        case .kalitYoq:
            return "API kalit kiritilmagan. Sozlamalar → LLM boʻlimiga oʻting."
        case .notogriKalit:
            return "API kalit notoʻgʻri yoki muddati tugagan."
        case .modelTopilmadi(let m):
            return "Model topilmadi: «\(m)». Sozlamalarda model nomini tekshiring."
        case .limit:
            return "Provayder limiti tugadi. Biroz kutib qayta urinib koʻring."
        case .tarmoq:
            return "Internetga ulanish yoʻq. Matn xom holda qoldi."
        case .server(let kod):
            return "Provayder xatosi (\(kod)). Keyinroq urinib koʻring."
        case .javobOqilmadi:
            return "Provayder javobini oʻqib boʻlmadi."
        }
    }
}

// MARK: - Tarmoq

final class LLMMijoz {

    /// Bitta soʻrov yuborib javobni oqim sifatida qaytaradi.
    /// 429 va 5xx da 3 martagacha eksponensial kutish bilan qayta urinadi.
    func oqim(
        provayder: Provayder, baseURL: String, model: String, kalit: String,
        system: String, user: String,
        onDelta: @escaping (String) -> Void
    ) async throws {
        guard !kalit.isEmpty else { throw LLMXato.kalitYoq }

        var kechikish: UInt64 = 2_000_000_000  // 2 s
        var oxirgiXato: LLMXato? = nil
        for urinish in 0..<3 {
            do {
                try await birMartaYubor(
                    provayder: provayder, baseURL: baseURL,
                    model: model, kalit: kalit,
                    system: system, user: user, onDelta: onDelta)
                return
            } catch let e as LLMXato {
                // Faqat vaqtinchalik xatolarni qayta urinamiz.
                switch e {
                case .limit, .server:
                    oxirgiXato = e
                    guard urinish < 2 else { throw e }
                    try? await Task.sleep(nanoseconds: kechikish)
                    kechikish *= 2
                default:
                    throw e
                }
            }
        }
        throw oxirgiXato ?? LLMXato.limit
    }

    private func birMartaYubor(
        provayder: Provayder, baseURL: String, model: String,
        kalit: String, system: String, user: String,
        onDelta: @escaping (String) -> Void
    ) async throws {
        let toza = baseURL.hasSuffix("/") ? String(baseURL.dropLast()) : baseURL
        let yol = provayder.adapter == .anthropic ? "/messages" : "/chat/completions"
        guard let url = URL(string: toza + yol) else { throw LLMXato.tarmoq }

        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.timeoutInterval = 120
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")

        switch provayder.adapter {
        case .openai:
            req.setValue("Bearer \(kalit)", forHTTPHeaderField: "Authorization")
        case .anthropic:
            req.setValue(kalit, forHTTPHeaderField: "x-api-key")
            req.setValue("2023-06-01", forHTTPHeaderField: "anthropic-version")
        }
        req.httpBody = try JSONSerialization.data(
            withJSONObject: soravTanasi(
                provayder: provayder, model: model,
                system: system, user: user))

        let (bytes, javob): (URLSession.AsyncBytes, URLResponse)
        do {
            (bytes, javob) = try await URLSession.shared.bytes(for: req)
        } catch {
            throw LLMXato.tarmoq
        }

        guard let http = javob as? HTTPURLResponse else { throw LLMXato.javobOqilmadi }
        switch http.statusCode {
        case 200..<300: break
        case 401, 403: throw LLMXato.notogriKalit
        case 404: throw LLMXato.modelTopilmadi(model)
        case 429: throw LLMXato.limit
        default: throw LLMXato.server(http.statusCode)
        }

        for try await satr in bytes.lines {
            let hodisa = provayder.adapter == .anthropic ? anthropicSSE(satr) : openaiSSE(satr)
            switch hodisa {
            case .matn(let m): onDelta(m)
            case .tugadi: return
            case .otkaz: continue
            }
        }
    }

    /// Provayderdan mavjud modellar roʻyxatini oladi.
    ///
    /// Nega bu kerak: model nomlarini kodda qotirib qoʻyib boʻlmaydi. Provayderlar
    /// eski modellarni olib tashlaydi va yangilarini chiqaradi — masalan DeepSeek
    /// presetimizdagi `deepseek-chat` uning oʻz roʻyxatida endi umuman yoʻq.
    /// Roʻyxatni har safar API'ning oʻzidan soʻrasak, ilova hech qachon
    /// eskirmaydi.
    ///
    /// Ikkala adapter ham `GET {baseURL}/models` ni qoʻllab-quvvatlaydi, faqat
    /// avtorizatsiya sarlavhasi boshqacha — chat soʻrovidagi bilan bir xil.
    func modellar(provayder: Provayder, baseURL: String, kalit: String) async throws -> [String] {
        guard !kalit.isEmpty else { throw LLMXato.kalitYoq }
        let tozaBase = baseURL.hasSuffix("/") ? String(baseURL.dropLast()) : baseURL
        guard let url = URL(string: tozaBase + "/models") else { throw LLMXato.javobOqilmadi }

        var req = URLRequest(url: url)
        req.httpMethod = "GET"
        req.timeoutInterval = 20
        switch provayder.adapter {
        case .openai:
            req.setValue("Bearer \(kalit)", forHTTPHeaderField: "Authorization")
        case .anthropic:
            req.setValue(kalit, forHTTPHeaderField: "x-api-key")
            req.setValue("2023-06-01", forHTTPHeaderField: "anthropic-version")
        }

        let (data, javob): (Data, URLResponse)
        do {
            (data, javob) = try await URLSession.shared.data(for: req)
        } catch {
            throw LLMXato.tarmoq
        }
        guard let http = javob as? HTTPURLResponse else { throw LLMXato.javobOqilmadi }
        switch http.statusCode {
        case 200..<300: break
        case 401, 403: throw LLMXato.notogriKalit
        // 404 — provayderda /models umuman yoʻq (baʼzi custom gateway'lar).
        // Bu xato emas: foydalanuvchi model nomini qoʻlda yozadi.
        case 404: return []
        case 429: throw LLMXato.limit
        default: throw LLMXato.server(http.statusCode)
        }
        return modellarniAjrat(data)
    }
}
