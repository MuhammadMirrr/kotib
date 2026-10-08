// Kompyuter yonganda avtomatik ishga tushirish.
//
// macOS versiyasida bu SMAppService / LaunchAgent edi (avtostart.swift).
// Windows'da registry `Run` kaliti — administrator huquqi kerak emas.
#pragma once

namespace rubai {

bool autoStartEnabled();

// Ilova ishga tushganda BIR MARTA chaqiriladi.
//
// Ikkita eski qoldiqni tozalaydi:
//   • «Audio-Matnga» nomidagi yozuv (1.1.0 gacha ishlatilgan);
//   • yozuv bor, lekin MAVJUD BOʻLMAGAN .exe ga koʻrsatadi — ilova
//     koʻchirilgan yoki oʻrnatuvchi uni Program Files ga oʻtkazgan.
//     Bunday holda yoʻl joriy .exe ga yangilanadi: foydalanuvchi
//     «avtostart yoqilsin» degan edi va bu niyat yangilanishdan
//     keyin ham saqlanishi kerak. Aks holda Windows har kirishda
//     yoʻq faylni ishga tushirmoqchi boʻlardi va foydalanuvchi
//     ilova ochilmaganini faqat keyin sezardi.
//
// Boshqa MAVJUD nusxaga koʻrsatuvchi yozuvga TEGILMAYDI — portativ
// nusxani bir marta ochish oʻrnatilgan nusxaning avtostartini
// oʻgʻirlab ketmasin.
void avtostartniTuzat();

// Yoqadi yoki oʻchiradi. Muvaffaqiyatda true.
bool setAutoStart(bool enable);

}  // namespace rubai
