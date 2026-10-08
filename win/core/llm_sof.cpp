// Ixtiyoriy LLM qatlamining SOF qismi: provayder roʻyxati, soʻrov tanasi,
// model roʻyxatini ajratish, SSE tahlili va matnni boʻlaklash.
//
// Nega alohida fayl: bu funksiyalar tarmoqqa ham, Windows API'ga ham
// tegmaydi va ular AYNAN macOS'dagi `llm_client.swift` bilan bir xil natija
// berishi kerak. Alohida turgani uchun ularni **macOS'da** ham kompilyatsiya
// qilib sinash mumkin (`win/tests/mac/sinov.sh`) — Windows mashinasini
// kutmasdan. Tarmoq va Credential Manager qismi `llm.cpp` da qoldi.
//
// macOS tomonida bu boʻlinish `llm_client.swift` (sof) va
// `llm_providers.swift` (Keychain) koʻrinishida.
#include "llm.h"

#include "json.h"
#include "util.h"

#include <algorithm>
#include <cwctype>
#include <set>
#include <string>
#include <vector>

namespace rubai {

namespace {

// Chat uchun ishlatib boʻlmaydigan modellarni tanish belgilari.
// ATAYLAB tor: shubha boʻlsa model roʻyxatda QOLADI. Notoʻgʻri yashirilgan
// model — foydalanuvchi uchun «nega yoʻq?» degan boshi berk savol; ortiqcha
// koʻringan model esa shunchaki eʼtiborsiz qoldiriladi.
bool chatEmasmi(const std::wstring& id) {
    std::wstring past = id;
    for (auto& c : past) c = static_cast<wchar_t>(towlower(c));

    for (const wchar_t* b : {L"embedding", L"whisper", L"dall-e", L"moderation", L"rerank"}) {
        if (past.find(b) != std::wstring::npos) return true;
    }

    // «tts» faqat ALOHIDA boʻlak sifatida — «tts-1», «gpt-4o-mini-tts».
    // Oddiy qidiruv boshqa nomlarni ham tutib qolishi mumkin.
    std::wstring bolak;
    for (size_t i = 0; i <= past.size(); ++i) {
        const wchar_t c = (i < past.size()) ? past[i] : L'-';
        if (c == L'-' || c == L'.' || c == L'/' || c == L':') {
            if (bolak == L"tts") return true;
            bolak.clear();
        } else {
            bolak += c;
        }
    }
    return false;
}

// SSE satridan `data:` qismini oladi. Boʻsh yoki `[DONE]` boʻlsa — boʻsh.
std::string sseTana(const std::string& satr) {
    if (satr.rfind("data:", 0) != 0) return {};
    std::string t = satr.substr(5);
    size_t b = t.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t o = t.find_last_not_of(" \t\r\n");
    return t.substr(b, o - b + 1);
}

std::wstring kesish(const std::wstring& s) {
    size_t b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) return {};
    size_t o = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, o - b + 1);
}

}  // namespace

// ---- Provayderlar ----------------------------------------------------------

const std::vector<Provayder>& provayderlar() {
    static const std::vector<Provayder> royxat = {
        {"google", L"Google AI Studio", L"https://generativelanguage.googleapis.com/v1beta/openai",
         L"gemini-2.5-flash", Adapter::OpenAI, ""},
        {"openrouter", L"OpenRouter", L"https://openrouter.ai/api/v1", L"google/gemini-2.5-flash",
         Adapter::OpenAI, ""},
        // `thinking` — DeepSeek'ning oʻz maydoni, standart holatda YOQIQ.
        // Bizning ishimiz transkriptni tozalash va xulosa qilish; fikrlash
        // zanjiri bunga sifat qoʻshmaydi, lekin javobni sekinlashtiradi va
        // chiqish tokenlarini (yaʼni toʻlovni) sezilarli koʻpaytiradi.
        {"deepseek", L"DeepSeek", L"https://api.deepseek.com/v1", L"deepseek-v4-flash",
         Adapter::OpenAI, "\"thinking\":{\"type\":\"disabled\"}"},
        {"openai", L"OpenAI", L"https://api.openai.com/v1", L"gpt-5-mini", Adapter::OpenAI, ""},
        {"groq", L"Groq", L"https://api.groq.com/openai/v1", L"llama-3.3-70b-versatile",
         Adapter::OpenAI, ""},
        {"anthropic", L"Anthropic", L"https://api.anthropic.com/v1", L"claude-haiku-4-5",
         Adapter::Anthropic, ""},
        {"custom", L"Boshqa (custom)", L"", L"", Adapter::OpenAI, ""},
    };
    return royxat;
}

