# macOS'dan Windows ARM64 ga cross-compile (llvm-mingw).
# Nega llvm-mingw va nega GCC-mingw emas: `win/core/audio_capture.cpp` WASAPI'ni
# `__uuidof` bilan chaqiradi — uni faqat clang tushunadi.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)

# Toolchain joyi: TOOLCHAIN muhit oʻzgaruvchisi yoki bogliqliklar.env dagi versiya.
include(${CMAKE_CURRENT_LIST_DIR}/llvm-mingw.cmake)
set(TC "${KOTIB_LLVM_MINGW_BIN}")
set(CMAKE_C_COMPILER   "${TC}/aarch64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${TC}/aarch64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER  "${TC}/aarch64-w64-mingw32-windres")
set(CMAKE_AR           "${TC}/llvm-ar")
set(CMAKE_RANLIB       "${TC}/llvm-ranlib")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

