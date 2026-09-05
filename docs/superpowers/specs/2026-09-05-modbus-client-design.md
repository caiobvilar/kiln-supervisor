# Modbus Client Design

Date: 2026-09-05
Status: approved (design review, 2026-09-05)

## 1. Context and goals

The next software-ready milestone after the Modbus RTU domain layer (CRC + framing,
merged on main) is a host-testable *client*: given a KM5P-compatible slave, the
client issues read/write requests over the UART port seam, times out if no valid
response arrives, and parses register values back out.

The register map of the KM5P_r0 controller is documented but **unverified** pending
bench read-back (see `docs/hardware/km5p-controller.md`). Encode/decode of the
register map semantics is therefore deferred; this client stays register-agnostic so
no unverified KM5P behavior is baked into tested code before bench verification.
Interlock note: this module cannot be reached by any remote path and contains no
"start firing" logic — the human-confirmation gate (KILN-FUN-002) belongs to a layer
above, and any real command transmission remains gated on bench + safety case.

## 2. Scope

In scope:
- Generic Modbus RTU client: read holding registers (fc 03), write single (fc 06),
  write multiple (fc 16), bounded response timeout, response validation, register
  value extraction.
- `kiln_registers.h`: constants-only header with the documented KM5P register
  numbers, every constant marked unverified. Nothing consumes them yet.
- L1 host tests on the `fake_uart` + `fake_clock` doubles.
- New requirement KILN-FUN-004 + SRS/RTM sync.

Deferred (in PLAN.md / LEDGER):
- Register-map encode/decode layer (typed `kiln_read_pv()` etc.) — only after bench
  verification.
- Automatic retries — caller chooses; this client is one-shot.
- Asynchronous / state-machine client — no need yet.
- Modbus *exception-code* decoding (which specific exception) — the client returns
  ERR_RESPONSE on any exception reply; mapping the exception code byte to a meaning
  happens later.

## 3. Components

- `src/domain/modbus_client.{c,h}` — new, the client.
- `src/domain/kiln_registers.h` — new, constants only.
- `src/domain/modbus_frame.{c,h}` — existing, reused (build/check).
- `src/domain/modbus_crc.{c,h}` — existing, used by frame check.
- `src/ports/i_uart.h`, `src/ports/i_clock.h` — existing port seams, only seams the
  client depends on.
- `src/adapters/host/fake_uart.{c,h}`, `src/adapters/host/fake_clock.c` — existing
  test doubles.
- `test/unit/test_modbus_client.c` — new.
- `CMakeLists.txt` — add `src/domain/modbus_client.c` to the `domain` library.
- `test/unit/CMakeLists.txt` — add `test_modbus_client` target.
- `docs/requirements/kiln.yaml`, `docs/02-srs.md`, `docs/06-rtm.md` —
  KILN-FUN-004 added; RTM regenerated with `tools/gen_rtm.py`.
- `PLAN.md`, `CHANGELOG.md` — milestone log entries.

Layering: domain code may include only `include/`, `src/domain/`, `src/ports/`
(`tools/check_layering.py` enforces). The client uses no hardware code.

## 4. API

```c
typedef enum {
    MODBUS_CLIENT_OK          =  0,
    MODBUS_CLIENT_ERR_TIMEOUT = -1, /* no complete valid response within timeout */
    MODBUS_CLIENT_ERR_CRC     = -2, /* response CRC mismatch */
    MODBUS_CLIENT_ERR_RESPONSE= -3, /* addr/fc/length/byte-count mismatch */
    MODBUS_CLIENT_ERR_UART    = -4, /* port write/read failure */
    MODBUS_CLIENT_ERR_PARAM   = -5, /* bad arguments (count>16, NULL, count>cap) */
} modbus_client_status_t;

#define MODBUS_CLIENT_MAX_REGS 16u

typedef struct {
    uart_t*  uart;
    clock_t* clock;
    uint64_t response_timeout_us;
    uint8_t  rx_buf[MODBUS_FRAME_MAX];
} modbus_client_t;

void modbus_client_init(modbus_client_t* c, uart_t* uart, clock_t* clock,
                        uint64_t response_timeout_us);

modbus_client_status_t modbus_client_read_registers(
    modbus_client_t* c, uint8_t addr, uint16_t start,
    uint16_t count, uint16_t* out, size_t out_cap);              /* fc 03 */
modbus_client_status_t modbus_client_write_single(
    modbus_client_t* c, uint8_t addr, uint16_t reg, uint16_t value);  /* fc 06 */
modbus_client_status_t modbus_client_write_multiple(
    modbus_client_t* c, uint8_t addr, uint16_t start,
    const uint16_t* values, size_t count);                       /* fc 16 */
```

Return values: `MODBUS_CLIENT_OK` on success; `MODBUS_CLIENT_ERR_PARAM` on bad
arguments before any UART traffic; otherwise one of TIMEOUT/CRC/RESPONSE/UART.

## 5. Control flow (per operation)

1. Argument check: null pointers, `count` in `[1, MODBUS_CLIENT_MAX_REGS]`,
   `out_cap >= count` (reads). Violation returns ERR_PARAM, no UART traffic.
