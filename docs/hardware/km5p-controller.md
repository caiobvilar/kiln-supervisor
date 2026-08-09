# KM5P_r0 industrial kiln controller (RS-485 peer for `kiln-supervisor`)

Not a board this program flashes, and not something `kiln-supervisor`
reimplements — this is the **existing industrial controller** that actually
runs the kiln's PID loop and drives the heater. The STM32F4 in
`kiln-supervisor` is an RS-485 client that commands and reads back the
KM5P_r0; the KM5P_r0 stays in charge of the kiln itself.

**Primary source in hand as of 2026-07-29:** COEL *KM5P — Controlador de
temperatura/processo com rampa/patamar*, "Manual de Instruções", rev 0
(POR), 02/16, cód. 59.001.207, Coelmatic Ltda (52 pages). All rows below cite
a section/page of this document. The manual's cover just says "KM5P", not
"KM5P_r0" — confirmed 2026-07-29 that this is a non-issue: COEL's own
product/manuals page (`coel.com.br/produto/km5p-controlador-de-temperatura/manuais/`)
hosts this exact file at
`cdn.media.coel.com.br/uploads/2015/12/Manual-de-Instrucoes-KM5P_r0.pdf` —
COEL's own naming convention uses `_r0` for "rev 0", so this is the correct
document for a `KM5P_r0` unit. Still doesn't confirm the *order-code
variant* (see below) — that's a separate, still-open question.

## Public documentation search, 2026-07-29 — no register map exists publicly

Searched for a COEL Modbus protocol/register-map document beyond this
instruction manual, to close the gap described below. Result: **there
isn't one published.**

- COEL's own manuals page for this product lists exactly one document —
  this same manual, rev 0 — no separate protocol/register-map PDF.
- Downloaded and checked the **KM3P** manual (`Manual-de-Instrucoes-KM3P_r00.pdf`,
  same `cdn.media.coel.com.br` host, same product family, same RS-485 +
  Modbus RTU feature) directly from COEL. It has the **identical structure
  and the identical gap**: a numbered Appendix A parameter dictionary, no
  stated register-address mapping, no live-measured-value register. This
  rules out "KM5P-specific oversight" — it's how COEL writes this entire
  product line's manuals.
- No community reverse-engineering found either (checked ESPHome/Modbus
  integration projects, GitHub, forums) — nobody has published a COEL
  K-series Modbus register map.
- One relevant data point, but **from a different manufacturer, not COEL**:
  WEG's CFW500 (a different Brazilian industrial-controller line) documents
  the convention "parameter number = holding register number, zero-offset."
  If COEL follows the same convention it would resolve the Nº-vs-register
  question below — but this is evidence about WEG's product, not a COEL
  confirmation. Treat as a testable hypothesis, not a fact:
  register-probe the real unit (read holding register N, compare to what
  parameter `[N]` shows on the front panel) before relying on it in code.

**Conclusion unchanged, now on firmer ground: no further public document
search is going to produce a register map.** The two remaining paths are
(1) ask COEL technical support directly for a protocol document, if one
exists outside their public download page, or (2) derive the map
empirically at the bench and record it here labeled `derived`, with the
test that produced each entry.

**2026-07-29, cont. — checked COEL's product support page directly**
(CG supplied `coel.com.br/produto/km5p-controlador-de-temperatura/suporte-tecnico/`).
No additional document: the support page links only to the same manuals
page checked above (still exactly the one rev.0 manual) and to a technical-
assistance page. No PC configuration software, firmware, or drivers are
hosted publicly for this product either. This does give path (1) an actual
channel, not previously recorded:

- Email: `vendas@coel.com.br`
- Phone: +55 (11) 2066-3211
- Web support-request form on the assistência técnica page (name/email/
  phone/description of issue), COEL states ~3 business days response time.

Path (1) is now actionable (CG can email/call/use the form to ask
specifically for a Modbus register-map document); path (2), empirical
derivation at the bench, remains unstarted and still requires real hardware.

## RESOLVED 2026-07-30 — Modbus register map now in hand

A COEL technician sent Caio a second document: Ascon Tecnologic S.r.l.,
*"Serial communication protocol ModBUS® for Programmers KM5/KR5/KX5"*,
doc. Modbus SW21-Rq-01-141118, firmware 1.0.0 (28 pages),
`ISTR_P_K-5series_E_01_--.pdf`. This is the document the search below
concluded didn't exist publicly — it wasn't public, it came directly from
COEL's own technical support.

**This is the actual register-map document**, and it resolves gap #1 and #3
below. It is an **Ascon Tecnologic** document, not a COEL one — strong
evidence COEL's KM5P/KM5P_r0 is an OEM/rebadge of Ascon's "Kube" family
(KM5/KR5/KX5). Confirms directly: register 21 (0x15), "Instrument
identification code", enumerates `27=KM5, 28=KX5, 29=KR5` — i.e. this unit's
own self-reported identity register uses the exact "KM5" name on the
physical nameplate, not a coincidental family resemblance.

