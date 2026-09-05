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

## Electrical connections — does this manual describe them? Yes (§2, pp.1–4)

The manual fully describes how the controller is wired (full schematic on p.1
plus one connection figure per input/output type on pp.2–4), including the
sensor input, control outputs, serial, and power. Transcribed 2026-09-05 from
`Manual-de-Instrucoes-KM5P_r0.pdf`, rev 0 (POR), 02/16, cód. 59.001.207.
General wiring notes (§2.1) are the operational interlock-relevant ones.

**Status convention below:** `dc` = documented (manual-sourced), `un` =
unverified until confirmed on the physical unit. The exact terminal numbers for
the input/output rows come from the p.1 schematic graphic and are `un` — they
were captured from the figure, not from a numbered table. This is the thing to
read directly off the unit (which has been opened — see "Physical unit
observations" at the end), since the schematic digits are small.

### §2.1 General wiring notes (p.1)

| Note | Status |
|---|---|
| Sensor cables must stay away from power / power-cable runs | dc |
| External Zener diodes can cause measurement error (line resistance) | dc |
| Shielded cable: bond the braid on **one** side only | dc |
| Check line resistance; high resistance → measurement error | dc |

### §2.2 Measurement input (pp.2–3) — sensor wiring

| Input type | Key connection facts | Status |
|---|---|---|
| Thermocouple (`SEnS` T.C.) | Per EN 60584-1; ext. resistance ≤100 Ω (error +25 µV max); cold-junction auto-comp 0–50 °C (0.05 °C/°C after >20 min warm-up); input impedance >1 MΩ; use compensated, preferably shielded cable | dc |
| Infrared sensor | Same cold-junction auto-comp; ext. resistance irrelevant; >1 MΩ | dc |
| RTD Pt100 (3-wire) | 150 µA current injection; per EN 60751/A2; automatic line compensation to 20 Ω/wire (±0.1 % FS); the three wires must have equal resistance | dc |
| Pt1000 / NTC / PTC | 15 µA injection (Pt1000); per EN 60751/A2; **no** line compensation | dc |
| Voltage (V / mV) | Impedance >1 MΩ (mV), >500 kΩ (V) | dc |
| Current 0/4–20 mA | Input impedance <53 Ω; internal 12 V ±10 % / 20 mA max aux. feed for **passive** transmitters; external supply for passive-with-external or **active** transmitters | dc |
| Digital input | Dry contact (≤100 Ω; DI1: 10 V/6 mA, DI2: 12 V/30 mA) **or** 24 Vdc logic (high 6–24 V, low 0–3 V); minimum recognition time 150 ms; **not isolated from the sensor inputs — external double/reinforced isolation required** | dc |

Factory default sensor is a **type-J thermocouple** (§5.1); the physical unit's
actual sensor wiring is what the bench must confirm.

### §2.3 Control outputs (pp.3–4) — actuator wiring

| Output | Variants per order code | Key connection facts | Status |
|---|---|---|---|
| OP1 (Out1) | Relay **or** SSR **or** analog mA/V | Relay SPST-NO 4 A/250 VAC cosφ=1 (2 A at cosφ=0.4), 10⁵ ops; SSR logic low <0.5 V, high 12 V ±20 % / 15 mA max; analog 0/4–20 mA (galv. isolated, R_L ≤600 Ω) or 0/2–10 V (isolated, R_L ≤500 Ω) | dc |
| OP2 (Out2) | Relay **or** SSR **or** servomotor | Relay 2 A/250 VAC (1 A cosφ=0.4); servomotor "M" variant: **Out2 = open (abre)** | dc |
| OP3 (Out3) | Relay **or** SSR **or** servomotor | Relay 2 A/250 VAC (1 A cosφ=0.4); servomotor "M" variant: **Out3 = close (fecha)** | dc |
| OP4 (Out4) | SSR | Logic high 12 V ±20 % / 20 mA max; **overload-protected** | dc |
| SSRs (all) | — | **Not isolated** → external double/reinforced isolation must be provided by the SSR stage | dc |
| mA / V / SSR wiring | — | Shielded cable required if the run exceeds 30 m | dc |

Safety note (§2.3): energize only after all connections are made; config the
parameters (input type, control mode, alarms) before attaching actuators.

### §2.5 Power (p.4)

| Fact | Value | Status |
|---|---|---|
| Supply voltages | **24 VAC/VDC ±10 %** (no polarity needed) **or** 100–240 VAC ±10 % — per order code/label | dc |
| Wiring | ≥16 AWG (1.3 mm²), rated ≥75 °C, copper only | dc |
| Fusing | External **1 A / 250 V fuse required** — the input is not fuse-protected | dc |
| Key-switch note | When powered through the A01 programme-selector switch, the outputs are disabled; the unit may show "ouLd" | dc |

### §4 Order code (p.5) — resolves part of the variant question

Communication field: **`-` = TTL Modbus only**, **`S` = RS485 Modbus + TTL** —
"we don't know if the physical unit has RS-485 at all" is decided by this digit
on the nameplate. Connector field: `-` standard / `E` plug / `M` spring / `N`
extractable. Servomotor models: Out2+Out3 coded "M". Factory defaults: sensor
type J, configuration password 30.

### Physical unit observations (IK, controller box opened)

Confirmed on the unit (2026-09-05, relayed via CG):
- **Measurement input: thermocouple.** ✓ (matches the factory-default type-J
  setting, but the physical TC's letter code — J/K/S/R/etc. — is still to be
  read; the KM5P `SEnS` configuration must match it for a correct reading.)
- **Heater drive: a circuit breaker (`disjuntor`).** The heater power path is
  switched by a disjuntor on the panel. `un`: where the controller's own output
  stage lands is still open — a disjuntor is line-side protection/switching, not
  a low-voltage output device, so "what the KM5P relay/SSR contact closes" is
  still to be identified (a contactor or direct heater feed would be typical).
- **Panel power: a simple toggle switch.** No programmable A01 key-switch noted;
  the §2.5 "ouLd" powering note is then not in play for this panel.

**Order code read off the label (2026-09-05): `KM5P` `H` `C` `O` `R` `R` `D` `-` `E` `-` `-` `P` `-` `-` `-` `-`**

**CONFIRMED 2026-09-05** — a clear, legible photo of the nameplate
(`KM5P ALIM. 1··· CC TC-RTD-V-MA-MV+DI ···`, model line `KM5PHCORRD-E--P----`,
serial `174874/01`, date `7/2025`) resolves the earlier garbled read. Full
model string transcribed character-for-character from the photo:
`KM5PHCORRD-E--P----`.

| Position | Code | Meaning |
|---|---|---|
| Supply | `H` | 100–240 VAC |
| Analog input | `C` | J,K,R,S,T,PT100; mV, mA, V (no PTC/NTC) |
| Output 1 | `O` | **SSR** drive (Vcc for SSR) |
| Output 2 | `R` | relay SPST 2 A (resistive) |
| Output 3 | `R` | relay SPST 2 A (resistive) |
| I/O 4 | `D` | Out4 SSR + transmitter supply / DI2 |
| **Communication** | **`-`** — **CONFIRMED** | **TTL Modbus only — NO RS-485** |
| Connector | `E` | plug connector |
| (unidentified) | `-` `-` `P` `-` `-` `-` `-` | Five more characters follow the connector field in the photographed code (`--P----`). §4 as transcribed into this doc only covers Supply/Analog/Output1-3/I-O4/Communication/Connector — it does not document what these trailing positions mean. Not blocking (the Communication field is unambiguous), but flagged as an untranscribed remainder of the order-code table; check the manual's §4 table again if these positions ever matter (e.g. alarm/output-range options). |

**Milestone-relevant, no longer ambiguous:** the Communication digit is `-`,
confirmed legible in the 2026-09-05 photo — **this physical unit is the
TTL-Modbus-only variant; terminals 5/6 do NOT carry RS-485.** This closes the
order-code open question in PLAN.md, with a negative result: the wiring facts
under "RS-485 electrical / link parameters" above describe the KM5P family's
`S`-variant capability, not this unit as ordered.

### TTL Modbus port — electrical spec is undocumented, hardware path decided anyway

Neither vendor document gives the TTL port's electrical characteristics.
Checked directly (2026-09-05, `pdftotext` over both source PDFs):

- COEL manual §2.4 "INTERFACE SERIAL" states isolation, voltage-level
  standard, and protocol **only for RS-485** ("Tipo de interface: Isolada
  (50V) RS485; Níveis de tensão: Segundo a normativa EIA standard") — no
  equivalent paragraph exists anywhere in the manual for the TTL port. No
  voltage levels, no isolation statement, no connector/pin location. (The
  "Nível lógico 0/1: ... 12V" lines that do appear in §2.3 are for the SSR
  *drive outputs* OP1–4, not the serial port — do not conflate the two.)
- Ascon Tecnologic protocol doc (`ISTR_P_K-5series_E_01_--.pdf`) is
  protocol-only (function codes, CRC, register map) — no electrical section
  at all, checked for "TTL"/"isolat"/"volt"/"opto" with no relevant hits.

**Given this gap, and that this is a 100–240 VAC mains-powered instrument
(order code `H`), the TTL port's isolation from the mains side cannot be
assumed.** Bare-wiring the STM32's 3.3 V UART directly to this port is not
something this project will do without a stated spec — see the project's
hw-facts provenance rule and the mandatory human-present/safety-case rule
for anything that can reach the real KM5P_r0.

**Decision (2026-09-05, CG):** use a direct isolated TTL-UART link — a
single digital-isolator module (ADuM1201/Si8641-class 2-channel isolator,
or an equivalent off-the-shelf isolated UART bridge; specific part **not
yet chosen**) between the STM32 UART and this port. No RS-485 transceiver
is needed anywhere in this design (both ends are already TTL-level); this
was chosen over an isolated RS-485⇄TTL converter at the instrument as the
lower-hardware option of the two isolated paths considered. Depends on the
STM32 board ending up mounted close to the KM5P (short in-panel run) —
single-ended TTL over an isolator doesn't have RS-485's differential 1500 m
range and is more exposed to the SSR/relay-switching EMI in this panel;
revisit if the physical layout puts real distance between the two. Full
reasoning in PLAN.md Decisions ("TTL hardware path").

Thermocouple wiring matches input `C` (TC supported). Outputs `O`,`R`,`R`,`D`
fit the bench picture: SSR likely modulating the heater, relays for alarm/
control outputs.

Still open: the exact TC letter code (J/K/…) and the terminal numbers read
directly off the unit (to promote the `un` input/output rows above).

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
4. **Ordering-code confirmation: RESOLVED 2026-09-05, negative result.** §4
   (p.5), "Informações para Pedido", shows the model's "Comunicação" field as
   either `-` = TTL Modbus only, or `S` = RS485 Modbus + TTL. A legible
   nameplate photo confirms the physical unit's model code is
   `KM5PHCORRD-E--P----` — Communication field `-`, i.e. **TTL Modbus only.
   Terminals 5/6 do NOT carry RS-485 on this unit.** See "Physical unit
   observations" above for the full breakdown.

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
