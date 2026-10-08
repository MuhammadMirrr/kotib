// `llm.cpp` dagi sof funksiyalar testlari.
//
// Holatlar macOS'dagi `tests/test_llm.swift` bilan bir xil maqsadda: soʻrov
// tanasi va SSE tahlili ikkala platformada AYNAN bir xil boʻlishi kerak.
// Bu yerdagi farq jimgina yuzaga chiqadi — foydalanuvchi Windows'da
// «hech narsa boʻlmadi» deb koʻradi, macOS'da esa hammasi ishlaydi.
#include "../core/llm.h"

#include <string>

namespace kotib_test {
void tekshir(const std::wstring& nom, bool shart);
void tengmi(const std::wstring& nom, const std::wstring& olingan, const std::wstring& kutilgan);
}  // namespace kotib_test

using kotib_test::tekshir;
using kotib_test::tengmi;
using namespace rubai;

namespace {

bool bormi(const std::string& matn, const std::string& boʻlak) {
    return matn.find(boʻlak) != std::string::npos;
}

}  // namespace

void llmTestlari() {
    // ---- Provayderlar ------------------------------------------------------
    tekshir(L"provayderlar boʻsh emas", !provayderlar().empty());
    tekshir(L"google topiladi", provayderTop("google") != nullptr);
    tekshir(L"yoʻq provayder nullptr", provayderTop("yoq") == nullptr);
    tekshir(L"anthropic adapteri", provayderTop("anthropic")->adapter == Adapter::Anthropic);
    tekshir(L"openai adapteri", provayderTop("openai")->adapter == Adapter::OpenAI);

    // ---- Soʻrov tanasi -----------------------------------------------------
    {
        const std::string t =
            soravTanasi(*provayderTop("openai"), L"gpt-5-mini", L"Sen yordamchisan", L"Salom");
        tekshir(L"openai: model bor", bormi(t, "\"model\":\"gpt-5-mini\""));
        tekshir(L"openai: stream yoqiq", bormi(t, "\"stream\":true"));
        tekshir(L"openai: system xabarlar ichida", bormi(t, "\"role\":\"system\""));
        tekshir(L"openai: user xabar", bormi(t, "\"role\":\"user\""));
        // OpenAI'da alohida `system` maydoni BOʻLMASLIGI kerak.
        tekshir(L"openai: alohida system maydoni yoʻq", !bormi(t, "\"system\":\"Sen"));
        tekshir(L"openai: max_tokens yoʻq", !bormi(t, "max_tokens"));
    }
    {
        const std::string t = soravTanasi(*provayderTop("anthropic"), L"claude-haiku-4-5",
                                          L"Sen yordamchisan", L"Salom");
        // Anthropic `system`ni ALOHIDA maydonda kutadi — xabarlar ichida emas.
        tekshir(L"anthropic: alohida system", bormi(t, "\"system\":\"Sen yordamchisan\""));
        tekshir(L"anthropic: system roli yoʻq", !bormi(t, "\"role\":\"system\""));
        tekshir(L"anthropic: max_tokens bor", bormi(t, "\"max_tokens\":16000"));
    }
    {
        // DeepSeek `thinking` ni oʻchirishni talab qiladi — u boshqa
        // provayderlarga YUBORILMASLIGI kerak (notanish maydon 400 beradi).
        const std::string d = soravTanasi(*provayderTop("deepseek"), L"m", L"s", L"u");
        tekshir(L"deepseek: thinking oʻchirilgan", bormi(d, "\"thinking\""));
        const std::string g = soravTanasi(*provayderTop("google"), L"m", L"s", L"u");
        tekshir(L"google: thinking yoʻq", !bormi(g, "thinking"));
    }
    {
        // Matndagi tirnoq va yangi qator JSON'ni buzmasligi kerak.
        const std::string t =
            soravTanasi(*provayderTop("openai"), L"m", L"s", L"U \"dedi\"\nva ketdi");
        tekshir(L"maxsus belgilar qochiriladi", bormi(t, "\\\"dedi\\\"") && bormi(t, "\\n"));
    }

    // ---- SSE: OpenAI -------------------------------------------------------
    {
        const auto h = openaiSSE("data: {\"choices\":[{\"delta\":{\"content\":\"salom\"}}]}");
        tekshir(L"openai SSE matn turi", h.tur == SSETuri::Matn);
        tengmi(L"openai SSE matni", h.matn, L"salom");
    }
    tekshir(L"openai SSE [DONE]", openaiSSE("data: [DONE]").tur == SSETuri::Tugadi);
    tekshir(L"openai SSE boʻsh delta",
            openaiSSE("data: {\"choices\":[{\"delta\":{}}]}").tur == SSETuri::Otkaz);
    tekshir(L"openai SSE data emas", openaiSSE("event: ping").tur == SSETuri::Otkaz);
    tekshir(L"openai SSE buzuq json", openaiSSE("data: {buzuq").tur == SSETuri::Otkaz);
    tekshir(L"openai SSE boʻsh satr", openaiSSE("").tur == SSETuri::Otkaz);

    // ---- SSE: Anthropic ----------------------------------------------------
    {
        const auto h = anthropicSSE("data: "
                                    "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_"
                                    "delta\",\"text\":\"salom\"}}");
        tekshir(L"anthropic SSE matn turi", h.tur == SSETuri::Matn);
        tengmi(L"anthropic SSE matni", h.matn, L"salom");
    }
    tekshir(L"anthropic SSE tugash",
            anthropicSSE("data: {\"type\":\"message_stop\"}").tur == SSETuri::Tugadi);
    tekshir(L"anthropic SSE boshqa tur",
            anthropicSSE("data: {\"type\":\"ping\"}").tur == SSETuri::Otkaz);
    // `text_delta` emas — masalan `thinking_delta` — matn sifatida
    // qabul qilinmasligi kerak.
    tekshir(L"anthropic SSE notoʻgʻri delta turi",
            anthropicSSE("data: "
                         "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"thinking_delta\","
                         "\"text\":\"x\"}}")
                    .tur == SSETuri::Otkaz);

    // ---- Model roʻyxati ----------------------------------------------------
    {
        const auto m = modellarniAjrat("{\"data\":[{\"id\":\"gpt-5\"},{\"id\":\"gpt-4\"}]}");
        tekshir(L"model soni", m.size() == 2);
        tengmi(L"tartib saqlanadi", m[0], L"gpt-5");
    }
    {
        // Chat boʻlmagan modellar filtrlanadi.
        const auto m = modellarniAjrat("{\"data\":[{\"id\":\"gpt-5\"},{\"id\":\"text-embedding-3\"}"
                                       ",{\"id\":\"tts-1\"},{\"id\":\"whisper-1\"}]}");
        tekshir(L"embedding/tts/whisper filtrlandi", m.size() == 1);
        tengmi(L"chat modeli qoldi", m[0], L"gpt-5");
    }
    {
        // Filtr HAMMASINI olib tashlasa — filtrsiz roʻyxat qaytadi.
        // Boʻsh roʻyxat foydalanuvchi uchun eng yomon natija.
        const auto m = modellarniAjrat("{\"data\":[{\"id\":\"tts-1\"},{\"id\":\"whisper-1\"}]}");
        tekshir(L"filtr hammasini olsa — filtrsiz", m.size() == 2);
    }
    {
        // «tts» faqat alohida boʻlak sifatida filtrlanadi.
        const auto m =
            modellarniAjrat("{\"data\":[{\"id\":\"gpt-4o-mini-tts\"},{\"id\":\"pottsville-x\"}]}");
        tekshir(L"tts boʻlagi filtrlandi, soʻz ichidagisi yoʻq", m.size() == 1);
        tengmi(L"soʻz ichidagi tts qoldi", m[0], L"pottsville-x");
    }
    {
        const auto m = modellarniAjrat("{\"data\":[{\"id\":\"a\"},{\"id\":\"a\"},{\"id\":\"b\"}]}");
        tekshir(L"takrorlanganlar tashlanadi", m.size() == 2);
    }
    tekshir(L"buzuq javob — boʻsh roʻyxat", modellarniAjrat("axlat").empty());
    tekshir(L"data yoʻq — boʻsh roʻyxat", modellarniAjrat("{}").empty());

    // ---- Boʻlaklash --------------------------------------------------------
    {
        const auto b = paragrafBolaklari(L"bir ikki\n\nuch tort\n\nbesh olti", 4);
        tekshir(L"soʻz chegarasi boʻyicha boʻlinadi", b.size() >= 2);
    }
    {
        // Bitta paragrafning oʻzi limitdan uzun boʻlsa — yaxlit qoladi.
        // Jumla oʻrtasidan kesish sifatni buzadi.
        const auto b = paragrafBolaklari(L"bir ikki uch tort besh olti yetti", 3);
        tekshir(L"uzun paragraf yaxlit qoladi", b.size() == 1);
    }
    {
        const auto b = paragrafBolaklari(L"bir\n\n\n\nikki", 100);
        tekshir(L"boʻsh paragraflar tashlanadi", b.size() == 1);
    }
    tekshir(L"boʻsh matn — boʻsh natija", paragrafBolaklari(L"", 10).empty());
    tekshir(L"faqat boʻshliq — boʻsh natija", paragrafBolaklari(L"   \n\n  ", 10).empty());
}
