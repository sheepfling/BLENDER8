# Architecture and emulation execution model

**Document:** ARCH-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## 1. What Jordan receives and what he implements

Give Jordan the MCU SDK, board wiring facts, component datasheets, firmware behavioral requirements,
a safe application stub, build support and visible test expectations. The instructor owns a working
hardware reference, not a prefilled application. Jordan writes GPIO scanning/debounce,
timer/interrupt setup, inverse motor mapping, tach observation, ADC conversion, pixel rendering,
protection logic and unit tests.

Do not start by requiring him to write button physics, motor physics, a CPU instruction decoder and
a GUI simultaneously. Once his firmware works against the reference, replace one component with his
implementation. The LCD is the natural first emulator exercise: a few registers,64 bytes of VRAM,
busy timing and scanout make visible, local cause-and-effect.

## 2. Architectural layers and dependencies

| Layer                         | Owner                | Contains                                                        | Must not know                                  |
| ----------------------------- | -------------------- | --------------------------------------------------------------- | ---------------------------------------------- |
| Application firmware          | Jordan               | Scan/debounce, speed mapping, faults, drawing                   | Simulator types, fixture time, plant truth     |
| MCU SDK                       | Instructor contract  | Reg/Irq, read8/write8/idle, vector signature                    | Button/motor/LCD object APIs                   |
| Register backend and runtime  | Instructor initially | MMIO effects, peripheral clock, IRQ dispatch                    | High-level blender decisions                   |
| Component models and nets     | Instructor initially | Contacts, mux, LCD, PWM-driven motor, tach, heat, analog sensor | The student's intent or private state machine  |
| Board composition             | Instructor           | Fixed wiring and independent STOP/power gate                    | A shortcut that answers firmware questions     |
| Scenarios and instrumentation | Instructor/test code | Presses, load/jam, noise, pixel/trace observation               | Permission to alter firmware behavior silently |

The firmware object target sees only `sdk/include` and `firmware/`. The simulator include tree is
not on that target's path. `check_boundaries.py` rejects obvious simulator imports/host timing
dependencies; this is a maintainability guardrail, not a security sandbox. The full source package
is for Rick; withholding reference source in phase1 is a handoff choice, not a substitute for
architectural isolation.

## 3. Firmware-side header shape

```cpp
namespace b8 {
    enum class Reg : std::uint16_t; // all addresses in registers.hpp
    enum class Irq : std::uint8_t;
    using Isr = void (*)();
    using VectorTable = std::array<Isr, 5>;
    std::uint8_t read8(Reg address);
    void write8(Reg address, std::uint8_t value);
    void idle();
}
namespace firmware {
    void reset();
    void step();
    extern const b8::VectorTable vectors;
}
```

Use unsigned8-bit register values and explicit widths for register/count types. The
hypothetical8-bit device does not force every host C++ integer or pointer to be8 bits. This revision
is a register-level simulator, not a binary-compatible Game Boy-like CPU emulator.

A free-function API is intentional: `b8::read8` feels like a device transaction without handing
firmware an abstract object capable of revealing the emulator. On a native host, ordinary volatile
pointers cannot magically intercept peripheral side effects. A different backend can preserve these
operations' semantics without per-call `#ifdef SIM` in the application.

## 4. Simulator-side headers

`signals.hpp` defines digital wires and the byte-peripheral transaction boundary. `analog.hpp`
defines an analog voltage wire and a thermal attachment node. Each device has its own header:
`button_assembly.hpp`, `mux8.hpp`, `lcd32.hpp`, `motor.hpp`, `temperature_sensor.hpp`,
`thermal_model.hpp`, `adc.hpp` and `mcu.hpp`. `board.hpp` connects them; `runtime.hpp` binds the
firmware ABI.

GPIO and PWM are connected through actual modeled logic nets, not through `set_speed()` calls. The
thermal model writes a motor-case node; the temperature sensor reads that attachment and drives
voltage; the ADC samples the voltage. Only its resulting register code reaches firmware. Debug
getters exist solely for instructor instrumentation and must never appear in the SDK.

