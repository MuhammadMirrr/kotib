// rubai-cli — yadroni tekshirish uchun konsol vositasi.
//
// Ilovaning oʻzi emas: core/ dagi kod toʻgʻri ishlayotganini tasdiqlaydi
// (model yuklash, transkripsiya, signal tekshiruvi, log).
//
//   rubai-cli <audio> [--cpu] [--model <yoʻl>] [--segmentlar]
//   rubai-cli --mics                 mikrofonlar roʻyxati
//   rubai-cli --record <soniya>      mikrofondan yozib transkripsiya qilish
//             [--saqla <fayl.wav>]   yozuvni WAV qilib ham saqlaydi (oʻlchov uchun)
//   rubai-cli --tarjima "<matn>" [--dan uzn_Latn] [--ga rus_Cyrl]
//                                    matnni tarjima qiladi (UI'siz)
//
// WAV boʻlmagan fayllar Media Foundation orqali dekodlanadi — bu Studiya
// (Fayl tabi) ishlatadigan yoʻlning aynan oʻzi. `--segmentlar` esa segment
// API'sini va matn formatlashni tekshiradi: shu ikkalasi bilan Studiya
// quvurining hammasi UI'siz sinaladi.

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "../core/audio_capture.h"
#include "../core/engine.h"
#include "../core/matn_format.h"
#include "../core/media_decode.h"
#include "../core/samples.h"
#include "../core/saqlanmagan.h"
#include "../core/util.h"
#if KOTIB_TARJIMA
#include "../core/tarjimon.h"
#include "../core/tillar.h"
#endif
#include "../core/wav.h"
#include "../core/whisper_bridge.h"

using namespace rubai;

namespace {

void print(const std::wstring& s) {
    DWORD written = 0;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    std::wstring line = s + L"\n";
    // Konsolga toʻgʻridan-toʻgʻri UTF-16 yozamiz — oʻzbek harflari
    // kod sahifasidan qatʼi nazar toʻgʻri koʻrinishi uchun.
    if (!WriteConsoleW(h, line.c_str(), (DWORD)line.size(), &written, nullptr)) {
        std::string utf8 = toUtf8(line);
        WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }
}

const wchar_t* verdictText(AudioVerdict v) {
    switch (v) {
        case AudioVerdict::Silent: return L"MIKROFON JIM — signal yoʻq";
        case AudioVerdict::TooShort: return L"juda qisqa";
        case AudioVerdict::VeryQuiet: return L"juda past, kuchaytiriladi";
        default: return L"normal";
    }
}

const wchar_t* kindText(MicKind k) {
    switch (k) {
        case MicKind::Loopback: return L"[TIZIM OVOZI — mikrofon emas]";
        case MicKind::Bluetooth: return L"[Bluetooth — sifat past]";
        case MicKind::Virtual: return L"[virtual]";
        default: return L"";
    }
}

int listMics() {
    const auto mics = listMicrophones();
    if (mics.empty()) {
        print(L"Kirish qurilmasi topilmadi.");
        return 1;
    }
    print(L"Mikrofonlar (tavsiya tartibida):");
    for (size_t i = 0; i < mics.size(); i++) {
        std::wstring line = L"  [" + std::to_wstring(i) + L"] " + mics[i].name;
        if (mics[i].isDefault) line += L"  (standart)";
        std::wstring k = kindText(mics[i].kind);
        if (!k.empty()) line += L"  " + k;
        print(line);
    }
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    logInit();

    std::wstring audioPath, modelPath, micId, saqlaYoli;
    bool useGpu = true;
    bool segmentlar = false;
    int recordSeconds = 0;
    std::wstring tarjimaMatni, tarjimaDan = L"uzn_Latn", tarjimaGa = L"rus_Cyrl";
    bool tarjimaRejimi = false;

    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"--cpu") {
            useGpu = false;
        } else if (a == L"--mics") {
            return listMics();
        } else if (a == L"--record" && i + 1 < argc) {
            recordSeconds = _wtoi(argv[++i]);
        } else if (a == L"--saqla" && i + 1 < argc) {
            saqlaYoli = argv[++i];
        } else if (a == L"--mic" && i + 1 < argc) {
            micId = argv[++i];
        } else if (a == L"--tarjima" && i + 1 < argc) {
            tarjimaRejimi = true;
            tarjimaMatni = argv[++i];
        } else if (a == L"--dan" && i + 1 < argc) {
            tarjimaDan = argv[++i];
        } else if (a == L"--ga" && i + 1 < argc) {
            tarjimaGa = argv[++i];
        } else if (a == L"--segmentlar") {
            segmentlar = true;
        } else if (a == L"--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else if (!a.empty() && a[0] != L'-') {
            audioPath = a;
        }
    }

#if KOTIB_TARJIMA
    // Tarjima rejimi — audio umuman kerak emas.
    //
    // Nega CLI'da: tarjima quvuri (matnni jumlalarga boʻlish → CTranslate2 →
    // qayta yigʻish) UI'dan mustaqil va uni shu yerda sinash mumkin. macOS
    // bilan natijani solishtirish ham shu yoʻl bilan qilinadi.
    if (tarjimaRejimi) {
        const std::string dan = toUtf8(tarjimaDan);
        const std::string ga = toUtf8(tarjimaGa);
        const Til* m = Tillar::top(dan);
        const Til* q = Tillar::top(ga);
        if (!m || !q) {
            print(L"XATO: til kodi notoʻgʻri (masalan uzn_Latn, rus_Cyrl)");
            return 2;
        }
        print(L"model: " + TarjimaModel::papka() +
              (TarjimaModel::tayyor() ? L"  (tayyor)" : L"  (TOPILMADI)"));

        // Ish fon oqimida ketadi — natijani kutamiz.
        std::wstring natija;
        TarjimaXatosi xato = TarjimaXatosi::Yoq;
        volatile bool tugadi = false;
        Tarjimon::birgalik().tarjimaQil(
            tarjimaMatni, *m, *q,
            [](int a, int b) {
                wchar_t s[64];
                swprintf(s, 64, L"  %d / %d jumla", a, b);
                print(s);
            },
            [&](std::wstring n, TarjimaXatosi x) {
                natija = std::move(n);
                xato = x;
                tugadi = true;
            });
        while (!tugadi) Sleep(50);

        if (xato != TarjimaXatosi::Yoq) {
            print(L"XATO: " + tarjimaXatoXabari(xato));
            return 1;
        }
        print(L"");
        print(natija);
        return 0;
    }
