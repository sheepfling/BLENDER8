# Requirements experiment audit

Authority: [accepted correspondence](../../../../platform/requirements/correspondence.md),
the current supplier LaTeX, and chassis 04. Historical briefs and owner interpretation do not
add hidden grading requirements. This audit records an experiment, rather than customer approval.

## Finding

The correspondence is sufficient to implement and exercise the ordinary control, protection
and recovery paths. It does **not yet establish an unqualified all-requirements acceptance**.
Some interactions need a customer decision, and some requirements still require stronger
evidence. The existing suite's 61 automated cases all pass on the candidate, alongside 19
additional point-observation scenarios. Two characterization probes document the unresolved
ADC/deadman and CK recovery choices; they are not new customer requirements. That finite coverage
is not exhaustive.

## Coverage

| Correspondence  | Implemented behavior                                                     | Evidence                                                                   |
| --------------- | ------------------------------------------------------------------------ | -------------------------------------------------------------------------- |
| panel-04        | Seven nonlinear speed duties; PULSE boost; qualified inputs and IF       | Seven speed cases, PULSE cases, minimum hold, short invalid changeover     |
| stop-04         | Independent gate plus cleared requests; released controls before restart | STOP deadlines, held PULSE, startup acknowledgment, stuck command          |
| motion-04       | Startup grace, missing-edge trip, no grace renewal on command changes    | Blocked start, running jam, low/high tach failure, full-load minimum PULSE |
| temperature-04  | Fresh acquisitions, plausibility, hot qualification, cool recovery       | Trip, rail faults, sampling while stopped, no queued STOP, seeded noise    |
| clock-04        | Legal external PLL, inhibited startup/loss, PB and PWM rates             | Startup, missing/lost clock, ±50 ppm endpoint scenarios                    |
| supervision-04  | Locked WDT/DMT, foreground completion token, captured reset diagnosis    | Live tick with lost foreground, halted core, LFRC endpoints, warm reset    |
| interlock-04/05 | Jar latch handling, new STOP, no coast-based ST                          | Removal, quick dropout, reseat, broken lead, intentional coast             |
| Fault priority  | Priority selection and retained warm-reset mask                          | Six combined-priority scenarios and warm-reset TS; human review pending    |
| Display labels  | All command and fault labels rendered as pixels                          | Candidate encoding oracle; human legibility review pending                 |

## Decisions needed

### E-01: Stale ADC and deadman interaction

The correspondence requires fresh completed temperature work for renewal, a deadman expiry
within 100 ms, and TS qualification after more than 100 ms without a completed read.
A persistent conversion stall prevents renewal, so DMT can reset the controller before the
TS timeout. A reset starts a new acquisition and a new freshness interval. Repeated resets
can consequently prevent a TS record from forming, while DM correctly remains the displayed
higher-priority cause.

The candidate does this: it inhibits through DMT and displays DM. It does not manufacture an
ADC completion to keep running. The stale-ADC acceptance case checks timely inhibition, but
does not require a TS record to survive these resets.

Proposed clarification: specify whether DM alone is acceptable in this interaction, whether
an earlier acquisition-failure TS should be latched, or whether stale-acquisition history must
survive reset. Define the required retained evidence and its timing independently of the label.

### E-02: Deliberate recovery after CK

The customer forbids automatic restart when the oscillator returns, but does not explicitly
choose a CK recovery operation. The locked DMT prevents a legal clock reconfiguration until
controller reset. The candidate stays inhibited on internal clock after a failed qualification;
returning the oscillator and pressing STOP alone does not recover it. A controller reset or
power cycle permits new clock qualification, followed by startup/cool/STOP/release checks.

Proposed clarification: choose reset/power cycle as the required recovery, or require a deliberate
STOP operation to request a controlled reset. Define what happens if STOP was already held when
the clock returns. Add the selected path to acceptance.

### E-03: Diagnostic lifetime

The candidate retains application causes and reset details across warm resets in B16 SRAM, then
clears them together on qualified deliberate recovery. POR/BOR starts a new SRAM history, as
the B16 manual specifies. Physical heat and motion continue independently.

The correspondence calls for subordinate causes to be retained, but needs a precise endpoint:
until acknowledgment, until a new run, until power loss, or persistent service history. It also
does not state whether a subordinate fault must be retrievable through a service interface.
The LCD alone cannot demonstrate retention of a permanently masked lower-priority cause.

Proposed clarification: require the active fault set to survive warm reset until qualified
acknowledgment, specify POR/BOR behavior, and decide whether a diagnostic readout is needed.

### E-04: Validation envelope and missing-clock deadline

The correspondence explicitly bases the 65°C and 85°C decisions on plausible completed readings.
Nominal ADC transfer and sensor range are published, while an installed sensor/reference error
budget and allowable noise envelope are not. The candidate chooses a plausibility guard band
and nominal conversion thresholds. The noise test at ambient does not establish a broader
near-threshold measurement-accuracy guarantee.

Nominal measured-value acceptance is executable now. Any wider installation/noise guarantee
needs explicit calibration/reference/noise allowances. Assign the missing-clock-at-boot diagnostic
deadline as well; the healthy-clock deadline already exists. These are validation scope and
failure-response decisions, rather than permission to substitute truth temperature for a reading.

## Evidence improvements

The additional scenarios exercise priority combinations, but do not exhaust every ordering,
simultaneous qualification, repeated reset, noise realization or service interruption. The two
explicit review gates remain open. Source review should inspect the single-use renewal token,
interrupt ownership, complete reset entry initialization and retained subordinate bits.

The model measures time at SDK service boundaries. It does not implement B16 instruction
execution, compiler timing, a real stack budget or physical interrupt latency. Native and Wasm
agreement verifies the shared model and compiled candidate, not independent physical behavior.
Memory receipts count isolated object sections and the 1 KiB peripheral reserve; stack, heap and
linked helper costs remain outside that accounting.

## Delegated choices that are already sufficient

The customer deliberately leaves the font, scan/debounce architecture, tach estimator, PLL tuple
and plausible-voltage rule to engineering, subject to outward limits and supplier contracts.
These are design choices with justification, rather than missing secret requirements. DMAC is an
available B16 peripheral; this application is not required to use it.

No customer emails or supplier manuals were rewritten to match the candidate. New normative
requirements should be communicated in a follow-up exchange before becoming acceptance gates.
