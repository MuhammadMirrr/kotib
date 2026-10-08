// whisper.cpp ustidan C shim (macOS, Metal): model va VAD yuklash, diktovka va
// Studiya transkripsiyasi, xato matni, backend nomi, log yoʻnaltirish.
// Inferens va VAD parametrlari `win/core/whisper_bridge.c` bilan AYNAN bir xil —
// `win/tests/mac/parametr-tekshir.sh` tekshiradi; «Ovozni boʻlaklash» boʻlimi
// ikkala faylda soʻzma-soʻz bir xil.

#include "whisper.h"
#include "ggml-backend.h"
#include "whisper_bridge.h"
#include "nutq_bolaklari.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct whisper_context *g_ctx = NULL;
// Xato matni, faol backend va log — `win/core/whisper_bridge.c` bilan bir xil
// (barqarorlik A5). Ilgari macOS'da whisper yiqilish sababini stderr'ga
// yozardi, grafik ilovada esa stderr yoʻq — logda faqat «kod 1» qolardi.
static char g_err[256] = "";
static char g_backend[32] = "";

static rubai_log_fn g_log = NULL;

static void set_err(const char *msg) { snprintf(g_err, sizeof(g_err), "%s", msg ? msg : ""); }

// ggml/whisper xabarlarini oʻz logimizga uzatadi.
//
// DEBUG tashlanadi: ggml backendlari har pipeline/kernel uchun bitta DEBUG
// qatori yozadi (Metal'da oʻnlab-yuzlab) — har model yuklanishida (180 s
// boʻsh turishdan keyin ham) log shu bilan toʻlib, kerakli qatorlarni
// siqib chiqarardi. CONT — oldingi xabarning davomi: oldingisi tashlangan
// boʻlsa, u ham tashlanadi.
static int g_oxirgisi_tashlandi = 0;

static void ggml_log_bridge(enum ggml_log_level level, const char *text, void *ud) {
    (void)ud;
    if (level == GGML_LOG_LEVEL_DEBUG || (level == GGML_LOG_LEVEL_CONT && g_oxirgisi_tashlandi)) {
        g_oxirgisi_tashlandi = 1;
        return;
    }
    g_oxirgisi_tashlandi = 0;
    if (g_log && text && text[0] && text[0] != '\n') g_log(text);
}

void rubai_set_log(rubai_log_fn fn) {
    g_log = fn;
    whisper_log_set(ggml_log_bridge, NULL);
    ggml_log_set(ggml_log_bridge, NULL);
}

// Yuklangan ggml backendlari orasidan GPU'sini topadi. Topilmasa "CPU".
static void detect_backend(int use_gpu) {
    snprintf(g_backend, sizeof(g_backend), "CPU");
    if (!use_gpu) return;
    size_t n = ggml_backend_reg_count();
    for (size_t i = 0; i < n; i++) {
        ggml_backend_reg_t reg = ggml_backend_reg_get(i);
        const char *name = reg ? ggml_backend_reg_name(reg) : NULL;
        if (!name) continue;
        // Metal'ning registr nomi "MTL" (ggml-metal.cpp, GGML_METAL_NAME) —
        // "Metal" bilan solishtirish hech qachon mos kelmasdi va macOS
        // har doim "CPU" deb koʻrsatardi.
        if (strcmp(name, "MTL") == 0) {
            snprintf(g_backend, sizeof(g_backend), "Metal");
            return;
        }
        if (strcmp(name, "Vulkan") == 0 || strcmp(name, "CUDA") == 0) {
            snprintf(g_backend, sizeof(g_backend), "%s", name);
            return;
        }
    }
}

static struct whisper_vad_context *g_vad = NULL;

// VAD modeli yoʻli — bir marta beriladi; har model yuklanishida (180 s boʻsh
// turishdan keyin ham) VAD ham yuklanadi.
static char g_vad_yol[1024] = "";

void rubai_set_vad_path(const char *path) {
    snprintf(g_vad_yol, sizeof(g_vad_yol), "%s", path ? path : "");
}

