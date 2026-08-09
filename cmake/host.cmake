# Host (native) toolchain file. Purpose: apply the host-build discipline --
# strict flags, reproducibility, sanitizer/coverage helpers -- without telling
# CMake it is cross-compiling (which a CMAKE_SYSTEM_NAME override would do even
# when it matches the host). A native build is CMake's default.
#
# The firmware sub-build is guarded by CMAKE_CROSSCOMPILING in the root
# CMakeLists, so this file must leave that flag FALSE.

# Reproducibility + identical flags to the target build where they overlap.
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)

# ASAN/UBSAN helper. Enable with `-DENABLE_SANITIZERS=ON`; the host-test-asan
# preset does exactly this (03-toolchain.md §4.1).
set(ENABLE_SANITIZERS OFF CACHE BOOL "Enable AddressSanitizer + UndefinedBehaviorSanitizer")
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Coverage helper. Enable with `-DENABLE_COVERAGE=ON`; gcovr reads the .gcda
# files produced by instrumented host tests.
set(ENABLE_COVERAGE OFF CACHE BOOL "Build host tests with gcov instrumentation")
if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()
