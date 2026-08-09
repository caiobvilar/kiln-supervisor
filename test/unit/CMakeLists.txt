# Unity is vendored into the toolchain container (no network at runtime --
# see the portfolio's ADR-0003 pattern), exposed here via UNITY_DIR.
if(DEFINED ENV{UNITY_DIR})
    set(UNITY_ROOT "$ENV{UNITY_DIR}")
else()
    set(UNITY_ROOT "/opt/unity")
endif()

if(NOT EXISTS "${UNITY_ROOT}/src/unity.c")
    message(FATAL_ERROR
        "Unity not found at '${UNITY_ROOT}'. Host tests must run inside the "
        "toolchain container, or set UNITY_DIR to a local Unity checkout.")
endif()

add_library(unity STATIC "${UNITY_ROOT}/src/unity.c")
target_include_directories(unity PUBLIC "${UNITY_ROOT}/src")

add_executable(test_domain test_domain.c)
target_link_libraries(test_domain PRIVATE domain unity)

add_test(NAME domain COMMAND test_domain)
