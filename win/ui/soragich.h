// Kichik modal yordamchilar: bir qatorli matn soʻrash va fayl tanlash.
//
// macOS'da bularning oʻrnini `NSAlert` + `accessoryView`, `NSOpenPanel` va
// `NSSavePanel` bajaradi. Windows'da tayyor «input box» yoʻq — shuning uchun
// eng kichik variant shu yerda bir marta yozilgan va hamma joyda ishlatiladi.
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace rubai {

// Bir qatorli matn soʻraydi. Bekor qilinsa — boʻsh satr.
std::wstring matnSora(HWND ota, const std::wstring& sarlavha, const std::wstring& izoh,
                      const std::wstring& tavsiya = L"");

// Ovozli/video fayl tanlash. Bekor qilinsa — boʻsh satr.
std::wstring faylSora(HWND ota);

// Matnni saqlash uchun joy soʻraydi. Bekor qilinsa — boʻsh satr.
std::wstring saqlashSora(HWND ota, const std::wstring& tavsiyaNom);

// Matnni almashish buferiga qoʻyadi.
bool buferGaQoy(HWND ota, const std::wstring& matn);

// Oddiy xabar oynasi — «Yaxshi» tugmasi bilan.
void ogohlantir(HWND ota, const std::wstring& sarlavha, const std::wstring& matn);

}  // namespace rubai
