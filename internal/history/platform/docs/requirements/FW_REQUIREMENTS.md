# Firmware behavior requirements

**Document:** REQ-FW · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

These requirements define the customer-visible behavior; the component datasheets define what the
hardware does. All values are chosen for this fictional teaching target, not approved real-appliance
limits. The core assignment is open-loop speed mapping with closed-loop **protection**, not PID
speed regulation.

The numbered clauses below are generated from `spec/firmware_requirements.json`.
`spec/teaching_profile.json` collects the numerical configuration; datasheets and code must agree
with that profile before changing a value. A machine-readable profile is not permission for firmware
to inspect simulator configuration at runtime.

## REQ-FW-001 — MCU-only boundary

Firmware shall include only its own headers, standard C++ headers permitted by ASSIGN-001, and the
Blender-8 SDK. It shall not reference simulator types, plant truth, fixture controls, host time, or
host threads.

## REQ-FW-002 — Cold, released startup

After reset, keep drive disabled until fresh plausible temperature readings remain at or below 65 C
for 2 s, STOP is released, and all eight mux contacts have been released for at least 20 ms. A
held/latching control at boot cannot start the motor. Only a later debounced command starts motion.

## REQ-FW-003 — Fail-off initialization

Initialize MOTOR_EN=0, PWM_CTRL.EN=0 and PWM_DUTY=0 before enabling interrupts or presenting
readiness. Unknown application state is non-running.

## REQ-FW-004 — Mux scan discipline

Scan all eight channels repeatedly, selecting PA0..PA2 and allowing at least 2 us settling before
reading PA3. Never treat the scan as an instantaneous eight-bit sample. Sample each contact at least
once every 8 ms.

## REQ-FW-005 — Debounced events

A contact transition is accepted only after consistent observations spanning at least 16 ms. With an
8 ms per-contact period, three equal samples meet this span. Final stable commands shall be
recognized within 40 ms; sub-50-ms PULSE taps need not be recognized.

## REQ-FW-006 — Illegal combinations

Zero or one speed contact, optionally plus PULSE, is legal. Two or more speed contacts observed
continuously over at least 40 ms latch IF and disable drive. Short sequential-scan or bounce
transients must not choose an arbitrary winning speed.

## REQ-FW-007 — STOP dominates

The assembly independently inhibits motor power and mechanically clears all commands on STOP.
Firmware shall independently clear MOTOR_EN, PWM_CTRL.EN and PWM_DUTY within 10 ms of STOP_N
assertion on PB2. STOP release never restores the previous run command.

## REQ-FW-008 — Linear nominal speed steps

Map speeds 1..7 to 3000, 5500, 8000, 10500, 13000, 15500 and 18000 nominal no-load RPM using
inversion of the nonlinear motor transfer. After 1 s at nominal no load, error shall be at most
max(100 RPM, 2% of target). Closed-loop regulation is not required.

## REQ-FW-009 — Momentary PULSE

While PULSE is held, add 2000 RPM to the selected target, capped at 20000 RPM; from idle command
2000 RPM. Release restores the still-selected speed or returns to idle. This is not a 10%-duty
waveform or a periodic chopping mode. STOP cancels it, including a PULSE held through STOP.

## REQ-FW-010 — PWM semantics

Use the specified code-to-duty relation, including the special 255=100% endpoint. When shutting
down, disable MOTOR_EN and PWM_CTRL.EN, then clear PWM_DUTY; a shadow-duty write alone is
insufficient for the shutdown deadline.

## REQ-FW-011 — Pixel display

Produce characters and graphics by writing LCD pixel bytes, not by invoking a host text renderer.
Display the selected speed, nP for boosted speed, P for standalone pulse, 0 for idle, and the
dominant fault label. A boot graphic is permitted while not ready.

## REQ-FW-012 — Nonblocking supervision

The foreground step and ISRs must return. Evaluate protection at least every 10 ms; no wall-clock
sleeps, busy delays hiding time from the emulator, or host threads. Keep register ownership explicit
between foreground and ISRs.

## REQ-FW-013 — Interrupt and time correctness

Configure prescalers, compare bytes and vector entries according to MCU-001. Acknowledge only the
IRQ bits serviced. Use coherent low/high reads and unsigned wrap-safe elapsed-time arithmetic;
coalesced pending flags are not an event queue.

## REQ-FW-014 — Tach observation

Compute speed from hardware tach-count differences over a rolling 200 ms window, using two rising
pulses/revolution and modulo-65536 differences. Track the time of last observed count change. A
no-edge reading is evidence of missing verified motion, not proof of a mechanical obstruction.

## REQ-FW-015 — ADC driver

Take AN0 conversions at intervals no greater than 10 ms. Respect BUSY/READY, default div4 conversion
timing and low-then-high result reads. Track freshness from completed conversions, not repeated
reads of the same result. Invalid or stale readings cannot authorize run.

## REQ-FW-016 — Temperature conversion

For the fictional endpoint-rounded ADC, reconstruct V=N\*3.3/1023 and T=(V-0.500)/0.010 C.
Arithmetic must handle negative temperatures without unsigned underflow. Separate raw ADC codes,
volts and Celsius in the implementation.

## REQ-FW-017 — Thermal trip

Two consecutive fresh, plausible temperature samples at or above 85 C latch TH. Remove commanded
motor power within 10 ms after the second qualifying sample. Protection is required in idle as well
as run; do not wait for a stall.

