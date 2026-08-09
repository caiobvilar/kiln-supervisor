# Design Review Checklists

[← Index](00-index.md) · [Process](02-process.md)

Copy the relevant checklist into `docs/reviews/`, fill it in, commit it. Reviewing your own work is legitimate — the artifact is the completed checklist and the issues it caught. **Record the issues it found**; a checklist with zero findings means you rushed it.

---

## G1 — Requirements Review (SRR)

- [ ] Every requirement uses an EARS pattern
- [ ] Every requirement has exactly one `shall`
- [ ] Every requirement has a verification method assigned
- [ ] Every quantitative requirement has a number, a unit, and a tolerance
- [ ] Every requirement traces to a stakeholder need or a parent requirement
- [ ] No banned words (`fast`, `robust`, `efficient`, `appropriate`, …)
- [ ] No implementation detail leaked into requirements
- [ ] Out-of-scope section is explicit
- [ ] Assumptions are listed and are testable/falsifiable
- [ ] Constraints (cost, parts availability, size, tooling) are captured
- [ ] Safety-relevant behaviour is covered by `if…then` requirements
- [ ] Someone reading only the SRS could build the right thing

**Findings:** _______  **Signed / date:** _______

---

## G2 — Schematic Review

### Power
- [ ] Power tree diagram exists; every rail has a source, a budget, and a measured-current expectation
- [ ] Current budget per rail computed with ≥ 30 % margin
- [ ] Regulator input/output capacitors match the datasheet's required type and value (**ceramic derating checked** — a 10 µF X5R 6.3 V at 5 V bias may be 4 µF)
- [ ] Regulator stability requirements met (min ESR, min/max Cout)
- [ ] Power sequencing requirements checked for every IC that has them
- [ ] Reverse-polarity protection present where a connector could be reversed
- [ ] Over-voltage / over-current protection where an external supply is possible
- [ ] Bulk capacitance sized from ripple-current and transient requirements, not guessed
- [ ] Thermal dissipation of each regulator calculated at worst case

### MCU / digital
- [ ] Every VDD/VDDA/VBAT/VREF pin decoupled per the datasheet's recommendation
- [ ] Reset circuit: pull-up + capacitor, with the value from the datasheet
- [ ] BOOT pins in a defined state; boot mode selectable if needed
- [ ] Debug interface present, with the correct pinout and orientation checked twice
- [ ] Crystal load capacitors computed: `C_L = 2·(C_load − C_stray)`, with C_stray estimated and stated
- [ ] All unused pins accounted for (configured, or explicitly left floating with justification)
- [ ] Pull-ups present on all I²C buses, value computed from bus capacitance and speed
- [ ] Level shifting where voltage domains meet; direction and speed adequate
- [ ] No 5 V signal on a non-5 V-tolerant pin (check the datasheet per-pin table, not the general blurb)
- [ ] Every IC's ENABLE/CS/RESET pin driven or tied, never floating

### Analog
- [ ] Op-amp supply rails cover the required input and output range (rail-to-rail claims verified)
- [ ] Input common-mode range covers the actual signal range at all supply corners
- [ ] Feedback network stability considered (capacitive load, phase margin, isolation resistor)
- [ ] Input bias current × source impedance error is within budget
- [ ] Anti-alias filter cutoff and order consistent with the sampling rate and required attenuation at f_s/2
- [ ] Reference buffered if the load is dynamic
- [ ] Resistor tolerance and tempco selected against the accuracy requirement, not by habit
- [ ] Noise budget computed and referenced

### Connectors and interfaces
- [ ] Every connector's pinout verified against the mating part's datasheet
- [ ] Polarity/keying prevents reversed insertion, or a reversed insertion is harmless
- [ ] ESD protection on every externally accessible signal
- [ ] Series termination / impedance considerations noted for fast signals
- [ ] USB-C CC pull-downs present (5.1 kΩ × 2) if sinking power

### General
- [ ] ERC clean, with every suppressed warning individually justified
- [ ] Every component has a value, a footprint, an MPN, and a distributor part number
- [ ] Every component's voltage, current, power, and temperature rating exceeds worst case with margin
- [ ] Test points on every power rail and every hard-to-probe critical signal
- [ ] LEDs for power and status
- [ ] Board name, revision, date, and author in a silkscreen field
- [ ] Second sources identified for the parts most likely to go out of stock
- [ ] Design reviewed against the datasheet's *typical application circuit* for every major IC, deviation by deviation

**Findings:** _______  **Signed / date:** _______

---

## G3 — Layout / DFM Review (Manufacturing Readiness)

### Stackup and planes
- [ ] Stackup chosen from the fab's published options; dielectric constants and thicknesses recorded
- [ ] Impedance-controlled traces computed with the fab's calculator against the chosen stackup
- [ ] Ground plane is continuous under every high-speed and every sensitive analog trace
- [ ] No signal crosses a plane split (or, where unavoidable, a stitching capacitor provides the return path)
- [ ] Plane stitching vias placed at layer transitions of any fast signal
- [ ] Return-path reasoning documented for the three most critical signals

