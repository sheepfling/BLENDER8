# Verification plan and acceptance matrix

> **Interface-03 revision:** Clock setup, watchdog/deadman progress, reset re-entry and qualified
> power behavior are added by [CLOCK_RESET_CONTRACT.md](CLOCK_RESET_CONTRACT.md). The following
> preserves the original peripheral/behavior exercise; fixed-clock assumptions apply only after
> establishing the approved peripheral rate.

**Document:** TEST-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## Three distinct test layers

**Component conformance:** implemented C++ tests check MCU registers, digital nets, button
mechanics/bounce, mux timing, LCD behavior, timers/IRQs/PWM, motor/tach, thermal dynamics, sensor
behavior and ADC transactions. These tests use instructor fixtures, because they verify the
reference hardware rather than the application.

**Application unit tests:** New Employee supplies tests for his debouncer, display encoding, inverse
mapping, time/counter arithmetic and protection/rearm state logic. A unit test may operate on a pure
algorithm with sample values, but shipping firmware cannot use simulator component interfaces.

**End-to-end firmware acceptance:** the 38 scenarios below are the behavioral contract. They must
eventually run through the MCU SDK against a continuously executed firmware application. The safe
starter does not satisfy them, and this package does not claim a completed application acceptance
runner.

## Quantitative grading conventions

Use simulated time, not wall-clock duration, for modeled deadlines. Every trace must identify
scenario, revision, fixture profile/seed, command time, observed final contact state, register
observations and physical output changes. Start normal-response timing after final mechanical
contact stabilization. Start stall timing from actual command enable or injected running jam as
specified; the 750/400-ms limits include estimator, grace/dwell and supervisory margins. Thermal
shutdown is measured after the second fresh qualifying ADC sample, not against an unobservable
immediate winding temperature.

Observe MOTOR_EN and PWM enable for firmware shutdown. The independent STOP gate can make an unsafe
firmware output look physically disabled; therefore test the firmware-owned outputs separately for
STOP conformance. Observe scanned LCD pixels, not only VRAM or intended strings. Choose a published
application font/layout for automated diagnostic-image comparison when implementing the acceptance
driver; the provided reference LCD tests verify pixel packing but do not presume New Employee's font.

For full-profile acceptance, vary scan phase, interrupt coincidence and bounded bounce/noise without
changing the published envelopes. Add process timeouts for host hangs. Numerical thermal checks use
analytical first-order limits and tolerances; they are not validation against a real motor.

## Acceptance scenarios — specified, not all executable yet

