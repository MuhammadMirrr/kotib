// Kotib — tarjimon qoʻllab-quvvatlaydigan tillar.
//
// NLLB-200 modeli 202 tilni biladi va ularning HAMMASI bitta model faylida
// yotadi — qoʻshimcha til qoʻshish na hajm, na tezlik jihatidan hech narsa
// turmaydi. Shuning uchun roʻyxat cheklanmagan: interfeys qidiruvli.
//
// Kodlar modelning oʻz lugʻatidan (`shared_vocabulary.json`) olingan —
// qoʻlda yozilmagan. Model almashtirilsa roʻyxat ham qayta olinishi kerak.
//
// Nomlarni macOS oʻzi beradi (`Locale(identifier: "uz")`), shuning uchun 202 ta
// nom qoʻlda tarjima qilinmaydi. Tizim nomlari `o‘zbek` koʻrinishida keladi —
// notoʻgʻri apostrof bilan; `apostrofniBirxillashtir` uni `oʻzbek` ga keltiradi
// (AGENTS.md dagi apostrof qoidasi).
//
// Foundation'dan boshqa hech narsa import qilmaydi — test.sh buni qamraydi.

import Foundation

struct Til: Equatable, Hashable {

    /// NLLB kodi, masalan "uzn_Latn". Modelga aynan shu beriladi.
    let nllb: String

    /// ISO til kodi ("uzn"), nom qidirish uchun.
    var kod: String { String(nllb.prefix(3)) }

    /// Yozuv kodi ("Latn").
    var yozuv: String { String(nllb.suffix(4)) }

    /// Foydalanuvchi koʻradigan nom, oʻzbekcha.
    var nom: String { Til.nomlar[nllb] ?? nllb }

    // Standart tanlovlar.
    static let uz = Til(nllb: "uzn_Latn")
    static let ru = Til(nllb: "rus_Cyrl")
    static let en = Til(nllb: "eng_Latn")

    /// Barcha tillar, oʻzbekcha nomi boʻyicha saralangan.
    static let hammasi: [Til] = kodlar.map(Til.init(nllb:))
        .sorted { $0.nom.localizedCaseInsensitiveCompare($1.nom) == .orderedAscending }

    static func topilsin(_ nllb: String) -> Til? {
        kodlar.contains(nllb) ? Til(nllb: nllb) : nil
    }

    // MARK: Nomlar

    /// Yozuv nomlari — bir til bir necha yozuvda boʻlganda ajratish uchun.
    private static let yozuvNomlari: [String: String] = [
        "Latn": "lotin", "Cyrl": "kirill", "Arab": "arab", "Deva": "devanagari",
        "Ethi": "efiop", "Hans": "soddalashtirilgan", "Hant": "anʼanaviy",
        "Beng": "bengal", "Guru": "gurmuxi", "Gujr": "gujarot", "Orya": "odia",
        "Taml": "tamil", "Telu": "telugu", "Knda": "kannada", "Mlym": "malayalam",
        "Sinh": "singal", "Thai": "tay", "Laoo": "laos", "Mymr": "birma",
        "Khmr": "kxmer", "Tibt": "tibet", "Grek": "grek", "Hebr": "ibroniy",
        "Armn": "arman", "Geor": "gruzin", "Jpan": "yapon", "Hang": "hangil",
        "Tfng": "tifinag", "Nkoo": "nko", "Olck": "ol-chiki", "Cans": "kanada"
    ]

    /// macOS bilmaydigan yoki oʻzbekchasi yoʻq tillar — qoʻlda yozilgan.
    ///
    /// `Locale(identifier: "uz")` maʼlumoti toʻliq emas: 13 til uchun umuman
    /// nom yoʻq, yana 11 tasi uchun u ruscha nom qaytaradi (CLDR fallback
    /// zanjiri). Oʻzbekcha interfeysda ruscha yoki inglizcha nom turishi
    /// notoʻgʻri, shuning uchun bular shu yerda.
    private static let qoshimchaNomlar: [String: String] = [
        "acm": "Iroq arabchasi", "acq": "Yaman arabchasi",
        "aeb": "Tunis arabchasi", "ajp": "Janubiy Levant arabchasi",
        "apc": "Shimoliy Levant arabchasi", "ary": "Marokash arabchasi",
        "arz": "Misr arabchasi", "azb": "Janubiy ozarbayjon",
        "bjn": "Banjar", "cjk": "Chokve",
        "crh": "Qrim tatarchasi", "dik": "Dinka",
        "dyu": "Diula", "fuv": "Nigeriya fulfuldesi",
        "hne": "Chxattisgarxi", "kbp": "Kabiye",
        "kon": "Kongo", "ltg": "Latgal",
        "luo": "Luo", "pbt": "Janubiy pushtu",
        "quy": "Ayakucho kechuasi", "taq": "Tamashek"
    ]

