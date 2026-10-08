// Transkriptlarni diskda saqlaydi. Ilova yopilsa ham mehnat yoʻqolmaydi.
//
// macOS'dagi `src/hujjat.swift` (`HujjatOmbori`) ning ekvivalenti. Papka
// tuzilishi va fayl nomlari ATAYLAB bir xil:
//
//   %APPDATA%\Kotib\hujjatlar\<uuid>\
//       hujjat.json        — maʼlumot (OXIRIDA yoziladi, «tayyor» belgisi)
//       segmentlar.json    — xom segmentlar (qayta formatlash, SRT uchun)
//       matn.txt           — oʻqishga qulay koʻrinish
//       xom.txt            — oʻzgartirilmagan
//       natijalar\<amal>.md — LLM natijalari
//
// `hujjat.json` ni OXIRIDA yozish — buzilishga qarshi himoya: fayl bor
// boʻlsa, hujjat toʻliq saqlangan. Yarim yozilgan papka roʻyxatga tushmaydi.
#pragma once

#include "matn_format.h"

#include <string>
#include <vector>

namespace rubai {

struct Hujjat {
    std::wstring id;
    std::wstring manbaNomi;
    std::wstring manbaYol;
    double davomiylik = 0;     // soniya
    long long yaratilgan = 0;  // Unix soniya
};

enum class MatnTuri { Chiroyli, Xom };

namespace HujjatOmbori {

// %APPDATA%\Kotib\hujjatlar
std::wstring ildiz();
std::wstring papka(const std::wstring& id);

// Yangi hujjatni segmentlari bilan saqlaydi. Boʻsh `id` — xato.
Hujjat saqla(const std::wstring& manbaYol, double davomiylik,
             const std::vector<Segment>& segmentlar, Apostrof apostrof);

std::wstring matnFayl(const std::wstring& id, MatnTuri tur);
bool matnSaqla(const std::wstring& id, MatnTuri tur, const std::wstring& matn);
std::wstring matnOqi(const std::wstring& id, MatnTuri tur);

// LLM natijasi. `amal` — amal identifikatori (masalan L"xulosa").
bool natijaSaqla(const std::wstring& id, const std::wstring& amal, const std::wstring& matn);
std::wstring natijaOqi(const std::wstring& id, const std::wstring& amal);

// Barcha hujjatlar, yangilari birinchi.
std::vector<Hujjat> royxat();

// Saqlangan segmentlar (qayta formatlash uchun).
std::vector<Segment> segmentlar(const std::wstring& id);

void ochir(const std::wstring& id);

}  // namespace HujjatOmbori
}  // namespace rubai
