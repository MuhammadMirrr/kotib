// Ed25519 imzo tekshiruvi — `imzo.h` ga qarang.
#include "imzo.h"

#include "../third_party/monocypher/monocypher-ed25519.h"

namespace rubai {

bool ed25519Tekshir(const uint8_t* ochiqKalit, size_t kalitUzunligi, const uint8_t* xabar,
                    size_t xabarUzunligi, const uint8_t* imzo, size_t imzoUzunligi) {
    if (!ochiqKalit || kalitUzunligi != 32 || !imzo || imzoUzunligi != 64) return false;
    // Monocypher boʻsh xabar uchun ham koʻrsatkich kutadi.
    static const uint8_t boshXabar = 0;
    if (!xabar) {
        if (xabarUzunligi != 0) return false;
        xabar = &boshXabar;
    }
    // crypto_ed25519_check: 0 — imzo toʻgʻri, -1 — notoʻgʻri.
    return crypto_ed25519_check(imzo, ochiqKalit, xabar, xabarUzunligi) == 0;
}

}  // namespace rubai
