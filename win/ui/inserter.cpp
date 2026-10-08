// Matnni faol oynaga kiritish va clipboard'ni saqlash/tiklash.
// Interfeys va izohlar — `inserter.h`.

#include "inserter.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "../core/util.h"

namespace rubai {

namespace {

// Clipboard'ni ochish. Boshqa ilova uni band qilgan boʻlishi mumkin,
// shuning uchun bir necha marta urinamiz.
bool openClipboard(HWND owner) {
    for (int i = 0; i < 10; i++) {
        if (OpenClipboard(owner)) return true;
        Sleep(20);
    }
    return false;
}

// ---- Clipboard'ni saqlash va tiklash (barqarorlik spec'i, C2) ---------------
//
// 1.1.0 da faqat matn (CF_UNICODETEXT) saqlanardi: foydalanuvchi rasm, fayl
// yoki boy matn nusxalagan boʻlsa, diktovkadan keyin ular butunlay yoʻqolardi.
// Ustiga transkript Windows clipboard tarixiga (Win+V) va Microsoft bulut
// clipboard'iga chiqib ketardi — «matn qurilmadan chiqmaydi» vaʼdasiga zid.
// macOS'dagi egizagi — src/clipboard.swift.

struct ClipboardFormati {
    UINT id = 0;
    std::vector<uint8_t> malumot;
    bool emf = false;  // CF_ENHMETAFILE — HGLOBAL emas, alohida koʻchiriladi
};

struct ClipboardNusxa {
    bool olindi = false;  // ochib boʻlmagan boʻlsa — tiklashga urinilmaydi
    std::vector<ClipboardFormati> formatlar;
};

// Saqlab-tiklab boʻlmaydigan yoki tizim oʻzi yasaydigan formatlar.
// CF_BITMAP — CF_DIB dan, CF_METAFILEPICT — CF_ENHMETAFILE dan tizim
// avtomatik yasaydi; «private» va GDI oraligʻidagi dastaklarning maʼnosi
// faqat egasiga maʼlum — ularni nusxalab boʻlmaydi.
bool saqlanmaydi(UINT f) {
    switch (f) {
        case CF_BITMAP:
        case CF_PALETTE:
        case CF_METAFILEPICT:
        case CF_OWNERDISPLAY:
        case CF_DSPTEXT:
        case CF_DSPBITMAP:
        case CF_DSPMETAFILEPICT:
        case CF_DSPENHMETAFILE: return true;
        default: break;
    }
    return (f >= CF_PRIVATEFIRST && f <= CF_PRIVATELAST) ||
           (f >= CF_GDIOBJFIRST && f <= CF_GDIOBJLAST);
}

ClipboardNusxa clipboardNusxaOl() {
    ClipboardNusxa n;
    if (!openClipboard(nullptr)) return n;
    n.olindi = true;
    for (UINT f = EnumClipboardFormats(0); f; f = EnumClipboardFormats(f)) {
        if (saqlanmaydi(f)) continue;
        HANDLE h = GetClipboardData(f);  // kechiktirilgan render shu yerda bajariladi
        if (!h) continue;
        ClipboardFormati cf;
        cf.id = f;
        if (f == CF_ENHMETAFILE) {
            const UINT hajm = GetEnhMetaFileBits(static_cast<HENHMETAFILE>(h), 0, nullptr);
            if (!hajm) continue;
            cf.malumot.resize(hajm);
            GetEnhMetaFileBits(static_cast<HENHMETAFILE>(h), hajm, cf.malumot.data());
            cf.emf = true;
        } else {
            const SIZE_T hajm = GlobalSize(h);
            const uint8_t* p = hajm ? static_cast<const uint8_t*>(GlobalLock(h)) : nullptr;
            if (!p) continue;
            cf.malumot.assign(p, p + hajm);
            GlobalUnlock(h);
        }
        n.formatlar.push_back(std::move(cf));
    }
    CloseClipboard();
    return n;
}

HGLOBAL globalNusxa(const void* p, size_t hajm) {
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, hajm ? hajm : 1);
    if (!h) return nullptr;
    void* q = GlobalLock(h);
    if (!q) {
        GlobalFree(h);
        return nullptr;
    }
    if (hajm) memcpy(q, p, hajm);
    GlobalUnlock(h);
    return h;
}

// SetClipboardData muvaffaqiyatli boʻlsa xotira tizimniki, aks holda — bizniki.
void qoy(UINT format, HGLOBAL h) {
    if (h && !SetClipboardData(format, h)) GlobalFree(h);
}

// Transkriptni yozadi va Windows'ga uni tarixga, bulutga va clipboard
// kuzatuvchilariga bermaslikni aytadi (rasmiy formatlar:
// learn.microsoft.com → «Cloud Clipboard and Clipboard History Formats»).
// `bizniki` — yozuvdan keyingi ketma-ketlik raqami: tiklashda «bizdan keyin
// hech kim yozmaganmi» degan tekshiruv uchun.
bool clipboardgaVaqtinchaYoz(const std::wstring& text, DWORD& bizniki) {
    HGLOBAL matn = globalNusxa(text.c_str(), (text.size() + 1) * sizeof(wchar_t));
    if (!matn) return false;
    if (!openClipboard(nullptr)) {
        GlobalFree(matn);
        return false;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, matn)) {
        CloseClipboard();
        GlobalFree(matn);
        return false;
    }
    static const UINT kuzatma =
        RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    static const UINT tarixga = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    static const UINT bulutga = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
    const DWORD yoq = 0;
    if (kuzatma) qoy(kuzatma, globalNusxa(&yoq, sizeof yoq));
    if (tarixga) qoy(tarixga, globalNusxa(&yoq, sizeof yoq));
    if (bulutga) qoy(bulutga, globalNusxa(&yoq, sizeof yoq));
    CloseClipboard();
    bizniki = GetClipboardSequenceNumber();
    return true;
}