// VAD yuklanmasa diktovka baribir ishlaydi — 1.1 dagi kabi, boʻlaksiz.
static void vad_yukla(void) {
    if (g_vad || !g_vad_yol[0]) return;
    struct whisper_vad_context_params vcp = whisper_vad_default_context_params();
    vcp.use_gpu = false;  // ~2 ms / 1 s ovoz — CPU yetarli
    g_vad = whisper_vad_init_from_file_with_params(g_vad_yol, vcp);
    if (!g_vad && g_log) g_log("VAD modeli yuklanmadi — ovoz boʻlaklarga boʻlinmaydi");
}

int rubai_load(const char *model_path) {
    if (g_ctx) return 0;
    if (!model_path || !model_path[0]) {
        set_err("empty model path");
        return 1;
    }
    struct whisper_context_params cp = whisper_context_default_params();
    cp.use_gpu = true;  // Metal (Apple GPU)
    cp.flash_attn = true;
    g_ctx = whisper_init_from_file_with_params(model_path, cp);
    if (!g_ctx) {
        set_err("whisper_init_from_file_with_params failed");
        g_backend[0] = '\0';
        return 1;
    }
    detect_backend(1);
    vad_yukla();
    set_err("");
    return 0;
}

static void segmentlarni_tozala(void);

void rubai_unload(void) {
    if (g_vad) {
        whisper_vad_free(g_vad);
        g_vad = NULL;
    }
    if (g_ctx) {
        whisper_free(g_ctx);
        g_ctx = NULL;
    }
    segmentlarni_tozala();
    g_backend[0] = '\0';
}

// ---- Ovozni boʻlaklash (S12) — macOS va Windows'da AYNAN bir xil ----------
//
// Bu fine-tune modelda `no_speech_prob` doim ~1e-10 — `no_speech_thold` hech
// qachon ishlamaydi va jimlik/shovqin «musiqa» boʻlib chiqardi; `no_timestamps`
// bilan whisper oynasi esa 30 s qatʼiy suriladi va dekoder chiqarmagan jumlalar
// butunlay yoʻqolardi. Silero VAD: nutq yoʻq — whisper chaqirilmaydi; uzun ovoz
// VAD topgan jimliklardan boʻlinadi (`nutq_bolaklari.h`). Oʻlchov (FLEURS,
// 10 × ~120 s): WER 30,19 % → 7,83 %, yoʻqolgan jumla 14/96 → 0, nutqsiz
// fayllarda gallyutsinatsiya 20/20 → 0/20; qisqa diktovka sifati — shovqin
// chegarasida.

// VAD boʻlaklash parametrlari — ikkala platformada AYNAN bir xil
// (`parametr-tekshir.sh` `vp.` qatorlarini ham solishtiradi).
static struct whisper_vad_params vad_parametrlari(void) {
    struct whisper_vad_params vp = whisper_vad_default_params();
    vp.threshold = 0.5f;
    vp.min_speech_duration_ms = 250;
    vp.min_silence_duration_ms = 300;
    vp.max_speech_duration_s = 25.0f;  // whisper oynasi 30 s — zaxira bilan
    vp.speech_pad_ms = 200;
    vp.samples_overlap = 0.1f;
    return vp;
}

