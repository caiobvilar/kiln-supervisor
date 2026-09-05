# Modbus RTU Domain Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement and host-test a hardware-free Modbus RTU framing + CRC-16 domain layer for the kiln-supervisor, wired into the existing CMake/CI gates.

**Architecture:** Hexagonal. Pure C domain code in `src/domain/` (no hardware includes — enforced by `tools/check_layering.py`), tested on the host with Unity via the existing `fake_uart` test double. The KM5P_r0 registers (PV=1, SP=6, control status=15, etc.) ride on this framing but are encoded/decoded generically; no kiln-command path exists (interlock-relevant project — host-side L1 work only).

**Tech Stack:** C11, CMake (host-test preset), Unity v2.7.0, GNU `make`/`ctest`. Tooling gates: `tools/gen_rtm.py check`, `tools/check_layering.py`.

**Spec:** `docs/hardware/km5p-controller.md` (CRC-16 §3.5.1; function codes 03/06/16 §3) and `docs/requirements/kiln.yaml` + `docs/02-srs.md` (KILN-INT-001, KILN-CON-001).

## Global Constraints

- **C11**, no GNU extensions; header include guard `#ifndef` convention used across repo.
- **Domain code may include only**: stdlib headers + `src/domain/`, `src/ports`, `src/` (see `tools/check_layering.py` `ALLOWED_PREFIXES`). No hardware/vendor includes.
- **Modbus RTU CRC-16**: poly `0xA001`, init `0xFFFF`, no final XOR, LSB of CRC transmitted first (from Ascon doc §3.5.1).
- **Function codes supported** (KM5P_r0): `0x03` read multiple (≤16 regs), `0x06` write single, `0x16` write multiple (≤16 regs).
- **MAX_ADU**: request 8 bytes, response up to 5 + 2·n. Register addresses are 16-bit; count must be 1..16 for fc 03/16.
- **CI coverage gate ≥80% on `src/`**; static analysis (cppcheck/clang-tidy/clang-format) must stay clean.
- **Requirement text bans** words: *fast, slow, robust, efficient, user-friendly, flexible, optimal, reasonable, appropriate, as needed, if possible, etc., and/or, support, process, handle* (any regex word of these fails `gen_rtm.py check`). Avoid them in requirement `text:` fields.
- Every task's requirements implicitly include this section.
- **Build parallelism:** never `-j` unbounded — cap at `-j2` (this machine's AGENTS.md; desktop must not be saturated).

### Current repo facts (verified 2026-09-03)

- `src/domain/domain.{c,h}` = placeholder returning `42`; `test/unit/test_domain.c` asserts it. These are replaced by real modules in Task A; the placeholder + its test are removed in Task 3.
- `src/adapters/host/fake_uart.{c,h}` provides `fake_uart_init`, `fake_uart_enqueue_rx`, `fake_uart_tx_bytes` (already in tree).
- `test/unit/CMakeLists.txt` builds `test_domain` from `test_domain.c` + `domain` + `unity`. New test executables follow the same pattern.
- Existing requirements YAML: KILN-FUN-001/002, KILN-PER-001, KILN-INT-001, KILN-CON-001 — all `status: draft`. SRS §7 verification-summary table must match YAML counts (Gate 8 of `gen_rtm.py`).
- `KILN-INT-001` has `verification: test`; `KILN-CON-001` has `verification: analysis`.

### File map

| File | Responsibility |
|---|---|
| `src/domain/modbus_crc.h/.c` | CRC-16/Modbus (poly 0xA001, init 0xFFFF) over a byte buffer |
| `src/domain/modbus_frame.h/.c` | Build/parse Modbus RTU ADUs for fc 03/06/16; request encode; CRC check |
| `src/domain/domain.{c,h}` | **Deleted** in Task 3 (placeholder) — replaced by the modules above |
| `test/unit/test_modbus_crc.c` | Unit tests for the CRC (tags `@verifies KILN-FUN-003`) |
| `test/unit/test_modbus_frame.c` | Unit tests for frame build/parse (tags `@verifies KILN-INT-001`, `@verifies KILN-CON-001`) |
| `test/unit/CMakeLists.txt` | Add `test_modbus_crc` + `test_modbus_frame` executables |
| `docs/requirements/kiln.yaml` | Add KILN-FUN-003, approve KILN-INT-001/KILN-CON-001 |
| `docs/02-srs.md` | Add framing requirement, sync §7 verification counts, fix stale OI-01 |
| `CMakeLists.txt` | `domain` library gains the two new .c files |
| `PLAN.md`, `CHANGELOG.md` | Handoff log entries |

---

### Task 1: CRC-16/Modbus

**Files:**
- Create: `src/domain/modbus_crc.h`, `src/domain/modbus_crc.c`
- Test: `test/unit/test_modbus_crc.c`
- Modify: `CMakeLists.txt` (`add_library(domain ...)`) and `test/unit/CMakeLists.txt` (add the new test target)

**Interfaces:**
- Produces (used by Task 2): `uint16_t modbus_crc16(const uint8_t* data, size_t n);`

---

- [ ] **Step 1: Write the failing test** — `test/unit/test_modbus_crc.c`:

```c
/* test/unit/test_modbus_crc.c -- CRC-16/Modbus known-vector tests. */
/* @verifies KILN-FUN-003 */
#include "unity.h"

#include "modbus_crc.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t req_010300000001[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x01 };
static const uint8_t req_010300010001[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01 };
static const uint8_t req_01060001000a[] = { 0x01, 0x06, 0x00, 0x01, 0x00, 0x0A };

void test_crc_empty(void) { TEST_ASSERT_EQUAL_UINT16(0xFFFF, modbus_crc16(NULL, 0)); }

void test_crc_example_req(void) { TEST_ASSERT_EQUAL_UINT16(0x0A84, modbus_crc16(req_010300000001, sizeof(req_010300000001))); }

void test_crc_pv_read_req(void) { TEST_ASSERT_EQUAL_UINT16(0xCAD5, modbus_crc16(req_010300010001, sizeof(req_010300010001))); }

void test_crc_write_single_req(void) { TEST_ASSERT_EQUAL_UINT16(0x0D58, modbus_crc16(req_01060001000a, sizeof(req_01060001000a))); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_empty);
    RUN_TEST(test_crc_example_req);
    RUN_TEST(test_crc_pv_read_req);
    RUN_TEST(test_crc_write_single_req);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --preset host-test && cmake --build --preset host-test -j2 && ctest --preset host-test --output-on-failure
```
Expected: FAIL — `modbus_crc.h` not found / unresolved `modbus_crc16`.

- [ ] **Step 3: Write minimal implementation**

`src/domain/modbus_crc.h`:
```c
#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include <stddef.h>
#include <stdint.h>

uint16_t modbus_crc16(const uint8_t* data, size_t n);

#endif /* MODBUS_CRC_H */
```

`src/domain/modbus_crc.c`:
```c
/* src/domain/modbus_crc.c -- CRC-16/Modbus (poly 0xA001, init 0xFFFF, LSB-first).
 * From Ascon Tecnologic "Serial communication protocol ModBUS for Programmers
 * KM5/KR5/KX5" §3.5.1 (see docs/hardware/km5p-controller.md). */
#include "modbus_crc.h"

uint16_t modbus_crc16(const uint8_t* data, size_t n)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xA001u : (crc >> 1);
        }
    }
    return crc;
}
```

- [ ] **Step 4: Modify `test/unit/CMakeLists.txt`** to add the new test target:

```cmake
add_executable(test_modbus_crc test_modbus_crc.c)
target_link_libraries(test_modbus_crc PRIVATE domain unity)
add_test(NAME modbus_crc COMMAND test_modbus_crc)
```

And modify root `CMakeLists.txt`:
```cmake
add_library(domain STATIC src/domain/domain.c src/domain/modbus_crc.c)
```

- [ ] **Step 5: Run tests, make them pass**

```bash
cmake --preset host-test && cmake --build --preset host-test -j2 && ctest --preset host-test --output-on-failure
```
Expected: `modbus_crc` PASS (all 4), `domain` PASS.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt test/unit/CMakeLists.txt src/domain/modbus_crc.c src/domain/modbus_crc.h test/unit/test_modbus_crc.c
git commit -m "feat: add CRC-16/Modbus domain module with known-vector tests"
```

---

### Task 2: Modbus RTU frame build/parse (fc 03/06/16)

**Files:**
- Create: `src/domain/modbus_frame.h`, `src/domain/modbus_frame.c`
- Test: `test/unit/test_modbus_frame.c`
- Modify: `CMakeLists.txt`, `test/unit/CMakeLists.txt`

**Interfaces:**
- Consumes: `uint16_t modbus_crc16(const uint8_t*, size_t)` (Task 1)
- Produces:
  ```c
  #define MODBUS_FC_READ_MULTIPLE   0x03
  #define MODBUS_FC_WRITE_SINGLE    0x06
  #define MODBUS_FC_WRITE_MULTIPLE  0x16
  #define MODBUS_FRAME_MAX          256u

  int modbus_build_request(uint8_t out[], size_t cap, uint8_t addr,
                           uint8_t fc, uint16_t reg_or_addr, uint16_t value_or_count);

  int modbus_frame_check(const uint8_t frame[], size_t len, uint8_t addr);
  ```

---

- [ ] **Step 1: Write the failing tests** — `test/unit/test_modbus_frame.c`:

```c
/* test/unit/test_modbus_frame.c -- Modbus RTU ADU build/parse tests. */
/* @verifies KILN-INT-001 */
/* @verifies KILN-CON-001 */
#include "unity.h"

#include <string.h>

#include "modbus_frame.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_read_request(void)
{
    uint8_t out[MODBUS_FRAME_MAX];
    int n = modbus_build_request(out, sizeof(out), 0x01, MODBUS_FC_READ_MULTIPLE, 0x0001, 0x0001);
    TEST_ASSERT_EQUAL_INT(8, n);
    uint8_t expect[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 8);
}

void test_build_write_single(void)
{
    uint8_t out[MODBUS_FRAME_MAX];
    int n = modbus_build_request(out, sizeof(out), 0x01, MODBUS_FC_WRITE_SINGLE, 0x0001, 0x000A);
    TEST_ASSERT_EQUAL_INT(8, n);
    uint8_t expect[] = { 0x01, 0x06, 0x00, 0x01, 0x00, 0x0A, 0x58, 0x0D };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 8);
}

