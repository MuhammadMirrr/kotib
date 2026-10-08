// Tarjima koʻprigi interfeysi (`tarjima_bridge.cpp`, CTranslate2 + SentencePiece).
// Windows nusxasi: `win/core/tarjima_bridge.h`.

#ifndef RUBAI_TARJIMA_BRIDGE_H
#define RUBAI_TARJIMA_BRIDGE_H

// Kotib — CTranslate2 + SentencePiece ustidan C koʻprik.
//
// whisper_bridge.h bilan bir xil naqsh: jarayon-global bitta model, sof C
// interfeys (Swift C++ ni toʻgʻridan-toʻgʻri olmaydi, shuning uchun butun
// C++ mantiq .cpp ichida qoladi).
//
// XOTIRA: rubai_tarjima malloc qiladi — chaqiruvchi rubai_tarjima_str_bosat
// bilan boʻshatadi. (whisper_bridge dagi rubai_segment_text tuzogʻi bu yerda
// YOʻQ: bu yerdagi barcha satrlar chaqiruvchiniki.)
//
// IP-XAVFSIZLIK: funksiyalar ichki mutex bilan seriyalangan. Lekin bitta ish
// ketayotganda ikkinchisi kutishga majbur boʻladi — navbatni Swift tomoni
// (tarjimon.swift) boshqaradi.

#ifdef __cplusplus
extern "C" {
#endif

// CTranslate2 papkasini yuklaydi (model.bin + shared_vocabulary.json +
// sentencepiece.bpe.model + config.json). 0 = muvaffaqiyat, 1 = xato.
// Allaqachon yuklangan boʻlsa darhol 0 qaytaradi.
int rubai_tarjima_yukla(const char *model_dir);

// Modelni RAM'dan boʻshatadi. Yuklanmagan boʻlsa hech narsa qilmaydi.
void rubai_tarjima_bosat(void);

// 1 = model yuklangan.
int rubai_tarjima_yuklanganmi(void);

// Bitta jumlani tarjima qiladi. Til kodlari NLLB nomlari: "uzn_Latn",
// "rus_Cyrl", "eng_Latn" va h.k. NULL = xato (model yuklanmagan yoki
// ichki xato). Qaytgan satrni rubai_tarjima_str_bosat bilan boʻshating.
char *rubai_tarjima(const char *matn, const char *manba_til, const char *maqsad_til);

void rubai_tarjima_str_bosat(char *s);

#ifdef __cplusplus
}
#endif

#endif