// Ovozni whisper chaqiruvlariga boʻladi. `*chiqish` — malloc (chaqiruvchi
// free qiladi). Qaytadi: oraliqlar soni, 0 — nutq yoʻq, -1 — xotira yoʻq.
// VAD yuklanmagan (modeli topilmagan) yoki ishlamagan boʻlsa — butun ovoz
// bitta oraliq: 1.1 dagi yoʻl, diktovka yoʻqolmaydi.
static int oraliqlar(const float *samples, int n_samples, rubai_oraliq **chiqish) {
    struct whisper_vad_segments *vs =
        g_vad ? whisper_vad_segments_from_samples(g_vad, vad_parametrlari(), samples, n_samples)
              : NULL;
    if (g_vad && !vs && g_log) g_log("VAD ishlamadi — ovoz boʻlinmasdan oʻgiriladi");
    const int nv = vs ? whisper_vad_segments_n_segments(vs) : 0;
    *chiqish = malloc(sizeof(rubai_oraliq) * (size_t)(nv > 0 ? nv : 1));
    float *t0 = malloc(sizeof(float) * (size_t)(nv > 0 ? nv : 1));
    float *t1 = malloc(sizeof(float) * (size_t)(nv > 0 ? nv : 1));
    int n = -1;
    if (*chiqish && t0 && t1) {
        if (!vs) {
            (*chiqish)[0].boshi = 0;
            (*chiqish)[0].oxiri = n_samples;
            n = 1;
        } else {
            for (int i = 0; i < nv; i++) {
                t0[i] = whisper_vad_segments_get_segment_t0(vs, i);
                t1[i] = whisper_vad_segments_get_segment_t1(vs, i);
            }
            n = rubai_nutq_bolaklari(t0, t1, nv, n_samples, *chiqish);
        }
    }
    free(t0);
    free(t1);
    if (vs) whisper_vad_free_segments(vs);
    if (n < 0) {
        free(*chiqish);
        *chiqish = NULL;
    }
    return n;
}

// Matnga boʻlak qoʻshadi: boshidagi boʻshliqlar tashlanadi, boʻlaklar orasiga
// bitta boʻshliq. Ilgari segmentlar ajratgichsiz ulanardi va oyna chegarasida
// soʻzlar yopishib qolardi («kerak.yoʻq»). 0 — muvaffaqiyat.
static int matnga_qosh(char **out, size_t *len, const char *t) {
    while (*t == ' ') t++;
    const size_t tl = strlen(t);
    if (tl == 0) return 0;
    const int ajratgich = *len > 0 ? 1 : 0;
    char *tmp = realloc(*out, *len + (size_t)ajratgich + tl + 1);
    if (!tmp) return 1;
    *out = tmp;
    if (ajratgich) (*out)[(*len)++] = ' ';
    memcpy(*out + *len, t, tl);
    *len += tl;
    (*out)[*len] = '\0';
    return 0;
}

char *rubai_transcribe(const float *samples, int n_samples, int n_threads) {
    if (!g_ctx) {
        set_err("model not loaded");
        return NULL;
    }
    if (!samples || n_samples <= 0) {
        set_err("no samples");
        return NULL;
    }

    struct whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    p.language = "uz";  // faqat o'zbek
    p.translate = false;
    p.beam_search.beam_size = 5;  // maksimal aniqlik
    p.n_threads = n_threads > 0 ? n_threads : 4;
    p.no_timestamps = true;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_special = false;
    p.print_timestamps = false;
    p.suppress_blank = true;
    p.no_speech_thold = 0.25f;  // bu modelda taʼsirsiz: no_speech_prob doim ~1e-10 (S12)
    p.logprob_thold = -1.0f;
    // Takrorlanish halqasidan himoya (S23): dekodlash entropiyasi past chiqsa
    // («oʻzbekiston respublikasi» ×15 kabi) whisper shu boʻlakni 0,2, 0,4 …
    // haroratda qayta dekodlaydi. 0 boʻlganda halqa matn sifatida kiritilardi.
    // Toza va shovqinli oʻlchov toʻplamlarida natija 0 dagi bilan bayt-ma-bayt
    // bir xil — qayta urinish faqat buzuq audioda ishga tushadi.
    p.temperature_inc = 0.2f;

    rubai_oraliq *o = NULL;
    const int n = oraliqlar(samples, n_samples, &o);
    char *out = n >= 0 ? malloc(1) : NULL;
    if (!out) {
        free(o);
        set_err("out of memory");
        return NULL;
    }
    out[0] = '\0';
    size_t len = 0;
    for (int k = 0; k < n; k++) {
        if (whisper_full(g_ctx, p, samples + o[k].boshi, o[k].oxiri - o[k].boshi) != 0) {
            free(o);
            free(out);
            set_err("whisper_full failed");
            return NULL;
        }
        const int ns = whisper_full_n_segments(g_ctx);
        for (int i = 0; i < ns; i++) {
            const char *t = whisper_full_get_segment_text(g_ctx, i);
            if (t && matnga_qosh(&out, &len, t)) {
                free(o);
                free(out);
                set_err("out of memory");
                return NULL;
            }
        }
    }
    free(o);
    set_err("");
    return out;
}

