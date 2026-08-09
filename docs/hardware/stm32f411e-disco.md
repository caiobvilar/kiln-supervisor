# STM32F411E-DISCO

MCU: STM32F411VET6 · Cortex-M4F · 512 KB flash · 128 KB SRAM
Primary documents: **UM1842** (board user manual), **RM0383** (reference manual),
**DS10314** (datasheet), **ES0287** (errata), board schematic **MB1115**.

> This sheet is seeded, not verified. Nothing below carries `status: verified`,
> which means **no agent may use any of it in code yet**. Verify a row when you
> need it, one at a time, against the document named in its `source` column,
> then date and initial it. See `the meta-repo's hardware-facts provenance rule`.
>
> Seeding a sheet with unverified rows is a to-do list, not a knowledge base.
> The distinction is the whole point.

## On-board ST-LINK / MCU identity

Confirmed by direct `st-info` readout against the physically connected board
(not a vendor-doc lookup — this is the one section in this sheet whose source
is the hardware itself).

| Fact | Value | Source | Status |
|---|---|---|---|
| Chip ID | 0x431 | st-info readout, 2026-07-26 | verified — CG |
| Dev type | STM32F411xC_xE | st-info readout, 2026-07-26 | verified — CG |
| Flash size | 524288 bytes (512 KB) | st-info readout, 2026-07-26 | verified — CG |
| SRAM size | 131072 bytes (128 KB) | st-info readout, 2026-07-26 | verified — CG |
| ST-LINK fw version | V2J45 | st-info readout, 2026-07-26 | verified — CG |
| ST-LINK serial | 48FF73068280514954440281 | st-info readout, 2026-07-26 | verified — CG |

## Clock

| Fact | Value | Source | Status |
|---|---|---|---|
| HSE source | dedicated 8 MHz crystal (X2), on MCU's OSC_IN/OSC_OUT (PH0/PH1) — **not** MCO from the ST-LINK MCU | MB1115 Rev B.2 schematic, sheet 3/6 ("STM32F411E-DISCOVERY MCU") | verified 2026-07-27 by CG against MB1115 B.2 |
| LSE | 32.768 kHz crystal (X3) footprint present but **not fitted** | MB1115 Rev B.2 schematic, sheet 3/6 | verified 2026-07-27 by CG against MB1115 B.2 |
| Max SYSCLK | ? | DS10314 | unverified |
| PLL config for max | ? | RM0383 §6 | unverified |

The dedicated-crystal question is settled: this board does not derive HSE
from the ST-LINK MCU's MCO. SYSCLK/PLL numbers still need the datasheet —
the schematic only shows the oscillator wiring, not the part's clock limits.

## User LEDs

| Signal | Pin | Active level | Source | Status |
|---|---|---|---|---|
| LD3 orange | PD13 | high (GPIO → R36 1K → LED → GND) | MB1115 Rev B.2 schematic, sheet 6/6 ("Peripherals") | verified 2026-07-27 by CG against MB1115 B.2 |
| LD4 green  | PD12 | high (GPIO → R38 330 → LED → GND) | MB1115 Rev B.2 schematic, sheet 6/6 | verified 2026-07-27 by CG against MB1115 B.2 |
| LD5 red    | PD14 | high (GPIO → R39 1K → LED → GND) | MB1115 Rev B.2 schematic, sheet 6/6 | verified 2026-07-27 by CG against MB1115 B.2 |
| LD6 blue   | PD15 | high (GPIO → R40 510 → LED → GND) | MB1115 Rev B.2 schematic, sheet 6/6 | verified 2026-07-27 by CG against MB1115 B.2 |

## User button

| Signal | Pin | Active level | Pull | Source | Status |
|---|---|---|---|---|---|
| B1 USER/WAKE-UP | PA0 | high (button ties PA0 toward VDD through R37/R34 when pressed) | R35 220K pull-down to GND when released | MB1115 Rev B.2 schematic, sheet 6/6 | verified 2026-07-27 by CG against MB1115 B.2 |

RESET (B2) is wired straight to NRST, as expected — not a fact worth a row.

## On-board sensors

