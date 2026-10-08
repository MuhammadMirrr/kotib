// Kotib — tarjima dvigatelining Swift qatlami.
//
// `Whisper` (whisper.swift) bilan bir xil naqsh:
//   • jarayon-global bitta model, alohida navbatda ishlaydi;
//   • `band` bayrogʻi — ish ketayotganda idle taymer modelni boʻshatmaydi;
//   • 180 soniya ishlatilmasa model RAM'dan boʻshaydi.
//
// Nega boʻshatish muhim: ovoz modeli ~1 GB, tarjima modeli ~1,5 GB RAM oladi.
// Ikkalasi doim xotirada tursa 16 GB li mashina swap'ga tushadi — bu ishlab
// chiqish paytida haqiqatan koʻrildi.
//
// Bekor qilish JUMLALAR ORASIDA ishlaydi: CTranslate2 tarjima oʻrtasida
// toʻxtashni qoʻllamaydi. Bitta jumla bir soniyadan kam vaqt oladi, shuning
// uchun foydalanuvchi uchun bu sezilmaydi.

import Foundation

final class Tarjimon {

    static let shared = Tarjimon()
    private init() {}

    /// Ish ketayotganini bildiradi — idle taymer shunga qarab kutadi.
    private(set) var band = false

    private let q = DispatchQueue(label: "com.rubaistt.dictation.tarjima")
    private var bekorSoralgan = false
    private var idleTimer: Timer?

    var yuklanganmi: Bool { rubai_tarjima_yuklanganmi() == 1 }

    // MARK: Model

    /// Modelni yuklaydi. Asosiy oqimda CHAQIRILMASIN — bir necha soniya oladi.
    @discardableResult
    func yukla() -> Bool {
        guard TarjimaModel.tayyor else {
            RubaiLog.write("tarjima: model papkasi toʻliq emas")
            return false
        }
        if rubai_tarjima_yukla(TarjimaModel.papka.path) == 0 { return true }
        RubaiLog.write("tarjima: model yuklanmadi (xotira yoki buzuq papka)")
        return false
    }

    func bosat() {
        guard yuklanganmi else { return }
        rubai_tarjima_bosat()
        RubaiLog.write("tarjima: model RAM'dan boʻshatildi")
    }

    /// 3 daqiqa ishlatilmasa modelni boʻshatadi. Ish ketayotgan boʻlsa taymer
    /// SHUNCHAKI oʻtkazib yuborilmaydi — qayta qurollanadi, shu bilan ish
    /// tugagach ham boʻshatish amalga oshadi (ilova.swift dagi bilan bir xil).
    private func bosatishniRejalashtir() {
        DispatchQueue.main.async {
            self.idleTimer?.invalidate()
            self.idleTimer = Timer.scheduledTimer(withTimeInterval: 180, repeats: false) { [weak self] _ in
                guard let self else { return }
                if self.band {
                    self.bosatishniRejalashtir()
                } else {
                    self.q.async { self.bosat() }
                }
            }
        }
    }

    // MARK: Tarjima

    func bekorQil() { bekorSoralgan = true }

    /// Matnni tarjima qiladi. Barcha callback'lar ASOSIY oqimda chaqiriladi.
    /// - jarayon: (bajarilgan, jami) — jumlalar boʻyicha.
    /// - tugadi: yigʻilgan matn, yoki xato.
    func tarjimaQil(
        matn: String, manba: Til, maqsad: Til,
        jarayon: @escaping (Int, Int) -> Void,
        tugadi: @escaping (Result<String, Error>) -> Void
    ) {

        let qatorlar = MatnBoluvchi.bol(matn)
        let manbaJumlalar = MatnBoluvchi.jumlalar(qatorlar)
        guard !manbaJumlalar.isEmpty else {
            // Tarjima qiladigan narsa yoʻq (boʻsh yoki faqat belgilar).
            DispatchQueue.main.async { tugadi(.success(matn)) }
            return
        }
        guard manba != maqsad else {
            DispatchQueue.main.async { tugadi(.success(matn)) }
            return
        }

        bekorSoralgan = false
        band = true
        let jami = manbaJumlalar.count
        // Anonim oʻlchov uchun: belgi soni va boshlanish vaqti. Matnning oʻzi
        // EMAS — faqat uzunligi va davomiyligi (statistika.swift maxfiylik).
        let belgiSoni = matn.count
        let boshlandi = Date().timeIntervalSinceReferenceDate

        q.async { [weak self] in
            guard let self else { return }
            defer {
                self.band = false
                self.bosatishniRejalashtir()
            }

            if !self.yuklanganmi, !self.yukla() {
                Statistika.amalYubor(
                    tur: "tarjima", belgi: belgiSoni,
                    ishlov_s: Date().timeIntervalSinceReferenceDate - boshlandi,
                    backend: "cpu", natija: "xato:model")
                DispatchQueue.main.async { tugadi(.failure(TarjimaXatosi.modelYuklanmadi)) }
                return
            }

            var tarjimalar: [String] = []
            tarjimalar.reserveCapacity(jami)
            for jumla in manbaJumlalar {
                if self.bekorSoralgan {
                    DispatchQueue.main.async { tugadi(.failure(TarjimaXatosi.bekorQilindi)) }
                    return
                }
                guard let p = rubai_tarjima(jumla, manba.nllb, maqsad.nllb) else {
                    // Bitta jumla chiqmasa butun ish toʻxtamaydi — asl matn
                    // qoladi. Foydalanuvchi matni logga YOZILMAYDI, faqat raqam.
                    RubaiLog.write("tarjima: \(tarjimalar.count + 1)-jumla tarjima qilinmadi")
                    tarjimalar.append(jumla)
                    continue
                }
                tarjimalar.append(String(cString: p))
                rubai_tarjima_str_bosat(p)
                let bajarilgan = tarjimalar.count
                DispatchQueue.main.async { jarayon(bajarilgan, jami) }
            }

            let natija = MatnBoluvchi.yig(qatorlar, tarjimalar: tarjimalar)
            Statistika.amalYubor(
                tur: "tarjima", belgi: belgiSoni,
                ishlov_s: Date().timeIntervalSinceReferenceDate - boshlandi,
                backend: "cpu")
            DispatchQueue.main.async { tugadi(.success(natija)) }
        }
    }
}

enum TarjimaXatosi: LocalizedError {
    case modelYuklanmadi
    case bekorQilindi

    var errorDescription: String? {
        switch self {
        case .modelYuklanmadi:
            return "Tarjima modeli yuklanmadi. Xotira yetarli boʻlmasa, boshqa ilovalarni yopib koʻring."
        case .bekorQilindi:
            return "Tarjima bekor qilindi."
        }
    }
}
