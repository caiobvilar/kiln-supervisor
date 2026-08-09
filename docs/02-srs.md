# Software/System Requirements Specification — kiln-supervisor

| | |
|---|---|
| Project | kiln-supervisor |
| Version | 1.0 |
| Status | draft |

This SRS is forward-looking: the project has no code yet (scaffolded state).
All requirements are in draft status and will be baselined at G1 review.

## 1. Purpose and scope

The kiln-supervisor project shall be a WiFi-connected supervisory node for a
ceramic kiln (1300 °C capability) that controls an existing industrial
KM5P_r0 controller over RS-485/Modbus RTU, exposing everything the KM5P_r0
allows (firing program upload/start/stop, setpoints, live temperature and
status readback, alarms) through a cloud-hosted dashboard.

## 2. Stakeholders and needs

| Need | Stakeholder | Need text |
|---|---|---|
| N-01 | Owner | A Modbus RTU client implementing the KM5P_r0 register map for remote kiln control and telemetry. |
| N-02 | Owner | A safety interlock requiring explicit human confirmation before any remote "start firing" command reaches the RS-485 link. |
| N-03 | Owner | Round-trip dashboard-to-RS485 latency <= 500 ms under nominal WiFi. |
| N-04 | Owner | Host-testable domain logic via port interfaces (no hardware includes in domain). |

## 3. Definitions and abbreviations

- **Modbus RTU** — Serial protocol (CRC-16, 9600/19200/38400 baud) over RS-485.
- **KM5P_r0** — Industrial kiln controller with public register map (TBD).
- **L1 gate** — Host (native) unit-test gate.

## 4. System context

Board: F411-DISCO (placeholder; I/O count not yet confirmed). RS-485 transceiver
(MAX485-class, part TBD) connects UART to KM5P_r0. WiFi module (TBD) provides
cloud connectivity. The KM5P_r0 remains the safety-critical control loop; this
project is a telemetry/remote-command layer.

## 5. Assumptions and constraints

- Safety: the safety interlock (N-02) is mandatory — no remote firing without
  human confirmation. See AGENTS.md Safety Interlocks.
- KM5P_r0 register map is not yet extracted (no public doc); this is an open
  blocker before code can be written.
- Board choice (F411-DISCO) is a placeholder; I/O count for UART+RS485+WiFi
  must be verified before commit.

## 6. Requirements

### 6.1 Functional

1. **KILN-FUN-001** (shall) — The Modbus RTU client shall implement the KM5P_r0
   register map to read live temperature, setpoints, and status, and to write
   firing program parameters and start/stop commands.
2. **KILN-FUN-002** (shall) — The safety interlock shall require an explicit
   human confirmation step before any remote "start firing" command is
   transmitted over the RS-485 link.

### 6.2 Performance

3. **KILN-PER-001** (shall) — The round-trip latency from a dashboard command
   to RS-485 frame transmission shall not exceed 500 ms under nominal WiFi
   conditions.

### 6.3 Interface

4. **KILN-INT-001** (shall) — The Modbus client shall communicate with the
   UART port through the port interface without hardware-specific code.

### 6.4 Constraints

5. **KILN-CON-001** (should) — The domain code shall compile on the host
   without any hardware or vendor includes.

## 7. Verification summary

| Method | Count |
|---|---|
| Test | 3 |
| Analysis | 2 |
| Inspection | 0 |
| Demonstration | 0 |

*Full traceability is in the generated `docs/06-rtm.md`.*

## 8. Open issues

| ID | Issue | Owner | Target |
|---|---|---|---|
| OI-01 | KM5P_r0 register map not publicly available — must be obtained or reverse-engineered. | Owner | before G1 |
| OI-02 | WiFi module and RS-485 transceiver parts not yet selected. | Owner | before G1 |
| OI-03 | Board I/O count (UART + RS485 + WiFi) must be verified against F411-DISCO pinout. | Owner | before G1 |

## 9. Change log

| Version | Date | Change | Commit |
|---|---|---|---|
| 1.0 | 2026-08-09 | Initial draft baseline. | |