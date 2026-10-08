// Nutq modelini ilova ichida yuklab olish.
//
// macOS'dagi `src/model_download.swift` ning ekvivalenti. Ilgari Windows'da
// bu yoʻl umuman yoʻq edi: model faqat oʻrnatuvchi bilan kelardi va u
// yoʻqolsa (oʻchirib yuborilgan, antivirus olib qoʻygan, portable nusxa)
// ilova boshi berk koʻchada qolardi — «Model topilmadi» deb aytardi, lekin
// nima qilishni koʻrsatmasdi.
//
// Model `%LOCALAPPDATA%\Kotib\models\` ga tushadi. `Engine::findModel`
// uchinchi navbatda aynan shu joyga qaraydi, shuning uchun oʻrnatuvchi
// qoʻygan nusxa (Program Files ichida) ustunligicha qoladi.
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace rubai {

class ModelYuklovchi {
public:
    // CDN'dagi model. Oʻrnatuvchidagi manzil bilan bir xil
    // (`installer/rubai.iss` → `ModelUrl`).
    static const wchar_t* kUrl;

    // Aniq hajm — koʻrsatkich uchun va yuklangandan keyin tekshirish uchun.
    // Yarim yuklangan fayl «model» boʻlib qolmasligi kerak.
    static constexpr long long kHajm = 823369796LL;
    // Tarqatilayotgan q8_0 — macOS'dagi `ModelTanlov.joriy.sha256` bilan bir xil.
    static constexpr const char* kSha256 =
        "1b02df434902015e1464611a7748927e42fcb55c49791c85239cf713c8edc1a3";

    ModelYuklovchi();
    ~ModelYuklovchi();
    ModelYuklovchi(const ModelYuklovchi&) = delete;
    ModelYuklovchi& operator=(const ModelYuklovchi&) = delete;

    // Fon oqimida yuklaydi. Qayta chaqiruvlar ISHCHI OQIMDAN keladi.
    void boshla(std::function<void(long long, long long)> jarayon,
                std::function<void(bool, std::wstring)> tugadi);

    // Bekor qilinganda yarim fayl SAQLANIB QOLADI — keyingi urinishda
    // oʻsha joydan davom etadi.
    void bekorQil();
    bool ketyaptimi() const;

private:
    struct Ichki;
    std::shared_ptr<Ichki> ichki_;
};

}  // namespace rubai