    /// Nom kirill alifbosidami — macOS oʻzbekcha nomni bilmasa ruschasini
    /// qaytaradi, uni ishlatib boʻlmaydi (ilova butunlay lotin).
    private static func kirilmi(_ s: String) -> Bool {
        s.unicodeScalars.contains { (0x0400...0x04FF).contains($0.value) }
    }

    /// Bitta til uchun asosiy nom: qoʻlda yozilgan → oʻzbekcha (lotin boʻlsa)
    /// → inglizcha → kodning oʻzi.
    private static func asosiyNom(_ kod: String, _ uzLocale: Locale, _ enLocale: Locale) -> String {
        if let q = qoshimchaNomlar[kod] { return q }
        if let u = uzLocale.localizedString(forLanguageCode: kod), !u.isEmpty, !kirilmi(u) { return u }
        if let e = enLocale.localizedString(forLanguageCode: kod), !e.isEmpty { return e }
        return kod
    }

    /// Hech bir manbadan nom topilmagan kodlar — testlar shu boʻsh boʻlishini
    /// tekshiradi. Boʻsh boʻlmasa, `qoshimchaNomlar` ga nom qoʻshish kerak.
    static var nomsizlar: [String] {
        let uzLocale = Locale(identifier: "uz")
        let enLocale = Locale(identifier: "en")
        return kodlar.filter { k in
            let kod = String(k.prefix(3))
            if qoshimchaNomlar[kod] != nil { return false }
            if let u = uzLocale.localizedString(forLanguageCode: kod), !u.isEmpty, !kirilmi(u) { return false }
            if let e = enLocale.localizedString(forLanguageCode: kod), !e.isEmpty { return false }
            return true
        }
    }

    /// `nllb` → oʻzbekcha nom. Bir marta hisoblanadi.
    ///
    /// Bir xil nomga ega bir necha yozuv boʻlsa (masalan "ace_Arab" va
    /// "ace_Latn"), nomga yozuv qavs ichida qoʻshiladi — aks holda roʻyxatda
    /// ikkita bir xil band chiqadi va qaysi biri nima ekani bilinmaydi.
    private static let nomlar: [String: String] = {
        let uzLocale = Locale(identifier: "uz")
        let enLocale = Locale(identifier: "en")
        var asos: [String: String] = [:]
        for k in kodlar {
            let xom = asosiyNom(String(k.prefix(3)), uzLocale, enLocale)
            asos[k] = boshHarf(apostrofniBirxillashtir(xom, .standart))
        }

        // 1-bosqich: bir xil nomga yozuvni qoʻshamiz — "Achin (arab)" / "Achin (lotin)".
        var sanoq: [String: Int] = [:]
        for (_, n) in asos { sanoq[n, default: 0] += 1 }
        var oraliq: [String: String] = [:]
        for (k, n) in asos {
            if sanoq[n, default: 0] > 1 {
                let y = yozuvNomlari[String(k.suffix(4))] ?? String(k.suffix(4))
                oraliq[k] = "\(n) (\(y))"
            } else {
                oraliq[k] = n
            }
        }

        // 2-bosqich: yozuv ham bir xil boʻlsa (macOS `aka` va `twi` ni ikkalasini
        // "Akan" deydi) — ISO kodini qoʻshamiz.
        var sanoq2: [String: Int] = [:]
        for (_, n) in oraliq { sanoq2[n, default: 0] += 1 }
        var natija: [String: String] = [:]
        for (k, n) in oraliq {
            natija[k] =
                sanoq2[n, default: 0] > 1
                ? "\(asos[k] ?? n) (\(String(k.prefix(3))))"
                : n
        }
        return natija
    }()

    private static func boshHarf(_ s: String) -> String {
        guard let birinchi = s.first else { return s }
        return String(birinchi).uppercased() + s.dropFirst()
    }