Full extracted register map (all groups: common variables, pre-Kube
compatibility variables, instrument ID, and all configuration parameter
groups) is transcribed in full at
`~/pCloudDrive/MyFiles/7 - Projetos/Forno/modbus_ascon_kube_register_map.md`
(outside this repo, alongside the source PDFs). What follows here is the
subset this project actually needs first — the live process value, control,
and comms registers — reproduced so the fact lives in-repo with its own
provenance, not only referenced externally.

| Fact | Value | Source | Status |
|---|---|---|---|
| Function codes supported | 03 (read multiple, ≤16 regs), 06 (write single), 16 (write multiple, ≤16 regs) | §3, p.4 | verified — matches COEL manual's RS-485/Modbus RTU claim, not yet bench-tested |
| CRC | CRC-16, poly 0xA001, init 0xFFFF, LSB transmitted first | §3.5.1, p.7–9 (C code given) | verified — document-sourced |
| PV (live measured value), register 1 (0x0001) | Signed int, `dP` decimal point; -10000=underrange, 10000=overrange, 10001=A/D overflow, 10003=variable unavailable | §5.1, p.11 | **unverified — not yet read from the physical unit** |
| Operative set point, register 3 (0x0003) | `dP`, r | §5.1, p.11 | unverified |
| SP (settable), register 6 (0x0006) | Range SPLL..SPLH, r/w | §5.1, p.11 | unverified |
| Alarms status bitmask, register 10 (0x000A) | bit0=AL1, bit1=AL2, bit2=AL3, bit9=LBA, bit10=power failure, bit11=generic error | §5.1, p.11 | unverified |
| Control status, register 15 (0x000F) | 0=Automatic, 1=Manual, 2=Standby, r/w | §5.1, p.12 | unverified |
| Alarms reset / ack, registers 13/14 (0x000D/0x000E) | 0/1, r/w | §5.1, p.12 | unverified |
| Program status (old-compat block), register 580 (0x0244) | 0=not configured..7=continue, r/w | §5.2, p.14 | unverified |
| Program step in execution, register 582 (0x0246) | 0=inactive..9=END | §5.2, p.14 | unverified |
| Instrument address, register 10337 (0x2861), param `Add` | oFF or 1..254, r/w | §5.4.10, p.23 | unverified — **this manual's Appendix-A-equivalent (`Ser` group) says 1..254, matching the COEL manual's parameter [98] `Add`, not the COEL manual's §2.4 claim of 1..255 — corroborates the 1–254 range as correct, resolving the errata note below** |
| Baud rate, register 10338 (0x2862), param `bAud` | 0=1200,1=2400,2=9600,3=19200,4=38400, r/w | §5.4.10, p.23 | unverified |
| Programmer status (PRG group), register 10367 (0x287F), param `Pr.St` | 0=reset,1=run,2=hold,3=continue(r/o), r/w | §5.4.12, p.23 | unverified |

**Errata resolved:** the address-range discrepancy flagged below (COEL manual
§2.4 says 1–255, Appendix A parameter [98] says 1–254) is now corroborated
by this second, independent document, which also states 1–254 for the
equivalent `Ser` group `Add` parameter. Treat **1–254** as correct; the
COEL manual's §2.4 "1 a 255" line is the error, not the appendix.

**Still not verified against real hardware.** Per `hw-facts` protocol, every
row above stays `unverified` until read back from the physical KM5P_r0 at
the bench (read register N, compare to what the front panel / a known
setpoint shows). This document ends the "no register map exists" search —
it does not end the need to bench-verify before any comms driver ships.

## RS-485 electrical / link parameters — verified

| Fact | Value | Source | Status |
|---|---|---|---|
| Interface type | Isolated (50 V) RS-485 | §2.4, p.4 | verified — CG, 2026-07-29 |
| Voltage levels | Per EIA standard | §2.4, p.4 | verified — CG, 2026-07-29 |
| Protocol | Modbus RTU | §2.4, p.4 | verified — CG, 2026-07-29 |
| Data format | 8 bits, no parity | §2.4, p.4 | verified — CG, 2026-07-29 |
| Stop bits | 1 | §2.4, p.4 | verified — CG, 2026-07-29 |
| Baud rate | Programmable, 1200/2400/9600 (default)/19200/38400 | §2.4 p.4 + Appendix A param [99] `bAud`, p.41 | verified — CG, 2026-07-29 |
| Max instruments per master | 30 | §2.4, p.4, note 1 | verified — CG, 2026-07-29 |
| Max cable length | 1500 m at 9600 baud | §2.4, p.4, note 2 | verified — CG, 2026-07-29 |
| Terminal D− | Terminal 6 | Fig. §2 wiring diagram, p.1, and §2.4 diagram, p.4 | verified — CG, 2026-07-29 |
| Terminal D+ | Terminal 5 | Fig. §2 wiring diagram, p.1, and §2.4 diagram, p.4 | verified — CG, 2026-07-29 |
| Power terminals | 9 = Neutro, 10 = Fase | §2.5, p.4 | verified — CG, 2026-07-29 |

