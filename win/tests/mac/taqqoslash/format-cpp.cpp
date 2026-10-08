// Windows tomonining matn formatlash chiqishi — Swift tomoni bilan bir xil
// koʻrinishda.
#include "matn_format.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace rubai;
namespace rubai {
std::string toUtf8(const std::wstring&);
std::wstring toWide(const std::string&);
}  // namespace rubai

namespace {

std::vector<std::wstring> qatorlar(const std::wstring& matn) {
    std::vector<std::wstring> chiqish;
    size_t boshi = 0;
    for (;;) {
        const size_t joy = matn.find(L'\n', boshi);
        chiqish.push_back(matn.substr(boshi, joy == std::wstring::npos ? joy : joy - boshi));
        if (joy == std::wstring::npos) break;
        boshi = joy + 1;
    }
    return chiqish;
}

bool boshmi(const std::wstring& s) { return s.find_first_not_of(L" \t\r") == std::wstring::npos; }

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    std::ifstream f(argv[1], std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();

    std::vector<std::vector<Segment>> hujjatlar;
    std::vector<Segment> joriy;
    for (const auto& q : qatorlar(toWide(ss.str()))) {
        if (!q.empty() && q[0] == L'#') continue;
        if (boshmi(q)) {
            if (!joriy.empty()) {
                hujjatlar.push_back(joriy);
                joriy.clear();
            }
            continue;
        }
        // `t0 t1 matn` — matnda boʻshliq boʻlishi mumkin, shuning uchun
        // faqat birinchi ikkita boʻshliq boʻyicha ajratamiz.
        const size_t b1 = q.find(L' ');
        if (b1 == std::wstring::npos) continue;
        const size_t b2 = q.find(L' ', b1 + 1);
        Segment s;
        s.t0 = std::stod(q.substr(0, b1));
        s.t1 = std::stod(q.substr(b1 + 1, b2 == std::wstring::npos ? b2 : b2 - b1 - 1));
        s.matn = (b2 == std::wstring::npos) ? L"" : q.substr(b2 + 1);
        joriy.push_back(std::move(s));
    }
    if (!joriy.empty()) hujjatlar.push_back(joriy);

    for (size_t i = 0; i < hujjatlar.size(); ++i) {
        printf("=== hujjat %zu\n", i);
        printf("--- xom\n%s\n", toUtf8(xomMatn(hujjatlar[i])).c_str());
        printf("--- chiroyli (standart apostrof)\n%s\n",
               toUtf8(chiroyliMatn(hujjatlar[i], Apostrof::Standart)).c_str());
        printf("--- chiroyli (oddiy apostrof)\n%s\n",
               toUtf8(chiroyliMatn(hujjatlar[i], Apostrof::Oddiy)).c_str());
        printf("--- diktovka (standart apostrof)\n%s\n",
               toUtf8(matnniTayyorla(xomMatn(hujjatlar[i]), Apostrof::Standart)).c_str());
        printf("--- diktovka (oddiy apostrof)\n%s\n",
               toUtf8(matnniTayyorla(xomMatn(hujjatlar[i]), Apostrof::Oddiy)).c_str());
    }
    return 0;
}
