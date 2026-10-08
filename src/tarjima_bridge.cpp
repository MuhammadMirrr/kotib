// Kotib — tarjima dvigateli ustidan C koʻprik. whisper_bridge.c ning jufti.
//
// Tokenizatsiya HuggingFace NLLB tokenizatoriga AYNAN mos boʻlishi shart:
//     [manba_til] + sentencepiece boʻlaklari + "</s>"
// Maqsad tomonda prefiks — [maqsad_til]. Natijadan birinchi token (til kodi)
// va "</s>" olib tashlanadi.
//
// Bu tartib lokal sinovda tekshirilgan (uzn_Latn → rus_Cyrl):
//     "Bugun havo juda yaxshi."
//       → uzn_Latn ▁Bugun ▁havo ▁juda ▁yaxshi . </s>
//       → "Сегодня очень хорошая погода."
// Tartib buzilsa model baribir natija beradi, lekin sifat JIMGINA tushadi —
// shuning uchun uni oʻzgartirmang, avval etalon bilan solishtiring.
//
// Nega CPU: int8 kvantlangan model protsessorda GPU'dagi fp32 dan uch barobar
// tez ishlaydi va videokarta talab qilmaydi (AGENTS.md → Tarjimon).

#include "tarjima_bridge.h"

#include <ctranslate2/translator.h>
#include <sentencepiece_processor.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

std::unique_ptr<ctranslate2::Translator> g_tr;
std::unique_ptr<sentencepiece::SentencePieceProcessor> g_sp;
std::mutex g_qulf;

// Beam qiymati sifat oʻlchovlari shu qiymatda olingani uchun 4 (AGENTS.md).
// Sekin mashinalar uchun 1 ga tushirish mumkin — sifat biroz pasayadi.
constexpr size_t kBeam = 4;

// Dekodlash uzunligi manba uzunligiga bogʻlanadi. Ilgari u doimiy 256 edi va
// uzun jumla JIMGINA kesilardi — foydalanuvchi na xato, na ogohlantirish
// koʻrardi. Koeffitsient 2: tarjima manbadan uzunroq boʻlishi normal (rus tili
// oʻzbekchadan uzunroq), +30 esa qisqa jumlalar uchun zaxira.
constexpr size_t kUzunlikKoeff = 2;
constexpr size_t kUzunlikZaxira = 30;

// Takrorlanuvchi 4 soʻzli birikmani taqiqlaydi.
//
// Nega kerak: tinish belgisiz uzun matnda (diktovka chiqishi shunday boʻlishi
// mumkin) butun abzats bitta "jumla" boʻlib modelga boradi va u takrorlanish
// sikliga tushadi — "мышление В дальнейшем мышление В дальнейшем...". Ilgari
// buni 256 lik chegara tasodifan kesib turardi; chegara manbaga bogʻlangach
// axlat ikki barobar koʻpaydi.
//
// Nega aynan 4: FLORES-200 da (200 jumla, uzn_Latn -> rus_Cyrl) chrF++ ga
// taʼsiri -0,03 — oʻlchov shovqinidan kichik, buzilgan matnda esa soʻz
// xilma-xilligi 0,08 dan 0,62 ga koʻtariladi. 3 kuchliroq himoya beradi
// (0,78) lekin tarjimada uch soʻzli birikma qonuniy takrorlanishi mumkin.
constexpr size_t kTakrorNgram = 4;

char *satr_nusxa(const std::string &s) {
    char *p = static_cast<char *>(std::malloc(s.size() + 1));
    if (!p) return nullptr;
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

}  // namespace

extern "C" int rubai_tarjima_yukla(const char *model_dir) {
    std::lock_guard<std::mutex> l(g_qulf);
    if (g_tr && g_sp) return 0;
    if (!model_dir) return 1;
    try {
        const std::string dir(model_dir);
        auto sp = std::make_unique<sentencepiece::SentencePieceProcessor>();
        if (!sp->Load(dir + "/sentencepiece.bpe.model").ok()) return 1;
        auto tr = std::make_unique<ctranslate2::Translator>(dir, ctranslate2::Device::CPU,
                                                            ctranslate2::ComputeType::INT8);
        g_sp = std::move(sp);
        g_tr = std::move(tr);
        return 0;
    } catch (...) {
        // Eng koʻp uchraydigan sabab — xotira yetmasligi yoki papka buzuqligi.
        g_sp.reset();
        g_tr.reset();
        return 1;
    }
}

extern "C" void rubai_tarjima_bosat(void) {
    std::lock_guard<std::mutex> l(g_qulf);
    g_tr.reset();
    g_sp.reset();
}

extern "C" int rubai_tarjima_yuklanganmi(void) {
    std::lock_guard<std::mutex> l(g_qulf);
    return (g_tr && g_sp) ? 1 : 0;
}

extern "C" char *rubai_tarjima(const char *matn, const char *manba_til, const char *maqsad_til) {
    std::lock_guard<std::mutex> l(g_qulf);
    if (!g_tr || !g_sp || !matn || !manba_til || !maqsad_til) return nullptr;
    try {
        std::vector<std::string> boleklar;
        if (!g_sp->Encode(matn, &boleklar).ok()) return nullptr;

        std::vector<std::string> tokenlar;
        tokenlar.reserve(boleklar.size() + 2);
        tokenlar.emplace_back(manba_til);
        for (auto &b : boleklar) tokenlar.push_back(b);
        tokenlar.emplace_back("</s>");

        ctranslate2::TranslationOptions opt;
        opt.beam_size = kBeam;
        opt.max_decoding_length = tokenlar.size() * kUzunlikKoeff + kUzunlikZaxira;
        opt.no_repeat_ngram_size = kTakrorNgram;

        auto natija = g_tr->translate_batch({tokenlar}, {{std::string(maqsad_til)}}, opt);
        if (natija.empty() || natija[0].output().empty()) return nullptr;

        // [0] — maqsad til kodi (prefiks); oxirida "</s>" boʻlishi mumkin.
        const auto &chiqish = natija[0].output();
        std::vector<std::string> toza;
        toza.reserve(chiqish.size());
        for (size_t i = 1; i < chiqish.size(); i++)
            if (chiqish[i] != "</s>") toza.push_back(chiqish[i]);

        std::string javob;
        if (!g_sp->Decode(toza, &javob).ok()) return nullptr;
        return satr_nusxa(javob);
    } catch (...) { return nullptr; }
}

extern "C" void rubai_tarjima_str_bosat(char *s) { std::free(s); }