    /// Model lugʻatidan olingan 202 ta kod (`shared_vocabulary.json`).
    static let kodlar: [String] = [
        "ace_Arab", "ace_Latn", "acm_Arab", "acq_Arab", "aeb_Arab", "afr_Latn",
        "ajp_Arab", "aka_Latn", "als_Latn", "amh_Ethi", "apc_Arab", "arb_Arab",
        "ars_Arab", "ary_Arab", "arz_Arab", "asm_Beng", "ast_Latn", "awa_Deva",
        "ayr_Latn", "azb_Arab", "azj_Latn", "bak_Cyrl", "bam_Latn", "ban_Latn",
        "bel_Cyrl", "bem_Latn", "ben_Beng", "bho_Deva", "bjn_Arab", "bjn_Latn",
        "bod_Tibt", "bos_Latn", "bug_Latn", "bul_Cyrl", "cat_Latn", "ceb_Latn",
        "ces_Latn", "cjk_Latn", "ckb_Arab", "crh_Latn", "cym_Latn", "dan_Latn",
        "deu_Latn", "dik_Latn", "dyu_Latn", "dzo_Tibt", "ell_Grek", "eng_Latn",
        "epo_Latn", "est_Latn", "eus_Latn", "ewe_Latn", "fao_Latn", "fij_Latn",
        "fin_Latn", "fon_Latn", "fra_Latn", "fur_Latn", "fuv_Latn", "gaz_Latn",
        "gla_Latn", "gle_Latn", "glg_Latn", "grn_Latn", "guj_Gujr", "hat_Latn",
        "hau_Latn", "heb_Hebr", "hin_Deva", "hne_Deva", "hrv_Latn", "hun_Latn",
        "hye_Armn", "ibo_Latn", "ilo_Latn", "ind_Latn", "isl_Latn", "ita_Latn",
        "jav_Latn", "jpn_Jpan", "kab_Latn", "kac_Latn", "kam_Latn", "kan_Knda",
        "kas_Arab", "kas_Deva", "kat_Geor", "kaz_Cyrl", "kbp_Latn", "kea_Latn",
        "khk_Cyrl", "khm_Khmr", "kik_Latn", "kin_Latn", "kir_Cyrl", "kmb_Latn",
        "kmr_Latn", "knc_Arab", "knc_Latn", "kon_Latn", "kor_Hang", "lao_Laoo",
        "lij_Latn", "lim_Latn", "lin_Latn", "lit_Latn", "lmo_Latn", "ltg_Latn",
        "ltz_Latn", "lua_Latn", "lug_Latn", "luo_Latn", "lus_Latn", "lvs_Latn",
        "mag_Deva", "mai_Deva", "mal_Mlym", "mar_Deva", "min_Latn", "mkd_Cyrl",
        "mlt_Latn", "mni_Beng", "mos_Latn", "mri_Latn", "mya_Mymr", "nld_Latn",
        "nno_Latn", "nob_Latn", "npi_Deva", "nso_Latn", "nus_Latn", "nya_Latn",
        "oci_Latn", "ory_Orya", "pag_Latn", "pan_Guru", "pap_Latn", "pbt_Arab",
        "pes_Arab", "plt_Latn", "pol_Latn", "por_Latn", "prs_Arab", "quy_Latn",
        "ron_Latn", "run_Latn", "rus_Cyrl", "sag_Latn", "san_Deva", "sat_Beng",
        "scn_Latn", "shn_Mymr", "sin_Sinh", "slk_Latn", "slv_Latn", "smo_Latn",
        "sna_Latn", "snd_Arab", "som_Latn", "sot_Latn", "spa_Latn", "srd_Latn",
        "srp_Cyrl", "ssw_Latn", "sun_Latn", "swe_Latn", "swh_Latn", "szl_Latn",
        "tam_Taml", "taq_Latn", "taq_Tfng", "tat_Cyrl", "tel_Telu", "tgk_Cyrl",
        "tgl_Latn", "tha_Thai", "tir_Ethi", "tpi_Latn", "tsn_Latn", "tso_Latn",
        "tuk_Latn", "tum_Latn", "tur_Latn", "twi_Latn", "tzm_Tfng", "uig_Arab",
        "ukr_Cyrl", "umb_Latn", "urd_Arab", "uzn_Latn", "vec_Latn", "vie_Latn",
        "war_Latn", "wol_Latn", "xho_Latn", "ydd_Hebr", "yor_Latn", "yue_Hant",
        "zho_Hans", "zho_Hant", "zsm_Latn", "zul_Latn"
    ]
}