### Placement
- [ ] Decoupling capacitors on the same side as their IC, within 2 mm, via to ground short
- [ ] Switching regulator hot loop area minimised and drawn in the review doc
- [ ] Sensitive analog kept away from switching nodes and digital clocks
- [ ] Crystal close to its pins, guard ring, no traces underneath
- [ ] Connectors placed so cables route away from sensitive circuitry
- [ ] Thermal components spread out; hot parts not next to temperature sensors
- [ ] Components not placed within the fab's edge clearance
- [ ] Enough clearance around connectors for the mating part *and the operator's fingers*

### Routing
- [ ] Trace widths adequate for current (IPC-2221 or a calculator; state the temperature rise assumed)
- [ ] Differential pairs matched in length and consistently spaced
- [ ] No acid traps, no acute angles
- [ ] Vias not in pads except where necessary (and then filled/plated, and the cost accepted)
- [ ] Thermal relief on through-hole ground connections
- [ ] Sense traces (current shunt, high-impedance analog) Kelvin-connected and routed as pairs
- [ ] Clearance/creepage adequate for the maximum voltage (IPC-2221 table cited)

### DFM / DFA
- [ ] DRC clean against the fab's rule set (imported, not assumed)
- [ ] Minimum trace/space, drill, annular ring within the fab's standard capability (or the cost of exceeding it is accepted knowingly)
- [ ] Silkscreen does not overlap pads; text ≥ 0.15 mm line width, legible
- [ ] Every component has a visible designator, or an assembly drawing compensates
- [ ] Polarity markers on all polarised parts, and pin 1 markers on all ICs
- [ ] Fiducials present (3 per assembled side)
- [ ] Panelisation / rails as required for assembly
- [ ] Footprints verified against manufacturer land patterns — **every single one, individually**
- [ ] **Component rotations checked against the assembly house's CPL convention** (this is the #1 cause of assembled-but-dead boards)
- [ ] BOM parts confirmed in stock at the fab/distributor *this week*
- [ ] Basic vs Extended part count reviewed against cost
- [ ] Mechanical: board outline, mounting holes, keep-outs, connector positions checked against a 3D model or a printed 1:1 paper mock-up
- [ ] 3D render inspected for collisions

### Outputs
- [ ] Fabrication package generated by script, not by hand
- [ ] Gerbers viewed in an independent viewer (not KiCad) before ordering
- [ ] Drill file checked: hole count and sizes plausible
- [ ] Repo tagged `hw/<BOARD>-<REV>` at the exact commit sent to fab

**Findings:** _______  **Signed / date:** _______  **Order placed:** _______

---

## Firmware Review

### Architecture
- [ ] `domain/` layer has zero hardware includes (CI-enforced)
- [ ] Business logic is not in adapters or in ISRs
- [ ] Dependencies injected at the composition root, not via globals
- [ ] Module interfaces documented in headers

### Correctness and safety
- [ ] No dynamic allocation after initialisation (or a documented, bounded policy)
- [ ] All buffer accesses bounds-checked; no unbounded `str*`/`sprintf`
- [ ] All fixed-width types (`uint32_t`, not `unsigned long`)
- [ ] Integer promotion and signedness reviewed for every arithmetic expression involving mixed types
- [ ] Every `switch` has a `default`; every enum switch handles all values
- [ ] All return values checked, or explicitly `(void)`-cast with a comment
- [ ] Shared state between ISR and thread context is `volatile` **and** access is atomic or protected — `volatile` alone is not a synchronisation primitive
- [ ] Critical sections are as short as possible and bounded; longest one measured
- [ ] No blocking calls in ISRs; ISR execution time bounded and measured
- [ ] Watchdog fed from a place that proves the system is actually healthy, not from a timer ISR
- [ ] Stack usage analysed (high-water mark or static analysis), with margin
- [ ] Error paths tested, not just happy paths
- [ ] Assertions used, with a defined production behaviour

### Real-time
- [ ] Every periodic task's period, WCET, and deadline documented
- [ ] Schedulability argued (utilisation bound or response-time analysis)
- [ ] Priority assignment justified; priority inversion considered
- [ ] Deadline-miss behaviour defined and tested

### Build and quality
- [ ] `-Wall -Wextra -Werror` (plus `-Wconversion` where practical) with zero warnings
- [ ] Static analysis clean; every suppression individually justified in the suppression file
- [ ] Unit coverage ≥ 80 % on portable layers
- [ ] Flash and RAM budgets defined and gated in CI
- [ ] Version information embedded in the binary
- [ ] Build reproducible

**Findings:** _______  **Signed / date:** _______

---

[← Index](00-index.md)
