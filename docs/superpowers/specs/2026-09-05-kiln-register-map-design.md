# Kiln Register-Map Layer Design

Date: 2026-09-05
Status: approved (design review, 2026-09-05)

## 1. Context and goals

The generic Modbus RTU client is done (KILN-FUN-004, merged). The next milestone
is the **register-map layer**: encode/decode of the KM5P_r0 register subset that is
actually documented in-repo (`docs/hardware/km5p-controller.md`, the Ascon Tecnologic
register map delivered via COEL support) into the instrument's own semantics, exposed
as typed accessors (`kiln_read_pv()` etc.) that drive `modbus_client`.

The register **values** remain unverified pending bench read-back, per
`docs/hardware/km5p-controller.md` and SRS OI-01. This layer does not end that: the
numbers stay in `kiln_registers.h` (constants-only, still marked unverified) and are
consumed by the accessors unchanged. What this milestone adds is the **semantics**
above the numbers — the documented meaning table (dP scaling, PV sentinels, alarm
bitmask, state enums) made host-testable against the vendor document, which (per the
agreed decision) is the ground truth for this milestone. Bench read-back remains a
separate Activity that corrects `kiln_registers.h`, not this layer's structure.

Interlock note: no remote "start firing" command path exists in this layer or below.
The human-confirmation gate (KILN-FUN-002) belongs to a layer above, and any real
command transmission stays gated on bench verification plus the safety case.

## 2. Scope

In scope:
- Pure conversion functions (wire integer ↔ documented instrument semantics).
- Typed I/O accessors over `modbus_client` for the full documented subset
  (operational cluster + comms/config readers):
  PV, operative SP, settable SP (write), alarms, alarm-ack (13/14), control mode,
  program status (580), program step (582), `Pr.St` (10367), `Add` (10337), `bAud`
  (10338).
- L1 host tests on `fake_uart` + `fake_clock` driving the real `modbus_client`,
  plus direct conversion-table tests.
- KILN-FUN-001 amended and approved (see §8); SRS/RTM sync.

Deferred (PLAN.md / LEDGER follow-ups, bench- or scope-gated):
- Ramp/soak program-table registers (P1.F..P8.E6, Nº 129–414) and remaining config
  parameters — outside the 11-register subset transcribed in-repo.[^1]
- SP range clamp against `SPLL`/`SPLH` — those registers are outside the subset.
- Meanings of the alarm-ack register pair (13 vs 14) — each accessor writes value 1
  to its own register; which physical ack each performs is a bench question.
- Wire value of `oFF` for `Add` (assumed 0 → `KILN_MAP_ADDR_OFF`; bench to confirm).
- Automatic retries and asynchronous/state-machine access — caller chooses; per
  operation is one-shot like the client.
- Register-number verification at the bench (OI-01) — corrects
  `kiln_registers.h`, not this layer.

[^1]: `SPLL`/`SPLH`, `dP`, and the program-table live in the full Ascon map
    transcribed outside the repo; adding their register numbers in-repo is a future
    milestone, not this one.

## 3. Components

- `src/domain/kiln_map.{c,h}` — new, the layer (pure conversions + typed accessors).
- `src/domain/kiln_registers.h` — existing, constants-only, consumed here (still
  unverified).
- `src/domain/modbus_client.{c,h}` — existing, the I/O substrate (only
  `read_registers` with `count == 1` and `write_single` are used; no fc 16).
- `src/domain/modbus_frame.{c,h}`, `modbus_crc.{c,h}` — existing, via the client.
- `src/ports/i_uart.h`, `i_clock.h` — existing seams, reachable only through the
  client.
- `src/adapters/host/fake_uart.{c,h}`, `fake_clock.{c,h}` — existing test doubles.
- `test/unit/test_kiln_map.c` — new.
- `CMakeLists.txt` — add `src/domain/kiln_map.c` to the `domain` library.
- `test/unit/CMakeLists.txt` — add `test_kiln_map` target (pulls in the fakes).
- `docs/requirements/kiln.yaml`, `docs/02-srs.md`, `docs/06-rtm.md` — KILN-FUN-001
  amended + approved; RTM regenerated with `tools/gen_rtm.py`.
- `PLAN.md`, `CHANGELOG.md` — milestone log entries.

Layering: domain code may include only `include/`, `src/domain/`, `src/ports/`
(`tools/check_layering.py` enforces). The layer uses no hardware code.

## 4. API

