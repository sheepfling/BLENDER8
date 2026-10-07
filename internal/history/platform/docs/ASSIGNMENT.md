# Jordon assignment and instructor handoff

> **Interface-03 revision:** Clock setup, watchdog/deadman progress, reset re-entry and qualified
> power behavior are added by [CLOCK_RESET_CONTRACT.md](CLOCK_RESET_CONTRACT.md). The following
> preserves the original peripheral/behavior exercise; fixed-clock assumptions apply only after
> establishing the approved peripheral rate.

**Document:** ASSIGN-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## Student mission

Build firmware that makes the collection of components in BOARD-001 behave according to REQ-FW. This
is a firmware assignment first, with an emulator-development follow-on. The display has pixels,
buttons have electrical contacts, the motor has PWM/tach, and temperature arrives as an ADC code.

## What is supplied

Supply component datasheets, the board wiring table, the complete MCU register SDK and a safe
firmware skeleton with reset/step/vector entry points. Supply a runnable reference target, build
instructions, visible component examples, deterministic fixture seeds/profiles and the complete
acceptance **contract**. Hidden tests may vary only within the published envelopes; do not grade
based on undocumented tricks.

The full repository is the instructor source package. For a firmware-only handoff, give New
Employee the
SDK, `firmware/`, selected docs/specs and an instructor-built matching host harness/library or keep
the simulator source read-only as a practical convenience. Native prebuilt libraries require
matching compiler/platform ABI; none is falsely advertised as cross-platform in this package. The
important restriction is that his firmware target does not depend on simulator headers.

Do not supply a finished debouncer, speed-control function, pixel font/rendering library,
temperature supervisor or reference application as the student's starter. The provided starter
intentionally keeps the motor disabled. `blender8_bench` is an instructor wiring exercise; reveal
only relevant snippets when a milestone needs help.

## Milestones

| Stage                    | Student work                                                                | Exit evidence                                                                           |
| ------------------------ | --------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| 0: one byte, one pixel   | Build target; reset safe; write an LCD byte; explain bit orientation        | 0x81 gives the two specified pixels and no unintended motor activity                    |
| 1: virtual pins          | Configure GPIO, select/read all mux channels, configure timer interrupt     | Timing trace showing selection, settling, sampled values and correct IRQ acknowledgment |
| 2: mechanical controls   | Debounce, one-hot selection, PULSE and STOP semantics                       | Nominal/slow-bounce tests; STOP-dominance and held-PULSE tests                          |
| 3: spin and observe      | Invert nonlinear RPM curve; produce PWM; count tach pulses                  | Quantization/error tests; correct speed differences; no PID required                    |
| 4: analog and protection | ADC state machine, voltage/temperature conversion, stall and thermal faults | Cold jam, hot rotating motor, sensor rail/stale data, priority and rearm tests          |
| 5: present and harden    | Pixel UI, fault display, long-run/wraparound and reset cases                | Repeatable acceptance evidence and a clear local build/test command                     |
| 6: emulate a peripheral  | Replace LCD first; preserve firmware and component contract                 | Old firmware still works; all LCD reference/conformance tests pass                      |

## Core/bonus boundary

Core: registers, interrupts/timers, muxed digital input/output, nonlinear open-loop motor mapping,
tach-based no-motion protection, ADC/temperature protection, pixel framebuffer and deterministic
tests.

Bonus: game-style boot animation, pixel thermometer/RPM bar, host scope panel, NTC sensor variant,
real low-energy hardware, current sensing, PID speed regulation or an instruction-set emulator. Do
not hide a PID assignment inside the stated open-loop calibration exercise.

## Fair review rubric

Safety/protection invariants30%; correct firmware behavior25%; timing/register discipline20%; test
evidence15%; code clarity and ownership10%. Animation/UI polish cannot compensate for a
restart-after-fault defect. A good answer also identifies what the sensors cannot know: ST is not
proof of a jam, and a plausible frozen temperature sensor is not automatically distinguishable from
stable temperature.

## C++ constraints

Use C++20 and standard fixed-width integer types. The SDK is deliberately small. Host
filesystem/thread/random/chrono APIs are not firmware dependencies. Floating-point math is
acceptable for this learning target unless the instructor explicitly assigns fixed-point as a later
constraint. Bounded application buffers and explicit state are preferable to allocation in ISRs. No
simulator namespace, debug getter or fixture time may leak into firmware.

## Useful instructor interventions

Provide a register trace or a single failing scenario, not the complete solution. Examples: “why did
the LCD drop the second byte?”, “why did STOP disable drive but RPM remain nonzero?”, “why did10%
PWM not mean2000 RPM?”, and “why did an unplugged temperature wire appear at full-scale?”. Once
Jordon can explain those outcomes, the abstractions are doing their job.
