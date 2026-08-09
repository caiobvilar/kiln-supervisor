set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# -mcpu and -mfloat-abi are chosen by the consuming project (per-board), so the
# toolchain file only sets the ABI-agnostic core flags. Override TARGET_CPU in
# the project CMakeLists if the default does not match the target.
set(TARGET_CPU "cortex-m4" CACHE STRING "ARM core to compile for")
set(CMAKE_C_FLAGS_INIT "-mcpu=${TARGET_CPU} -mthumb")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=${TARGET_CPU} -mthumb")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=${TARGET_CPU} -mthumb")

set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3 -gdwarf-4")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Os -g0")
set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g3")

# Strip absolute source paths so builds are reproducible and the artifact is
# path-independent (03-toolchain.md §5).
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

find_program(ARM_SIZE arm-none-eabi-size REQUIRED)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