// Nusxani aslidek qaytaradi — faqat clipboard'ni bizdan keyin hech kim
// oʻzgartirmagan boʻlsa (foydalanuvchining yangi tanlovi ustidan yozilmaydi).
bool clipboardTikla(const ClipboardNusxa& n, DWORD bizniki) {
    if (!n.olindi || GetClipboardSequenceNumber() != bizniki) return false;
    if (!openClipboard(nullptr)) return false;
    // Ochish uchun kutgan paytimizda kimdir yozgan boʻlishi mumkin.
    if (GetClipboardSequenceNumber() != bizniki) {
        CloseClipboard();
        return false;
    }
    EmptyClipboard();
    for (const auto& f : n.formatlar) {
        if (f.emf) {
            HENHMETAFILE e =
                SetEnhMetaFileBits(static_cast<UINT>(f.malumot.size()), f.malumot.data());
            if (e && !SetClipboardData(CF_ENHMETAFILE, e)) DeleteEnhMetaFile(e);
        } else {
            qoy(f.id, globalNusxa(f.malumot.data(), f.malumot.size()));
        }
    }
    CloseClipboard();
    return true;
}

INPUT keyInput(WORD vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    return in;
}

INPUT unicodeInput(wchar_t ch, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = 0;
    in.ki.wScan = ch;
    in.ki.dwFlags = KEYEVENTF_UNICODE | (up ? KEYEVENTF_KEYUP : 0);
    return in;
}

// Foydalanuvchi hotkey uchun bosgan modifikatorlar hali qoʻyib
// yuborilmagan boʻlishi mumkin. Ctrl+V yuborishdan oldin ularni
// "qoʻyib yuborilgan" holatga keltiramiz, aks holda ilova Ctrl+Alt+V
// yoki Shift+Ctrl+V koʻradi va boshqa amal bajaradi.
void releaseStuckModifiers(std::vector<INPUT>& seq) {
    const WORD mods[] = {VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN};
    for (WORD vk : mods) {
        if (GetAsyncKeyState(vk) & 0x8000) seq.push_back(keyInput(vk, true));
    }
    // Ctrl alohida: uni Ctrl+V uchun BOSILGAN holatda ushlab turamiz,
    // shuning uchun bu yerda qoʻyib yubormaymiz.
}

bool sendAll(const std::vector<INPUT>& seq) {
    if (seq.empty()) return true;
    const UINT sent = SendInput((UINT)seq.size(), const_cast<INPUT*>(seq.data()), sizeof(INPUT));
    return sent == seq.size();
}

bool pasteViaCtrlV() {
    std::vector<INPUT> seq;
    releaseStuckModifiers(seq);

    const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (!ctrlDown) seq.push_back(keyInput(VK_CONTROL, false));
    seq.push_back(keyInput('V', false));
    seq.push_back(keyInput('V', true));
    if (!ctrlDown) seq.push_back(keyInput(VK_CONTROL, true));

    return sendAll(seq);
}