Board revisions changed the fitted parts on this family — the gyro and the
accel/mag are not the same across all F411E-DISCO revisions. The schematic
on hand is **MB1115 Rev B.2**, dated 2014-10-16, explicitly covering PCB
labels `B-02` and `B-03` (per its own revision note: "Rev B.2 --> PCB label
MB1115 B-02 ... STM32F401VCT6U replaced by STM32F411VET6U"). **Confirm the
physical board's silkscreen actually reads `MB1115 B-02` or `B-03` before
trusting the rows below** — a different printed revision means a different
schematic and possibly different fitted parts.

| Part | Function | Bus | Address / CS | Source | Status |
|---|---|---|---|---|---|
| L3GD20 (U7) | 3-axis gyro | SPI1 — SCK=PA5, MISO=PA6, MOSI=PA7, CS=PE3, INT1=PE0, INT2/DRDY=PE1 | CS on PE3 | MB1115 Rev B.2 schematic, sheet 6/6 | verified 2026-07-27 by CG against MB1115 B.2 (pins/bus only — I2C/SPI address bits and electrical specs still need the L3GD20 datasheet) |
| LSM303DLHC (U5) | accel + mag (6-axis) | I2C1 — SCL=PB6, SDA=PB9, INT1=PE4, INT2=PE5, DRDY=PE2 | I2C address not given in schematic | MB1115 Rev B.2 schematic, sheet 6/6 | verified 2026-07-27 by CG against MB1115 B.2 (pins/bus only — I2C address needs the LSM303DLHC datasheet) |
| MP45DT02 | PDM microphone | I2S — CLK_IN=PC3(via R51/R63), DOUT/PDM_OUT=PB10 | — | MB1115 Rev B.2 schematic, sheet 4/6 ("Audio") | verified 2026-07-27 by CG against MB1115 B.2 |
| CS43L22 | audio DAC | I2C1 (SDA=PB9, SCL=PB6, addr 0x94 per schematic label) + I2S3 (MCLK=PC7, SCLK=PC10, SDIN=PC12, LRCK=PA4), RESET=PD4 | I2C addr 0x94 (schematic label) | MB1115 Rev B.2 schematic, sheet 4/6 | verified 2026-07-27 by CG against MB1115 B.2 |

Board revision on hand: **MB1115 B-02**, serial 217380895 — confirmed
2026-07-27 by CG against the physical board's silkscreen. Schematic on file
(MB1115 Rev B.2) applies directly; the L3GD20/LSM303DLHC rows above are
confirmed current for this specific board, not just "one of the revisions
this schematic covers."

## Debug and serial

| Fact | Value | Source | Status |
|---|---|---|---|
| Programmer | on-board ST-LINK/V2, SWD only, implemented by a dedicated STM32F103C8T6 (U2) | MB1115 Rev B.2 schematic, sheet 2/6 ("ST-LINK/V2 (SWD only)") | verified 2026-07-27 by CG against MB1115 B.2 |
| Virtual COM port | **absent by default** — the USART2(PA2/PA3)-to-ST-LINK bridge exists in principle (JP1 header, solder bridges SB10/SB11) but ships **not fitted** | MB1115 Rev B.2 schematic, sheet 2/6 | verified 2026-07-27 by CG against MB1115 B.2 |
| Diag UART plan | wire an external USB-UART adapter to PA2/PA3 (USART2), exposed on header P1 — no board rework needed since those pins are already broken out | MB1115 Rev B.2 schematic, sheet 1/6 (P1/P2 headers) | verified 2026-07-27 by CG against MB1115 B.2 |

No VCP by factory default, confirmed (not just believed) — the debug protocol
needs an external USB-UART adapter on PA2/PA3. Until that is wired this board
cannot reach L3 (HIL) / L4 (acceptance) in the program's test-level scheme,
and the plan says so rather than pretending.

## Errata

Check **ES0287** for the silicon revision printed on the package before
debugging anything that "should work". Record relevant items here as their own
rows so they are findable later.

| ID | Summary | Applies to rev | Workaround | Status |
|---|---|---|---|---|
| | | | | |