| ID  | Scenario                         | Requirements | Stimulus and required result                                                                                                                                                                      |
| --- | -------------------------------- | ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A01 | Boot with no command             | 002,003      | Power-on at 25 C, all controls released. **Expect:** Remain disabled during2-s qualification; become READY without running.                                                                       |
| A02 | Boot with latched speed          | 002,025      | Latch speed4, cycle panel power, leave speed held. **Expect:** Never run until STOP releases all controls and a subsequent fresh command occurs.                                                  |
| A03 | Warm power cycle                 | 002,025      | Trip TH; cycle power while motor case/sensor remain hot. **Expect:** Stay disabled; no reset-to-room-temperature shortcut.                                                                        |
| A04 | Nominal speed sweep              | 008,010      | After READY, command each of seven speeds at L=0 for 1 s. **Expect:** Each steady speed meets max(100 RPM,2%) tolerance; monotonic evenly spaced nominal targets.                                 |
| A05 | Low standalone PULSE             | 009,008      | From READY press PULSE for 100 ms; then release. **Expect:** Command2000 RPM nominal while held, not10% duty; restore idle after debounce.                                                        |
| A06 | Boost and cap                    | 009          | Select speed7, hold PULSE, release it. **Expect:** Target20000 RPM while held, then18000; do not restart stall grace while boosting.                                                              |
| A07 | Slow contact bounce              | 004,005      | Use slow5-ms bounce on speed and PULSE transitions with scan-phase offsets. **Expect:** One accepted transition per final state; response within 40 ms of stability.                              |
| A08 | STOP versus held PULSE           | 007,009      | Run with PULSE held; press STOP, release STOP while PULSE stays held. **Expect:** Gate opens immediately; firmware disables; no restart until PULSE release/fresh press.                          |
| A09 | STOP during scan/ISR             | 007,013      | Assert STOP at every phase of mux scan and during ordinary interrupt work. **Expect:** Independent drive inhibit; firmware-owned outputs cleared within the 10-ms requirement.                    |
| A10 | Conflicting contacts             | 006          | Inject two stuck-closed speed sense contacts for>40 ms. **Expect:** Latch IF; no arbitrary speed winner; recover only through deliberate rearm.                                                   |
| A11 | Sequential scan transient        | 004,006      | Change from speed1 to7 between channel samples under healthy mechanics. **Expect:** Do not latch IF from a short scan-skew/bounce transient.                                                      |
| A12 | LCD busy fault                   | 011          | Exercise rendering with ISR interruptions and closely spaced writes. **Expect:** No lost accepted frame bytes; honor busy timing and clear errors deliberately.                                   |
| A13 | VRAM layout/wrap                 | 011          | Render known pixel test pattern and full64-byte frame. **Expect:** Vertical bit order correct, bounds respected and scanned result matches intended image.                                        |
| A14 | ADC conversion driver            | 015,016      | AN0 nominal temperatures plus AN1 ground; vary conversion phase. **Expect:** Correct52-us readiness, low/high snapshot and conversion units; no stale read treated fresh.                         |
| A15 | Negative temperature math        | 016          | Inject a sensor-equilibrated temperature below0 C but within rated range. **Expect:** Signed converted temperature is correct, not an unsigned huge overtemperature.                              |
| A16 | Cold startup jam                 | 019,020,021  | At25 C, after READY jam rotor before requesting speed3. **Expect:** Startup grace tolerated, then ST; disable within 750 ms; do not wait for heat.                                                |
| A17 | Cold running jam                 | 020,021      | Run speed3 for 1 s at nominal load, then hard jam. **Expect:** ST disables within 400 ms of jam; PWM and enable off even though case stays cool.                                                  |
| A18 | Hot but rotating                 | 017,021      | Sustain heavy load while tach remains far above stall threshold, or use labeled hot-case setup. **Expect:** TH disables independently of stall; no need for tach failure.                         |
| A19 | Combined qualified causes        | 021,022,023  | Arrange same-evaluation qualifying low-motion and two high-temperature samples. **Expect:** Retain ST and TH; display TH; physical disable is not delayed for UI.                                 |
| A20 | ST then delayed sensor heat      | 021,022,023  | Qualify ST near hot case while sensor lags; TH later qualifies during faulted idle. **Expect:** Keep ST, add TH, change display priority toTH; never re-enable drive.                             |
| A21 | Thermal threshold noise          | 017,024      | Apply bounded ADC/sensor noise near85 C and during cooldown near65 C. **Expect:** Trip only on two consecutive qualified high samples; no auto-clear or chatter-driven restart.                   |
| A22 | Tach startup delay               | 019,020      | Healthy minimum command with normal inertia; vary initial rotor phase. **Expect:** No false ST before motion develops; grace is bounded and not endlessly extended.                               |
| A23 | Transient tach dropout           | 014,020      | Suppress tach for\<100 ms after stable normal motion, then restore. **Expect:** No200-ms-qualified ST; sustained dropout still trips.                                                             |
| A24 | Tach disconnected while spinning | 014,020      | Set tach stuck low while actual motor remains spinning. **Expect:** ST as unverified motion; do not claim simulator truth tells firmware it is only a tach fault.                                 |
| A25 | Intentional coastdown            | 014,021,027  | Command normal STOP and observe falling but nonzero RPM. **Expect:** No new ST while disabled; drive-off deadline is not a zero-RPM deadline.                                                     |
| A26 | Sensor open/short                | 018          | Inject open, short-ground, short-supply separately while running. **Expect:** Fresh rail conversion latches TS and disables; invalid rail is not interpreted as trustworthy TH.                   |
| A27 | ADC stale completion             | 015,018      | Fault-inject missed/disabled acquisition while old result is plausible. **Expect:** Freshness expiry at 100 ms followed by disable within 10 ms; repeated code reads do not reset age.            |
| A28 | Plausible frozen sensor          | 018,028      | Freeze VOUT at a valid room-temperature voltage while case warms. **Expect:** Demonstrate observability limitation; infallible detection is not an acceptance requirement.                        |
| A29 | Fault clearing alone             | 022,026      | Clear jam or restore tach after ST without operator acknowledgment. **Expect:** Still latched and disabled.                                                                                       |
| A30 | Cold-fault acknowledgment        | 024          | While faulted obtain2 s of valid≤65 C readings; then freshly press/release STOP; then new speed. **Expect:** Only the complete sequence enables a new run; acknowledgment itself never starts it. |
| A31 | STOP held during cooling         | 024          | Press STOP while hot and hold it through 2 s below 65 C. **Expect:** No queued acknowledgment; require a new STOP press after permissive qualifies.                                               |
| A32 | Cooling not sufficient           | 024,026      | Let TH cool below 65 C without pressing STOP. **Expect:** Remain latched and disabled; TH label remains until deliberate recovery.                                                                |
| A33 | Food cooling versus load         | 017,021,028  | Compare L=1 with food coupling0 versus0.15 W/K at 0 C; preserve all other parameters. **Expect:** Thermal path changes temperatures but never exempts ST/TH decisions or changes thresholds.      |
| A34 | Counter wrap                     | 013,014      | Run across timer/software-time and tach counter wrap boundaries. **Expect:** No underflow-driven speed spike, missed fault or false restart; use bounded modulo arithmetic.                       |
| A35 | IRQ contention and ownership     | 012,013,015  | Make timer, tach, VBLANK and ADC events pending together; mask/unmask briefly. **Expect:** Priority/acknowledgment correct, no torn samples, no assumption that flags queue events.               |
| A36 | Deterministic replay             | 028,030      | Repeat same nominal/slow-bounce/noise-seeded fixture and command timing. **Expect:** Same logical outputs within defined numerical tolerance, regardless of host pacing.                          |
| A37 | Thermal fault during startup     | 017,019      | High valid temperature develops during500-ms stall grace. **Expect:** TH remains active; grace masks only stall qualification.                                                                    |
| A38 | Fault UI deadline                | 011,023,027  | Qualify each fault cause at multiple LCD frame phases. **Expect:** Dominant diagnostic appears in scanned pixels within 100 ms while drive remains disabled.                                      |

