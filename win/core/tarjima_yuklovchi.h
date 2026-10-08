// Tarjima modelini yuklab olish va ochish.
//
// macOS'dagi `src/tarjima_yuklovchi.swift` ning ekvivalenti.
//
// Model 3,4 GB va ilova ichida kelmaydi. Yuklash `.part` fayl va HTTP Range
// bilan davom ettiriladi — sekin yoki uzilib turadigan internetda bu shart.
//
// Ochish tartibi MUHIM: arxiv avval vaqtinchalik papkaga ochiladi va faqat
// `TarjimaModel::tayyormi` toʻgʻri deganda asl joyiga koʻchiriladi. Aks holda
// uzilib qolgan ochish yarim papka qoldiradi va ilova uni «tayyor» deb
// oʻqiydi — CTranslate2 esa tushunarsiz xato bilan quladi.
//
// Butunlik tekshiruvi: `.tar.gz` ning oʻzida gzip CRC32 bor, shuning uchun
// buzilgan yoki yarim fayl `tar` da noldan boshqa kod bilan tugaydi. Alohida
// SHA-256 fayli qoʻshilmaydi — u yana bitta CDN obyekti va yana bitta kesh
// tuzogʻi boʻlardi.
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace rubai {

class TarjimaYuklovchi {
public:
    // CDN'dagi arxiv. Prefiks HECH QACHON qayta ishlatilmaydi: obyektlarda
    // `Cache-Control: immutable` va Cloudflare eski nusxani bir yil ushlaydi.
    static const wchar_t* kUrl;

    TarjimaYuklovchi();
    ~TarjimaYuklovchi();
    TarjimaYuklovchi(const TarjimaYuklovchi&) = delete;
    TarjimaYuklovchi& operator=(const TarjimaYuklovchi&) = delete;

    // Fon oqimida yuklab oladi va ochadi.
    // MUHIM: qayta chaqiruvlar ISHCHI OQIMDAN keladi — UI'ga `PostMessage`
    // bilan uzating.
    //
    // `jarayon(olingan, jami)` — baytlarda; `jami` 0 boʻlsa nomaʼlum.
    // `tugadi(ok, xato)` — `ok` false boʻlsa `xato` oʻzbekcha xabar.
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
