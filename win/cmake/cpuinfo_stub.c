// ARM64 Windows uchun cpuinfo zaxira taʼrifi.
//
// cpuinfo kutubxonasi Windows'ning ARM64 versiyasini qoʻllab-quvvatlamaydi
// (uning CMake'i `CPUINFO_SUPPORTED_PLATFORM=0` qoʻyadi va `cpuinfo_isa`
// obyektini umuman qurmaydi). ruy esa uni har doim mavjud deb hisoblaydi va
// linker «undefined symbol: cpuinfo_isa» beradi.
//
// Nolga toʻldirilgan taʼrif AYNAN kerakli xatti-harakatni beradi: ruy hech
// qanday ARM kengaytmasi (dotprod, i8mm) yoʻq deb hisoblab, ARM64 da doim
// mavjud boʻlgan asosiy NEON yadrolarini tanlaydi. Bu — toʻgʻri va xavfsiz
// tanlov; ARM64 build faqat VM'da sinash uchun ishlatiladi (docs/windows/WINDOWS-PARITET.md).
//
// x64 da bu fayl QATNASHMAYDI: u yerda cpuinfo toʻliq ishlaydi.
#include <cpuinfo.h>

struct cpuinfo_arm_isa cpuinfo_isa;