## Executable reference inventory

The following individual reference cases are registered with CTest. Additional CTest entries check
safe-starter behavior and local specification/dependency consistency. Exact run results are recorded
in VALIDATION.md.

- `reference.reset`
- `reference.registers`
- `reference.gpio_contention`
- `reference.mux_select`
- `reference.mux_settle`
- `reference.button_interlock`
- `reference.button_bounce`
- `reference.pulse_stop`
- `reference.stop_gate`
- `reference.power_retention`
- `reference.lcd_pixels`
- `reference.lcd_busy`
- `reference.lcd_wrap`
- `reference.lcd_vblank`
- `reference.timer_period`
- `reference.timer_prescale`
- `reference.timer_latch`
- `reference.timer_write_order`
- `reference.timer_one_shot`
- `reference.irq_w1c`
- `reference.irq_mask`
- `reference.irq_dispatch`
- `reference.irq_priority`
- `reference.pwm_endpoints`
- `reference.pwm_latch`
- `reference.motor_curve`
- `reference.motor_load`
- `reference.motor_coast`
- `reference.tach_count`
- `reference.wiring`
- `reference.adc_reset`
- `reference.adc_conversion`
- `reference.adc_scalers`
- `reference.adc_busy_error`
- `reference.adc_sample_hold`
- `reference.adc_result_latch`
- `reference.adc_overrun`
- `reference.adc_channels`
- `reference.adc_irq`
- `reference.adc_abort`
- `reference.adc_reference`
- `reference.adc_fault_rails`
- `reference.sensor_linear`
- `reference.sensor_lag`
- `reference.sensor_seed`
- `reference.sensor_frozen`
- `reference.motor_jam`
- `reference.tach_no_motion_signature`
- `reference.thermal_loss`
- `reference.thermal_cooling`
- `reference.thermal_food`
- `reference.thermal_overheat`
- `reference.thermal_power_retention`
- `reference.stop_sense`
- `reference.slow_bounce`
- `reference.tach_wrap`

## Test double and oracle discipline

A component under test must not compute its own expected answer using the same helper it is meant to
validate. The supplied suite includes independent analytical checks (thermal exponential cooling,
motor inverse relation, byte/counter snapshots, sampled ADC voltage) and simple boundary checks.
This is useful conformance evidence but not exhaustive proof. Expand through randomized/property
tests and mutation tests once the core assignment is in use.

Keep deliberate fault injection explicit: a hard jam overrides rotor motion; a tach failure
overrides only the observed tach wire; a sensor open overrides only the analog wire; an initial
hot-case fixture changes physical state, not the student's temperature variable. A single sensor's
plausible frozen output is an acknowledged detection limitation, not a grading trap.