void test_build_write_multiple_reject_count_gt16(void)
{
    uint8_t out[MODBUS_FRAME_MAX];
    TEST_ASSERT_LESS_THAN(0, modbus_build_request(out, sizeof(out), 0x01, MODBUS_FC_WRITE_MULTIPLE, 0x0001, 17));
}

void test_frame_check_accepts_valid(void)
{
    uint8_t frame[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA };
    int n = modbus_frame_check(frame, sizeof(frame), 0x01);
    TEST_ASSERT_GREATER_OR_EQUAL(0, n);
}

void test_frame_check_rejects_corrupt_crc(void)
{
    uint8_t frame[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCB }; /* bad CRC LSB */
    TEST_ASSERT_LESS_THAN(0, modbus_frame_check(frame, sizeof(frame), 0x01));
}

void test_frame_check_rejects_wrong_address(void)
{
    uint8_t frame[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA };
    TEST_ASSERT_LESS_THAN(0, modbus_frame_check(frame, sizeof(frame), 0x02));
}

void test_frame_check_rejects_too_short(void)
{
    uint8_t frame[] = { 0x01, 0x03, 0x00 };
    TEST_ASSERT_LESS_THAN(0, modbus_frame_check(frame, sizeof(frame), 0x01));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_read_request);
    RUN_TEST(test_build_write_single);
    RUN_TEST(test_build_write_multiple_reject_count_gt16);
    RUN_TEST(test_frame_check_accepts_valid);
    RUN_TEST(test_frame_check_rejects_corrupt_crc);
    RUN_TEST(test_frame_check_rejects_wrong_address);
    RUN_TEST(test_frame_check_rejects_too_short);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Expected: FAIL — `modbus_frame.h` not found / undefined functions.

