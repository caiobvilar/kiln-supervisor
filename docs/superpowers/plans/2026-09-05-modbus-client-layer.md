# Modbus Client Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a host-testable generic Modbus RTU client (read holding registers, write single, write multiple, bounded timeout) plus an unverified KM5P register-constants header, an approved KILN-FUN-004 requirement, and L0 cross-build + static-analysis verification.

**Architecture:** Hexagonal domain module `src/domain/modbus_client.{c,h}` sits on the existing `i_uart`/`i_clock` port seams (only seams it touches) and reuses `modbus_frame` (build/check) + `modbus_crc`. One-shot synchronous request/response: build frame → tx over `i_uart` → poll for a complete expected-length response with a bounded deadline → validate (addr, CRC, fc, byte-count) → parse register words. Fully host-testable against `fake_uart` + `fake_clock`.

**Tech Stack:** C11 (no GNU extensions), Unity 2.7.0 (host L1), CMake presets, arm-none-eabi cross toolchain + cppcheck/clang-tidy/clang-format in the `workbench-toolchain` container.

**Spec:** `docs/superpowers/specs/2026-09-05-modbus-client-design.md` (approved design; the plan argues from it).

## Global Constraints

- **Branch:** create `modbus-client` from `main` and commit every task there; integrate at the end via the superpowers:finishing-a-development-branch skill (do NOT merge during a task). Current `main` has the spec commits (`2d02536`, `d3599ff`).
- **C11**, `CMAKE_C_EXTENSIONS OFF`; the `domain` library already carries this.
- **Layering:** domain files may include only `include/`, `src/domain/`, `src/ports/` (enforced by `tools/check_layering.py`; headers that resolve under `src/domain` or `src/ports` are allowed).
- **Parallelism (user hard rule / AGENTS.md):** builds capped at `-j2`, NEVER bare `-j`. This machine has 32+ cores and saturating them locks the desktop.
- **Host test setup:** the vendor copy of Unity at `/opt/unity` is NOT writable on this host — configure with `UNITY_DIR=/home/hellscoffe/unity` (env-only, never committed). Cross/flash failure surface lives in the `workbench-toolchain` Podman container (see Task 5).
- **Wire contract:** CRC-16/Modbus (poly 0xA001, init 0xFFFF, LSB-first, CRC LSB transmitted first) — already implemented by `modbus_crc16`. All test vectors below carry pre-computed CRCs; do not recompute by hand.
- **Requirement wording:** banned words from `tools/gen_rtm.py` (fast/slow/robust/efficient/user-friendly/flexible/optimal/reasonable/appropriate/"as needed"/"if possible"/"etc."/"and/or"/support/process/handle). Use the exact wording from the spec/KILN-FUN-004 below.
- **Repository gates (`.github/workflows/ci.yml`), all must pass:** `cppcheck --enable=warning,style,performance,portability --error-exitcode=1 --suppressions-list=.cppcheck-suppressions src/`, `clang-tidy -p build/host-test src/domain/*.c`, `clang-format --dry-run --Werror $(find src test -name '*.[ch]')`, `python tools/gen_rtm.py check`, `python tools/check_layering.py`, `gcovr --filter 'src/' --fail-under-line 80`.
- **clang-format:** format new/edited files with `clang-format -i` before committing (host `~/.local/bin/clang-format` is v22; the container pins Debian bookworm's v14 — Task 5 re-verifies with the pinned version and reconciles any formatting-only diffs in its own commit).
- **Register constants in `kiln_registers.h` are UNVERIFIED** (bench read-back pending per `docs/hardware/km5p-controller.md` and SRS §5/OI-01). Nothing may depend on them; they carry `/* unverified */` markers.

---

### Task 1: Branch + register constants header

**Files:**
- Create: `src/domain/kiln_registers.h`

**Interfaces:**
- Produces: `kiln_registers.h` — constants only, no requirement tag, consumed by nothing yet.

- [ ] **Step 1: Create the branch**

```bash
git checkout main && git pull --ff-only 2>/dev/null || true
git checkout -b modbus-client
```

- [ ] **Step 2: Create `src/domain/kiln_registers.h`**

```c
#ifndef KILN_REGISTERS_H
#define KILN_REGISTERS_H

/* KM5P_r0 Modbus holding-register numbers, from the Ascon Tecnologic register
 * map recorded in docs/hardware/km5p-controller.md.
 *
 * ALL VALUES UNVERIFIED -- bench read-back against the physical unit is pending
 * (SRS OI-01). No code may depend on these until verified; they exist here so
 * the documented numbers are auditable in one place.
 */
#define KILN_REG_PV          1u    /* 0x0001 -- live measured value, signed, dP */
#define KILN_REG_OP_SETPOINT 3u    /* 0x0003 -- operative set point, dP, r */
#define KILN_REG_SP          6u    /* 0x0006 -- settable set point, r/w */
#define KILN_REG_ALARMS      10u   /* 0x000A -- alarm status bitmask */
#define KILN_REG_ALARM_ACK_A 13u   /* 0x000D -- alarm reset/ack, r/w */
#define KILN_REG_ALARM_ACK_B 14u   /* 0x000E -- alarm reset/ack, r/w */
#define KILN_REG_CONTROL     15u   /* 0x000F -- 0 Auto, 1 Manual, 2 Standby, r/w */
#define KILN_REG_PROG_STATUS 580u  /* 0x0244 -- program status, 0..7, r/w */
#define KILN_REG_PROG_STEP   582u  /* 0x0246 -- program step in execution, 0..9 */
#define KILN_REG_ADDR        10337u /* 0x2861 -- param Add, oFF or 1..254, r/w */
#define KILN_REG_BAUD        10338u /* 0x2862 -- param bAud, 0..4, r/w */
#define KILN_REG_PR_ST       10367u /* 0x287F -- param Pr.St, 0 reset..3 continue */

#endif /* KILN_REGISTERS_H */
```

No test is possible or warranted here: the numbers are, by definition, unverified (a test would just re-assert the same unverified facts). Correctness is a copy-check against `docs/hardware/km5p-controller.md`.

- [ ] **Step 3: Review** — re-read the doc table (lines ~101–112) and diff-check every value and its `0x` comment against the header. Fix any typo before committing.

- [ ] **Step 4: Commit**

```bash
git add src/domain/kiln_registers.h
git commit -m "feat: add unverified KM5P register constants header"
```

---

### Task 2: Client header + read path (fc 03) + read tests

> **Amendment (controller ruling, in-execution):** the brief as first written could
> not build: the task tests declare `fake_uart_ctx_t` / `fake_clock_ctx_t` as stack
> instances, but the fake typedefs are opaque (struct bodies are private to
> `src/adapters/host/fake_uart.c` / `fake_clock.c`), so the test cannot size a
> stack variable of an incomplete type; and no CMake target compiled the fakes.
> Minimal enabling fix that keeps the task's test and implementation verbatim:
> 1. Open the ctx struct in each fake header (`fake_uart.h`, `fake_clock.h`) —
>    move the struct body from the `.c` into the header in place of the opaque
>    `typedef struct ..._s ..._t;` forward declaration.
> 2. Each fake `.c` includes its own header (replacing its private typedef and
>    its direct `<i_uart.h>` / `<i_clock.h>` include; the header includes the port
>    header transitively).
> 3. `test/unit/CMakeLists.txt` adds `fake_uart.c` + `fake_clock.c` to
>   `test_modbus_client`'s sources and `src/adapters/host` to its include dirs.
> Layering gate only scans `src/domain`, so this does not violate it.

**Files:**
- Create: `src/domain/modbus_client.h`
- Create: `src/domain/modbus_client.c`
- Create: `test/unit/test_modbus_client.c`
- Modify: `CMakeLists.txt:12`
- Modify: `test/unit/CMakeLists.txt`
- Modify (amendment): `src/adapters/host/fake_uart.h`, `src/adapters/host/fake_uart.c`
- Modify (amendment): `src/adapters/host/fake_clock.h`, `src/adapters/host/fake_clock.c`

**Interfaces:**
- Consumes: `modbus_build_request`, `modbus_frame_check` (from `src/domain/modbus_frame.h`); `modbus_crc16` (`src/domain/modbus_crc.h`); `uart_t`/`uart_write`/`uart_read`/`uart_available` (`src/ports/i_uart.h`); `clock_t`/`clock_ticks_us` (`src/ports/i_clock.h`); host doubles `fake_uart_*`, `fake_clock_*`.
- Produces (used by Task 3 and tests): header `modbus_client.h` declaring `modbus_client_status_t`, `MODBUS_CLIENT_MAX_REGS`, `modbus_client_t`, `modbus_client_init`, and all three operations (write ops are implemented in Task 3 but their prototypes live here now). Static helpers `recv_frame`, `validate_frame`, `tx_request` in `.c` are the write path's shared machinery.

- [ ] **Step 1: Write the failing tests** — create `test/unit/test_modbus_client.c`:

```c
/* test/unit/test_modbus_client.c -- generic Modbus client request/response tests. */
/* @verifies KILN-FUN-004 */
#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "fake_clock.h"
#include "fake_uart.h"
#include "i_clock.h"
#include "modbus_client.h"

/* A clock that advances a fixed step on every read, so a bounded-timeout poll
 * loop terminates deterministically in tests. */
typedef struct {
    uint64_t now;
    uint64_t step_us;
} tick_clock_ctx_t;

static uint64_t tick_ticks(clock_t* c)
{
    tick_clock_ctx_t* k = (tick_clock_ctx_t*)c->ctx;
    uint64_t t = k->now;
    k->now += k->step_us;
    return t;
}

static const clock_vtable_t tick_clock_vt = { tick_ticks };

static void tick_clock_init(clock_t* c, tick_clock_ctx_t* k, uint64_t step_us)
{
    k->now = 0;
    k->step_us = step_us;
    c->vt = &tick_clock_vt;
    c->ctx = k;
}

void setUp(void) {}
void tearDown(void) {}

static void make_client(uart_t* u, fake_uart_ctx_t* uctx, clock_t* c, fake_clock_ctx_t* cctx,
                        modbus_client_t* cli)
{
    fake_uart_init(u, uctx);
    fake_clock_init(c, cctx);
    modbus_client_init(cli, u, c, 100000u);
}

void test_read_registers_success(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t rsp[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x67, 0x9F};
    fake_uart_enqueue_rx(&uctx, rsp, sizeof(rsp));

    const uint16_t expected[] = {0x0123, 0x4567, 0x89AB};
    uint16_t regs[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, regs, 3);

    const uint8_t expect_tx[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x03, 0x54, 0x0B};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 8);
}

void test_read_registers_timeout_when_no_response(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    tick_clock_ctx_t clkctx;
    modbus_client_t cli;
    fake_uart_init(&u, &uctx);
    tick_clock_init(&clk, &clkctx, 10000u);
    modbus_client_init(&cli, &u, &clk, 50000u);

    uint16_t regs[1] = {0xFFFF};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_TIMEOUT,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));

    size_t n = 0;
    (void)fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
}

void test_read_registers_timeout_on_partial_frame(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    tick_clock_ctx_t clkctx;
    modbus_client_t cli;
    fake_uart_init(&u, &uctx);
    tick_clock_init(&clk, &clkctx, 10000u);
    modbus_client_init(&cli, &u, &clk, 50000u);

    const uint8_t partial[] = {0x01, 0x03};
    fake_uart_enqueue_rx(&uctx, partial, sizeof(partial));
    uint16_t regs[1] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_TIMEOUT,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));
}

void test_read_registers_rejects_bad_crc(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t bad[] = {0x01, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAC, 0x67, 0x9F};
    fake_uart_enqueue_rx(&uctx, bad, sizeof(bad));
    uint16_t regs[3] = {0xFFFF, 0xFFFF, 0xFFFF};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_CRC,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
    const uint16_t untouched[] = {0xFFFF, 0xFFFF, 0xFFFF};
    TEST_ASSERT_EQUAL_UINT16_ARRAY(untouched, regs, 3);
}

void test_read_registers_rejects_wrong_slave_address(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t other[] = {0x02, 0x03, 0x06, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x73, 0x6F};
    fake_uart_enqueue_rx(&uctx, other, sizeof(other));
    uint16_t regs[3] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
}

void test_read_registers_rejects_byte_count_mismatch(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t miscnt[] = {0x01, 0x03, 0x05, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x54, 0x9F};
    fake_uart_enqueue_rx(&uctx, miscnt, sizeof(miscnt));
    uint16_t regs[3] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 3));
}

void test_read_registers_exception_reply_is_response_error(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t exc[] = {0x01, 0x83, 0x02, 0xC0, 0xF1};
    fake_uart_enqueue_rx(&uctx, exc, sizeof(exc));
    uint16_t regs[1] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_RESPONSE,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, regs, 1));
}

void test_read_registers_params(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    uint16_t regs[2] = {0};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 0, regs, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 17, regs, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 3, regs, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_read_registers(&cli, 0x01, 0x0001, 1, NULL, 0));

    size_t n = 99;
    (void)fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(0, n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_registers_success);
    RUN_TEST(test_read_registers_timeout_when_no_response);
    RUN_TEST(test_read_registers_timeout_on_partial_frame);
    RUN_TEST(test_read_registers_rejects_bad_crc);
    RUN_TEST(test_read_registers_rejects_wrong_slave_address);
    RUN_TEST(test_read_registers_rejects_byte_count_mismatch);
    RUN_TEST(test_read_registers_exception_reply_is_response_error);
    RUN_TEST(test_read_registers_params);
    return UNITY_END();
}
```

- [ ] **Step 2: Wire CMake and fake contexts** —
  in `CMakeLists.txt:12` change the `domain` sources line to:

```cmake
add_library(domain STATIC src/domain/modbus_crc.c src/domain/modbus_frame.c src/domain/modbus_client.c)
```

and at the end of `test/unit/CMakeLists.txt` append:

```cmake
add_executable(test_modbus_client test_modbus_client.c)
target_sources(test_modbus_client PRIVATE
    ${CMAKE_SOURCE_DIR}/src/adapters/host/fake_uart.c
    ${CMAKE_SOURCE_DIR}/src/adapters/host/fake_clock.c)
target_include_directories(test_modbus_client PRIVATE ${CMAKE_SOURCE_DIR}/src/adapters/host)
target_link_libraries(test_modbus_client PRIVATE domain unity)
add_test(NAME modbus_client COMMAND test_modbus_client)
```

Then open the fake ctx types (amendment): move the private struct in
`src/adapters/host/fake_uart.c` into `fake_uart.h` (and `fake_clock.c` into
`fake_clock.h`), and make each fake `.c` include its own header.

- [ ] **Step 3: Run tests to verify they fail**

```bash
export UNITY_DIR=/home/hellscoffe/unity
rm -rf build/host-test
cmake --preset host-test && cmake --build --preset host-test
```

Expected: configure/build FAILS — `modbus_client.h` does not exist yet (`fatal error: modbus_client.h: No such file or directory` for the test, or a missing-source error for the domain library).

- [ ] **Step 4: Create `src/domain/modbus_client.h`**

```c
#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "i_clock.h"
#include "i_uart.h"
#include "modbus_frame.h"

typedef enum {
    MODBUS_CLIENT_OK = 0,
    MODBUS_CLIENT_ERR_TIMEOUT = -1,
    MODBUS_CLIENT_ERR_CRC = -2,
    MODBUS_CLIENT_ERR_RESPONSE = -3,
    MODBUS_CLIENT_ERR_UART = -4,
    MODBUS_CLIENT_ERR_PARAM = -5,
} modbus_client_status_t;

#define MODBUS_CLIENT_MAX_REGS 16u

typedef struct {
    uart_t* uart;
    clock_t* clock;
    uint64_t response_timeout_us;
    uint8_t rx_buf[MODBUS_FRAME_MAX];
} modbus_client_t;

void modbus_client_init(modbus_client_t* c, uart_t* uart, clock_t* clock,
                        uint64_t response_timeout_us);

modbus_client_status_t modbus_client_read_registers(
    modbus_client_t* c, uint8_t addr, uint16_t start, uint16_t count,
    uint16_t* out, size_t out_cap);

modbus_client_status_t modbus_client_write_single(modbus_client_t* c, uint8_t addr,
                                                  uint16_t reg, uint16_t value);

modbus_client_status_t modbus_client_write_multiple(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start,
                                                    const uint16_t* values, size_t count);

#endif /* MODBUS_CLIENT_H */
```

- [ ] **Step 5: Create `src/domain/modbus_client.c`** (read path + the shared receive/validate/tx machinery the write ops in Task 3 reuse):

```c
/* src/domain/modbus_client.c -- generic Modbus RTU client over the UART port.
 * One-shot request/response with a bounded timeout: build a request, transmit
 * it, wait for a complete expected-length response, validate, then parse. */
#include "modbus_client.h"

#include "modbus_crc.h"
#include "modbus_frame.h"

static modbus_client_status_t tx_request(modbus_client_t* c, const uint8_t* frame, size_t n)
{
    if (uart_write(c->uart, frame, n) != (int)n) {
        return MODBUS_CLIENT_ERR_UART;
    }
    return MODBUS_CLIENT_OK;
}

static modbus_client_status_t recv_frame(modbus_client_t* c, size_t expected, size_t* out_len)
{
    uint64_t deadline = clock_ticks_us(c->clock) + c->response_timeout_us;
    size_t len = 0;
    for (;;) {
        if (uart_available(c->uart) > 0) {
            int n = uart_read(c->uart, &c->rx_buf[len], expected - len);
            if (n < 0) {
                return MODBUS_CLIENT_ERR_UART;
            }
            len += (size_t)n;
        }
        /* No expected reply can have 0x80 set in its fc byte (0x03/0x06/0x16),
         * so this is unambiguously a slave exception reply. */
        if (len >= 5 && (c->rx_buf[1] & 0x80u)) {
            return MODBUS_CLIENT_ERR_RESPONSE;
        }
        if (len >= expected) {
            *out_len = len;
            return MODBUS_CLIENT_OK;
        }
        if (clock_ticks_us(c->clock) >= deadline) {
            return MODBUS_CLIENT_ERR_TIMEOUT;
        }
    }
}

static modbus_client_status_t validate_frame(modbus_client_t* c, size_t len, uint8_t addr,
                                             uint8_t fc, uint8_t byte_count)
{
    if (modbus_frame_check(c->rx_buf, len, addr) < 0) {
        uint16_t crc = modbus_crc16(c->rx_buf, len - 2);
        uint16_t embedded = (uint16_t)((uint16_t)c->rx_buf[len - 2] |
                                       ((uint16_t)c->rx_buf[len - 1] << 8));
        return crc == embedded ? MODBUS_CLIENT_ERR_RESPONSE : MODBUS_CLIENT_ERR_CRC;
    }
    if (c->rx_buf[1] != fc) {
        return MODBUS_CLIENT_ERR_RESPONSE;
    }
    if (fc == MODBUS_FC_READ_MULTIPLE && c->rx_buf[2] != byte_count) {
        return MODBUS_CLIENT_ERR_RESPONSE;
    }
    return MODBUS_CLIENT_OK;
}

void modbus_client_init(modbus_client_t* c, uart_t* uart, clock_t* clock,
                        uint64_t response_timeout_us)
{
    c->uart = uart;
    c->clock = clock;
    c->response_timeout_us = response_timeout_us;
}

modbus_client_status_t modbus_client_read_registers(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start, uint16_t count,
                                                    uint16_t* out, size_t out_cap)
{
    if (c == NULL || c->uart == NULL || c->clock == NULL || out == NULL) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    if (count < 1 || count > MODBUS_CLIENT_MAX_REGS) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    if ((size_t)count > out_cap) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    uint8_t req[MODBUS_FRAME_MAX];
    if (modbus_build_request(req, sizeof(req), addr, MODBUS_FC_READ_MULTIPLE, start, count) < 0) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    modbus_client_status_t st = tx_request(c, req, 8u);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    size_t len = 0;
    st = recv_frame(c, (size_t)(5 + 2 * count), &len);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    st = validate_frame(c, len, addr, MODBUS_FC_READ_MULTIPLE, (uint8_t)(2 * count));
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    for (size_t i = 0; i < (size_t)count; i++) {
        out[i] = (uint16_t)(((uint16_t)c->rx_buf[3 + 2 * i] << 8) |
                            (uint16_t)c->rx_buf[4 + 2 * i]);
    }
    return MODBUS_CLIENT_OK;
}
```

- [ ] **Step 6: Run tests to verify they pass**

```bash
export UNITY_DIR=/home/hellscoffe/unity
cmake --build --preset host-test
ctest --preset host-test
```

Expected: `modbus_crc`, `modbus_frame`, `modbus_client` all PASS (ctest exit 0).

- [ ] **Step 7: Format + commit**

```bash
clang-format -i src/domain/modbus_client.h src/domain/modbus_client.c test/unit/test_modbus_client.c
cmake --build --preset host-test && ctest --preset host-test   # still green after format
git add src/domain/modbus_client.h src/domain/modbus_client.c test/unit/test_modbus_client.c \
        CMakeLists.txt test/unit/CMakeLists.txt
git commit -m "feat: add generic Modbus client read path (fc 03) with bounded timeout"
```

---

### Task 3: Write single (fc 06) + write multiple (fc 16) + write tests

**Files:**
- Modify: `src/domain/modbus_client.c` (add write ops + in-line fc 16 PDU builder)
- Modify: `test/unit/test_modbus_client.c` (add write tests + a fail-write UART double)

**Interfaces:**
- Consumes: Task 2's `recv_frame`, `validate_frame`, `tx_request` (static in `modbus_client.c`); prototypes already declared in `modbus_client.h`.
- Produces: implemented `modbus_client_write_single`, `modbus_client_write_multiple`. NOTE: `modbus_build_request` cannot encode a fc 16 data body (it emits the response-shaped 8-byte ADU), so the fc 16 request PDU is composed in-line and CRC'd with `modbus_crc16` (spec §5 step 2).

- [ ] **Step 1: Add the failing tests** — append these functions to `test/unit/test_modbus_client.c` and add them to `main()`:

```c
/* A UART whose write always fails, to exercise ERR_UART. */
typedef struct {
    int unused;
} fail_uart_ctx_t;

static int fail_write(uart_t* u, const uint8_t* data, size_t n)
{
    (void)u;
    (void)data;
    (void)n;
    return -1;
}

static int fail_read(uart_t* u, uint8_t* data, size_t n)
{
    (void)u;
    (void)data;
    (void)n;
    return -1;
}

static int fail_available(uart_t* u)
{
    (void)u;
    return 0;
}

static const uart_vtable_t fail_uart_vt = { fail_write, fail_read, fail_available };

static void fail_uart_init(uart_t* u)
{
    static fail_uart_ctx_t ctx;
    ctx.unused = 0;
    u->vt = &fail_uart_vt;
    u->ctx = &ctx;
}

void test_write_single_success(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t echo[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x0A, 0x58, 0x0D};
    fake_uart_enqueue_rx(&uctx, echo, sizeof(echo));

    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK,
                          modbus_client_write_single(&cli, 0x01, 0x0001, 0x000A));

    const uint8_t expect_tx[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x0A, 0x58, 0x0D};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(8, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 8);
}

void test_write_multiple_success(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint8_t echo[] = {0x01, 0x16, 0x00, 0x01, 0x00, 0x02, 0x98, 0x08};
    fake_uart_enqueue_rx(&uctx, echo, sizeof(echo));
    const uint16_t values[] = {0x0064, 0x00C8};

    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_OK,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, values, 2));

    const uint8_t expect_tx[] = {0x01, 0x16, 0x00, 0x01, 0x00, 0x02, 0x04,
                                 0x00, 0x64, 0x00, 0xC8, 0x92, 0x35};
    size_t n = 0;
    const uint8_t* tx = fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(13, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect_tx, tx, 13);
}

void test_write_single_uart_failure(void)
{
    uart_t u;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    fail_uart_init(&u);
    fake_clock_init(&clk, &clkctx);
    modbus_client_init(&cli, &u, &clk, 100000u);

    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_UART,
                          modbus_client_write_single(&cli, 0x01, 0x0001, 0x000A));
}

void test_write_multiple_params(void)
{
    uart_t u;
    fake_uart_ctx_t uctx;
    clock_t clk;
    fake_clock_ctx_t clkctx;
    modbus_client_t cli;
    make_client(&u, &uctx, &clk, &clkctx, &cli);

    const uint16_t values[] = {0x0001, 0x0002};
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, NULL, 2));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, values, 0));
    TEST_ASSERT_EQUAL_INT(MODBUS_CLIENT_ERR_PARAM,
                          modbus_client_write_multiple(&cli, 0x01, 0x0001, values, 17));

    size_t n = 99;
    (void)fake_uart_tx_bytes(&uctx, &n);
    TEST_ASSERT_EQUAL_UINT(0, n);
}
```

In `main()` add after the existing `RUN_TEST(...)` calls:

```c
    RUN_TEST(test_write_single_success);
    RUN_TEST(test_write_multiple_success);
    RUN_TEST(test_write_single_uart_failure);
    RUN_TEST(test_write_multiple_params);
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
export UNITY_DIR=/home/hellscoffe/unity
cmake --build --preset host-test && ctest --preset host-test --output-on-failure
```

Expected: FAIL in the new tests — unresolved `modbus_client_write_single`/`modbus_client_write_multiple` (linker).

- [ ] **Step 3: Implement the write ops** — append to `src/domain/modbus_client.c`:

```c
static modbus_client_status_t write_multiple_frame(modbus_client_t* c, uint8_t addr,
                                                   uint16_t start, const uint16_t* values,
                                                   size_t count)
{
    uint8_t req[MODBUS_FRAME_MAX];
    size_t n = 9 + 2 * count;
    req[0] = addr;
    req[1] = MODBUS_FC_WRITE_MULTIPLE;
    req[2] = (uint8_t)(start >> 8);
    req[3] = (uint8_t)start;
    req[4] = (uint8_t)(count >> 8);
    req[5] = (uint8_t)count;
    req[6] = (uint8_t)(2 * count);
    for (size_t i = 0; i < count; i++) {
        req[7 + 2 * i] = (uint8_t)(values[i] >> 8);
        req[8 + 2 * i] = (uint8_t)values[i];
    }
    uint16_t crc = modbus_crc16(req, n - 2);
    req[n - 2] = (uint8_t)crc;
    req[n - 1] = (uint8_t)(crc >> 8);
    return tx_request(c, req, n);
}

modbus_client_status_t modbus_client_write_single(modbus_client_t* c, uint8_t addr,
                                                  uint16_t reg, uint16_t value)
{
    if (c == NULL || c->uart == NULL || c->clock == NULL) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    uint8_t req[MODBUS_FRAME_MAX];
    if (modbus_build_request(req, sizeof(req), addr, MODBUS_FC_WRITE_SINGLE, reg, value) < 0) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    modbus_client_status_t st = tx_request(c, req, 8u);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    size_t len = 0;
    st = recv_frame(c, 8u, &len);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    return validate_frame(c, len, addr, MODBUS_FC_WRITE_SINGLE, 0u);
}

modbus_client_status_t modbus_client_write_multiple(modbus_client_t* c, uint8_t addr,
                                                    uint16_t start, const uint16_t* values,
                                                    size_t count)
{
    if (c == NULL || c->uart == NULL || c->clock == NULL || values == NULL) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    if (count < 1 || count > MODBUS_CLIENT_MAX_REGS) {
        return MODBUS_CLIENT_ERR_PARAM;
    }
    modbus_client_status_t st = write_multiple_frame(c, addr, start, values, count);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    size_t len = 0;
    st = recv_frame(c, 8u, &len);
    if (st != MODBUS_CLIENT_OK) {
        return st;
    }
    return validate_frame(c, len, addr, MODBUS_FC_WRITE_MULTIPLE, 0u);
}
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
export UNITY_DIR=/home/hellscoffe/unity
cmake --build --preset host-test && ctest --preset host-test
```

Expected: all test suites green (including the four new tests).

- [ ] **Step 5: Format + commit**

```bash
clang-format -i src/domain/modbus_client.c test/unit/test_modbus_client.c
cmake --build --preset host-test && ctest --preset host-test
git add src/domain/modbus_client.c test/unit/test_modbus_client.c
git commit -m "feat: add Modbus client write paths (fc 06 and fc 16) with in-line PDU"
```

---

### Task 4: KILN-FUN-004 requirement + SRS/RTM sync + admin logs

**Files:**
- Modify: `docs/requirements/kiln.yaml` (add KILN-FUN-004 after KILN-FUN-003)
- Modify: `docs/02-srs.md` (§6.1 item 4, §7 counts)
- Modify: `docs/06-rtm.md` (regenerated by `tools/gen_rtm.py` — do NOT hand-edit)
- Modify: `PLAN.md`, `CHANGELOG.md`

**Interfaces:**
- Consumes: `test/unit/test_modbus_client.c` (already carries `/* @verifies KILN-FUN-004 */`).

- [ ] **Step 1: Add the requirement to `docs/requirements/kiln.yaml`** — append this entry after the KILN-FUN-003 block (after line 83):

```yaml
- id: KILN-FUN-004
  text: >
    The Modbus client shall issue read and write requests over the UART port
    interface and validate the response within a bounded timeout.
  type: functional
  rationale: >
    N-01 requires a Modbus RTU client; request/response with a bounded timeout
    is host-testable without hardware and prerequisites the register-map layer.
  parent: N-01
  priority: shall
  verification: test
  status: approved
  test_case:
    - test/unit/test_modbus_client.c
```

- [ ] **Step 2: Sync `docs/02-srs.md`** — in §6.1 functional list, after item 3 (`KILN-FUN-003`), add:

```markdown
4. **KILN-FUN-004** (shall) — The Modbus client shall issue read and write
   requests over the UART port interface and validate the response within a
   bounded timeout.
```

In §7 Verification summary change the counts:

```markdown
| Test | 6 |
| Analysis | 1 |
```

(Test goes 5 → 6; Analysis stays 1. Gate 8: the SRS §7 totals must exactly match the YAML; the other rows stay 0.)

- [ ] **Step 3: Regenerate and check the RTM**

```bash
python tools/gen_rtm.py gen
python tools/gen_rtm.py check
git diff --stat docs/06-rtm.md   # should show only the KILN-FUN-004 additions
```

Expected: `check` prints 0 violations; `gen` then `check` is idempotent (no diff when re-run).

- [ ] **Step 4: Verify the whole host suite + layering**

```bash
export UNITY_DIR=/home/hellscoffe/unity
cmake --build --preset host-test && ctest --preset host-test
python tools/check_layering.py
```

Expected: all host tests pass; layering prints `OK: 7 domain files, no hardware includes`.

- [ ] **Step 5: Update `PLAN.md` and `CHANGELOG.md`** — one short entry each, matching the repo's existing milestone-log format. Text must match the plan wording exactly. Suggested CHANGELOG line:

```
- Milestone: generic Modbus RTU client (fc 03/06/16) with bounded response timeout over the UART port seam — KILN-FUN-004 approved; register-constants header added (unverified); fc16 framing helper limitation recorded.
```

- [ ] **Step 6: Run the repo gates locally (host tools where available) and commit**

```bash
clang-format --dry-run --Werror $(find src test -name '*.[ch]')
git add docs/requirements/kiln.yaml docs/02-srs.md docs/06-rtm.md PLAN.md CHANGELOG.md
git commit -m "docs: approve KILN-FUN-004, sync SRS/RTM, log milestone"
```

Expected: `clang-format --dry-run` prints nothing and exits 0.

---

### Task 5: L0 cross-build + static-analysis + coverage gates (toolchain container)

**Files:**
- Modify (only if needed / one-time): `Containerfile.toolchain` (cap gtest build parallelism, see step 1)
- No source changes expected unless a gate flags something (then fix + commit as a separate step).

**Interfaces:**
- Consumes: everything from Tasks 1–4 on branch `modbus-client`.

The pinned toolchain (`arm-none-eabi`, `cppcheck`, `clang-tidy`, pinned `clang-format` from Debian bookworm, Unity at `/opt/unity`, python venv with `gcovr`) lives in the `workbench-toolchain` Podman container it is not on this host (verified: `which arm-none-eabi-gcc cppcheck clang-tidy` all empty). This task builds the image once and runs the CI gate suite inside it. If you prefer installing these on the host with dnf instead, say so before starting; the commands below are the container route.

- [ ] **Step 1: Build the toolchain container (one-time)**

```bash
podman build -t workbench-toolchain -f Containerfile.toolchain .
```

Caveats:
- Network + time heavy (ARM GCC 13.2 tarball, Renode deb, googletest source, Unity, CMock, python venv). Run when machine idleness is acceptable.
- The `Dockerfile`'s googletest step uses a bare `cmake --build ... -j`. Per the machine's AGENTS.md rule (never saturate all cores on this 32+ core box), patch that step to `-j2` BEFORE the build, and commit it as its own small commit:

```bash
# In Containerfile.toolchain, replace the gtest build line:
#   cmake --build /tmp/gtest-build -j \
# with:
#   cmake --build /tmp/gtest-build -j2 \
git add Containerfile.toolchain && git commit -m "ci: cap toolchain-container gtest build at -j2"
```

- [ ] **Step 2: Run the full gate suite inside the container**

```bash
podman run --rm -i --userns=keep-id -v "$PWD":/w:Z -w /w localhost/workbench-toolchain bash -s <<'EOF'
set -e
cmake -S . -B build/ci-host  -DCMAKE_TOOLCHAIN_FILE=cmake/host.cmake -DCMAKE_BUILD_TYPE=Debug  -DUNITY_DIR=/opt/unity
cmake --build build/ci-host -j2
ctest --test-dir build/ci-host -j2
cmake -S . -B build/ci-target -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ci-target -j2
cppcheck --enable=warning,style,performance,portability --error-exitcode=1 --suppressions-list=.cppcheck-suppressions src/
clang-tidy -p build/ci-host src/domain/*.c
clang-format --dry-run --Werror $(find src test -name '*.[ch]')
python tools/gen_rtm.py check
python tools/check_layering.py
EOF
```

Expected: every gate printed clean (exit 0 for all). If a gate fails: fix the finding in the normal workflow (TDD: add/extend a test when behavior is at fault; formatting-only → `clang-format -i`), re-run that gate, then commit the fix with a message describing the finding.

- [ ] **Step 3: Coverage gate (domain ≥80%)**

```bash
podman run --rm -i --userns=keep-id -v "$PWD":/w:Z -w /w localhost/workbench-toolchain bash -s <<'EOF'
set -e
cmake -S . -B build/ci-cov --preset host-test -DENABLE_COVERAGE=ON -DUNITY_DIR=/opt/unity
cmake --build build/ci-cov -j2
ctest --test-dir build/ci-cov --output-on-failure
gcovr --filter 'src/' --fail-under-line 80
EOF
```

Expected: gcovr passes ≥80% on `src/`. If the new client code drags domain coverage below 80 (look at `gcovr --filter 'src/domain/'` output), add the missing-branch tests (e.g., the read→write-shared paths' edge cases) in a fresh TDD step and commit.

- [ ] **Step 4: ASan/UBSan pass (host preset already configured locally)**

```bash
export UNITY_DIR=/home/hellscoffe/unity
cmake --build build/host-asan
ctest --test-dir build/host-asan
```

Expected: all suites (modbus_crc, modbus_frame, modbus_client) pass on the sanitizer build.

- [ ] **Step 5: Confirm the branch diff is self-consistent and commit any gate fixes**

```bash
git status --short        # no stray files (build/ci-* is untracked; ignore)
git log --oneline main..modbus-client
```

No further commit needed if steps 2–4 were clean (Task 5 is verification). Leftover local dirs `build/ci-*` are untracked and must not be committed.

---

## After All Tasks

Integrate the branch with the superpowers:finishing-a-development-branch skill (inspect status/diff/log, decide merge strategy, update PLAN and CHANGELOG if the skill's workflow requires it, merge, delete branch, run the merged-tree gate suite). Then, only if the user asks, push to `origin`.

## Verification Summary (final)

- `ctest --test-dir build/host-test -j2`: all suites pass (modbus_crc, modbus_frame, modbus_client).
- `python tools/gen_rtm.py check`: 0 violations. KILN-FUN-004 approved with `test_case` `test/unit/test_modbus_client.c`.
- `python tools/check_layering.py`: 7 domain files, no hardware includes.
- Container gates: cppcheck (error-exitcode 1), clang-tidy (domain), clang-format --dry-run --Werror, gcovr ≥80%, cross `target` build green (domain only, no cubemx export yet).
- Host ASan/UBSan build green.

## Known Follow-ups (recorded, not this milestone)

- `modbus_build_request` fc 16 emits only the response-shaped 8-byte ADU — the client composes the fc 16 PDU in-line today; extending the framing helper signature is a future framing-layer revision.
- Exception replies surface as `ERR_RESPONSE`; mapping the specific Modbus exception code to a meaning is deferred.
- Register-map encode/decode layer (typed `kiln_read_pv()` etc.) requires bench verification first (`KILN-FUN-001` stays draft; `kiln_registers.h` is unverified).