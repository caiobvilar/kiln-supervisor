# Test Plan Template — `docs/05-test-plan.md`

[← Index](00-index.md) · [Process](02-process.md) · [Toolchain](03-toolchain.md)

---

```markdown
# Verification & Validation Plan — <PROJECT NAME>

| | |
|---|---|
| Project | Pxx |
| Version | 1.0 |
| SRS baseline | v1.0 @ `<sha>` |

## 1. Scope

*What is being verified, against which SRS baseline, and what is explicitly not verified in this cycle.*

## 2. Verification strategy

| Level | Environment | Trigger | Owner |
|---|---|---|---|
| L1 Unit | Host x86, gcc + clang, ASAN/UBSAN | every push | CI |
| L2 SITL | Renode, `<platform>.repl` | every push | CI |
| L3 HIL | Rig #1, DUT `<board>-<rev>` | merge to main + nightly | CI (self-hosted) |
| L4 Acceptance | Bench, semi-manual | before each release tag | manual |

## 3. Test environment

### 3.1 Hardware configuration
| Item | Identifier | Calibration / notes |
|---|---|---|
| DUT | PORTA-01 Rev B, S/N 003 | |
| Oscilloscope | Rigol DHO804, FW x.y.z | self-cal performed YYYY-MM-DD |
| PSU | Riden RD6006, set 5.000 V, limit 0.5 A | |
| Logic analyzer | DSLogic U3Pro16 @ 100 MSa/s | |

### 3.2 Software configuration
| Item | Version |
|---|---|
| Firmware under test | `<tag>` / `<sha>` |
| Toolchain | arm-none-eabi-gcc 14.2.Rel1 |
| Renode | 1.15.x |
| Test harness | pytest 8.x, this repo `<sha>` |

## 4. Entry / exit criteria

**Entry:** SRS baselined; firmware builds clean; rig health check passes.
**Exit:** every `shall` requirement has a PASS result or a documented, approved deviation; zero open severity-1 defects.

## 5. Test cases

*One table row per case; full detail in section 6.*

| TC ID | Title | Level | Verifies | Automated |
|---|---|---|---|---|
| TC-Pxx-001 | Ring buffer full behaviour | L1 | Pxx-FUN-007 | yes |
| TC-Pxx-018 | Frame processing WCET | L3 | Pxx-PER-003 | yes |
| TC-Pxx-031 | Power-cut during flash write | L3 | Pxx-REL-003 | yes |

## 6. Test case detail

### TC-Pxx-018 — Frame processing WCET

- **Verifies:** Pxx-PER-003
- **Level:** L3 (HIL)
- **Objective:** demonstrate that the worst-case frame processing time meets the 4.0 ms budget.
- **Preconditions:** DUT flashed with `<sha>`, Release config; input driven by the P07 generator at 1.000 kHz, −6 dBFS; ambient 20–26 °C.
- **Equipment:** DUT, P07 generator, DHO804 on the frame-marker GPIO.
- **Procedure:**
  1. Enable the frame-marker GPIO output (rises at frame start, falls at completion).
  2. Configure scope: 5 ms/div, trigger on rising edge, persistence on, measure positive pulse width, statistics on.
  3. Run for 10⁶ frames (≈ 2 h 58 min at 46.9 fps) with the DWT cycle counter also logging per-frame cycles over the debug UART.
  4. Export scope statistics and the UART log.
- **Pass criteria:** maximum measured processing time ≤ 4.0 ms across both measurement methods; the two methods agree within 5 %.
- **Data collected:** `reports/<date>/wcet_scope.csv`, `reports/<date>/wcet_dwt.csv`
- **Analysis script:** `tools/analyze/plot_wcet.py`
- **Result:** *(filled after execution)* PASS — max 2.71 ms (DWT), 2.75 ms (scope), Δ = 1.5 %
- **Evidence:** `docs/media/wcet_histogram.png`

## 7. Fault injection cases

| TC ID | Fault injected | Method | Expected behaviour | Verifies |
|---|---|---|---|---|
| TC-Pxx-041 | DMA transfer error | Corrupt the DMA config register at runtime via a debug command | Error counter increments, channel recovers < 1 ms | Pxx-REL-001 |
| TC-Pxx-042 | Supply brown-out to 2.4 V for 10 ms | PSU under script control | BOR resets cleanly, no flash corruption | Pxx-REL-004 |
| TC-Pxx-043 | Sensor disconnected | Analog switch on the rig | Plausibility check flags, safe state entered < 50 ms | Pxx-SAF-006 |

## 8. Regression policy

| Change type | Required re-verification |
|---|---|
| Any source change | L1 + L2 |
| Change to a driver or the timing-critical path | L1 + L2 + L3 |
| PCB revision | full L3 + L4, plus bring-up procedure re-executed |
| Toolchain version bump | full suite + reproducibility check + size-budget review |

## 9. Traceability

*Generated: see `docs/06-rtm.md`. CI fails if any approved `shall` requirement lacks a linked test case.*

## 10. Deviations and waivers

| ID | Requirement | Deviation | Justification | Approved |
|---|---|---|---|---|
| DEV-01 | Pxx-PER-004 | Measured −97 dB vs −100 dB required | Limited by available source purity; root cause identified as generator THD, not DUT. Re-test deferred to Rev B with a notch filter. | YYYY-MM-DD |
```

---

## Test report (generated) — `docs/07-test-report.md`

Do not hand-write this. Generate it from JUnit XML + the measurement CSVs:

```markdown
# Test Report — Pxx v1.2.0
Executed: 2026-11-03 · Firmware `a3f9c2e` · Rig #1 · Harness `b71d045`

## Summary
| Level | Cases | Pass | Fail | Skip |
|---|---|---|---|---|
| L1 Unit | 148 | 148 | 0 | 0 |
| L2 SITL | 22 | 22 | 0 | 0 |
| L3 HIL | 31 | 30 | 0 | 1 |
| **Total** | **201** | **200** | **0** | **1** |

Coverage (host, portable layers): lines 87.4 %, branches 79.1 %

## Requirement status
| Status | Count |
|---|---|
| verified | 41 |
| deferred (documented) | 2 |
| not verified | 0 |

## Key measurements
| Requirement | Limit | Measured | Margin |
|---|---|---|---|
| Pxx-PER-003 | ≤ 4.00 ms | 2.71 ms | 32 % |
| Pxx-PER-001 | 0 dropped samples / 24 h | 0 | — |

## Skipped
TC-Pxx-052 — requires the thermal chamber substitute; scheduled for the next cycle.
```

---

## Defect log — `docs/defects.md`

| ID | Severity | Found in | Description | Root cause | Fix | Status |
|---|---|---|---|---|---|---|
| D-014 | 1 | TC-Pxx-031 | Device fails to boot after power cut at 47 % of flash write | Metadata page erased before the new record was written; no journaling | Journaled metadata with alternating pages + sequence numbers | Closed `f2a91c4` |

**Keep this public.** A visible defect log with root causes is one of the most credible things in an engineering portfolio. Hiding your bugs makes the work look fake.

---

[← Index](00-index.md)