- [ ] **Step 3: Write implementation**

`src/domain/modbus_frame.h`:
```c
#ifndef MODBUS_FRAME_H
#define MODBUS_FRAME_H

#include <stddef.h>
#include <stdint.h>

#define MODBUS_FC_READ_MULTIPLE   0x03
#define MODBUS_FC_WRITE_SINGLE    0x06
#define MODBUS_FC_WRITE_MULTIPLE  0x16

#define MODBUS_FRAME_MAX          256u

int modbus_build_request(uint8_t out[], size_t cap, uint8_t addr,
                         uint8_t fc, uint16_t reg_or_addr, uint16_t value_or_count);

int modbus_frame_check(const uint8_t frame[], size_t len, uint8_t addr);

#endif /* MODBUS_FRAME_H */
```

`src/domain/modbus_frame.c`:
```c
/* src/domain/modbus_frame.c -- Modbus RTU ADU build/parse (fc 03/06/16).
 * Requests: 8 bytes (addr, fc, reg hi/lo, value/count hi/lo, CRC lo/hi).
 * Response validation computes CRC over all but the last 2 bytes and
 * cross-checks the address field (KM5P_r0 supports fc 03/06/16, docs/hardware). */
#include "modbus_frame.h"

#include "modbus_crc.h"

int modbus_build_request(uint8_t out[], size_t cap, uint8_t addr,
                         uint8_t fc, uint16_t reg_or_addr, uint16_t value_or_count)
{
    if ((fc == MODBUS_FC_READ_MULTIPLE || fc == MODBUS_FC_WRITE_MULTIPLE)
        && (value_or_count < 1 || value_or_count > 16)) {
        return -1;
    }
    if (cap < 8) {
        return -1;
    }
    uint8_t pdu[8] = {
        addr, fc,
        (uint8_t)(reg_or_addr >> 8), (uint8_t)reg_or_addr,
        (uint8_t)(value_or_count >> 8), (uint8_t)value_or_count,
        0, 0,
    };
    uint16_t crc = modbus_crc16(pdu, 6);
    pdu[6] = (uint8_t)crc;         /* LSB first */
    pdu[7] = (uint8_t)(crc >> 8);  /* MSB */
    for (size_t i = 0; i < 8; ++i) {
        out[i] = pdu[i];
    }
    return 8;
}

int modbus_frame_check(const uint8_t frame[], size_t len, uint8_t addr)
{
    if (len < 5) { /* addr, fc, data, crc lo, crc hi */
        return -1;
    }
    if (frame[0] != addr) {
        return -1;
    }
    uint16_t crc = modbus_crc16(frame, len - 2);
    uint16_t recv = (uint16_t)(frame[len - 1] << 8) | frame[len - 2]; /* LSB first on wire */
    if (crc != recv) {
        return -1;
    }
    return (int)(len - 2); /* number of data bytes incl. addr+fc */
}
```