// `aborted` — yopishqoq bayroq: bir marta true boʻlsa, shu chaqiruv davomida
// shunday qoladi. whisper_full baʼzan bekor qilishdan keyin ham 0 (muvaffaqiyat)
// qaytarishi mumkin (masalan, oxirgi hisoblash grafigi allaqachon tugagan boʻlsa) —
// shuning uchun natijani emas, shu bayroqni tekshiramiz.
struct rubai_abort_ctx {
    rubai_abort_cb cb;
    void *ud;
    bool aborted;
};

static bool rubai_abort_shim(void *data) {
    struct rubai_abort_ctx *a = (struct rubai_abort_ctx *)data;
    if (!a || !a->cb) return false;
    bool r = a->cb(a->ud);
    if (r) a->aborted = true;
    return r;
}

// Bir nechta whisper chaqiruvining umumiy jarayoni: oldingi oraliqlar +
// joriysining ulushi, butun ovozga nisbatan.
struct rubai_prog_ctx {
    rubai_progress_cb cb;
    void *ud;
    long long oldin, joriy, jami;  // namunalar
};

static void rubai_prog_shim(struct whisper_context *ctx, struct whisper_state *st, int progress,
                            void *data) {
    (void)ctx;
    (void)st;
    struct rubai_prog_ctx *p = (struct rubai_prog_ctx *)data;
    if (!p || !p->cb || p->jami <= 0) return;
    p->cb((int)((p->oldin + p->joriy * progress / 100) * 100 / p->jami), p->ud);
}

// Studiya natijasi — oʻz xotiramizda: VAD bilan bir segment = bitta boʻlak
// (vaqt VAD'dan), aks holda whisper segmentlari. Keyingi chaqiruvgacha yoki
// `rubai_unload` gacha yashaydi.
struct rubai_segment {
    char *matn;
    int64_t t0, t1;  // 10 ms birliklarda
};
static struct rubai_segment *g_seg = NULL;
static int g_seg_n = 0, g_seg_sigim = 0;

static void segmentlarni_tozala(void) {
    for (int i = 0; i < g_seg_n; i++) free(g_seg[i].matn);
    g_seg_n = 0;
}

// Matnni nusxalab qoʻshadi (boʻsh matn — oʻtkaziladi). 0 — muvaffaqiyat.
static int segment_qosh(const char *matn, int64_t t0, int64_t t1) {
    while (*matn == ' ') matn++;
    if (!*matn) return 0;
    if (g_seg_n == g_seg_sigim) {
        const int yangi = g_seg_sigim ? g_seg_sigim * 2 : 64;
        struct rubai_segment *tmp = realloc(g_seg, sizeof(*g_seg) * (size_t)yangi);
        if (!tmp) return 1;
        g_seg = tmp;
        g_seg_sigim = yangi;
    }
    const size_t l = strlen(matn);
    char *nusxa = malloc(l + 1);
    if (!nusxa) return 1;
    memcpy(nusxa, matn, l + 1);
    g_seg[g_seg_n].matn = nusxa;
    g_seg[g_seg_n].t0 = t0;
    g_seg[g_seg_n].t1 = t1;
    g_seg_n++;
    return 0;
}

