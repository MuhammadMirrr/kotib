// Avto-yangilanish (Windows). Tavsif va qoidalar — `yangilovchi.h`.
#include "yangilovchi.h"

#include "config.h"
#include "kotib_versiya.h"
#include "util.h"
#include "yuklovchi.h"

#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <ctime>
#include <thread>
#include <vector>

namespace rubai {

namespace {

// Fon oqimidan keladigan natijalar (`xabar` ning wp qiymati).
enum Hodisa : WPARAM {
    kTekshiruvTugadi = 1,  // lp: TekshiruvNatijasi*
    kJarayon = 2,          // lp: Jarayon*
    kYuklashTugadi = 3,    // lp: YuklashNatijasi*
};

struct TekshiruvNatijasi {
    DWORD holat = 0;  // HTTP status; 0 — tarmoq xatosi
    std::string tana;
};

struct Jarayon {
    long long olingan = 0, jami = 0;
};

struct YuklashTugadi {
    bool ok = false;
    std::wstring xato;
    std::wstring yol;
    HANDLE qulf = INVALID_HANDLE_VALUE;
};

constexpr wchar_t kHost[] = L"stat.mirqobilov.com";
// Faqat bitta soat qadami — yangilanishning oʻzi 24 soatlik qoidada
// (`uygonishdaTekshirish`). Uzoq `SetTimer` uyqu va soat oʻzgarishida
// ishonchsiz, soatlik tekshiruv esa arzon (tarmoqqa chiqmaydi).
constexpr UINT kSoatMs = 3600 * 1000;
constexpr UINT kBoshlashMs = 60 * 1000;
constexpr UINT kBoshPaytMs = 30 * 1000;
constexpr UINT kMajburiyBoshlashMs = 10 * 1000;
// Yangilanish oʻrnatuvchisi ~30–45 MB. Manifestdagi hajm bundan katta
// boʻlsa — xato (xotiraga oʻqiladi va imzo tekshiriladi).
constexpr long long kMaksHajm = 512LL * 1024 * 1024;

long long hozirUnix() { return static_cast<long long>(std::time(nullptr)); }
long long monoton() { return static_cast<long long>(GetTickCount64() / 1000); }

std::string joriyVersiya() { return KOTIB_VER_STR; }

// Kompyuterning OʻZ arxitekturasi. x64 Kotib ARM64 Windows'da emulyatsiyada
// ishlashi mumkin — yangilanish baribir mashinaga mosini olsin.
std::string arxitektura() {
    using Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    USHORT jarayon = 0, mashina = 0;
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2")));
    if (fn && fn(GetCurrentProcess(), &jarayon, &mashina)) {
        if (mashina == IMAGE_FILE_MACHINE_ARM64) return "arm64";
        if (mashina == IMAGE_FILE_MACHINE_AMD64) return "x64";
    }
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "x64";
#endif
}

bool tasodifiy(void* bufer, ULONG n) {
    return BCRYPT_SUCCESS(
        BCryptGenRandom(nullptr, static_cast<PUCHAR>(bufer), n, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

// Bosqichli tarqatish chelagi (0–99): bir marta tanlanadi va saqlanadi.
// Serverga hech qachon yuborilmaydi.
int chelak() {
    Settings s = loadSettings();
    if (s.yangilanishChelak >= 0 && s.yangilanishChelak <= 99) return s.yangilanishChelak;
    uint32_t r = 0;
    if (!tasodifiy(&r, sizeof r)) r = static_cast<uint32_t>(GetTickCount64());
    s.yangilanishChelak = static_cast<int>(r % 100);
    saveSettings(s);
    return s.yangilanishChelak;
}

std::wstring sanaVaqt(long long unix) {
    const std::time_t t = static_cast<std::time_t>(unix);
    std::tm m{};
    if (localtime_s(&m, &t) != 0) return L"?";
    wchar_t b[32];
    swprintf(b, 32, L"%02d.%02d.%04d %02d:%02d", m.tm_mday, m.tm_mon + 1, m.tm_year + 1900,
             m.tm_hour, m.tm_min);
    return b;
}

// Manifestni oladi. HTTP holatini ham qaytaradi: 404 — «hali reliz yoʻq»
// (muvaffaqiyatli javob), 5xx va tarmoq xatosi — qayta urinish.
TekshiruvNatijasi manifestOl(const std::wstring& yol) {
    TekshiruvNatijasi n;
    HINTERNET sessiya = WinHttpOpen(L"Kotib", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sessiya) return n;
    WinHttpSetTimeouts(sessiya, 10000, 10000, 10000, 15000);
    HINTERNET ulanish = WinHttpConnect(sessiya, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET sorov =
        ulanish ? WinHttpOpenRequest(ulanish, L"GET", yol.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                : nullptr;
    if (sorov && WinHttpSendRequest(sorov, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(sorov, nullptr)) {
        DWORD holat = 0, uz = sizeof holat;
        WinHttpQueryHeaders(sorov, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &holat, &uz, WINHTTP_NO_HEADER_INDEX);
        bool toliq = true;
        DWORD bor = 0;
        while (WinHttpQueryDataAvailable(sorov, &bor) && bor > 0) {
            // Manifest bir necha KB. 64 KB dan oshsa — bu bizning javob emas.
            if (n.tana.size() + bor > 64 * 1024) {
                toliq = false;
                break;
            }
            std::string bolak(bor, '\0');
            DWORD oqildi = 0;
            if (!WinHttpReadData(sorov, bolak.data(), bor, &oqildi)) {
                toliq = false;
                break;
            }
            n.tana.append(bolak, 0, oqildi);
        }
        n.holat = toliq ? holat : 0;
    }
    if (sorov) WinHttpCloseHandle(sorov);
    if (ulanish) WinHttpCloseHandle(ulanish);
    WinHttpCloseHandle(sessiya);
    return n;
}

// Tekshiruvdan oʻtgan faylni oʻqish uchun ochadi va YOZISH/OʻCHIRISHGA
// qulflaydi (FILE_SHARE_READ xolos). Xatoda INVALID_HANDLE_VALUE.
HANDLE qulflabOch(const std::wstring& yol) {
    return CreateFileW(yol.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

// Yuklangan `.part` → tasodifiy nomli `.exe`, qulflab ochiladi va hajm →
// sha256 → Ed25519 tekshiriladi. Fon oqimida.
YuklashTugadi tekshirVaQulfla(const std::wstring& qism, const std::wstring& papka,
                              const std::string& versiya, const YangilanishFayli& f,
                              const std::array<uint8_t, 32>& kalit) {
    YuklashTugadi r;
    uint8_t t[4] = {};
    tasodifiy(t, sizeof t);
    wchar_t q[16];
    swprintf(q, 16, L"%02x%02x%02x%02x", t[0], t[1], t[2], t[3]);
    r.yol = papka + L"\\Kotib-" + toWide(versiya) + L"-yangilash-" + q + L".exe";
    if (!MoveFileExW(qism.c_str(), r.yol.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        r.xato = L"fayl nomi oʻzgarmadi (" + std::to_wstring(GetLastError()) + L")";
        return r;
    }
    HANDLE h = qulflabOch(r.yol);
    if (h == INVALID_HANDLE_VALUE) {
        r.xato = L"fayl ochilmadi (" + std::to_wstring(GetLastError()) + L")";
        DeleteFileW(r.yol.c_str());
        return r;
    }
    LARGE_INTEGER hajm{};
    std::vector<uint8_t> bayt;
    bool oqildi = GetFileSizeEx(h, &hajm) && hajm.QuadPart == f.hajm;
    if (oqildi) {
        bayt.resize(static_cast<size_t>(hajm.QuadPart));
        size_t jami = 0;
        while (oqildi && jami < bayt.size()) {
            DWORD n = 0;
            const DWORD soʻr = static_cast<DWORD>(std::min<size_t>(bayt.size() - jami, 8u << 20));
            oqildi = ReadFile(h, bayt.data() + jami, soʻr, &n, nullptr) && n > 0;
            jami += n;
        }
    }
    const FaylXatosi x =
        !oqildi ? FaylXatosi::Hajm
                : faylniTekshir(bayt.data(), bayt.size(), baytlarSha256(bayt.data(), bayt.size()),
                                f, kalit.data());
    if (x != FaylXatosi::Yoq) {
        CloseHandle(h);
        DeleteFileW(r.yol.c_str());
        r.xato = x == FaylXatosi::Hajm     ? L"hajm mos emas"
                 : x == FaylXatosi::Sha256 ? L"sha256 mos emas"
                                           : L"imzo notoʻgʻri";
        return r;
    }
    r.ok = true;
    r.qulf = h;
    return r;
}

}  // namespace

Yangilovchi::~Yangilovchi() {
    if (tayyor_ && tayyor_->qulf != INVALID_HANDLE_VALUE) CloseHandle(tayyor_->qulf);
}

std::wstring Yangilovchi::papka() const {
    const std::wstring p = localAppDataDir() + L"\\yangilanish";
    ensureDir(p);
    return p;
}

// ---- Ishga tushish -------------------------------------------------------------

void Yangilovchi::boshla(HWND oyna, UINT xabarId) {
    oyna_ = oyna;
    xabarId_ = xabarId;
    yangilanganBolsaXabarBer();
    eskiOrnatuvchilarniOchir();

    // Ochiq kalit — kompilyatsiya vaqtida `scripts/yangilanish-kaliti.pub`
    // dan (`kotib_versiya.h`). Yaroqsiz boʻlsa — yangilovchi oʻchiq.
    std::vector<uint8_t> k;
    if (!base64Och(KOTIB_YANGILANISH_KALITI, k) || k.size() != 32) {
        logWrite(L"yangilanish: ochiq kalit yaroqsiz — avto-yangilanish oʻchiq");
        return;
    }
    std::copy(k.begin(), k.end(), kalit_.begin());
    faol_ = true;

    // Birinchi tekshiruv — 60 s keyin (tarmoq tayyor boʻlsin). Majburiy talab
    // diskda turgan boʻlsa — tezroq: spec «darhol tekshiruv va yuklab olish».
    const bool majburiy = majburiyHolat() != Qaror::Hech;
    SetTimer(oyna_, kYangilovchiTekshiruvTaymer, majburiy ? kMajburiyBoshlashMs : kBoshlashMs,
             nullptr);
    if (majburiy) {
        logWrite(L"yangilanish: majburiy talab diskda — Kotib " + loadSettings().majburiyMin);
        if (ozgardi) ozgardi();
    }
}

void Yangilovchi::toxtat() {
    toxta_->store(true);
    if (!oyna_) return;
    KillTimer(oyna_, kYangilovchiTekshiruvTaymer);
    KillTimer(oyna_, kYangilovchiQaytaTaymer);
    KillTimer(oyna_, kYangilovchiBoshPaytTaymer);
}

// Oldingi ishga tushishdan beri versiya oʻzgargan va u aynan biz kutgan
// yangilanish boʻlsa — xabar (macOS'dagi `yangilanganBolsaXabarBer`).
void Yangilovchi::yangilanganBolsaXabarBer() {
    Settings s = loadSettings();
    const std::wstring joriy = toWide(joriyVersiya());
    const std::wstring oldingi = s.oxirgiIshlaganVersiya;
    const std::wstring kutilgan = s.kutilganVersiya;
    const std::wstring izoh = s.kutilganIzoh;
    if (oldingi == joriy) return;
    s.oxirgiIshlaganVersiya = joriy;
    if (kutilgan == joriy) {
        s.kutilganVersiya.clear();
        s.kutilganIzoh.clear();
    }
    saveSettings(s);
    if (oldingi.empty() || kutilgan != joriy) return;

    logWrite(L"yangilanish: " + oldingi + L" → " + joriy);
    const std::wstring matn = L"Kotib " + joriy + L" ga yangilandi";
    if (bildir) bildir(L"Kotib", matn + (izoh.empty() ? L"" : L"\n" + izoh));
    if (yangilandi) {
        yangilandi(izoh.empty() ? matn : matn + L" — " + izoh, L"yangilandi-" + joriy);
    }
}

// Oʻrnatib boʻlingan yoki eskirgan oʻrnatuvchilar. Faqat hali kutilayotgan
// versiyaniki qoladi: kompyuter oʻrnatishdan oldin oʻchirilgan boʻlsa, u
// manifest kelgach yuklashsiz qayta tekshiriladi (`yukla`) — sekin internetda
// 30–45 MB ni qaytadan tortmaslik uchun. `.part` lar ham qoladi (davom
// ettirish). Band fayl (oʻrnatuvchi hali yopilmagan) oʻchmaydi — keyingi safar.
void Yangilovchi::eskiOrnatuvchilarniOchir() {
    const std::wstring p = papka();
    const std::wstring kutilgan = loadSettings().kutilganVersiya;
    const std::wstring saqla = L"Kotib-" + kutilgan + L"-yangilash-";
    WIN32_FIND_DATAW f{};
    HANDLE h = FindFirstFileW((p + L"\\*.exe").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring nom = f.cFileName;
        if (!kutilgan.empty() && nom.rfind(saqla, 0) == 0) continue;
        DeleteFileW((p + L"\\" + nom).c_str());
    } while (FindNextFileW(h, &f));
    FindClose(h);
}

// ---- Tekshiruv -------------------------------------------------------------------

bool Yangilovchi::taymer(UINT_PTR id) {
    switch (id) {
        case kYangilovchiTekshiruvTaymer:
            // Birinchisi 60 s, keyingilari — har soat.
            SetTimer(oyna_, kYangilovchiTekshiruvTaymer, kSoatMs, nullptr);
            kerakBolsaTekshir(L"jadval");
            // Majburiy bannerdagi «N soatdan keyin» va muhlat tugashi.
            if (majburiyHolat() != Qaror::Hech && ozgardi) ozgardi();
            return true;
        case kYangilovchiQaytaTaymer:
            KillTimer(oyna_, kYangilovchiQaytaTaymer);
            qaytaKutilyapti_ = false;
            if (!tekshiruvKetyapti_ && !yuklashKetyapti_) tekshir(L"qayta urinish");
            return true;
        case kYangilovchiBoshPaytTaymer: boshPaytniTekshir(); return true;
    }
    return false;
}

void Yangilovchi::uygondi() { kerakBolsaTekshir(L"uygʻonish"); }

void Yangilovchi::kerakBolsaTekshir(const wchar_t* sabab) {
    // Qayta urinish taymeri kutayotgan boʻlsa — oʻsha jadval ishlaydi.
    if (!faol_ || tekshiruvKetyapti_ || yuklashKetyapti_ || qaytaKutilyapti_) return;
    // Majburiy talab bor va yangilanish hali tayyor emas — 24 soat kutilmaydi,
    // soatlik taymer har safar urinadi (spec «Majburiy yangilanish» 1).
    const bool majburiy = majburiyHolat() != Qaror::Hech && !tayyor_;
    // 24 soat — oxirgi muvaffaqiyatdan YOKI qayta urinishlar tugaganidan:
    // oflayn kompyuter soatiga emas, kuniga bir marta urinsin (spec «keyin jadval»).
    const long long oxirgi = std::max(loadSettings().yangilanishMuvaffaqiyat, tugaganUrinish_);
    if (!majburiy && !uygonishdaTekshirish(oxirgi > 0 ? oxirgi : -1, hozirUnix())) return;
    tekshir(sabab);
}

void Yangilovchi::hozirTekshir() {
    if (!faol_) {
        if (bildir) bildir(L"Avto-yangilanish oʻchiq", L"Bu nusxada yangilanish kaliti yoʻq.");
        return;
    }
    foydalanuvchiSoradi_ = true;
    if (tekshiruvKetyapti_ || yuklashKetyapti_) return;  // natija baribir aytiladi
    tekshir(L"foydalanuvchi");
}

void Yangilovchi::tekshir(const wchar_t* sabab) {
    tekshiruvKetyapti_ = true;
    logWrite(std::wstring(L"yangilanish: tekshiruv (") + sabab + L")");
    // Sinov kanali (`settings.ini`: yangilanish.kanal=sinov) — oʻsha server,
    // oʻsha kalit, alohida KV yozuvi. Foydalanuvchilarga taʼsir qilmasdan
    // haqiqiy infratuzilmada sinash uchun.
    const std::wstring yol = loadSettings().yangilanishKanali == L"sinov"
                                 ? L"/v1/yangilanish/win-sinov.json"
                                 : L"/v1/yangilanish/win.json";
    HWND oyna = oyna_;
    const UINT id = xabarId_;
    std::thread([oyna, id, yol] {
        auto* n = new TekshiruvNatijasi(manifestOl(yol));
        if (!PostMessageW(oyna, id, kTekshiruvTugadi, reinterpret_cast<LPARAM>(n))) delete n;
    }).detach();
    if (ozgardi) ozgardi();
}

void Yangilovchi::xabar(WPARAM wp, LPARAM lp) {
    switch (wp) {
        case kTekshiruvTugadi: {
            std::unique_ptr<TekshiruvNatijasi> n(reinterpret_cast<TekshiruvNatijasi*>(lp));
            tekshiruvKetyapti_ = false;
            if (n->holat == 404) {
                muvaffaqiyat(std::nullopt);  // server bor, reliz hali yoʻq
            } else if (n->holat == 200) {
                Manifest m;
                if (javobniTekshir(n->tana, kalit_.data(), "win", m)) {
                    muvaffaqiyat(m);
                } else {
                    // Imzo yoki maydon notoʻgʻri — eʼtiborsiz (spec «Xavfsizlik»).
                    xato(L"manifest imzosi yoki tarkibi notoʻgʻri — eʼtiborsiz qoldirildi",
                         Bosqich::Tekshiruv);
                }
            } else if (n->holat == 0) {
                xato(L"tarmoq xatosi", Bosqich::Tekshiruv);
            } else {
                xato(L"server javobi " + std::to_wstring(n->holat), Bosqich::Tekshiruv);
            }
            break;
        }
        case kJarayon: {
            std::unique_ptr<Jarayon> j(reinterpret_cast<Jarayon*>(lp));
            olingan_ = j->olingan;
            jami_ = j->jami;
            if (ozgardi) ozgardi();
            break;
        }
        case kYuklashTugadi: {
            std::unique_ptr<YuklashTugadi> r(reinterpret_cast<YuklashTugadi*>(lp));
            yuklashKetyapti_ = false;
            if (!r->ok) {
                xato(L"yuklash: " + r->xato, Bosqich::Yuklash);
                break;
            }
            if (toxta_->load()) {
                CloseHandle(r->qulf);
                break;
            }
            tayyorniBekorQil(nullptr);
            tayyor_ = Tayyor{yuklanayotgan_, yuklanayotganIzoh_,     r->yol,
                             r->qulf,        yuklanayotganMajburiy_, monoton()};
            // Qayta ochilgach «Kotib X ga yangilandi» uchun.
            Settings s = loadSettings();
            s.kutilganVersiya = toWide(tayyor_->versiya);
            s.kutilganIzoh = toWide(tayyor_->izoh);
            saveSettings(s);
            logWrite(L"yangilanish: " + toWide(tayyor_->versiya) +
                     L" yuklandi va tekshirildi — boʻsh paytda oʻrnatiladi");
            if (foydalanuvchiSoradi_ && bildir) {
                bildir(L"Kotib " + toWide(tayyor_->versiya) + L" tayyor",
                       L"Boʻsh paytda oʻrnatiladi va Kotib qayta ochiladi.");
            }
            foydalanuvchiSoradi_ = false;
            SetTimer(oyna_, kYangilovchiBoshPaytTaymer, kBoshPaytMs, nullptr);
            if (ozgardi) ozgardi();
            break;
        }
    }
}

void Yangilovchi::muvaffaqiyat(const std::optional<Manifest>& m) {
    const long long hozir = hozirUnix();
    // `chelak()` sozlamalarga oʻzi yozadi — `s` undan KEYIN oʻqilsin, aks
    // holda pastdagi `saveSettings(s)` yangi chelakni oʻchirib yuborardi.
    const int ch = chelak();
    Settings s = loadSettings();
    s.yangilanishMuvaffaqiyat = hozir;
    tekshiruvXato_ = 0;
    oxirgiXato_.clear();
    KillTimer(oyna_, kYangilovchiQaytaTaymer);
    qaytaKutilyapti_ = false;

    Qaror q = Qaror::Hech;
    if (m) {
        const std::string joriy = joriyVersiya();
        // Majburiy muhlat siyosat BIRINCHI marta olingan paytdan sanaladi va
        // diskda saqlanadi (spec «Majburiy yangilanish»). Talab yoʻqolsa — tozalanadi.
        const bool majburiyTalab =
            !m->minVersiya.empty() && versiyaTaqqosla(joriy, m->minVersiya) < 0;
        if (majburiyTalab) {
            // Yangi talab (boshqa min) — muhlat qaytadan sanaladi.
            const std::wstring min = toWide(m->minVersiya);
            if (s.majburiyKorilgan < 0 || s.majburiyMin != min) s.majburiyKorilgan = hozir;
            s.majburiyMin = min;
            s.majburiyMuhlat = m->muhlatSoat;
        } else {
            s.majburiyMin.clear();
            s.majburiyKorilgan = -1;
        }
        q = siyosatQarori(joriy, m->versiya, m->minVersiya, m->muhlatSoat, m->foiz, ch,
                          s.majburiyKorilgan, hozir);
    }
    if (!m) {
        // Reliz yoʻq — majburiy talab ham yoʻq (kill switch: KV tozalangan).
        s.majburiyMin.clear();
        s.majburiyKorilgan = -1;
    }
    saveSettings(s);

    if (q == Qaror::Hech) {
        // Taklif qaytarib olingan boʻlishi mumkin (kill switch) — tayyor
        // turgan yangilanish ham oʻrnatilmaydi.
        tayyorniBekorQil(L"serverda endi taklif qilinmaydi");
        // Oldingi ishga tushishdan qolgan (hali qayta tekshirilmagan) fayl ham.
        Settings k = loadSettings();
        if (!k.kutilganVersiya.empty()) {
            k.kutilganVersiya.clear();
            k.kutilganIzoh.clear();
            saveSettings(k);
        }
        eskiOrnatuvchilarniOchir();
        if (foydalanuvchiSoradi_ && bildir) {
            bildir(L"Siz eng oxirgi versiyadasiz", L"Kotib " + toWide(joriyVersiya()));
        }
        foydalanuvchiSoradi_ = false;
        if (ozgardi) ozgardi();
        return;
    }
    if (q == Qaror::Blokla) {
        // Blok kartasi — S9. Hozircha: darhol oʻrnatishga intilamiz.
        logWrite(L"yangilanish: majburiy muhlat tugagan — " + toWide(m->versiya));
    }

    const bool majburiy = q != Qaror::Yukla;
    if (tayyor_ && tayyor_->versiya == m->versiya) {
        tayyor_->majburiy = majburiy;
        if (foydalanuvchiSoradi_ && bildir) {
            bildir(L"Kotib " + toWide(m->versiya) + L" tayyor",
                   L"Boʻsh paytda oʻrnatiladi va Kotib qayta ochiladi.");
        }
        foydalanuvchiSoradi_ = false;
        if (ozgardi) ozgardi();
        return;
    }
    std::string arx = arxitektura();
    if (!m->fayllar.count(arx)) {
        // Faqat ARM64 kompyuterda x64 faylga tushamiz — u emulyatsiyada ishlaydi.
        // Teskarisi (x64 da ARM64 fayl) ishlamaydi: oʻrnatish yiqilib, har
        // urinishda qayta yuklanardi.
        if (arx == "arm64" && m->fayllar.count("x64")) {
            arx = "x64";
        } else {
            logWrite(L"yangilanish: " + toWide(m->versiya) + L" — bu arxitektura (" + toWide(arx) +
                     L") uchun fayl yoʻq");
            foydalanuvchiSoradi_ = false;
            if (ozgardi) ozgardi();
            return;
        }
    }
    // Tayyor turgan BOSHQA versiya (masalan, qaytarib olingan 1.2.2, endi taklif
    // 1.2.1) — darhol bekor: aks holda yangisi yuklanguncha boʻsh paytda uni
    // oʻrnatib yuborishi mumkin edi.
    if (tayyor_) tayyorniBekorQil(L"serverda endi boshqa versiya taklif qilinmoqda");
    yukla(*m, arx, majburiy);
}

void Yangilovchi::xato(const std::wstring& sabab, Bosqich bosqich) {
    logWrite(L"yangilanish: xato — " + sabab);
    oxirgiXato_ = sabab;
    int& n = bosqich == Bosqich::Yuklash ? yuklashXato_ : tekshiruvXato_;
    const long long k = qaytaUrinishKechikishi(n);
    if (k < 0) {
        // Urinishlar tugadi — odatdagi jadval: keyingisi shu paytdan 24 soat keyin.
        n = 0;
        tugaganUrinish_ = hozirUnix();
        KillTimer(oyna_, kYangilovchiQaytaTaymer);
        qaytaKutilyapti_ = false;
        logWrite(L"yangilanish: qayta urinishlar tugadi — keyingisi 24 soatdan keyin");
    } else {
        ++n;
        SetTimer(oyna_, kYangilovchiQaytaTaymer, static_cast<UINT>(k * 1000), nullptr);
        qaytaKutilyapti_ = true;
    }
    if (foydalanuvchiSoradi_ && bildir) {
        bildir(L"Yangilanishni tekshirib boʻlmadi",
               L"Internet ulanishini tekshirib, keyinroq qayta urinib koʻring.");
    }
    foydalanuvchiSoradi_ = false;
    if (ozgardi) ozgardi();
}

// ---- Yuklash ---------------------------------------------------------------------

void Yangilovchi::yukla(const Manifest& m, const std::string& arx, bool majburiy) {
    const YangilanishFayli& f = m.fayllar.at(arx);
    // Yuklash xatolari hisobi versiyaga bogʻliq: yangi versiya — yangi jadval.
    if (m.versiya != xatoVersiya_) {
        xatoVersiya_ = m.versiya;
        yuklashXato_ = 0;
    }
    if (f.hajm > kMaksHajm) {
        xato(L"fayl juda katta (" + std::to_wstring(f.hajm) + L" bayt)", Bosqich::Yuklash);
        return;
    }
    const std::wstring p = papka();
    const std::wstring qism = p + L"\\Kotib-" + toWide(m.versiya) + L"-" + toWide(arx) + L".part";

    // Boshqa versiyaning chala yuklanishi — endi kerak emas.
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((p + L"\\*.part").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring y = p + L"\\" + fd.cFileName;
            if (y != qism) DeleteFileW(y.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    yuklashKetyapti_ = true;
    yuklanayotgan_ = m.versiya;
    yuklanayotganMajburiy_ = majburiy;
    yuklanayotganIzoh_ = qisqaIzoh(m.izoh);
    olingan_ = 0;
    jami_ = f.hajm;
    logWrite(L"yangilanish: " + toWide(m.versiya) + L" (" + toWide(arx) + L", " +
             std::to_wstring(f.hajm) + L" bayt) yuklanmoqda");
    if (ozgardi) ozgardi();

    HWND oyna = oyna_;
    const UINT id = xabarId_;
    auto toxta = toxta_;
    const std::array<uint8_t, 32> kalit = kalit_;
    const std::string versiya = m.versiya;
    std::thread([oyna, id, toxta, kalit, versiya, f, qism, p] {
        auto yubor = [&](YuklashTugadi* r) {
            if (!PostMessageW(oyna, id, kYuklashTugadi, reinterpret_cast<LPARAM>(r))) {
                if (r->qulf != INVALID_HANDLE_VALUE) CloseHandle(r->qulf);
                delete r;
            }
        };
        // Oldingi ishga tushishda yuklangan, lekin oʻrnatilmagan fayl — qayta
        // tekshiriladi; oʻtmasa oʻchadi va odatdagi yuklash boshlanadi.
        WIN32_FIND_DATAW fd{};
        HANDLE t =
            FindFirstFileW((p + L"\\Kotib-" + toWide(versiya) + L"-yangilash-*.exe").c_str(), &fd);
        if (t != INVALID_HANDLE_VALUE) {
            const std::wstring mavjud = p + L"\\" + fd.cFileName;
            FindClose(t);
            auto* r = new YuklashTugadi(tekshirVaQulfla(mavjud, p, versiya, f, kalit));
            if (r->ok) {
                yubor(r);
                return;
            }
            logWrite(L"yangilanish: oldin yuklangan fayl yaroqsiz (" + r->xato +
                     L") — qayta yuklanadi");
            delete r;
        }

        int oxirgiFoiz = -1;
        const YuklashNatijasi y = faylYukla(
            toWide(f.url), qism, f.hajm,
            [&](long long olingan, long long jami) {
                const int foiz = jami > 0 ? static_cast<int>(olingan * 100 / jami) : 0;
                if (foiz == oxirgiFoiz) return;
                oxirgiFoiz = foiz;
                auto* j = new Jarayon{olingan, jami};
                if (!PostMessageW(oyna, id, kJarayon, reinterpret_cast<LPARAM>(j))) delete j;
            },
            [&] { return toxta->load(); });
        auto* r = new YuklashTugadi;
        if (!y.ok) {
            r->xato = y.xato;
            // Disk toʻla yoki fayl buzilgan boʻlishi mumkin: hajmdan oshgan
            // `.part` keyingi safar ham oʻtmaydi — oʻchiramiz. Kichigi qoladi
            // (davom ettirish uchun).
            WIN32_FILE_ATTRIBUTE_DATA a{};
            if (GetFileAttributesExW(qism.c_str(), GetFileExInfoStandard, &a)) {
                const long long n =
                    (static_cast<long long>(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
                if (n > f.hajm) DeleteFileW(qism.c_str());
            }
        } else {
            *r = tekshirVaQulfla(qism, p, versiya, f, kalit);
        }
        yubor(r);
    }).detach();
}

void Yangilovchi::tayyorniBekorQil(const wchar_t* sabab, bool faylniOchir) {
    if (!tayyor_) return;
    if (sabab) logWrite(L"yangilanish: " + toWide(tayyor_->versiya) + L" bekor — " + sabab);
    if (tayyor_->qulf != INVALID_HANDLE_VALUE) CloseHandle(tayyor_->qulf);
    if (faylniOchir) DeleteFileW(tayyor_->yol.c_str());
    tayyor_.reset();
    KillTimer(oyna_, kYangilovchiBoshPaytTaymer);
    // Fayl qolsa `kutilgan` ham qoladi: ishga tushishda u oʻchirilmaydi
    // (`eskiOrnatuvchilarniOchir`) va `yukla` uni qayta tekshirib ishlatadi.
    if (!faylniOchir) return;
    Settings s = loadSettings();
    s.kutilganVersiya.clear();
    s.kutilganIzoh.clear();
    saveSettings(s);
}

// ---- Oʻrnatish -------------------------------------------------------------------

void Yangilovchi::boshPaytniTekshir() {
    if (!tayyor_) {
        KillTimer(oyna_, kYangilovchiBoshPaytTaymer);
        return;
    }
    // Majburiy yangilanish boʻsh paytni kutmaydi — faqat diktovka tugashini.
    const bool majburiy = tayyor_->majburiy || majburiyHolat() != Qaror::Hech;
    const bool mumkin =
        majburiy ? !(yozilyaptimi && yozilyaptimi())
                 : ornatishMumkinmi(bandmi && bandmi(), oxirgiFaollik ? oxirgiFaollik() : -1,
                                    tayyor_->kutishBoshlandi, monoton());
    if (mumkin) ornat();
}

void Yangilovchi::hozirOrnat() {
    if (!tayyor_) {
        hozirTekshir();
        return;
    }
    if (yozilyaptimi && yozilyaptimi()) {
        if (bildir) bildir(L"Diktovka tugagach oʻrnatiladi", L"Yozuv yoʻqolmasligi uchun kutamiz.");
        return;
    }
    ornat();
}

void Yangilovchi::ornat() {
    KillTimer(oyna_, kYangilovchiBoshPaytTaymer);
    const Tayyor t = *tayyor_;
    // `/LOG` — jim oʻrnatish nima uchun muvaffaqiyatsiz boʻlganini bilish uchun.
    // `/KOTIBYANGILASH` — oʻrnatuvchi tugagach Kotib'ni qayta ochadi (S7).
    std::wstring buyruq = L"\"" + t.yol +
                          L"\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /KOTIBYANGILASH /LOG=\"" +
                          papka() + L"\\ornatish.log\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(t.yol.c_str(), buyruq.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &si, &pi)) {
        const DWORD e = GetLastError();
        // Fayl tekshirilgan — oʻchirilmaydi; qayta urinish jadvali (15 daq → 1 soat
        // → 4 soat, keyin kuniga bir) uni qayta yuklamasdan ishlatadi. Sabab
        // antivirus yoki huquq boʻlsa, har boʻsh daqiqada urinib yotmaydi.
        tayyorniBekorQil(nullptr, false);
        xato(L"oʻrnatuvchi ishga tushmadi (" + std::to_wstring(e) + L")", Bosqich::Yuklash);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    // Jarayon yaratildi — fayl endi uning qoʻlida, qulf kerak emas.
    CloseHandle(t.qulf);
    tayyor_->qulf = INVALID_HANDLE_VALUE;
    logWrite(L"yangilanish: " + toWide(t.versiya) +
             L" oʻrnatuvchisi ishga tushdi — Kotib yopiladi va qayta ochiladi");
    toxtat();
    if (chiqish) chiqish();
}

// ---- Majburiy yangilanish (S9) ----------------------------------------------

Qaror Yangilovchi::majburiyHolat() const {
    const Settings s = loadSettings();
    return majburiyQaror(joriyVersiya(), toUtf8(s.majburiyMin), s.majburiyMuhlat,
                         s.majburiyKorilgan, hozirUnix());
}

Yangilovchi::MajburiyKorinish Yangilovchi::majburiyKorinish() const {
    MajburiyKorinish k;
    const Settings s = loadSettings();
    const long long hozir = hozirUnix();
    const Qaror q = majburiyQaror(joriyVersiya(), toUtf8(s.majburiyMin), s.majburiyMuhlat,
                                  s.majburiyKorilgan, hozir);
    if (q == Qaror::Hech) return k;
    k.bor = true;

    std::wstring holat;
    if (tayyor_) {
        holat = L"tayyor, diktovka tugagach oʻrnatiladi";
        k.tugma = L"Hozir oʻrnatish";
    } else if (yuklashKetyapti_) {
        holat = L"yuklanmoqda… " + std::to_wstring(jami_ > 0 ? olingan_ * 100 / jami_ : 0) + L" %";
    } else if (tekshiruvKetyapti_) {
        holat = L"tekshirilmoqda…";
    } else {
        holat = oxirgiXato_.empty() ? std::wstring(L"yuklab olinadi")
                                    : L"yuklab boʻlmadi (" + oxirgiXato_ + L")";
        k.tugma = L"Qayta urinish";
    }

    if (q == Qaror::Blokla) {
        k.matn = L"Yangilanish majburiy — diktovka toʻxtatildi. Kotib " + s.majburiyMin + L": " +
                 holat + L".";
        // «Saytdan yuklab olish» — FAQAT avtomatik yuklash muvaffaqiyatsiz boʻlganda:
        // yuklash ketayotganda u odamni «demak saytdan oʻzim yuklashim kerak» deb
        // oʻylatadi (egasi, 2026-10-08). Xatoda esa boshqa chiqish yoʻli yoʻq.
        k.sayt = !oxirgiXato_.empty() && !tayyor_ && !yuklashKetyapti_ && !tekshiruvKetyapti_;
    } else {
        // Soat orqaga surilgan boʻlsa ham manfiy chiqmasin.
        const long long otgan = std::max(0LL, hozir - s.majburiyKorilgan);
        const long long qoldi = std::max(1LL, (s.majburiyMuhlat * 3600LL - otgan + 3599) / 3600);
        k.matn = L"Muhim yangilanish: Kotib " + s.majburiyMin + L" — " + holat + L". " +
                 std::to_wstring(qoldi) + L" soatdan keyin diktovka toʻxtaydi.";
    }
    return k;
}

void Yangilovchi::majburiyTugma() {
    if (tayyor_) {
        hozirOrnat();
        return;
    }
    if (tekshiruvKetyapti_ || yuklashKetyapti_) return;
    KillTimer(oyna_, kYangilovchiQaytaTaymer);
    qaytaKutilyapti_ = false;
    tekshir(L"majburiy — qayta urinish");
}

// ---- Sozlamalar qatori -------------------------------------------------------

std::wstring Yangilovchi::tayyorVersiya() const {
    return tayyor_ ? toWide(tayyor_->versiya) : std::wstring();
}

Yangilovchi::Korinish Yangilovchi::korinish() const {
    Korinish k;
    const std::wstring joriy = toWide(joriyVersiya());
    if (!faol_) {
        k.matn = L"Kotib " + joriy + L" · avto-yangilanish oʻchiq";
        k.tugma = L"Hozir tekshirish";
        k.tugmaFaol = false;
    } else if (tayyor_) {
        k.matn = L"Kotib " + toWide(tayyor_->versiya) + L" oʻrnatishga tayyor";
        k.tugma = L"Oʻrnatish";
    } else if (yuklashKetyapti_) {
        const long long foiz = jami_ > 0 ? olingan_ * 100 / jami_ : 0;
        k.matn =
            L"Kotib " + toWide(yuklanayotgan_) + L" yuklanmoqda… " + std::to_wstring(foiz) + L" %";
        k.tugma = L"Hozir tekshirish";
        k.tugmaFaol = false;
    } else if (tekshiruvKetyapti_) {
        k.matn = L"Tekshirilmoqda…";
        k.tugma = L"Hozir tekshirish";
        k.tugmaFaol = false;
    } else {
        const long long oxirgi = loadSettings().yangilanishMuvaffaqiyat;
        k.matn = L"Kotib " + joriy + L" · oxirgi tekshiruv: " +
                 (oxirgi > 0 ? sanaVaqt(oxirgi) : std::wstring(L"hali yoʻq"));
        k.tugma = L"Hozir tekshirish";
    }
    return k;
}

void Yangilovchi::tugmaBosildi() {
    if (tayyor_) {
        hozirOrnat();
    } else {
        hozirTekshir();
    }
}

}  // namespace rubai