- [ ] **Step 4: Add test target + build the new sources**

`test/unit/CMakeLists.txt`:
```cmake
add_executable(test_modbus_frame test_modbus_frame.c)
target_link_libraries(test_modbus_frame PRIVATE domain unity)
add_test(NAME modbus_frame COMMAND test_modbus_frame)
```

`CMakeLists.txt`:
```cmake
add_library(domain STATIC src/domain/domain.c src/domain/modbus_crc.c src/domain/modbus_frame.c)
```

- [ ] **Step 5: Run tests, make them pass**

```bash
cmake --preset host-test && cmake --build --preset host-test -j2 && ctest --preset host-test --output-on-failure
```
Expected: `modbus_crc` PASS, `modbus_frame` PASS, `domain` PASS.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt test/unit/CMakeLists.txt src/domain/modbus_frame.c src/domain/modbus_frame.h test/unit/test_modbus_frame.c
git commit -m "feat: add Modbus RTU frame build/parse for fc 03/06/16"
```

---

### Task 3: Remove placeholder, approve requirements, sync SRS, add traceability tags

**Files:**
- Delete: `src/domain/domain.c`, `src/domain/domain.h`, `test/unit/test_domain.c`
- Modify: `CMakeLists.txt`, `test/unit/CMakeLists.txt`, `docs/requirements/kiln.yaml`, `docs/02-srs.md`
- Test: `test/unit/test_modbus_frame.c` (add `@verifies` tags if not already), `test/unit/test_modbus_crc.c`

**Interfaces:**
- Consumes: nothing new (clean-up).
- Produces: requirements KILN-FUN-003 / approved KILN-INT-001, KILN-CON-001, per `gen_rtm.py` Gates 1–3 & 8.

---

- [ ] **Step 1: Delete the placeholder + its test**

```bash
git rm src/domain/domain.c src/domain/domain.h test/unit/test_domain.c
```

- [ ] **Step 2: Strip it from the build**

`CMakeLists.txt`:
```cmake
add_library(domain STATIC src/domain/modbus_crc.c src/domain/modbus_frame.c)
```

`test/unit/CMakeLists.txt`: remove the `test_domain` lines (keep `unity`, `modbus_crc`, `modbus_frame` targets).

- [ ] **Step 3: Add `@verifies` tags** — already present in Tasks 1–2 test files:
  - `test/unit/test_modbus_crc.c` has `/* @verifies KILN-FUN-003 */`
  - `test/unit/test_modbus_frame.c` has `/* @verifies KILN-INT-001 */` and `/* @verifies KILN-CON-001 */`
  Verify they are present; add if missing.

- [ ] **Step 4: Update `docs/requirements/kiln.yaml`**

Append the framing requirement and approve the existing interface/constraint requirements that the code now satisfies:

```yaml
- id: KILN-FUN-003
  text: >
    The Modbus client shall build and validate Modbus RTU frames with CRC-16
    for function codes 03, 06, and 16, bounding multi-register transfers to
    sixteen registers.
  type: functional
  rationale: >
    N-01 requires a Modbus RTU client; framing and CRC-16 (Ascon doc §3.5.1)
    are the wire contract, host-testable without hardware.
  parent: N-01
  priority: shall
  verification: test
  status: approved
  test_case:
    - test/unit/test_modbus_crc.c
    - test/unit/test_modbus_frame.c
