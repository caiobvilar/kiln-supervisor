cmake_minimum_required(VERSION 3.25)
project(project_slug C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- Host-testable domain logic. This is the part the L1 (host unit test)
# gate exercises on the native compiler with no MCU or CubeMX dependency.
# Domain code may not include anything hardware-specific (see
# tools/check_layering.py).
add_library(domain STATIC src/domain/domain.c)
target_include_directories(domain PUBLIC include src/domain src/ports)
target_link_libraries(domain PUBLIC m)

# ---- Host-side unit tests. Only built when not cross-compiling.
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(test/unit)
endif()

# ---- Cross firmware. Per the program's ADR pattern, the target-side CMake
# project (linker script, startup, HAL init) comes from STM32CubeMX's export
# into cubemx/. The root CMakeLists owns the portable, host-testable part.
# Until a project generates its CubeMX export, the cross build produces the
# domain library alone -- enough to smoke-test the toolchain.
if(CMAKE_CROSSCOMPILING)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cubemx/CMakeLists.txt")
        add_subdirectory(cubemx)
    else()
        message(STATUS
            "no cubemx/ project yet -- building host-testable domain only "
            "(generate the STM32CubeMX CMake export to add firmware)")
    endif()
endif()
