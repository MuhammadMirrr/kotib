// Kotib q4 sinovi — ilovaning OʻZ kod yoʻlidan transkripsiya qiluvchi CLI.
//
//   kotib-sinov --model <model.bin> [--vad <silero.bin>] --out <papka> <audio1> [audio2 …]
//   (--vad — ilovadagi kabi Silero VAD bilan boʻlaklash, S12; berilmasa boʻlaksiz)
//
// Har fayl uchun:
//   media_decode.swift (AVFoundation) → 16 kHz mono float32
//   audio_util.swift `kuchaytir`      → diktovka yoʻli bilan bir xil normallash
//   whisper_bridge.c `rubai_transcribe` (uz, beam 5, no_speech 0.25, flash attn)
//   text_format.swift `apostrofniBirxillashtir(.standart)`
// Yoziladi: <out>/<nom>.txt (normallangan), <out>/<nom>.raw.txt (whisper xom),
//           <out>/vaqt.tsv, <out>/meta.json.

import Foundation

func xato(_ s: String) -> Never {
    FileHandle.standardError.write(("XATO: " + s + "\n").data(using: .utf8)!)
    exit(1)
}

func ms(_ a: UInt64, _ b: UInt64) -> Double { Double(b &- a) / 1_000_000.0 }

var args = Array(CommandLine.arguments.dropFirst())
var modelYoli: String? = nil
var chiqish: String? = nil
var vadYoli: String? = nil
var fayllar: [String] = []
while !args.isEmpty {
    let a = args.removeFirst()
    switch a {
    case "--model": modelYoli = args.isEmpty ? nil : args.removeFirst()
    case "--out": chiqish = args.isEmpty ? nil : args.removeFirst()
    case "--vad": vadYoli = args.isEmpty ? nil : args.removeFirst()
    default: fayllar.append(a)
    }
}
guard let model = modelYoli, let out = chiqish, !fayllar.isEmpty else {
    xato("foydalanish: kotib-sinov --model <model.bin> --out <papka> <audio…>")
}
try? FileManager.default.createDirectory(atPath: out, withIntermediateDirectories: true)

// Ilova bilan bir xil oqimlar soni (whisper.swift: max(4, yadrolar - 2)).
let threads = Int32(max(4, ProcessInfo.processInfo.activeProcessorCount - 2))

// 1) Model yuklash (VAD — ilovadagi kabi, yuklashdan oldin)
if let vadYoli { rubai_set_vad_path(vadYoli) }
let t0 = DispatchTime.now().uptimeNanoseconds
let rc = rubai_load(model)
let t1 = DispatchTime.now().uptimeNanoseconds
guard rc == 0 else { xato("rubai_load = \(rc) (\(model))") }
let yuklashMs = ms(t0, t1)

// 2) Isitish: 2 s jimlik. Birinchi chaqiruv Metal yadrolarini tayyorlaydi —
// uning narxi alohida yoziladi va RTF ga qoʻshilmaydi.
let jimlik = [Float](repeating: 0, count: 32000)
let t2 = DispatchTime.now().uptimeNanoseconds
jimlik.withUnsafeBufferPointer { buf in
    if let c = rubai_transcribe(buf.baseAddress, Int32(buf.count), threads) { rubai_free_str(c) }
}
let t3 = DispatchTime.now().uptimeNanoseconds
let isitishMs = ms(t2, t3)

var tsv = "fayl\taudio_s\tdecode_ms\ttranscribe_ms\trtf\tbelgilar\n"
var jamiAudio = 0.0, jamiTr = 0.0

for f in fayllar {
    let url = URL(fileURLWithPath: f)
    let nom = url.deletingPathExtension().lastPathComponent
    let d0 = DispatchTime.now().uptimeNanoseconds
    let namunalar: [Float]
    do {
        let m = try mediaMalumot(url)
        namunalar = try namunalarniOqi(
            url, boshi: 0, oxiri: m.davomiylik,
            progress: { _ in }, bekor: { false })
    } catch {
        let x = (error as? MediaXato)?.xabar ?? "\(error)"
        FileHandle.standardError.write("DEKOD XATO \(url.lastPathComponent): \(x)\n".data(using: .utf8)!)
        tsv += "\(nom)\tNA\tNA\tNA\tNA\tDEKOD_XATO\n"
        continue
    }
    let d1 = DispatchTime.now().uptimeNanoseconds
    let tayyor = kuchaytir(namunalar)
    let audioS = Double(tayyor.count) / 16000.0

    var matn = ""
    let r0 = DispatchTime.now().uptimeNanoseconds
    tayyor.withUnsafeBufferPointer { buf in
        if let c = rubai_transcribe(buf.baseAddress, Int32(buf.count), threads) {
            matn = String(cString: c).trimmingCharacters(in: .whitespacesAndNewlines)
            rubai_free_str(c)
        }
    }
    let r1 = DispatchTime.now().uptimeNanoseconds
    let trMs = ms(r0, r1)
    jamiAudio += audioS; jamiTr += trMs / 1000.0

    let normal = apostrofniBirxillashtir(matn, .standart)
    try? (matn + "\n").write(toFile: out + "/" + nom + ".raw.txt", atomically: true, encoding: .utf8)
    try? (normal + "\n").write(toFile: out + "/" + nom + ".txt", atomically: true, encoding: .utf8)
    tsv += String(
        format: "%@\t%.3f\t%.1f\t%.1f\t%.4f\t%d\n",
        nom, audioS, ms(d0, d1), trMs, trMs / 1000.0 / audioS, normal.count)
    print(
        String(
            format: "  %@  audio=%.1fs  transkripsiya=%.2fs  RTF=%.3f",
            nom, audioS, trMs / 1000.0, trMs / 1000.0 / audioS))
}
rubai_unload()

try? tsv.write(toFile: out + "/vaqt.tsv", atomically: true, encoding: .utf8)
let meta: [String: Any] = [
    "model": model, "threads": Int(threads),
    "load_ms": yuklashMs, "warmup_ms": isitishMs,
    "audio_s_total": jamiAudio, "transcribe_s_total": jamiTr,
    "rtf_total": jamiAudio > 0 ? jamiTr / jamiAudio : 0
]
if let d = try? JSONSerialization.data(withJSONObject: meta, options: [.prettyPrinted, .sortedKeys]) {
    try? d.write(to: URL(fileURLWithPath: out + "/meta.json"))
}
print(
    String(
        format: "yuklash=%.0f ms  isitish=%.0f ms  jami audio=%.1fs  jami transkripsiya=%.2fs  RTF=%.3f",
        yuklashMs, isitishMs, jamiAudio, jamiTr, jamiAudio > 0 ? jamiTr / jamiAudio : 0))