```c
typedef enum {
    KILN_MAP_OK                =  0,
    KILN_MAP_ERR_TIMEOUT       = -1, /* client: no complete valid response */
    KILN_MAP_ERR_CRC           = -2, /* client: response CRC mismatch */
    KILN_MAP_ERR_RESPONSE      = -3, /* client: addr/fc/length/byte-count mismatch */
    KILN_MAP_ERR_UART          = -4, /* client: port write/read failure */
    KILN_MAP_ERR_PARAM         = -5, /* bad arguments (incl. dp outside 0..3) */
    KILN_MAP_ERR_VALUE         = -6, /* wire code outside the documented set /
                                        encode result outside the raw value range */
    KILN_MAP_PV_UNDERRANGE     = -7, /* PV == -10000 */
    KILN_MAP_PV_OVERRANGE      = -8, /* PV == 10000 */
    KILN_MAP_PV_ADC_OVERFLOW   = -9, /* PV == 10001 */
    KILN_MAP_PV_UNAVAILABLE    = -10, /* PV == 10003 */
    KILN_MAP_ADDR_OFF          = -11, /* Add == oFF (raw 0, assumed) */
} kiln_map_status_t;

typedef enum {
    KILN_ALARM_ACK_1 = 0, /* register 13 (0x000D) */
    KILN_ALARM_ACK_2 = 1, /* register 14 (0x000E) */
} kiln_alarm_ack_t;
typedef enum {
    KILN_CONTROL_AUTO    = 0,
    KILN_CONTROL_MANUAL  = 1,
    KILN_CONTROL_STANDBY = 2,
} kiln_control_mode_t;
typedef enum {
    KILN_PR_RESET     = 0,
    KILN_PR_RUN       = 1,
    KILN_PR_HOLD      = 2,
    KILN_PR_CONTINUE  = 3,
} kiln_pr_status_t;
typedef enum {
    KILN_BAUD_1200   = 0,
    KILN_BAUD_2400   = 1,
    KILN_BAUD_9600   = 2,
    KILN_BAUD_19200  = 3,
    KILN_BAUD_38400  = 4,
} kiln_baud_t;

typedef struct {
    bool al1;            /* bit0  */
    bool al2;            /* bit1  */
    bool al3;            /* bit2  */
    bool lba;            /* bit9  -- loop break alarm */
    bool power_failure;  /* bit10 */
    bool generic_error;  /* bit11 */
} kiln_alarms_t;

/* --- pure conversions (no I/O) --- */

/* Signed, device-resolution (raw already carries the "dP" decimal point:
 * 24.5 at dp=1 is wire 245). dp in 0..3 (else ERR_PARAM) sets the caller's
 * resolution contract. out = (int32_t)raw. */
kiln_map_status_t kiln_pv_decode(int16_t raw, uint8_t dp, int32_t* out);
kiln_map_status_t kiln_sp_decode(int16_t raw, uint8_t dp, int32_t* out);
/* value is in the same device resolution; wire word = value, must fit uint16
 * wire range (else ERR_VALUE). */
kiln_map_status_t kiln_sp_encode(int32_t value, uint8_t dp, uint16_t* out);

void kiln_alarms_decode(uint16_t raw, kiln_alarms_t* out);

/* State/enum codecs validate the raw code against the documented set. */
kiln_map_status_t kiln_control_decode(uint16_t raw, kiln_control_mode_t* out);
kiln_map_status_t kiln_control_encode(kiln_control_mode_t m, uint16_t* out);
kiln_map_status_t kiln_prog_status_decode(uint16_t raw, uint16_t* out); /* 0..7 */
kiln_map_status_t kiln_prog_status_encode(uint16_t code, uint16_t* out); /* 0..7 */
kiln_map_status_t kiln_prog_step_decode(uint16_t raw, uint16_t* out);  /* 0..9 */
kiln_map_status_t kiln_pr_status_decode(uint16_t raw, kiln_pr_status_t* out);
kiln_map_status_t kiln_pr_status_encode(kiln_pr_status_t p, uint16_t* out);
kiln_map_status_t kiln_baud_decode(uint16_t raw, kiln_baud_t* out);
kiln_map_status_t kiln_baud_encode(kiln_baud_t b, uint16_t* out);
kiln_map_status_t kiln_add_decode(uint16_t raw, uint16_t* out); /* oFF -> ADDR_OFF */
kiln_map_status_t kiln_add_encode(uint16_t addr, uint16_t* out); /* 1..254 */

/* --- typed I/O accessors (drive modbus_client) --- */

kiln_map_status_t kiln_read_pv(modbus_client_t* c, uint8_t slave,
                               uint8_t dp, int32_t* out);
kiln_map_status_t kiln_read_op_setpoint(modbus_client_t* c, uint8_t slave,
                                        uint8_t dp, int32_t* out);
kiln_map_status_t kiln_write_setpoint(modbus_client_t* c, uint8_t slave,
                                      int32_t value, uint8_t dp);
kiln_map_status_t kiln_read_alarms(modbus_client_t* c, uint8_t slave,
                                   kiln_alarms_t* out);
kiln_map_status_t kiln_write_alarm_ack(modbus_client_t* c, uint8_t slave,
                                       kiln_alarm_ack_t ack);
kiln_map_status_t kiln_read_control_mode(modbus_client_t* c, uint8_t slave,
                                         kiln_control_mode_t* out);
kiln_map_status_t kiln_write_control_mode(modbus_client_t* c, uint8_t slave,
                                          kiln_control_mode_t m);
kiln_map_status_t kiln_read_prog_status(modbus_client_t* c, uint8_t slave,
                                        uint16_t* out);
kiln_map_status_t kiln_write_prog_status(modbus_client_t* c, uint8_t slave,
                                         uint16_t code);
kiln_map_status_t kiln_read_prog_step(modbus_client_t* c, uint8_t slave,
                                      uint16_t* out);
kiln_map_status_t kiln_read_pr_status(modbus_client_t* c, uint8_t slave,
                                      kiln_pr_status_t* out);
kiln_map_status_t kiln_write_pr_status(modbus_client_t* c, uint8_t slave,
                                       kiln_pr_status_t p);
kiln_map_status_t kiln_read_instrument_address(modbus_client_t* c, uint8_t slave,
                                               uint16_t* out);
kiln_map_status_t kiln_write_instrument_address(modbus_client_t* c, uint8_t slave,
                                                uint16_t addr);
kiln_map_status_t kiln_read_baud(modbus_client_t* c, uint8_t slave,
                                 kiln_baud_t* out);
kiln_map_status_t kiln_write_baud(modbus_client_t* c, uint8_t slave,
                                  kiln_baud_t b);
```