bool typeUnicode(const std::wstring& text) {
    std::vector<INPUT> seq;
    releaseStuckModifiers(seq);
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) seq.push_back(keyInput(VK_CONTROL, true));
    if (!sendAll(seq)) return false;

    // Katta matnni boʻlaklab yuboramiz — SendInput navbati cheklangan
    // va baʼzi ilovalar tez oqimni tashlab yuboradi.
    constexpr size_t kChunk = 32;
    std::vector<INPUT> chunk;
    chunk.reserve(kChunk * 2);

    for (size_t i = 0; i < text.size(); i++) {
        const wchar_t ch = text[i];
        if (ch == L'\r') continue;
        if (ch == L'\n') {
            chunk.push_back(keyInput(VK_RETURN, false));
            chunk.push_back(keyInput(VK_RETURN, true));
        } else {
            chunk.push_back(unicodeInput(ch, false));
            chunk.push_back(unicodeInput(ch, true));
        }
        if (chunk.size() >= kChunk * 2) {
            if (!sendAll(chunk)) return false;
            chunk.clear();
            Sleep(1);
        }
    }
    return sendAll(chunk);
}

}  // namespace

namespace {

// Jarayon tokenining yaxlitlik darajasi (SECURITY_MANDATORY_*_RID).
// Oʻqib boʻlmasa — -1.
long yaxlitlik(HANDLE jarayon) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(jarayon, TOKEN_QUERY, &token)) return -1;
    DWORD n = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &n);
    std::vector<uint8_t> b(n);
    long natija = -1;
    if (n && GetTokenInformation(token, TokenIntegrityLevel, b.data(), n, &n)) {
        const auto* l = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(b.data());
        const UCHAR soni = *GetSidSubAuthorityCount(l->Label.Sid);
        if (soni > 0) natija = static_cast<long>(*GetSidSubAuthority(l->Label.Sid, soni - 1));
    }
    CloseHandle(token);
    return natija;
}

}  // namespace

bool foregroundWindowIsElevated() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || pid == GetCurrentProcessId()) return false;

    // Ilgari faqat `OpenProcess` muvaffaqiyati tekshirilardi — lekin
    // PROCESS_QUERY_LIMITED_INFORMATION administrator jarayoniga ham beriladi,
    // shuning uchun funksiya HECH QACHON true qaytarmasdi (E5). UIPI qoidasi
    // yaxlitlik darajasiga qaraydi — xuddi shuni solishtiramiz.
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return true;  // ochib ham boʻlmadi — demak bizdan yuqori
    const long ular = yaxlitlik(proc);
    CloseHandle(proc);
    const long biz = yaxlitlik(GetCurrentProcess());
    // Tokenini oʻqib boʻlmasa — u ham himoyalangan, yaʼni bizdan yuqori.
    if (ular < 0) return true;
    return biz >= 0 && ular > biz;
}

bool matnniClipboardgaQoy(const std::wstring& text) {
    DWORD bizniki = 0;
    return clipboardgaVaqtinchaYoz(text, bizniki);
}

InsertResult insertText(const std::wstring& text, InsertMode mode) {
    InsertResult r;

    if (text.empty()) {
        r.error = L"Matn boʻsh";
        return r;
    }

    if (mode == InsertMode::Type) {
        // Bu rejimda clipboard ishlatilmaydi — foydalanuvchining
        // nusxalangan matni saqlanib qoladi.
        if (typeUnicode(text)) {
            r.ok = true;
            return r;
        }
        r.error = L"Matn kiritilmadi";
        return r;
    }

    // Avval foydalanuvchi clipboard'ining TOʻLIQ nusxasi (barcha formatlar),
    // keyin transkript — tarix va bulutga tushmaydigan qilib.
    auto nusxa = std::make_shared<ClipboardNusxa>(clipboardNusxaOl());
    DWORD bizniki = 0;
    if (!clipboardgaVaqtinchaYoz(text, bizniki)) {
        r.error = L"Clipboard band — matn qoʻyilmadi.\nBir oz kutib qayta urinib koʻring.";
        return r;
    }

    // Clipboard egasi almashishi uchun qisqa pauza. Busiz baʼzi ilovalar
    // eski mazmunni qoʻyib yuboradi.
    Sleep(40);

    if (!pasteViaCtrlV()) {
        // Matn clipboard'da QOLADI — foydalanuvchi oʻzi Ctrl+V bosadi.
        r.error = L"Matn joylanmadi, lekin u clipboard'da — Ctrl+V bosing.";
        return r;
    }

    // Eski mazmunni toʻliq tiklaymiz. Paste yakunlanishi uchun kutamiz —
    // juda erta tiklasak, ilova eski mazmunni qoʻyadi. Shu orada
    // foydalanuvchi oʻzi nusxalasa, ketma-ketlik raqami oʻzgaradi va
    // tiklash uning tanlovi ustidan yozmaydi.
    std::thread([nusxa, bizniki] {
        Sleep(1000);
        clipboardTikla(*nusxa, bizniki);
    }).detach();

    r.ok = true;
    return r;
}

}  // namespace rubai
