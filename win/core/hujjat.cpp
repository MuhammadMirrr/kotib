// Transkriptlarni (Studiya hujjatlarini) diskda saqlash.
// Interfeys va izohlar — `hujjat.h`.

#include "hujjat.h"

#include "json.h"
#include "util.h"

#include <windows.h>
#include <objbase.h>  // CoCreateGuid

#include <algorithm>
#include <ctime>
#include <fstream>

namespace rubai {

namespace {

// Yoʻlga aylanishi mumkin boʻlgan qiymatlarni tekshiradi.
//
// Bu himoya emas, balki KAFOLAT: `id` va `amal` bizning kodimizdan keladi,
// lekin ular yoʻlga qoʻshiladi va bir kun tashqi manbadan kelib qolsa
// («../../Windows/System32» kabi) — natija fojia boʻlardi. macOS tomonida
// ham ayni shu tekshiruv bor.
bool xavfsizNom(const std::wstring& s) {
    if (s.empty() || s.size() > 64) return false;
    for (wchar_t c : s) {
        const bool harf = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
        const bool raqam = (c >= L'0' && c <= L'9');
        if (!harf && !raqam && c != L'-' && c != L'_') return false;
    }
    return true;
}

// Tasodifiy UUID — macOS'dagi `UUID().uuidString` shakli.
std::wstring yangiId() {
    GUID g;
    if (CoCreateGuid(&g) != S_OK) return {};
    wchar_t bufer[40];
    swprintf(bufer, 40, L"%08lX-%04X-%04X-%04X-%02X%02X%02X%02X%02X%02X", g.Data1, g.Data2, g.Data3,
             static_cast<unsigned>((g.Data4[0] << 8) | g.Data4[1]), g.Data4[2], g.Data4[3],
             g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return bufer;
}

// Faylni atomik yozadi: avval vaqtinchalik faylga, keyin almashtiradi.
// Yozish paytida ilova yopilsa, eski fayl butun qoladi.
bool faylYoz(const std::wstring& yol, const std::string& malumot) {
    const std::wstring vaqtinchalik = yol + L".tmp";
    {
        std::ofstream f(vaqtinchalik.c_str(), std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(malumot.data(), static_cast<std::streamsize>(malumot.size()));
        if (!f) return false;
    }
    return MoveFileExW(vaqtinchalik.c_str(), yol.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

std::string faylOqi(const std::wstring& yol) {
    std::ifstream f(yol.c_str(), std::ios::binary);
    if (!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Papkani ichidagilari bilan oʻchiradi.
void papkaniOchir(const std::wstring& yol) {
    WIN32_FIND_DATAW topilgan{};
    HANDLE h = FindFirstFileW((yol + L"\\*").c_str(), &topilgan);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring nom = topilgan.cFileName;
            if (nom == L"." || nom == L"..") continue;
            const std::wstring toliq = yol + L"\\" + nom;
            if (topilgan.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                papkaniOchir(toliq);
            } else {
                DeleteFileW(toliq.c_str());
            }
        } while (FindNextFileW(h, &topilgan));
        FindClose(h);
    }
    RemoveDirectoryW(yol.c_str());
}

std::wstring faylNomi(const std::wstring& yol) {
    const size_t slash = yol.find_last_of(L"\\/");
    return slash == std::wstring::npos ? yol : yol.substr(slash + 1);
}

}  // namespace

namespace HujjatOmbori {

std::wstring ildiz() {
    const std::wstring d = appDataDir() + L"\\hujjatlar";
    ensureDir(d);
    return d;
}

std::wstring papka(const std::wstring& id) { return ildiz() + L"\\" + id; }

std::wstring matnFayl(const std::wstring& id, MatnTuri tur) {
    return papka(id) + (tur == MatnTuri::Xom ? L"\\xom.txt" : L"\\matn.txt");
}

bool matnSaqla(const std::wstring& id, MatnTuri tur, const std::wstring& matn) {
    if (!xavfsizNom(id)) return false;
    return faylYoz(matnFayl(id, tur), toUtf8(matn));
}

std::wstring matnOqi(const std::wstring& id, MatnTuri tur) {
    if (!xavfsizNom(id)) return {};
    return toWide(faylOqi(matnFayl(id, tur)));
}

bool natijaSaqla(const std::wstring& id, const std::wstring& amal, const std::wstring& matn) {
    if (!xavfsizNom(id) || !xavfsizNom(amal)) return false;
    const std::wstring dir = papka(id) + L"\\natijalar";
    ensureDir(dir);
    return faylYoz(dir + L"\\" + amal + L".md", toUtf8(matn));
}

std::wstring natijaOqi(const std::wstring& id, const std::wstring& amal) {
    if (!xavfsizNom(id) || !xavfsizNom(amal)) return {};
    return toWide(faylOqi(papka(id) + L"\\natijalar\\" + amal + L".md"));
}

Hujjat saqla(const std::wstring& manbaYol, double davomiylik,
             const std::vector<Segment>& segmentlar, Apostrof apostrof) {
    Hujjat h;
    h.id = yangiId();
    if (h.id.empty()) return {};

    h.manbaNomi = faylNomi(manbaYol);
    h.manbaYol = manbaYol;
    h.davomiylik = davomiylik;
    h.yaratilgan = static_cast<long long>(std::time(nullptr));

    const std::wstring dir = papka(h.id);
    if (!ensureDir(dir) || !ensureDir(dir + L"\\natijalar")) {
        logWrite(L"XATO: hujjat papkasi yaratilmadi");
        return {};
    }

    bool yaxshi = true;

    // Segmentlar — keyinchalik qayta formatlash yoki SRT uchun.
    {
        JsonYozuvchi y;
        y.royxatBoshla();
        for (const auto& s : segmentlar) {
            y.obyektBoshla();
            y.kalit("t0");
            y.son(s.t0);
            y.kalit("t1");
            y.son(s.t1);
            y.kalit("matn");
            y.satr(s.matn);
            y.obyektTugat();
        }
        y.royxatTugat();
        yaxshi = faylYoz(dir + L"\\segmentlar.json", y.matn()) && yaxshi;
    }

    yaxshi = matnSaqla(h.id, MatnTuri::Xom, xomMatn(segmentlar)) && yaxshi;
    yaxshi = matnSaqla(h.id, MatnTuri::Chiroyli, chiroyliMatn(segmentlar, apostrof)) && yaxshi;

    // Maʼlumot OXIRIDA — bu «hujjat toʻliq» degan belgi.
    if (yaxshi) {
        JsonYozuvchi y;
        y.obyektBoshla();
        y.kalit("id");
        y.satr(h.id);
        y.kalit("manbaNomi");
        y.satr(h.manbaNomi);
        y.kalit("manbaYol");
        y.satr(h.manbaYol);
        y.kalit("davomiylik");
        y.son(h.davomiylik);
        y.kalit("yaratilgan");
        y.butun(h.yaratilgan);
        y.obyektTugat();
        yaxshi = faylYoz(dir + L"\\hujjat.json", y.matn());
    }

    if (!yaxshi) {
        // Qisman yozilgan hujjat roʻyxatda «buzuq» boʻlib turmasin.
        papkaniOchir(dir);
        logWrite(L"XATO: hujjat saqlanmadi, papka olib tashlandi");
        return {};
    }
    return h;
}

std::vector<Segment> segmentlar(const std::wstring& id) {
    std::vector<Segment> natija;
    if (!xavfsizNom(id)) return natija;

    const Json j = Json::ajrat(faylOqi(papka(id) + L"\\segmentlar.json"));
    for (size_t i = 0; i < j.hajmi(); ++i) {
        Segment s;
        s.t0 = j[i]["t0"].son();
        s.t1 = j[i]["t1"].son();
        s.matn = j[i]["matn"].satr();
        natija.push_back(std::move(s));
    }
    return natija;
}

std::vector<Hujjat> royxat() {
    std::vector<Hujjat> natija;

    WIN32_FIND_DATAW topilgan{};
    HANDLE h = FindFirstFileW((ildiz() + L"\\*").c_str(), &topilgan);
    if (h == INVALID_HANDLE_VALUE) return natija;

    do {
        if (!(topilgan.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const std::wstring nom = topilgan.cFileName;
        if (nom == L"." || nom == L"..") continue;

        // `hujjat.json` yoʻq boʻlsa — hujjat yarim saqlangan, tashlab ketamiz.
        const Json j = Json::ajrat(faylOqi(papka(nom) + L"\\hujjat.json"));
        if (!j.bormi()) continue;

        Hujjat d;
        d.id = j["id"].satr();
        d.manbaNomi = j["manbaNomi"].satr();
        d.manbaYol = j["manbaYol"].satr();
        d.davomiylik = j["davomiylik"].son();
        d.yaratilgan = static_cast<long long>(j["yaratilgan"].son());
        if (d.id.empty()) continue;
        natija.push_back(std::move(d));
    } while (FindNextFileW(h, &topilgan));
    FindClose(h);

    std::sort(natija.begin(), natija.end(),
              [](const Hujjat& a, const Hujjat& b) { return a.yaratilgan > b.yaratilgan; });
    return natija;
}

void ochir(const std::wstring& id) {
    if (!xavfsizNom(id)) return;
    papkaniOchir(papka(id));
}

}  // namespace HujjatOmbori
}  // namespace rubai
