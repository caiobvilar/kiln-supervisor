# PLAN — kiln-supervisor

> **Migrated 2026-08-09** from the workbench harness repo into the portfolio
> meta-repo. Now `projects/T3-bootloader-ota/kiln-supervisor`, following the
> program's reference repository layout (`03-toolchain.md §2`). Gate numbers
> use the program's four test levels (L1 unit, L2 SITL, L3 HIL, L4 acceptance)
> from `03-toolchain.md §3`.

Durable state across context windows. Read this first; update it before you
stop. If a future session cannot resume from this file alone, it is not
detailed enough.

## Goal

Build a WiFi-connected supervisory node for a ceramic kiln (1300 °C
capability) that **controls an existing industrial KM5P_r0 controller over
RS-485**, exposing everything the KM5P_r0 allows (firing program
upload/start/stop, setpoints, live temperature and status readback, alarms)
through a cloud-hosted dashboard. The STM32F4 does not replace or reimplement
the KM5P_r0's control loop — the KM5P_r0 stays the thing actually running the
PID and driving the kiln; this project is a remote-control/telemetry layer in
front of it, talking its RS-485/Modbus RTU protocol.

Placed in **T3** (bootloader / OTA / fleet): the deliverable is a connected,
remotely-managed industrial product — device identity, secure remote command,
telemetry to a backend, staged control — the P11/P12 world of
[11-T3-bootloader-ota.md](https://github.com/caiobvilar/Embedded_Portfolio/blob/main/11-T3-bootloader-ota.md). Its comms core
(Modbus RTU client, CRC-16, register map) is itself a T1-style driver but the
project's identity is the connected supervisory product.

## Target

- Board: `f411-disco` — **placeholder, not yet confirmed.** Reused from
  `gesture-imu`/`vibration-fault-predict` since it is the only STM32F4 already
  owned with an FPU and no display baggage. Confirm this is acceptable once
  the I/O count is known — this board needs a UART routed through an RS-485
  transceiver (e.g. MAX485-class, part not yet chosen) to reach the KM5P_r0,
  plus whatever the WiFi module needs.
- Highest gate reached: **none — scaffolded, no code yet.**
- Constraint that drives the design: **safety, not RAM or latency.** The
  KM5P_r0, not this project's own hardware, does the mains-side switching —
  but this project can still remotely command a 1300 °C kiln to start firing
  over WiFi, unattended, which is the same category of risk as the program's
  motor/power interlocks, just arrived at over a network instead of a GPIO.
  See the Safety Interlocks in [AGENTS.md](https://github.com/caiobvilar/Embedded_Portfolio/blob/main/AGENTS.md): nothing
  that can command the real KM5P_r0 is flashed or powered without a human
  physically present and a safety case written. Any remote "start firing"
  path from the cloud dashboard needs its own confirmation step before it
  reaches the RS-485 link, not just at the firmware boundary.

## Steps

Tick as gates pass, not as code is written. Gates per [02-process.md](https://github.com/caiobvilar/Embedded_Portfolio/blob/main/02-process.md),
test levels per [03-toolchain.md §3](https://github.com/caiobvilar/Embedded_Portfolio/blob/main/03-toolchain.md).

- [ ] **SRS + G1.** EARS requirements (register set, poll rate, remote-command
      confirmation, alarm handling, safety interlocks) — before any comms code.
- [ ] **Bench-verify the Modbus register map against the real unit** — read
      register 1 (PV) and compare to the front panel's displayed temperature,
      confirm `Add`/`bAud` (10337/10338) are readable/writable as documented,
      per the provenance rule. This is the actual blocker, not "does a
      register map exist."
- [ ] L0 builds cross + host
- [ ] Static analysis clean, deviations recorded in `docs/adr/`
- [ ] L1 host unit tests — Modbus RTU framing, CRC-16, register encode/decode
      tested without hardware (host fake of the RS-485 port)
- [ ] L2 SITL (Renode) — two-node emulation (STM32F4 ↔ KM5P model over a
      virtual UART), selftest passes in emulation
- [ ] L3 HIL — flashes, `?selftest` passes against the real KM5P_r0 over
      RS-485 (with a human present — this project is interlock-relevant)
- [ ] L4 acceptance — human-confirmed physical behaviour: first real firing,
      watched, at a safe soak temperature before ever trusting a full
      1300 °C ramp

## Decisions

Link ADRs. Do not restate them here.

- Toolchain: [docs/adr/0001-cubemx-cmake-toolchain.md](docs/adr/0001-cubemx-cmake-toolchain.md) applies as to every STM32
  project.
- KM5P_r0 is an **Ascon Tecnologic "Kube"-family OEM/rebadge**: the COEL
  instruction manual describes the RS-485 link but not a register map; the
  actual Modbus protocol document (`ISTR_P_K-5series_E_01_--.pdf`) came from
  a COEL technician 2026-07-30 and gives function codes 03/06/16, CRC-16,
  and the register map (register 1 = live PV, etc.). Full transcription of
  every register group lives at
  `~/pCloudDrive/MyFiles/7 - Projetos/Forno/modbus_ascon_kube_register_map.md`
  (outside this repo, alongside the source PDFs). The subset this project
  needs is recorded with provenance in
  `docs/hardware/km5p-controller.md`.
- Not yet decided: RS-485 transceiver part, WiFi module path, cloud
  dashboard target, and whether the `f411-disco` board fits the I/O budget.

## Open questions

Facts you need and do not have. Name the document and section. This is the
handoff list — the human answers these between sessions.

- [x] ~~KM5P_r0 manual~~ — received and read in full 2026-07-29 (COEL
      "Manual de Instruções", rev 0 POR, 02/16, cód. 59.001.207). Gave the
      RS-485 electrical/link parameters (baud, format, isolation, wiring
      terminals — all `verified` in `docs/hardware/km5p-controller.md`)
      and the full configuration-parameter dictionary (Appendix A). It did
      **not** give a confirmed Modbus register map, and has no register at
      all for the live measured temperature.
- [x] ~~Modbus register map / protocol document~~ — **received 2026-07-30**,
      via a COEL technician directly (not the public download page): Ascon
      Tecnologic S.r.l., "Serial communication protocol ModBUS® for
      Programmers KM5/KR5/KX5" (`ISTR_P_K-5series_E_01_--.pdf`). Gives
      function codes (03/06/16), CRC-16 algorithm, and a full register map
      including register 1 = live PV. Recorded in
      `docs/hardware/km5p-controller.md` under "RESOLVED 2026-07-30", all
      rows still `unverified` pending a bench read-back.
- [ ] **Bench-verify the register map against the real unit** before writing
      any comms driver code — read register 1 and compare to the front
      panel's displayed temperature, confirm `Add`/`bAud` (10337/10338) are
      readable/writable as documented, per the provenance rule. This is now
      the actual blocker.
- [ ] **Confirm order-code variant.** The manual's ordering-code table (§4)
      shows the "Comunicação" field can be `-` (TTL only) or `S` (RS-485 +
      TTL). Check the physical unit's nameplate/model code — if it isn't
      the `S` variant, terminals 5/6 may not carry RS-485 at all.
- [x] ~~Resolve the instrument-address range discrepancy~~ — §2.4 says 1–255,
      Appendix A parameter [98] `Add` says 1–254. **Corroborated 2026-07-30**:
      the Ascon protocol document independently states 1–254, so treat
      1–254 as correct. Not yet bench-tested against the real unit's actual
      accepted range.
- [ ] Confirm `f411-disco` as the target board, or pick a different STM32F4
      if the I/O budget doesn't fit (RS-485 transceiver + WiFi module,
      concurrently, plus whatever UART/SPI each needs).
- [ ] RS-485 transceiver IC on the STM32F4 side — not yet chosen. Needs a
      part (e.g. MAX485-class) picked and verified against its own datasheet
      for direction-control timing (DE/RE), not assumed from a generic
      "RS-485 chip" mental model.
- [ ] WiFi hardware path — no WiFi-capable board is owned today. Likely an
      external module (e.g. ESP8266 in AT-command mode, or ESP32 as a
      network co-processor) bridged over UART or SPI, not something on-board.
      Needs a part decision and its own `docs/hardware/` sheet before any
      code touches it.
- [ ] Cloud dashboard target — which host/provider, MQTT vs HTTPS/webhook,
      and auth model are all undecided pending overall scope.
- [ ] Write the SRS (`docs/02-srs.md`) and sign G1 — blocks any code.

## Log

Newest last. One line per session: what moved, what broke, where you stopped.

- `2026-08-09` — Migrated from workbench `projects/04-kiln-controller` into
  `projects/T3-bootloader-ota/kiln-supervisor` per the program layout. Content
  unchanged; the harness interlock/inventory language is restated in program
  terms (Safety Interlocks in the meta-repo AGENTS.md); hardware sheet + ADRs
  copied in.
- `2026-07-30` — A COEL technician sent CG the actual missing document:
  Ascon Tecnologic's "Serial communication protocol ModBUS® for Programmers
  KM5/KR5/KX5" (`ISTR_P_K-5series_E_01_--.pdf`). Read all 28 pages, extracted
  the full register map to
  `~/pCloudDrive/MyFiles/7 - Projetos/Forno/modbus_ascon_kube_register_map.md`,
  and recorded the subset this project needs (PV, control, comms registers)
  in `docs/hardware/km5p-controller.md` with citations, all still
  `unverified`. This also corroborates the 1–254 address range over the
  COEL manual's 1–255, and confirms (via the unit's own ID register) that
  KM5P is an Ascon "Kube" family OEM/rebadge. The blocking open question is
  no longer "does a register map exist" — it's "bench-verify these
  registers against the real unit" before any comms code gets written.
- `2026-07-29` (same session, cont.) — CG pointed at COEL's product support
  page directly. Checked it and its linked manuals/assistência-técnica
  pages: no additional document, still just the one rev.0 manual, no
  configuration software or firmware hosted publicly. Did surface a
  concrete support contact (`vendas@coel.com.br`, +55 (11) 2066-3211, web
  form, ~3 business day SLA), recorded in `docs/hardware/km5p-controller.md`.
- `2026-07-29` (same session, cont.) — At CG's suggestion, searched online
  for a better COEL datasheet/protocol document. Confirmed there isn't one
  publicly: COEL's own manuals page lists only this same rev-0 manual;
  checked the sibling KM3P manual directly from COEL's CDN and it has the
  identical register-map gap; no community reverse-engineering project
  exists either. Also confirmed via COEL's own hosted filename that `_r0`
  correctly means "rev 0". Full search trail recorded in
  `docs/hardware/km5p-controller.md`.
- `2026-07-29` (same session, cont.) — CG provided the KM5P manual
  (pCloudDrive path). Read all 52 pages. Recorded the RS-485 electrical/link
  facts as `verified` in `docs/hardware/km5p-controller.md` (baud/format/
  isolation/wiring terminals/max-instruments/max-cable-length, all cited to
  section and page). Found and recorded a genuine discrepancy in the manual
  itself (instrument address range: §2.4 says 1–255, Appendix A parameter
  [98] says 1–254) rather than silently picking one. The bigger finding:
  this instruction manual is **not sufficient** to write the Modbus driver —
  no confirmed register map, no register for the live measured temperature.
  Flagged as new open questions: get COEL's separate Modbus protocol
  document if one exists, or derive the register map empirically at the
  bench; also confirm the physical unit's order-code variant actually has
  RS-485 (vs TTL-only). Still no comms code, still no CMake project.
- `2026-07-29` (same session) — CG corrected the architecture: this project
  **controls an existing industrial KM5P_r0 kiln controller over RS-485**;
  it does not reproduce or replace the KM5P_r0's own control loop. Rewrote
  Goal/Target/Open Questions accordingly and renamed the hardware doc's
  framing from "functional spec to reimplement" to "RS-485 protocol this
  project speaks to." Still blocked on the KM5P_r0 manual, which CG is
  downloading next, before any comms code or hardware fact sheet gets
  written.
- `2026-07-29` — Project scaffolded from `00-template` at CG's request:
  WiFi-connected supervisory node for a ceramic kiln (1300 °C), with a
  cloud-hosted dashboard. No code, no CMake project yet. `.board` set to
  `f411-disco` as a placeholder pending confirmation. Explicitly flagged as
  interlock-relevant (remote-triggered 1300 °C firing, even though the mains
  switching itself is inside the KM5P_r0) rather than treating it like the
  lower-stakes 01-03 trio.