2. Build the request with `modbus_build_request` into a stack tx buffer.
3. `uart_write` the whole request; any failure or short write returns ERR_UART.
4. Derive the expected response length from the request:
   - fc 06 / fc 16: the slave echoes the 8-byte request → `len == 8`.
   - fc 03: `len == 5 + 2*count` and `frame[2] == 2*count` (byte count field).
5. Poll loop until `rx_len == expected`:
   - `uart_available`, then `uart_read` available bytes into `rx_buf`.
   - Exception rule: whenever `rx_len >= 5` and `rx_buf[1]` has the bit 0x80 set,
     treat the reply as a Modbus exception and return ERR_RESPONSE immediately. This
     is unambiguous: no legit prefix of any expected reply has the 0x80 bit in its
     fc byte (0x03 / 0x06 / 0x16, or data after the fc byte).
   - Compute the deadline once (`ticks_us + timeout`); if the loop iteration occurs
     after the deadline and the frame is still incomplete, return ERR_TIMEOUT.
6. Validate the complete expected-length frame with `modbus_frame_check(rx_buf, len,
   addr)`; then require `frame[1] == expected fc` and, for fc 03, `frame[2] ==
   2*count`. Failure maps to ERR_CRC (CRC bad) or ERR_RESPONSE (addr/len/fc/byte-count).
7. fc 06 / fc 16 success → ERR_OK (nothing further to parse; response echo is
   validated by frame check).
   fc 03 success → extract `count` big-endian register words from `frame[3..]` into
   `out[]`, return ERR_OK.

Notes:
- `rx` returns bytes actually read and never blocks (port contract); the loop is
  bounded by the timeout, not by `available` counts, so a receiver that
  delivers one byte at a time still terminates.
- The clock is only read for timeout enforcement; a clock that never advances makes
  the client wait forever exactly as a wall-clock would.
- `rx_buf` is reset (`rx_len = 0`) at the start of each operation; state is
  per-operation, so the client is re-entrant between calls (two clients may share
  one uart only if the caller serializes — documented, not enforced).

## 6. Error handling summary

| Situation                                   | Result                    |
|---------------------------------------------|---------------------------|
| Invalid arguments                           | ERR_PARAM, no UART I/O    |
| Write fails / short write                   | ERR_UART                  |
| No complete frame before deadline           | ERR_TIMEOUT               |
| Frame completes but CRC bad                 | ERR_CRC, registers unwritten |
| Wrong addr / bad fc echo / byte-count mismatch | ERR_RESPONSE           |
| Modbus exception reply (fc 0x80)               | ERR_RESPONSE, immediately |

## 7. Testing (L1, host)

`test_modbus_client.c` with `fake_uart` + `fake_clock`, `@verifies KILN-FUN-004`:
- fc 03 success: valid response arrives, exact tx request asserted via
  `fake_uart_tx_bytes`, returned register values asserted.
- fc 06 success; fc 16 success: echo response, ERR_OK, tx asserted.
- Timeout: no response → ERR_TIMEOUT; partial frame + clock advanced past deadline →
  ERR_TIMEOUT.
- CRC error: full frame, bad CRC → ERR_CRC; `out` untouched.
- Wrong address: valid frame for a different slave → ERR_RESPONSE.
- Byte-count mismatch on fc 03 (excess/short) → ERR_RESPONSE.
- Exception reply (`frame[1] & 0x80`) → ERR_RESPONSE immediately, before timeout.
- Params: count 0 / count 17 / count>cap / NULL out → ERR_PARAM with zero tx bytes.
- Clock already past deadline before any response → ERR_TIMEOUT.

Verification gates (all clean at `-j2`):
- `ctest --test-dir build -j2` host suite green.
- `tools/gen_rtm.py check` 0 violations; `tools/gen_rtm.py gen` no diff.
- `tools/check_layering.py` OK.
- `clang-format --dry-run --Werror` clean.
- `cmake --build build -j2` clean.

## 8. Requirements and traceability

- New requirement **KILN-FUN-004** (priority: shall, verification: test;
  approval status per the `tools/gen_rtm.py` gate: draft until its test_case is
  implemented and green, approved via the same review process used for FUN-003) —
  wording:
  "The Modbus client shall issue read and write requests over the UART port
  interface and validate the response within a bounded timeout."
  `test_case: [test/unit/test_modbus_client.c]`. No banned words
  (fast/slow/robust/efficient/process/handle/support/and/or/etc.).
- KILN-FUN-001 stays draft — requires the register-map encode/decode layer, which is
  bench-gated. FUN-003 (framing) is already approved and reused here.
- `kiln_registers.h` carries no requirement (constants only, all unverified).

## 9. Non-goals / safety posture

- No register-map semantics, no typed kiln API, no retries, no async — deferred
  (Section 2).
- No safety logic in this module. The RS-485 command path (if any) that could reach
  a firing command stays behind KILN-FUN-002's human confirmation and is not
  reachable from any remote path in this milestone.
- Real-UART framing (inter-frame gap) is out of scope for L1: the host doubles are
  synchronous; the expected-length model works over both the fake and the eventual
  real adapter, but USB-serial timing validation is a bench activity.