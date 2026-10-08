// Ixtiyoriy LLM qatlami — Studiya «amallari» (tozalash, xulosa) uchun.
//
// macOS'dagi `src/llm_client.swift` va `src/llm_providers.swift` ning
// ekvivalenti. Provayder roʻyxati, soʻrov tanasi va SSE tahlili AYNAN bir
// xil — ular ikkala platformada bir xil natija berishi kerak va shu sabab
// test bilan qotirilgan.
//
// **Ilova hech qanday API kalit bilan kelmaydi.** Diktovka va Studiya kalitsiz
// toʻliq ishlaydi; LLM — qoʻshimcha. Kalit foydalanuvchining oʻzi kiritadi va
// u Windows Credential Manager'da saqlanadi (macOS'da Keychain), hech qachon
// logga yozilmaydi.
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace rubai {

enum class Adapter { OpenAI, Anthropic };

struct Provayder {
    std::string id;
    std::wstring nom;
    std::wstring baseURL;
    std::wstring standartModel;
    Adapter adapter = Adapter::OpenAI;
    // Faqat SHU provayder tushunadigan qoʻshimcha soʻrov maydonlari, tayyor
    // JSON boʻlagi sifatida. Boshqa provayderlarga yuborilmaydi — notanish
    // maydon 400 berishi mumkin.
    std::string qoshimchaJson;
};

// Tayyor presetlar. Model maydoni HAR DOIM tahrirlanadi — preset faqat
// boshlangʻich qiymat beradi, chunki model roʻyxatlari tez oʻzgaradi.
const std::vector<Provayder>& provayderlar();
const Provayder* provayderTop(const std::string& id);

// ---- Sof funksiyalar (testlanadi) ------------------------------------------

// Chat soʻrovining JSON tanasi. Provayderlar orasidagi farqlar shu yerda
// toʻplanadi: Anthropic `system`ni alohida maydonda kutadi, DeepSeek
// `thinking`ni oʻchirishni talab qiladi.
std::string soravTanasi(const Provayder& p, const std::wstring& model, const std::wstring& system,
                        const std::wstring& user);

// Provayderning `/models` javobidan model identifikatorlarini ajratadi.
// Kelgan tartib saqlanadi, takrorlanganlar tashlanadi, chat boʻlmagan
// modellar filtrlanadi — LEKIN filtr hammasini olib tashlasa, filtrsiz
// roʻyxat qaytariladi.
std::vector<std::wstring> modellarniAjrat(const std::string& json);

enum class SSETuri { Matn, Tugadi, Otkaz };
struct SSEHodisa {
    SSETuri tur = SSETuri::Otkaz;
    std::wstring matn;
};

SSEHodisa openaiSSE(const std::string& satr);
SSEHodisa anthropicSSE(const std::string& satr);

// Uzun matnni paragraf chegaralarida boʻlaklarga ajratadi. Bitta paragrafning
// oʻzi limitdan uzun boʻlsa — u yaxlit qoladi: jumla oʻrtasidan kesish
// sifatni buzadi.
std::vector<std::wstring> paragrafBolaklari(const std::wstring& matn, size_t maksSoz);

// ---- Kalit saqlash ---------------------------------------------------------
// Windows Credential Manager. macOS'da bu Keychain (`llm_providers.swift`).
// Kalitning OʻZI hech qachon logga yozilmaydi — faqat xato kodi.
namespace Kalitlar {
bool saqla(const std::string& provayderId, const std::wstring& kalit);
std::wstring oqi(const std::string& provayderId);
bool ochir(const std::string& provayderId);
}  // namespace Kalitlar

// ---- Sozlama ---------------------------------------------------------------
// macOS'dagi `LLMSozlama` (`llm_providers.swift`) ning ekvivalenti. Qiymatlar
// `settings.ini` da (`llm.provayder`, `llm.baseURL`, `llm.model`), kalit esa
// Credential Manager'da yotadi.
//
// Har chaqiruvda sozlama fayli qaytadan oʻqiladi: u Sozlamalar oynasida
// oʻzgarishi mumkin va Studiya tabi eskirgan qiymat bilan ishlamasligi kerak.
namespace LLMSozlama {

std::string tanlanganId();
const Provayder* tanlangan();

// Boʻsh saqlangan qiymat — provayderning oʻz standarti.
std::wstring baseURL();
std::wstring model();

// LLM ishlatishga tayyormi: provayder tanlangan, kalit bor, URL va model boʻsh emas.
bool sozlanganmi();
std::wstring joriyKalit();

// Provayder almashganda URL va model yangi presetga qaytadi.
void provayderniTanla(const std::string& id);

}  // namespace LLMSozlama

// ---- Oqim mijozi -----------------------------------------------------------

struct LLMSorov {
    Provayder provayder;
    std::wstring baseURL;  // boʻsh — provayderning oʻzi
    std::wstring model;
    std::wstring kalit;
    std::wstring system;
    std::wstring user;
};

// Provayderdan model roʻyxatini oladi: `GET <baseURL>/models`. Ikkala adapter
// ham buni qoʻllab-quvvatlaydi, faqat avtorizatsiya sarlavhasi boshqacha.
//
// Qaytaradi: boʻsh satr — muvaffaqiyat (roʻyxat `chiqish` da), aks holda
// xato xabari. 404 — xato EMAS: baʼzi gateway'larda `/models` umuman
// yoʻq va foydalanuvchi model nomini qoʻlda yozadi; bunda boʻsh roʻyxat
// va boʻsh xato qaytadi.
//
// Bloklaydi — fon oqimidan chaqiring.
std::wstring modellarniOl(const Provayder& p, const std::wstring& baseURL,
                          const std::wstring& kalit, std::vector<std::wstring>& chiqish);

// Javobni boʻlak-boʻlak qaytaradi. `bolak` ISHCHI OQIMDA chaqiriladi.
// `bekor` true qaytarsa oqim toʻxtaydi.
// Qaytaradi: boʻsh satr — muvaffaqiyat, aks holda xato xabari (oʻzbekcha).
std::wstring llmOqim(const LLMSorov& s, const std::function<void(const std::wstring&)>& bolak,
                     const std::function<bool()>& bekor);

}  // namespace rubai