The current devices are concrete reference classes rather than a large inheritance framework. To
replace a component in phase2, preserve its header contract or supply a link-compatible
implementation; use the existing `BytePeripheral` polymorphic boundary for the LCD. Do not promise
every future hardware adapter is a drop-in C++ subclass: the reusable contract is primarily pin,
transaction and timing semantics.

## 5. Logical-time scheduler

One board tick is 1 us. At each tick, the reference advances in this fixed order:

```text
increment absolute fixture time
if powered: advance MCU timers, ADC and PWM
settle button contacts, mux propagation and independent enable gate
advance motor mechanics; update thermal plant on each1-ms boundary
advance sensor lag and analog output
if powered: update LCD row scanout and latch tach/VBLANK edges
return to runtime; at the SDK boundary, deliver at most one pending IRQ
```

External fixture changes are applied at explicit caller-controlled logical times; `Board::settle()`
propagates zero-delay wiring. Future arbitrary timestamped event-queue playback must preserve this
ordering. The delivered bench applies a finite scripted sequence directly; `scenarios/*.json` are
scenario specifications, not already-supported CLI input files.

Register reads observe the state before their subsequent1-us advancement. ADC sampling therefore
observes the analog value present at its sampling boundary, and an exact same-tick sensor update
occurs later in the stated order. This resolves ties reproducibly. Direct fixture calls can advance
the board by long durations without executing firmware; that is useful for component tests but is
not a valid way to test a running firmware control loop.

Ordinary C++ computation does not consume simulated time. ISR reentry only happens at SDK boundaries
and nesting is suppressed. A student loop that never returns and never accesses the SDK can hang the
host rather than advance the device; use a wall-clock process timeout to terminate such tests. That
watchdog is a test-harness guard, not a modeled hardware watchdog. Cycle-accurate execution/CPU
workload timing is an explicit later extension.

## 6. Suggested firmware scheduling

Use T0 for a 1-ms tick and event scheduling. A reasonable scan pipeline reads the prior mux
selection after it has settled, then selects the next channel; every contact is observed every 8 ms.
Three consistent observations span16 ms. Use foreground-owned state for debounce and protection;
keep the ISR short. Use a 200-ms rolling tach window with modulo count differences. Start AN0
conversions every 10 ms, collect fresh completed samples without blocking, and keep supervision
intervals≤10 ms. Transfer a 64-byte shadow LCD frame during VBLANK while respecting DATA busy
timing.

This is one viable scheduling design, not a requirement to copy a single architecture. Correct
polling or interrupt-driven ADC collection is acceptable when deadlines, coherence and ownership are
met. The student's test design should expose interrupt masking, latch reentrancy and
missed/coalesced flags rather than rely on host-speed luck.

## 7. Model fidelity and constraints

Digital nets model logical resolution, not signal integrity or voltage thresholds. The mux models a
published2-us delay, not a transistor implementation. The LCD bus models ordered byte transfers, not
individual strobe edges. The motor model is an averaged nonlinear RPM curve with first-order inertia
and a two-pulse tach. Thermal behavior is a lumped case node plus sensor lag, with a weak optional
food-temperature path. The ADC models explicit acquisition/conversion timing, quantization and
register behavior, not every commercial converter error.

The model does not decide ST/TH/TS for Jordan. Those diagnoses are application behavior. Keep the
independent mechanical STOP gate, but do not let an instructor convenience auto-disable a jammed/hot
motor and thereby make broken firmware pass.

## 8. Later progression

After firmware acceptance: replace LCD; replace mux/contact behavior under the same envelope;
implement alternative ADC/sensor or motor models; add a graphical host panel that renders scanout
and changes physical fixture controls; add a real hardware backend. Optional subsequent studies
include NTC inversion, current-sense input on AN1, speed regulation with saturation/anti-windup, and
eventually a fictional CPU instruction set/interpreter. Each adds a new learning objective rather
than becoming a hidden prerequisite for the first blinking pixel.
