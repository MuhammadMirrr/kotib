// Ed25519 imzo tekshiruvi — avto-yangilanish manifesti va fayllari uchun.
//
// Monocypher (`win/third_party/monocypher/`, RFC 8032 moduli) ustidagi yupqa
// qatlam. macOS'dagi egizagi — `src/imzo.swift` (CryptoKit). Win32'ga
// tegmaydi, shuning uchun macOS'dagi `sinov.sh` ham sinaydi.
#pragma once

#include <cstddef>
#include <cstdint>

namespace rubai {

// `imzo` — `xabar` ning `ochiqKalit` ga mos haqiqiy Ed25519 imzosimi.
// Uzunligi notoʻgʻri kalit (≠32) yoki imzo (≠64) — shunchaki false.
bool ed25519Tekshir(const uint8_t* ochiqKalit, size_t kalitUzunligi, const uint8_t* xabar,
                    size_t xabarUzunligi, const uint8_t* imzo, size_t imzoUzunligi);

}  // namespace rubai
