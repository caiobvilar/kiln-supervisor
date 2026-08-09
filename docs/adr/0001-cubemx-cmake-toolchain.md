# ADR 0001 — STM32CubeMX + CMake for all STM32 firmware

**Status:** accepted — 2026-07-26

All STM32 board projects (`f072-disco`, `f411-disco`, `f469-disco`) are
configured through STM32CubeMX and built with CMake, using CubeMX's built-in
CMake project generation (clock tree, pin/peripheral init, HAL selection,
and the matching linker script + startup file all come out of CubeMX rather
than being hand-rolled per project). `tools/verify.sh`'s `cmake --preset
host` / `cmake --preset target` split stays as-is; the target preset points
at the toolchain file CubeMX emits alongside the project.

Rationale: hand-writing linker scripts and startup code per board is exactly
the class of task that produces plausible-but-wrong memory maps and vector
tables — the failure mode `hw-facts` exists to block — while CubeMX's output
is generated from ST's own device database, not recalled from memory. The
tradeoff is HAL overhead (relevant on the F072's 16 KB RAM budget) and a
generated-code tree that needs a clear boundary against hand-written
application code; both are visible costs, not hidden ones. `docs/hardware/`
fact sheets are still required and still gate what hand-written code may
assume — CubeMX generating a pin as `PA5` does not make that pin verified
for purposes of a comment or a driver written outside CubeMX's own files.
