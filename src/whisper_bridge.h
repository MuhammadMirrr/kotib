// whisper.cpp C shim interfeysi (`whisper_bridge.c`). Swift unga
// `Bridging.h` orqali kiradi. Windows nusxasi: `win/core/whisper_bridge.h`.

#ifndef RUBAI_WHISPER_BRIDGE_H
#define RUBAI_WHISPER_BRIDGE_H

// Modelni yuklaydi (bir marta). 0 = muvaffaqiyat.
int rubai_load(const char *model_path);

// Silero VAD modeli yoʻli (S12) — `rubai_load` dan OLDIN, bir marta. Har
// model yuklanishida VAD ham yuklanadi: nutq yoʻq ovozda whisper chaqirilmaydi,
// uzun ovoz jimliklardan boʻlinadi (`nutq_bolaklari.h`). Yoʻl berilmasa yoki
// VAD yuklanmasa — 1.1 dagi kabi boʻlaksiz ishlaydi.
void rubai_set_vad_path(const char *path);

// Modelni (va VAD'ni) RAM'dan boʻshatadi.
void rubai_unload(void);

// 16kHz mono float32 namunalardan lotin oʻzbek matn qaytaradi.
// Qaytgan satrni rubai_free_str bilan boʻshating. NULL = xato.
char *rubai_transcribe(const float *samples, int n_samples, int n_threads);

void rubai_free_str(char *s);

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
const char *rubai_segment_text(int i);
int64_t rubai_segment_t0(int i);  // 10 ms birliklarda
int64_t rubai_segment_t1(int i);

// Oxirgi xatoning qisqa tavsifi (ingliz tilida, log uchun). Hech qachon NULL emas.
const char *rubai_last_error(void);

// Faol backend nomi: "Metal", "CPU" yoki "" (yuklanmagan).
const char *rubai_backend_name(void);

// whisper.cpp va ggml oʻz xabarlarini stderr'ga chiqaradi. Grafik ilovada
// stderr yoʻq, shuning uchun ularni oʻz logimizga yoʻnaltiramiz.
// fn = NULL boʻlsa xabarlar butunlay oʻchiriladi.
// rubai_load dan OLDIN chaqirilishi kerak. (`win/core/whisper_bridge.h` bilan bir xil.)
typedef void (*rubai_log_fn)(const char *msg);
void rubai_set_log(rubai_log_fn fn);

#endif
