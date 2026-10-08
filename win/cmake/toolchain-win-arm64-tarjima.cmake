# CTranslate2 uchun ARM64 toolchain'ning varianti.
#
# Nega alohida fayl: CTranslate2 arxitekturani `CMAKE_SYSTEM_PROCESSOR MATCHES
# "(arm64)|(aarch64)"` bilan aniqlaydi va bu tekshiruv REGISTRGA SEZGIR.
# Asosiy toolchain'da qiymat `ARM64` — whisper.cpp va win/CMakeLists.txt shu
# nom bilan ishlaydi. Mos kelmasa CTranslate2 `CT2_ARM64_BUILD` ni qoʻymaydi,
# lekin CPU dispatch'ni yoqib yuboradi va `CPU_ISA_DISPATCH` boʻsh makroga
# aylanib, kompilyatsiya «use of undeclared identifier 'ISA'» bilan yiqiladi.
include(${CMAKE_CURRENT_LIST_DIR}/toolchain-win-arm64.cmake)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
