# embedded-template — the reusable infra for every project repo

This is the engineering platform behind all 20 projects of the embedded
portfolio program ([portfolio meta-repo](https://github.com/caiobvilar/Embedded_Portfolio)):
the CMake toolchain files, CI pipelines, requirements tooling, fake HAL, and
doc skeletons that make a new project repo start already-green.

It exists so that **consistency across repos is itself a signal** — every
project repo uses the same build, the same gates, the same doc structure, so
an interviewer (or future-me) can orient in 30 seconds.

## What's inside

| Path | Contents |
|---|---|
| `cmake/` | `arm-none-eabi.cmake` (cross toolchain, pin by board), `host.cmake` (native + ASAN/UBSAN + coverage helpers) |
| `.github/workflows/` | `ci.yml`, `sitl.yml`, `hil.yml`, `release.yml` — the four pipelines, parameterised |
| `tools/` | `gen_rtm.py` (requirements↔tests RTM gate), `check_layering.py`, `check_size_budget.py`, `sign_image.py` (Ed25519) |
| `src/ports/` | Hexagonal port interfaces: `i_uart.h`, `i_spi.h`, `i_i2c.h`, `i_clock.h` |
| `src/adapters/host/` | Test doubles for the ports (recording fake UART, scripted clock) |
| `templates/docs/` | SRS, test-plan, design-review-checklist, project-README skeletons |
| `Containerfile.toolchain` + `requirements.*` | Reproducible toolchain container (pin everything, hash-locked Python deps) |
| `.clang-format` `.clang-tidy` `.cppcheck-suppressions` | Zero-warning static-analysis posture |

## Create a new project repo from this template

```bash
# on GitHub, "Use this template" -- or:
gh repo create <project-slug> --template caiobvilar/embedded-template --public
```

Then:
1. Write `docs/02-srs.md` (from `templates/docs/srs-template.md`) and
   `docs/requirements/*.yaml` — **before any code** (program rule 1).
2. Set the flash/RAM budgets in `ci.yml`'s size-gate step and the target in
   `cmake/arm-none-eabi.cmake` (per-board `-mcpu`).
3. Generate the CubeMX project into `cubemx/` for the firmware half (see the
   portfolio's ADR pattern for the CubeMX/CMake integration boundary).
4. Put pure logic in `src/domain/` (host-testable), hardware in
   `src/adapters/stm32f4/`, interfaces in `src/ports/`.
5. `cmake --preset host-test && ctest --preset host-test` — the loop is
   milliseconds, so you actually run it.

## License

Code and tooling: **Apache-2.0** · Documentation: **CC BY 4.0**
(per the program's publishing rules, 06-publishing §2.3).