const Provayder* provayderTop(const std::string& id) {
    for (const auto& p : provayderlar()) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

// ---- Soʻrov tanasi ---------------------------------------------------------

std::string soravTanasi(const Provayder& p, const std::wstring& model, const std::wstring& system,
                        const std::wstring& user) {
    JsonYozuvchi y;
    y.obyektBoshla();
    y.kalit("model");
    y.satr(model);
    y.kalit("stream");
    y.mantiq(true);

    if (p.adapter == Adapter::Anthropic) {
        // Anthropic `system`ni xabarlar ichida emas, alohida maydonda kutadi.
        y.kalit("max_tokens");
        y.butun(16000);
        y.kalit("system");
        y.satr(system);
        y.kalit("messages");
        y.royxatBoshla();
        y.obyektBoshla();
        y.kalit("role");
        y.satr(L"user");
        y.kalit("content");
        y.satr(user);
        y.obyektTugat();
        y.royxatTugat();
    } else {
        y.kalit("messages");
        y.royxatBoshla();
        y.obyektBoshla();
        y.kalit("role");
        y.satr(L"system");
        y.kalit("content");
        y.satr(system);
        y.obyektTugat();
        y.obyektBoshla();
        y.kalit("role");
        y.satr(L"user");
        y.kalit("content");
        y.satr(user);
        y.obyektTugat();
        y.royxatTugat();
    }
    y.obyektTugat();

    std::string tana = y.matn();
    if (!p.qoshimchaJson.empty() && tana.size() > 2 && tana.back() == '}') {
        tana.pop_back();
        tana += "," + p.qoshimchaJson + "}";
    }
    return tana;
}

// ---- Model roʻyxati --------------------------------------------------------

std::vector<std::wstring> modellarniAjrat(const std::string& json) {
    const Json j = Json::ajrat(json);
    const Json& malumot = j["data"];

    std::set<std::wstring> korilgan;
    std::vector<std::wstring> hammasi;
    for (size_t i = 0; i < malumot.hajmi(); ++i) {
        const std::wstring id = malumot[i]["id"].satr();
        if (id.empty()) continue;
        if (korilgan.insert(id).second) hammasi.push_back(id);
    }

    std::vector<std::wstring> filtrlangan;
    for (const auto& m : hammasi) {
        if (!chatEmasmi(m)) filtrlangan.push_back(m);
    }
    // Filtr hammasini olib tashlasa — filtrsiz roʻyxat. Boʻsh roʻyxat
    // foydalanuvchi uchun eng yomon natija.
    return filtrlangan.empty() ? hammasi : filtrlangan;
}

// ---- SSE -------------------------------------------------------------------

SSEHodisa openaiSSE(const std::string& satr) {
    const std::string tana = sseTana(satr);
    if (tana.empty()) return {};
    if (tana == "[DONE]") return {SSETuri::Tugadi, {}};

    const Json j = Json::ajrat(tana);
    const std::wstring matn = j["choices"][static_cast<size_t>(0)]["delta"]["content"].satr();
    if (matn.empty()) return {};
    return {SSETuri::Matn, matn};
}

SSEHodisa anthropicSSE(const std::string& satr) {
    const std::string tana = sseTana(satr);
    if (tana.empty() || tana == "[DONE]") return {};

    const Json j = Json::ajrat(tana);
    const std::wstring tur = j["type"].satr();
    if (tur == L"message_stop") return {SSETuri::Tugadi, {}};
    if (tur != L"content_block_delta") return {};
    if (j["delta"]["type"].satr() != L"text_delta") return {};

    const std::wstring matn = j["delta"]["text"].satr();
    if (matn.empty()) return {};
    return {SSETuri::Matn, matn};
}

// ---- Boʻlaklash ------------------------------------------------------------

std::vector<std::wstring> paragrafBolaklari(const std::wstring& matn, size_t maksSoz) {
    std::vector<std::wstring> paragraflar;
    size_t boshi = 0;
    for (;;) {
        const size_t joy = matn.find(L"\n\n", boshi);
        const std::wstring p = kesish(
            matn.substr(boshi, joy == std::wstring::npos ? std::wstring::npos : joy - boshi));
        if (!p.empty()) paragraflar.push_back(p);
        if (joy == std::wstring::npos) break;
        boshi = joy + 2;
    }
    if (paragraflar.empty()) return {};

    auto sozSoni = [](const std::wstring& s) {
        size_t n = 0;
        bool ichida = false;
        for (wchar_t c : s) {
            if (c == L' ') {
                ichida = false;
            } else if (!ichida) {
                ichida = true;
                ++n;
            }
        }
        return n;
    };

    std::vector<std::wstring> bolaklar;
    std::wstring joriy;
    size_t joriySoz = 0;

    for (const auto& p : paragraflar) {
        const size_t soz = sozSoni(p);
        if (joriySoz > 0 && joriySoz + soz > maksSoz) {
            bolaklar.push_back(joriy);
            joriy.clear();
            joriySoz = 0;
        }
        if (!joriy.empty()) joriy += L"\n\n";
        joriy += p;
        joriySoz += soz;
    }
    if (!joriy.empty()) bolaklar.push_back(joriy);
    return bolaklar;
}

}  // namespace rubai
