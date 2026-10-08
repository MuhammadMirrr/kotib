// Mikrofondan yozib olish — WASAPI.
//
// macOS versiyasidagi `Recorder` sinfining ekvivalenti (yozuvchi.swift).
// Natija: 16 kHz mono float32 — whisper aynan shuni kutadi.
#pragma once

#include <string>
#include <vector>

namespace rubai {

// Mikrofonning turi. Windows'da qurilmalar roʻyxati chalgʻituvchi boʻladi:
// "Stereo Mix" mikrofon emas (kompyuter ovozini yozadi), Bluetooth
// quloqchin esa past sifatli. Foydalanuvchini ogohlantirish uchun kerak.
enum class MicKind {
    Normal,
    Loopback,   // Stereo Mix / "What U Hear" — ovoz emas, tizim ovozi
    Bluetooth,  // HFP profil: 8-16 kHz, siqilgan — aniqlik pasayadi
    Virtual,    // Iriun, OBS, VB-Cable — manba ishlamasa jim oqim beradi
};

struct MicDevice {
    std::wstring id;    // WASAPI qurilma ID (sozlamalarda saqlanadi)
    std::wstring name;  // foydalanuvchiga koʻrinadigan nom
    MicKind kind = MicKind::Normal;
    bool isDefault = false;

    // Foydalanuvchi uchun ogohlantirish matni; ogohlantirish yoʻq boʻlsa boʻsh.
    std::wstring warning() const;
};

// Tizimdagi faol kirish qurilmalari. Xato boʻlsa boʻsh roʻyxat.
std::vector<MicDevice> listMicrophones();

// Yozib oluvchi. Bir vaqtda faqat bitta yozuv.
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    // Yozishni boshlaydi. deviceId boʻsh boʻlsa tizim standarti ishlatiladi.
    //
    // Har chaqiruvda WASAPI klienti YANGIDAN yaratiladi. macOS versiyasi ham
    // shunday qiladi (yozuvchi.swift) — qurilma almashtirilgandan yoki
    // kompyuter uyqudan uygʻongandan keyin eski klient qotib qoladi.
    bool start(const std::wstring& deviceId, std::wstring& error);

    // Yozishni toʻxtatib, 16 kHz mono float32 namunalarni qaytaradi.
    std::vector<float> stop();

    bool isRecording() const;

    // Yozish davomida oqim uzilganmi (qurilma chiqarib olindi va h.k.).
    bool deviceLost() const;

    // Oxirgi kelgan boʻlakning signal darajasi (0…1, taxminan RMS).
    // Yozish oynasidagi toʻlqin chizigʻi uchun — boshqa hech qayerda
    // ishlatilmaydi va aniqligi muhim emas, faqat «jonli» koʻrinsin.
    float daraja() const;

private:
    struct Impl;
    Impl* d;
};

}  // namespace rubai
