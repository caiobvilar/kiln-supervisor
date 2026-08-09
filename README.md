# kiln-supervisor — RS-485/Modbus supervisory node for a 1300 °C kiln

**A WiFi-connected supervisory node that commands an existing industrial
KM5P_r0 kiln controller over RS-485/Modbus RTU, with a cloud dashboard — the
protocol research is done and verified against vendor documents; the comms
driver is gated on a bench read-back of the real unit.**

The KM5P_r0 keeps running the kiln's PID loop. This project is the
remote-control and telemetry layer in front of it: firing-program
upload/start/stop, setpoints, live temperature and alarm readback, exposed
through a dashboard — and the honest engineering that got there: the COEL
manual alone had no register map, so the actual Ascon Tecnologic Modbus
protocol document was obtained from COEL's technical support and its register
table is recorded with full provenance in this repo.

> **Status: migrated scaffold.** No code yet, and deliberately so — the
> register map is documented but `unverified` until a bench read-back against
> the physical unit. This is an interlock-relevant project: nothing that can
> command a 1300 °C kiln is flashed or powered without a human present and a
> safety case written.

## What this demonstrates

| | |
|---|---|
| **Comms** | Modbus RTU client (function codes 03/06/16, CRC-16) over an isolated RS-485 link |
| **Integration** | Interfacing an existing industrial controller on its own protocol — no reimplementation |
| **Safety** | Remote-command path with confirmation steps; interlock discipline for kiln-control hardware |
| **Process** | Protocol sourcing under a real vendor gap; provenance-tracked register facts |

## Headline facts (verified vs pending)

| Fact | Value | Status |
|---|---|---|
| RS-485 link | Isolated (50 V), Modbus RTU, 8N1, 1200–38400 baud | **verified** — COEL manual §2.4 |
| Wiring | D− = terminal 6, D+ = terminal 5 | **verified** — COEL manual |
| PV register | register 1 (0x0001), live measured temperature | documented — **pending bench read-back** |
| Instrument address | 1–254 (`Add`), corroborated across both vendor docs | documented — **pending bench read-back** |

Full provenance in [docs/hardware/km5p-controller.md](docs/hardware/km5p-controller.md).

## Architecture (planned)

```
cloud dashboard ◄── WiFi module ──► STM32F4 ◄──RS-485──► KM5P_r0 (runs the kiln)
    (MQTT / HTTPS,    (ESP32/ESP8266,    (Modbus RTU client,   (existing controller,
     to be decided)    part to be chosen) CRC-16, reg map)      PID + heater)
```

## Repository layout

| Path | Contents |
|---|---|
| `src/domain/` | Modbus RTU framing, CRC-16, register encode/decode — host-testable |
| `src/ports/` | RS-485 UART interface + host fake |
| `src/adapters/` | Real + fake transport implementations |
| `docs/adr/` | Toolchain decisions (0001, 0003) |
| `docs/hardware/` | KM5P_r0 protocol sheet + STM32F411E-DISCO sheet |

## Build and run

Not yet buildable — no CMake project exists, and by design no comms code until
the register map is bench-verified. SRS + G1 come first.

## Documentation

- [PLAN.md](PLAN.md) — durable state, the protocol saga, open questions, log
- [docs/hardware/km5p-controller.md](docs/hardware/km5p-controller.md) — the RS-485 facts and the Modbus register map with provenance

## What I'd do differently

- The first scaffold assumed the COEL manual would be enough to write a
  Modbus driver. It wasn't — no register map, no live-temperature register.
  The cost of that assumption was a full search campaign; the correct first
  move was to request the protocol document from the vendor up front.
- The order-code variant question (does this unit even have RS-485?) should
  have been checked on the nameplate before any protocol work — it still is
  not confirmed, and it gates whether any of the wiring facts apply.

## License

Code: **Apache-2.0** (`LICENSE`) · Documentation: **CC BY 4.0**
(`docs/LICENSE-docs.md`) — per the program's publishing rules
([06-publishing.md §2.3](https://github.com/caiobvilar/Embedded_Portfolio/blob/main/06-publishing.md)).