`slave` is the instrument address for the frame (the client's Modbus addr byte);
`Add`/`bAud`/`Pr.St` accessors read or write the instrument's own *config*
registers for that same instrument.

## 5. Conversion semantics

- **dP scale** (PV, operative SP, settable SP): per the map's "Signed int, dP"
  convention the wire word *is already the device-resolution value* — the
  displayed number with the decimal point removed (24.5 °C at `dp=1` reads as
  raw `245`). Decode is therefore identity on the signed word: `out =
  (int32_t)raw`; `dp ∈ {0,1,2,3}` (else ERR_PARAM) validates the call and tells
  the caller the resolution of `out` (units of `10^dp`), it does not scale.
  Encode (SP write): the caller supplies the value in the same device resolution
  and the wire word is that value itself, range-checked to the uint16 wire range
  (else ERR_VALUE). The layer does no unit math; the instrument's configured dP
  is the caller's resolution contract.
- **PV sentinels** are recognized on the raw wire word first: `-10000`
  underrange, `10000` overrange, `10001` A/D overflow, `10003` unavailable →
  the matching `KILN_MAP_PV_*` status, `out` untouched. Any other value decodes
  normally.
- **Alarms bitmask**: bit0→`al1`, bit1→`al2`, bit2→`al3`, bit9→`lba`,
  bit10→`power_failure`, bit11→`generic_error`; the reserved bits (3–8, 12–15)
  are ignored (not represented).
- **State/enum codecs validate**: a raw code outside the documented set returns
  ERR_VALUE and leaves `out` untouched — never silently mapped, so an instrument
  anomaly or a wrong register number surfaces as an error, not a bogus reading.
- **Program status (580)** and **program step (582)** are opaque codes `0..7` /
  `0..9`; only the range is pinned (bench names the intermediate values). The
  layer returns the code as `uint16_t`, range-checked.
- **`Add`**: wire `0` is assumed to be `oFF` → `KILN_MAP_ADDR_OFF`; wire `1..254`
  decodes to the address; any other wire value is ERR_VALUE. Encode accepts
  `1..254` only — the layer never writes `oFF` (that would disable serial comms;
  not a supervisor operation). All documented against the map, bench to confirm.
- **`bAud`**: `0..4` map to 1200/2400/9600/19200/38400; op code beyond 4 →
  ERR_VALUE.

## 6. I/O accessor flow (per operation)

1. Argument check: null pointers, `dp` in 0..3 where applicable, enum/range
   validity for write values. Violation → ERR_PARAM, no UART traffic.
2. Writes: encode the semantic value to the wire word, then
   `modbus_client_write_single(c, slave, KILN_REG_*, wire)`. All this layer's
   writes are single-register fc 06; fc 16 is not used.
3. Reads: `modbus_client_read_registers(c, slave, KILN_REG_*, 1, &raw, 1)`, then
   decode per §5.
4. Client status maps 1:1 to `KILN_MAP_ERR_*` (same names). Any non-OK status
   leaves the caller's out-pointer untouched, matching the client's own contract.
5. `kiln_read_pv` additionally converts the PV sentinel statuses to
   `KILN_MAP_PV_*`.

## 7. Testing (L1, host)

`test_kiln_map.c` against `fake_uart` + `fake_clock` driving the **real**
`modbus_client`, plus direct conversion tests, `@verifies KILN-FUN-001`:

- dP semantics: for each `dp ∈ 0..3`, raw 245 ↔ out 245 (24.5 °C, resolution
  0.1) and a negative raw decodes as a negative out; `dp` outside 0..3 →
  ERR_PARAM with `out` untouched.
- PV sentinels: each of the four map to the right status with `out` untouched;
  an `SP` encode/decode round-trips a valid value.
- Enum codec tables: every control mode, `Pr.St`, bAud, prog status/step code;
  **out-of-range raws → ERR_VALUE with `out` untouched** (e.g. control `3`,
  bAud `5`, prog step `10`).
- Alarms bitmask vectors: one bit at a time and a representative packed value.
- `kiln_add_decode/encode`: `oFF`(0) → ADDR_OFF; addresses 1, 254, and 0/255
  boundary behavior; encode rejects 0 and 255.
- I/O accessors, wire-exact: each read accessor enqueues a valid response frame
  (values drawn from the doc table) and asserts the decoded result AND the
  transmitted request (`fake_uart_tx_bytes` — register constant + count 1).
  Each write accessor asserts the exact fc 06 frame against `kiln_registers.h`.
- I/O error pass-through: one representative read and one write fail with
  TIMEOUT/CRC, and `out` stays untouched.
- The register constants used by the tests are `kiln_registers.h` symbols, so the
  suite doubles as a self-check that every accessor targets its documented
  register.

Verification gates (all clean at `-j2`):
- `ctest --test-dir build/host-test -j2` host suite green (crc, frame, client,
  kiln_map).
- `tools/gen_rtm.py check` 0 violations; `gen` idempotent.
- `tools/check_layering.py` OK.
- clang-format (container-pinned v14) `--dry-run --Werror` clean.
- Container gates: cppcheck, clang-tidy, arm-none-eabi cross-build, gcovr ≥80%.
- Host ASan/UBSan build green.

## 8. Requirements and traceability

- **KILN-FUN-001** (was draft) is **amended and approved** by this milestone.
  Amended wording (the old text promised "write firing program parameters", i.e.
  the ramp/soak program-table outside this milestone's documented subset; as a
  draft it is shaped here to the verifiable scope, and the program-table coverage
  is recorded as a follow-up):

  > "The register-map layer shall translate between raw KM5P_r0 register values
  > and the documented instrument semantics for the live-value, setpoint, control,
  > alarm, program-status, and RS-485 configuration registers, for both read and
  > write access."

  `test_case: [test/unit/test_kiln_map.c]`. No banned words
  (fast/slow/robust/efficient/user-friendly/flexible/optimal/reasonable/
  appropriate/"as needed"/"if possible"/"etc."/"and/or"/support/process/handle).
- KILN-FUN-004 stays as-is; this layer consumes it. FUN-003/INT-001/CON-001
  unchanged.
- `kiln_registers.h` carries no requirement (constants only, still unverified;
  corrected by the bench Activity, SRS OI-01).

## 9. Non-goals / safety posture

- No program-table or full-configuration register access, no SPLL/SPLH clamp, no
  retries, no async — deferred (Section 2).
- No bench verification of register numbers here — the bench Activity (OI-01)
  stays the only thing that flips `kiln_registers.h` from unverified.
- No safety logic in this module. The layer can transmit ordinary control writes
  (setpoint, control mode) but no "start firing" command path exists; any such
  command stays behind KILN-FUN-002's human confirmation and is not reachable
  from any remote path in this milestone.
- Real-UART timing (inter-frame gap) stays out of scope for L1: the host doubles
  are synchronous, and USB-serial timing validation is a bench activity.