## REQ-FW-018 — Temperature-signal fault

Any completed AN0 code below 62 or above 574, or no fresh completed sample for more than 100 ms,
latches TS. Disable drive within 10 ms after qualification. A constant but plausible voltage is not,
by itself, proof of sensor failure.

## REQ-FW-019 — Start grace

Open a 500 ms stall grace period only on an actual commanded off-to-on transition. Speed changes and
PULSE boosts while already running do not restart it. Thermal and temperature-signal protection
remain active during grace.

## REQ-FW-020 — Stall qualification

After grace, while RUN/PULSE is commanded and the motor driver is commanded enabled, a stall
candidate is present when the 200 ms speed estimate is below 300 RPM OR no tach count change has
been observed for at least 150 ms. If this persists for 200 ms, latch ST and disable drive within 10
ms. A full startup jam must shut down within 750 ms; a sudden jam after established normal running
must shut down within 400 ms.

## REQ-FW-021 — Independent protection causes

A cold no-motion condition shuts down without waiting for heating. A rotating but hot motor shuts
down without waiting for no-motion. Intentional disable/coasting does not qualify a new ST fault.
When multiple conditions qualify in the same evaluation, retain every qualified cause.

## REQ-FW-022 — Latched faults

Keep independent latched bits ST=1, TH=2, TS=4, IF=8. Clearing a physical fault, recovering tach
pulses, releasing PULSE, or cooling alone shall not clear the latch or restart motion. Firmware
shall continue ADC sampling and fault indication after shutdown.

## REQ-FW-023 — Diagnostic display

Render the highest-priority latched cause within 100 ms: TS before TH before IF before ST. TH means
thermal shutdown, not merely reduced power. Preserve subordinate cause bits in software; TH may
dominate an already-qualified ST, but a prior TH shutdown does not create an ST simply because the
rotor later stops.

## REQ-FW-024 — Deliberate rearm

While faulted, require fresh plausible temperature at or below 65 C continuously for 2 s, plus no
currently invalid input combination. Only a new STOP press beginning after these conditions qualify,
stable for 20 ms, may acknowledge the fault. STOP must then be released and all command contacts
released for 20 ms before READY. A new command edge is needed to run. Holding STOP while the motor
cools is not a queued restart or acknowledgment.

## REQ-FW-025 — Power-cycle behavior

Electrical reset clears MCU state but not mechanical latches, rotor momentum, case heat, or sensor
thermal lag in the reference plant. Firmware must repeat the startup interlock; cycling power cannot
cause a warm or still-selected machine to restart.

## REQ-FW-026 — No automated retries

The core assignment prohibits automatic jam retries, reverse kicks, automatic fault resets, and
restarting merely because temperature falls. Physical jam removal cannot be verified with the
available sensors at zero drive; it remains an operator responsibility.

## REQ-FW-027 — Timing and indication

Normal accepted input-to-command response is at most 40 ms after the final contact state stabilizes.
Shutdown timing is measured at MOTOR_EN and PWM enable, not at RPM=0. Fault-label timing is measured
at scanned LCD pixels, allowing a display frame.

## REQ-FW-028 — Reproducibility

Tests must pass on nominal and specified slow-bounce fixtures and bounded documented disturbances,
independent of host execution speed. Random fixture disturbances must record their seed. No hidden
test may demand behavior outside the published component envelope.

## REQ-FW-029 — No simulator shortcuts

The motor plant, thermal plant and sensor model shall not perform the student firmware fault
decision. Test shutdown by observing the firmware-owned enable/PWM signals. Separate independent
mechanical STOP inhibition from firmware fault-response correctness.

## REQ-FW-030 — Student evidence

Deliver local unit tests for scan/debounce, inverse mapping, wrap-safe tach estimation, ADC scaling,
fault arbitration and rearm. Complete the acceptance scenarios in TEST-001 before claiming firmware
acceptance. GUI polish, closed-loop speed regulation and an instruction-set emulator are follow-ons,
not prerequisites.

## Protection state and priority

Use a run state and a separate fault mask, rather than losing fault history in one mutually
exclusive enum. A useful state arrangement is BOOT_LOCK → READY ↔ RUN/PULSE; any qualifying
protection condition enters FAULT_LATCHED. Acknowledgment enters WAIT_FOR_RELEASE, not RUN.
Power-off is controlled outside firmware.

Evaluate temperature validity first. An implausible sample sets TS and cannot also be interpreted as
a trustworthy overtemperature measurement. Evaluate TH and eligible ST from the same pre-shutdown
observation snapshot, then inhibit drive. While already disabled, continue thermal monitoring but do
not generate a new no-motion fault from expected coast-down.

A representative supervision structure is:

```text
acquire fresh MCU observations
update debounced input events and tach-window state
qualify temperature validity, temperature trip and eligible no-motion
latch every newly qualified cause
if any fault or STOP or not yet ready: disable drive
else: update nominal PWM request
update the desired pixel frame without blocking protection
```

This is a behavioral outline, not a supplied firmware solution. The student owns the actual timer,
scan, acquisition, protection and rendering implementation.

## Clock/power/reliability extension (interface 03)

Product R-031..R-040 are controlled in Kestrel correspondence P4 and copied machine-readably to
`spec/clock_reliability_requirements.json`. Their derivation, implementation boundaries and
acceptance scenarios are in `docs/CLOCK_RESET_CONTRACT.md`. These extend, not silently renumber, the
preceding 30.