int rubai_transcribe_segments(const float *samples, int n_samples, int n_threads,
                              rubai_progress_cb pcb, void *pud, rubai_abort_cb acb, void *aud) {
    segmentlarni_tozala();
    if (!g_ctx) {
        set_err("model not loaded");
        return 1;
    }
    if (!samples || n_samples <= 0) {
        set_err("no samples");
        return 1;
    }

    struct whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    // Parametrlar rubai_transcribe bilan AYNAN bir xil — bittasidan tashqari.
    p.language = "uz";
    p.translate = false;
    p.beam_search.beam_size = 5;
    p.n_threads = n_threads > 0 ? n_threads : 4;
    // YAGONA FARQ: VAD yoʻq boʻlsa segment chegaralari whisper'dan kerak. VAD
    // bilan vaqt boʻlakdan olinadi — whisper vaqt belgilari bu modelda buzuq
    // (butun 0–30 s oynalar, ortiqcha birinchi soʻz, takrorlanmaydigan natija).
    p.no_timestamps = g_vad ? true : false;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_special = false;
    p.print_timestamps = false;
    p.suppress_blank = true;
    p.no_speech_thold = 0.25f;
    p.logprob_thold = -1.0f;
    p.temperature_inc = 0.2f;

    struct rubai_prog_ctx pctx = {pcb, pud, 0, 0, n_samples};
    struct rubai_abort_ctx actx = {acb, aud, false};
    if (pcb) {
        p.progress_callback = rubai_prog_shim;
        p.progress_callback_user_data = &pctx;
    }
    if (acb) {
        p.abort_callback = rubai_abort_shim;
        p.abort_callback_user_data = &actx;
    }

    rubai_oraliq *o = NULL;
    const int n = oraliqlar(samples, n_samples, &o);
    if (n < 0) {
        set_err("out of memory");
        return 1;
    }
    for (int k = 0; k < n; k++) {
        const int uzunlik = o[k].oxiri - o[k].boshi;
        pctx.joriy = uzunlik;
        const int rc = whisper_full(g_ctx, p, samples + o[k].boshi, uzunlik);
        // Bekor qilingan bayrogʻini natija kodidan mustaqil tekshiramiz: bekor qilish
        // hisoblash allaqachon tugagandan keyin kelishi mumkin, va shunda whisper_full
        // 0 (muvaffaqiyat) qaytaradi — lekin foydalanuvchi baribir bekor qilgan.
        if (actx.aborted || (acb && acb(aud))) {
            free(o);
            segmentlarni_tozala();
            set_err("bekor qilindi");
            return 2;
        }
        if (rc != 0) {
            free(o);
            segmentlarni_tozala();
            set_err("whisper_full failed");
            return 1;
        }
        const int64_t siljish = o[k].boshi / 160;  // namuna → 10 ms
        const int ns = whisper_full_n_segments(g_ctx);
        int xato = 0;
        if (g_vad) {
            char *matn = malloc(1);
            size_t len = 0;
            xato = !matn;
            if (matn) matn[0] = '\0';
            for (int i = 0; !xato && i < ns; i++) {
                const char *t = whisper_full_get_segment_text(g_ctx, i);
                if (t) xato = matnga_qosh(&matn, &len, t);
            }
            if (!xato) xato = segment_qosh(matn, siljish, o[k].oxiri / 160);
            free(matn);
        } else {
            for (int i = 0; !xato && i < ns; i++) {
                const char *t = whisper_full_get_segment_text(g_ctx, i);
                if (t) {
                    xato = segment_qosh(t, siljish + whisper_full_get_segment_t0(g_ctx, i),
                                        siljish + whisper_full_get_segment_t1(g_ctx, i));
                }
            }
        }
        if (xato) {
            free(o);
            segmentlarni_tozala();
            set_err("out of memory");
            return 1;
        }
        pctx.oldin += uzunlik;
    }
    free(o);
    set_err("");
    return 0;
}

int rubai_n_segments(void) { return g_seg_n; }
const char *rubai_segment_text(int i) { return i >= 0 && i < g_seg_n ? g_seg[i].matn : NULL; }
int64_t rubai_segment_t0(int i) { return i >= 0 && i < g_seg_n ? g_seg[i].t0 : 0; }
int64_t rubai_segment_t1(int i) { return i >= 0 && i < g_seg_n ? g_seg[i].t1 : 0; }

void rubai_free_str(char *s) { free(s); }

const char *rubai_last_error(void) { return g_err; }

const char *rubai_backend_name(void) { return g_backend; }