**Disputed fact — instrument address range.** §2.4 (p.4) states "Endereço:
Programável de 1 a 255", but Appendix A parameter [98] `Add` (p.41) states
the range is "1 a 254" (with `oFF` also selectable = serial comms unused).
Both are in the same manual and disagree by one. Until resolved, assume the
narrower, later-in-document range (1–254) is authoritative for firmware
validation, but don't hardcode 255 as a legal address without checking the
actual instrument's accepted range at the bench.

## What this manual does NOT give — the real gap

**Superseded 2026-07-30 for points 1 and 3 below** — see "RESOLVED
2026-07-30" above. The Ascon Tecnologic protocol document gives the
function-code detail and a register map (register 1 = PV, matching this
manual's parameter names). Point 2 (no PV register *in this manual*) stands
as written — the PV register came from the other document, not this one.
Left as-is below for the historical record of what was and wasn't known
before that document arrived.

1. **No confirmed Modbus register map.** Appendix A (pp.35–52) lists every
   configuration parameter with a numeric index (`Nº` column, 1–414, minus
   105–125 reserved) — but the manual never states that this index *is* the
   Modbus holding-register address. COEL-family instruments are known to
   often use the parameter index as the register offset, but that is an
   external pattern, not something this document confirms. **Do not encode
   `Nº` as a register address in firmware until it's tested against the
   real unit** (e.g., read register N and see if it returns/accepts the
   value shown on the front panel for parameter `[N]`), per
   `the meta-repo's hardware-facts provenance rule` — inference is fine, but it must be
   labeled `derived` and shown as a derivation, not asserted as verified.
2. **No live process-value (PV / current temperature) register at all.**
   Appendix A's parameter list is a *configuration* dictionary (input
   signal setup, output setup, alarms, control, setpoints, ramp/soak
   programs) — there is no entry for "measured temperature" or "current
   PV" anywhere in it. Reading the live temperature — the single most
   basic thing this project needs over RS-485 — is not documented in this
   manual at all.
3. **No documented function-code-level detail** (which Modbus function
   codes are supported — 03/06/16 read/write holding registers being the
   typical Modbus RTU set — nor which registers, if any, are read-only vs
   read-write).
4. **Ordering-code confirmation still needed.** §4 (p.5), "Informações para
   Pedido", shows the model's "Comunicação" field as either `-` = TTL
   Modbus only, or `S` = RS485 Modbus + TTL. This manual describes the
   instrument family generically; it doesn't confirm which variant the
   physical KM5P_r0 unit actually is. If it was ordered without the `S`
   option, terminals 5/6 may not carry RS-485 at all. Check the unit's
   model code / nameplate before wiring anything to those terminals.

**Conclusion, as of 2026-07-29: this instruction manual establishes the
physical/electrical RS-485 link and the parameter *names*, but a separate
document (a Modbus communication protocol / register-map sheet, distinct
from this instruction manual) is needed before any comms driver can be
written.** That document arrived 2026-07-30 (see "RESOLVED" above) — from
Ascon Tecnologic via COEL support, not published on either company's public
site. The remaining work is no longer "find the document," it's "bench-verify
the registers against the real unit" before writing the driver.

## Parameter reference (informational only — NOT a register map)

Selected parameters likely to matter most once the register question is
resolved (all from Appendix A, pp.35–52; the `Nº` column is the
configuration index discussed above, not a confirmed register address):

| Nº | Parameter | Description |
|---|---|---|
| 1 | SEnS | Input sensor type (thermocouple/RTD/linear — this unit's actual wiring/sensor type still needs confirming against the physical kiln, separately from this doc) |
| 56 | cont | Control type (PID / ON-OFF / servomotor) |
| 77–81 | SP, SP2–4, A.SP | Setpoints and active-setpoint selection |
| 98 | Add | RS-485 instrument address |
| 99 | bAud | RS-485 baud rate |
| 100 | trSP | Master/slave retransmission selection |
| 126–128 | PAGE, Pr.n, Pr.St | Active program page/number/status (run/hold/reset) — `Pr.St` is the closest thing in this manual to a remote start/stop control, and even that is documented only as a front-panel/parameter-level concept, not confirmed reachable over Modbus |
| 129–414 | P1.F … P8.E6 | Full ramp/soak program table for programs 1–8 (6 segment-pairs × 8 programs) |

## Errata / disputes

- Instrument address range disputed between §2.4 (1–255) and Appendix A
  parameter [98] (1–254) — **resolved 2026-07-30**: the Ascon Tecnologic
  protocol document independently states 1–254 for the same parameter,
  corroborating the appendix over §2.4. Still not bench-tested against the
  real unit's accepted range.