#else
    if (tarjimaRejimi) {
        print(L"Bu build tarjimasiz yigʻilgan (KOTIB_TARJIMA=0).");
        return 2;
    }
#endif

    if (audioPath.empty() && recordSeconds <= 0) {
        print(L"Ishlatish:");
        print(L"  rubai-cli <audio> [--cpu] [--model <yoʻl>] [--segmentlar]");
        print(L"  rubai-cli --mics");
        print(L"  rubai-cli --record <soniya> [--mic <id>] [--saqla <fayl.wav>]");
        print(L"  rubai-cli --tarjima \"<matn>\" [--dan uzn_Latn] [--ga rus_Cyrl]");
        return 2;
    }

    std::vector<float> pcm;
    std::wstring err;

    if (recordSeconds > 0) {
        AudioCapture cap;
        if (!cap.start(micId, err)) {
            print(L"XATO: " + err);
            return 1;
        }
        print(L"Yozilmoqda (" + std::to_wstring(recordSeconds) + L" soniya)...");
        Sleep((DWORD)recordSeconds * 1000);
        pcm = cap.stop();
        if (cap.deviceLost()) print(L"OGOHLANTIRISH: audio oqimi uzildi");
        if (!saqlaYoli.empty()) {
            // Ilova saqlanmagan ovozni yozadigan format (16 kHz PCM16 mono) —
            // akustik sinovlarni Mac'dagi stt-baho bilan qayta oʻlchash uchun.
            const std::string baytlar = saqlanmagan::wav(pcm);
            FILE* f = _wfopen(saqlaYoli.c_str(), L"wb");
            if (!f || fwrite(baytlar.data(), 1, baytlar.size(), f) != baytlar.size()) {
                print(L"XATO: yozilmadi: " + saqlaYoli);
            }
            if (f) fclose(f);
        }
    } else if (!readWav16kMono(audioPath, pcm, err)) {
        // WAV emas (yoki buzuq) — Media Foundation'ga beramiz. Studiya
        // aynan shu yoʻldan yuradi, shuning uchun uni ham shu yerda sinaymiz.
        print(L"WAV oʻqilmadi (" + err + L") — Media Foundation bilan urinamiz");
        const MediaXato mx = namunalarniOqi(audioPath, 0, 0, nullptr, nullptr, pcm);
        if (mx != MediaXato::Yoq) {
            print(L"XATO: " + mediaXatoXabari(mx, L""));
            return 1;
        }
    }

    const AudioCheck check = checkSamples(pcm);
    wchar_t info[256];
    swprintf(info, 256, L"audio: %.1fs, peak=%.4f  (%s)", check.seconds, check.peak,
             verdictText(check.verdict));
    print(info);

    if (check.verdict == AudioVerdict::Silent) {
        print(L"Transkripsiya oʻtkazilmadi — audioda ovoz yoʻq.");
        return 1;
    }

    Engine& engine = Engine::instance();
    if (!modelPath.empty()) engine.setModelPath(modelPath);
    engine.setUseGpu(useGpu);

    const std::wstring resolved = modelPath.empty() ? Engine::findModel() : modelPath;
    print(L"model: " + (resolved.empty() ? L"TOPILMADI" : resolved));

    if (segmentlar) {
        // Studiya yoʻli: segment API + matn formatlash. `engine` navbatidan
        // oʻtmaydi, chunki bu vosita bitta oqimda ishlaydi va boshqa hech
        // kim modelga tegmaydi.
        const std::vector<float> tayyor = prepareSamples(pcm);
        std::vector<Segment> segs;
        std::wstring segXato;
        const int rc =
            engine.transcribeSegments(tayyor, nullptr, nullptr, nullptr, nullptr, segs, segXato);
        if (rc != 0) {
            print(L"XATO: segment API " + std::to_wstring(rc) + L" — " +
                  (segXato.empty() ? toWide(rubai_last_error()) : segXato));
            engine.shutdown();
            return 1;
        }
        print(L"segmentlar: " + std::to_wstring(segs.size()));
        print(L"");
        print(L"--- xom ---");
        print(xomMatn(segs));
        print(L"");
        print(L"--- tayyor ---");
        print(chiroyliMatn(segs, Apostrof::Standart));
        engine.shutdown();
        return 0;
    }

    const TranscribeResult r = engine.transcribe(prepareSamples(pcm));

    if (!r.ok()) {
        print(L"XATO: " + r.error);
        engine.shutdown();
        return 1;
    }

    swprintf(info, 256, L"backend: %s, %.2fs (%.2fx realtime)", engine.backendName().c_str(),
             r.seconds, check.seconds > 0 ? r.seconds / check.seconds : 0.0);
    print(info);
    print(L"");
    print(r.text.empty() ? L"(nutq aniqlanmadi)" : r.text);

    engine.shutdown();
    return 0;
}
