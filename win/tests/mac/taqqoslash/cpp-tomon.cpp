// Korpusdagi har qatorni boʻlaklab, Swift tomoni bilan bir xil kanonik
// koʻrinishda chiqaradi.
#include "matn_boluvchi.h"

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

int main(int argc, char** argv) {
    std::ifstream f(argv[1], std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::wstring matn = toWide(ss.str());

    // Swift `split(omittingEmptySubsequences: false)` bilan bir xil.
    std::vector<std::wstring> qatorlar;
    size_t boshi = 0;
    for (;;) {
        const size_t joy = matn.find(L'\n', boshi);
        qatorlar.push_back(matn.substr(boshi, joy == std::wstring::npos ? joy : joy - boshi));
        if (joy == std::wstring::npos) break;
        boshi = joy + 1;
    }

    for (size_t i = 0; i < qatorlar.size(); ++i) {
        printf("--- %zu\n", i);
        const auto b = MatnBoluvchi::bol(qatorlar[i]);
        for (size_t j = 0; j < b.size(); ++j) {
            for (const auto& bolak : b[j]) {
                if (bolak.tur == Bolak::Tur::Jumla) {
                    printf("  %zu J |%s|%s|\n", j, toUtf8(bolak.matn).c_str(),
                           toUtf8(bolak.qoshimcha).c_str());
                } else {
                    printf("  %zu X |%s|\n", j, toUtf8(bolak.matn).c_str());
                }
            }
        }
    }
    return 0;
}