```

Also change `KILN-INT-001` and `KILN-CON-001` `status: draft` → `status: approved`:
- `KILN-INT-001` (verification: test) → `test_case: [test/unit/test_modbus_frame.c]`
- `KILN-CON-001` (verification: analysis) → `test_case: []` (analysis requirements do not need test_case; leave `test_case` absent/empty)

- [ ] **Step 5: Update `docs/02-srs.md`**

- §6.1 add: **KILN-FUN-003** (shall) — build/validate Modbus RTU frames (CRC-16, fc 03/06/16, ≤16 regs multi-register).
- §7 Verification summary: reconcile the table so it exactly matches the YAML totals (Gate 8 fails otherwise). After these edits the active requirements are: `verification: test` = FUN-001, FUN-002, PER-001, FUN-003, INT-001 → **Test 5**; `verification: analysis` = CON-001 → **Analysis 1**. Set the table accordingly.
- §5 + §8: replace stale OI-01 "register map not publicly available — must be obtained" with a **RESOLVED** note pointing to `docs/hardware/km5p-controller.md` "RESOLVED 2026-07-30". Update §5 Assumptions that describe the register map as "not yet extracted".

- [ ] **Step 6: Regenerate RTM and run all gates**

```bash
python tools/gen_rtm.py gen
python tools/gen_rtm.py check
python tools/check_layering.py
```

- [ ] **Step 7: Run full host suite + static analysis**

```bash
cmake --preset host-test && cmake --build --preset host-test -j2 && ctest --preset host-test --output-on-failure
cmake --preset host-asan && cmake --build --preset host-asan -j2 && ctest --preset host-asan --output-on-failure
clang-format --dry-run --Werror $(find src test -name '*.[ch]')
python tools/gen_rtm.py check
python tools/check_layering.py
```
Expected: all PASS, RTM + layering clean.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "feat: add Modbus framing requirement, approve traceability, drop placeholder"
```

---

### Task 4: Update PLAN.md + CHANGELOG (handoff log)

**Files:**
- Modify: `PLAN.md`, `CHANGELOG.md`

- [ ] **Step 1: Add a Log line to `PLAN.md`** (newest last, matching existing format)

```markdown
- `2026-09-03` — Implemented host-testable Modbus domain layer: `modbus_crc` +
  `modbus_frame` (fc 03/06/16), known-vector L1 tests via fake_uart seam (no
  hardware used). Added KILN-FUN-003, approved KILN-INT/CON-001, synced SRS §7 +
  RTM. Still hardware-blocked: bench-verify register map, order-code variant,
  board/transceiver/WiFi choices (see Open questions) — comms stays host-only,
  no kiln-command path (interlock).
```

- [ ] **Step 2: `CHANGELOG.md`** — under `[Unreleased]` add one line (Conventional Commits style):

```markdown
- feat: add host-testable Modbus RTU framing + CRC-16 domain layer (fc 03/06/16, KILN-FUN-003)
```

- [ ] **Step 3: Commit**

```bash
git add PLAN.md CHANGELOG.md
git commit -m "docs: record Modbus framing milestone in PLAN + CHANGELOG"
```
