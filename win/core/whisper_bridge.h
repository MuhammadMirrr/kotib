// whisper.cpp C shim interfeysi (`whisper_bridge.c`). macOS nusxasi:
// `src/whisper_bridge.h` — umumiy funksiyalar imzosi bir xil.

#ifndef RUBAI_WHISPER_BRIDGE_H
#define RUBAI_WHISPER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Modelni yuklaydi (bir marta). 0 = muvaffaqiyat.
// macOS versiyasi bilan bir xil imzo — oʻzgartirilmaydi.
int rubai_load(const char *model_path);

// Windows kengaytmasi: GPU'ni majburan oʻchirish mumkin (CPU fallback uchun).
// use_gpu = 0 boʻlsa faqat CPU backend ishlatiladi.
int rubai_load_ex(const char *model_path, int use_gpu);

#ifdef _WIN32
#include <wchar.h>
// Windows kengaytmasi: yoʻl UTF-16 da, fayl `_wfopen` bilan ochiladi va
// whisper.cpp'ga oʻz oʻquvchimiz (`whisper_model_loader`) orqali beriladi.
// Kirillcha foydalanuvchi nomi (C:\Users\Аброр\…) va 8.3 qisqa nomlar
// oʻchirilgan disk uchun: llvm-mingw'da whisper.cpp yoʻlni tor `ifstream`
// bilan (ANSI kod sahifasida) ochadi va bunday yoʻl ochilmaydi.
int rubai_load_w(const wchar_t *model_path, int use_gpu);

// Silero VAD modeli yoʻli (S12) — yuklashdan OLDIN, bir marta (UTF-16, fayl
// `_wfopen` bilan ochiladi). macOS'dagi `rubai_set_vad_path` egizagi: har model
// yuklanishida VAD ham yuklanadi; berilmasa yoki yuklanmasa — boʻlaksiz ishlaydi.
void rubai_set_vad_path_w(const wchar_t *path);
#endif

// Modelni (va VAD'ni) RAM'dan boʻshatadi.
void rubai_unload(void);

// 16kHz mono float32 namunalardan lotin oʻzbek matn qaytaradi.
// Qaytgan satrni rubai_free_str bilan boʻshating. NULL = xato.
char *rubai_transcribe(const float *samples, int n_samples, int n_threads);

void rubai_free_str(char *s);

// ---- Studiya (audio fayldan matn) uchun segment API'si ---------------------
//
// macOS'dagi `src/whisper_bridge.h` dan bir xil koʻchirilgan. `rubai_transcribe`
// dan yagona ATAYLAB farqi: `no_timestamps = false` — segment chegaralari
// kerak, chunki matn shu chegaralar boʻyicha paragraflarga boʻlinadi.

#include <stdint.h>
#include <stdbool.h>

// Progress: 0..100. UI oqimini bloklamaslik uchun tez qaytsin.
typedef void (*rubai_progress_cb)(int percent, void *ud);
// true qaytarsa transkripsiya toʻxtatiladi.
typedef bool (*rubai_abort_cb)(void *ud);

// whisper_full ni segmentlar bilan ishga tushiradi.
// 0 = muvaffaqiyat, 1 = xato, 2 = bekor qilingan.
int rubai_transcribe_segments(const float *samples, int n_samples, int n_threads,
                              rubai_progress_cb pcb, void *pud, rubai_abort_cb acb, void *aud);

// Yuqoridagi 0 qaytargandan keyin natijani oʻqish uchun.
int rubai_n_segments(void);
// Ichki bufferga koʻrsatkich — chaqiruvchi NUSXALAB olsin, free QILMASIN.
// Keyingi transkripsiya bu bufferni qayta ishlatadi.
const char *rubai_segment_text(int i);
int64_t rubai_segment_t0(int i);  // 10 ms birliklarda
int64_t rubai_segment_t1(int i);

// Oxirgi xatoning qisqa tavsifi (ingliz tilida, log uchun). Hech qachon NULL emas.
const char *rubai_last_error(void);

// Faol backend nomi: "Vulkan", "CUDA", "CPU" yoki "" (yuklanmagan).
// Foydalanuvchiga qaysi rejimda ishlayotganini koʻrsatish uchun.
const char *rubai_backend_name(void);

// whisper.cpp va ggml oʻz xabarlarini stderr'ga chiqaradi. Grafik ilovada
// stderr yoʻq, shuning uchun ularni oʻz logimizga yoʻnaltiramiz.
// fn = NULL boʻlsa xabarlar butunlay oʻchiriladi.
// rubai_load dan OLDIN chaqirilishi kerak.
typedef void (*rubai_log_fn)(const char *msg);
void rubai_set_log(rubai_log_fn fn);

#ifdef __cplusplus
}
#endif

#endif
