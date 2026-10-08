// Diktovka tarixi — oxirgi 200 ta yozuv, JSON faylda.
// Interfeys va izohlar — `tarix.h`.

#include "tarix.h"

#include "json.h"
#include "util.h"

#include <windows.h>
#include <objbase.h>  // CoCreateGuid

#include <ctime>
#include <fstream>

namespace rubai {

namespace {

std::wstring yangiId() {
    GUID g;
    if (CoCreateGuid(&g) != S_OK) return {};
    wchar_t bufer[40];
    swprintf(bufer, 40, L"%08lX-%04X-%04X-%04X-%02X%02X%02X%02X%02X%02X", g.Data1, g.Data2, g.Data3,
             static_cast<unsigned>((g.Data4[0] << 8) | g.Data4[1]), g.Data4[2], g.Data4[3],
             g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return bufer;
}

std::string faylOqi(const std::wstring& yol) {
    std::ifstream f(yol.c_str(), std::ios::binary);
    if (!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

DiktovkaTarixi::DiktovkaTarixi(std::wstring fayl) : fayl_(std::move(fayl)) {}

DiktovkaTarixi& DiktovkaTarixi::birgalik() {
    static DiktovkaTarixi t(appDataDir() + L"\\diktovka-tarixi.json");
    return t;
}

std::vector<DiktovkaYozuvi> DiktovkaTarixi::oqi() const {
    std::vector<DiktovkaYozuvi> natija;
    const Json j = Json::ajrat(faylOqi(fayl_));
    for (size_t i = 0; i < j.hajmi(); ++i) {
        DiktovkaYozuvi y;
        y.id = j[i]["id"].satr();
        y.matn = j[i]["matn"].satr();
        y.sana = static_cast<long long>(j[i]["sana"].son());
        y.davomiylik = j[i]["davomiylik"].son();
        if (y.id.empty()) continue;
        natija.push_back(std::move(y));
    }
    return natija;
}

bool DiktovkaTarixi::qoshish(const std::wstring& matn, double davomiylik, long long sana) {
    DiktovkaYozuvi y;
    y.id = yangiId();
    y.matn = matn;
    y.sana = sana > 0 ? sana : static_cast<long long>(std::time(nullptr));
    y.davomiylik = davomiylik;
    if (y.id.empty()) return false;

    std::vector<DiktovkaYozuvi> royxat = oqi();
    royxat.insert(royxat.begin(), std::move(y));
    if (royxat.size() > kChegara) royxat.resize(kChegara);
    return yoz(royxat);
}

bool DiktovkaTarixi::ochir(const std::wstring& id) {
    std::vector<DiktovkaYozuvi> royxat = oqi();
    for (auto it = royxat.begin(); it != royxat.end(); ++it) {
        if (it->id == id) {
            royxat.erase(it);
            return yoz(royxat);
        }
    }
    return false;
}

bool DiktovkaTarixi::tozala() { return yoz({}); }

bool DiktovkaTarixi::yoz(const std::vector<DiktovkaYozuvi>& royxat) const {
    JsonYozuvchi y;
    y.royxatBoshla();
    for (const auto& r : royxat) {
        y.obyektBoshla();
        y.kalit("id");
        y.satr(r.id);
        y.kalit("matn");
        y.satr(r.matn);
        y.kalit("sana");
        y.butun(r.sana);
        y.kalit("davomiylik");
        y.son(r.davomiylik);
        y.obyektTugat();
    }
    y.royxatTugat();

    // Atomik yozish — uzilib qolsa yarim fayl qolmaydi.
    const std::wstring vaqtinchalik = fayl_ + L".tmp";
    {
        std::ofstream f(vaqtinchalik.c_str(), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(y.matn().data(), static_cast<std::streamsize>(y.matn().size()));
        if (!f) return false;
    }
    return MoveFileExW(vaqtinchalik.c_str(), fayl_.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}  // namespace rubai
