# Source inventory and SWIL/HWIL boundary

**Historical P6 owner engineering note — B8 interface 03, native platform 0.3.0.** Its
`reference-platform/` paths describe the older bundled snapshot, which is retained in the unpacked
original under `.local/source-archives/original-intake/`. The active platform is `platform/` version
0.6.0. Read `platform/docs/ARCHITECTURE.md` and `platform/docs/COMPONENTS-AND-HWIL.md` for current
code boundaries and jar-interlock status. This note is not a claim of a physical HWIL
implementation.

## Three different interfaces

The application receives only `reference-platform/sdk/include/blender8/`. `device.hpp` declares
`read8(Reg)`, `write8(Reg,uint8_t)`, `idle()`, `Isr`, and the five-entry `VectorTable`.
`registers.hpp` defines every register address and interrupt mask. Application reset/step entry
points and the vector table live in `firmware/firmware.hpp`. No host clock, button object, RPM
getter, thermal state, or fixture control is in that API.

The device implementation lives in `reference-platform/sim/`. Its component headers and pin/net
objects are a second, owner-only surface. The test fixture and board composition are a third
surface: they may press controls, vary load, inject wiring/clock faults and inspect plant truth.
CMake intentionally omits simulator include paths from the `student_firmware` target. This is
architectural isolation, not an adversarial security sandbox.

## Included headers and source

Paths below are relative to `reference-platform/`.

| Responsibility                                         | Header(s)                                                           | Implementation / driver                                   |
| ------------------------------------------------------ | ------------------------------------------------------------------- | --------------------------------------------------------- |
| Firmware register API                                  | `sdk/include/blender8/device.hpp`, `registers.hpp`                  | SDK symbols bound in `sim/src/runtime.cpp`                |
| Logic wiring and byte device                           | `sim/include/blender8/sim/signals.hpp`                              | Header-defined net resolution / `BytePeripheral` contract |
| Analog voltage / thermal attachment                    | `sim/include/blender8/sim/analog.hpp`                               | Header-defined voltage/thermal nodes                      |
| Interlocked buttons                                    | `sim/include/blender8/sim/button_assembly.hpp`                      | `sim/src/button_assembly.cpp`                             |
| 8:1 selector                                           | `sim/include/blender8/sim/mux8.hpp`                                 | `sim/src/mux8.cpp`                                        |
| Pixel display                                          | `sim/include/blender8/sim/lcd32.hpp`                                | `sim/src/lcd32.cpp`                                       |
| Motor, tach and thermal plant                          | `motor.hpp`, `thermal_model.hpp` in the simulator include directory | Corresponding `.cpp` files                                |
| Temperature sensor / ADC                               | `temperature_sensor.hpp`, `adc.hpp`                                 | Corresponding `.cpp` files                                |
| External oscillator and clock tree                     | `clock_source.hpp`, `clock_tree.hpp`                                | Oscillator header / `sim/src/clock_tree.cpp`              |
| Watchdog / deadman                                     | `supervision.hpp`                                                   | `sim/src/supervision.cpp`                                 |
| Supply ramp, switch and fuse states                    | `power_domain.hpp`                                                  | Header-defined `PowerDomain`                              |
| MCU registers and peripheral state                     | `mcu.hpp`                                                           | `sim/src/mcu.cpp`                                         |
| Net composition and independent enables                | `board.hpp`                                                         | `sim/src/board.cpp`                                       |
| Native firmware execution and interrupt/reset delivery | `runtime.hpp`                                                       | `sim/src/runtime.cpp`                                     |

Unqualified header names in this historical table are under `sim/include/blender8/sim/`. The source
ZIP contains `interfaces/source-inventory.json` with SHA-256 hashes generated during bundling.
Actual types/signatures in the active `platform/` headers control, not an inferred API from this
table.

## Time, connections and resolution

`Tick` is the fixture's absolute integer microsecond time. `Board::advance(delta)` advances hardware
time; `Board::settle()` resolves immediate wiring. GPIO, contact, mux, PWM and tach signals use real
model nets. The model does not translate a firmware speed number directly into `set_rpm`.

`DigitalNet` distinguishes LOW, HIGH, high impedance and UNKNOWN. Board bias resolves a released
contact; opposing driven values are invalid rather than a chosen winner. `AnalogNet` carries a
voltage and disconnection/bias behavior, not a SPICE circuit. `ThermalNode` connects motor case and
sensor attachment. `ClockNet` carries frequency plus validity: an eight-megahertz oscillator is not
expanded into 125-nanosecond digital transitions in a one-microsecond fixture.

`BytePeripheral::bus_read(Tick,uint8_t)` and `bus_write(Tick,uint8_t,uint8_t)` form an actual
replaceable byte-transaction boundary. LCD write recovery and scanout are independent of host clock
changes. Most other devices are concrete reference classes attached to nets, not subclasses of a
universal `IDevice` interface. Replacement must preserve connection and timing semantics and update
board construction where needed; it is not automatically achieved by inheriting a guessed class.

## Binding and scheduling

A native SDK call performs an ordered register operation followed by one microsecond of service time
and an interrupt opportunity. `idle()` advances that service interval; it is neither host sleep nor
a low-power instruction. The selected system and peripheral clocks advance their own counters within
the fixture. Ordinary native C++ arithmetic does not consume simulated instruction cycles.

`Runtime::run_for(duration, reset_fn, step_fn)` invokes the bounded application callbacks. `Runtime`
permits one active binding per process. Tests needing parallel devices should use separate processes
or work directly with component fixtures rather than create two active SDK runtimes in the same
process.

The MCU has five fixed-priority non-nesting interrupt sources. WDT, DMT and missing-clock reset do
not depend on maskable dispatch. Reset abandons the old execution context through the binding's
reset exception/unwind path; it must not resume an old ISR and restore stale enable bits. External
display RAM, mechanical latches, rotor motion and retained heat have their own reset/power behavior.

## What would have to change for HWIL

Two migration arrangements are possible, but neither is delivered as a physical adapter here.

**Firmware on a physical controller:** retain application-level separation, supply a backend
implementing the B8 register semantics, and decide which operations can be mapped to actual
registers and which require compatibility logic. A real MCU does not become B8 merely because its
pins have similar names. WDT keys, timer latches, reset causes, clock switching and service timing
must be reconciled explicitly. The one-microsecond native service binding is not a physical
CPU-cycle promise.

**Native firmware with selected real peripherals:** implement timed GPIO/PWM/analog/byte-bus
bridging at the device boundary. Host USB/serial/Ethernet transport latency is not automatically
compatible with two-microsecond mux settling or eight-microsecond LCD recovery. Fast local edges and
capture should be handled by a suitable local real-time controller or fixture, with timestamped
transport and explicit synchronization. This is a migration requirement, not a provided driver.

In either arrangement, keep fixture-only inspection and control out of the firmware SDK. Preserve
the independent drive-inhibit chain outside application scheduling. Define ownership for reset,
contact bias, PWM generation, edge counting, ADC conversion, watchdog time and clock failure before
claiming interchangeability. A network round trip is not a substitute for a component's hardware
deadline.

## Conformance evidence needed before a backend is called compatible

Use the included generic register/component tests where their fixture permits adaptation. Measure
register ordering and low/high capture; timer phase and prescale; PWM shadow/disable behavior;
external-bus rejection/recovery; ADC sample-hold/freshness; IRQ ordering/W1C; clock qualification
and pending commit; WDT/DMT timeout/key faults; reset state/retention. Add physical latency,
voltage/grounding, power and failure testing for the actual fixture. The native suite cannot
establish physical safety or electrical compatibility by itself.

The current baseline has **no jar interlock component**. Correspondence P5 added S2 raw sensing,
PB3/PB4 observation and U_IL dropout/arming state for chassis 04. That work remains listed in
`spec/jar-interlock-cases.json` above the baseline. Ordinary B8 GPIO is sufficient at the firmware
boundary; board composition and independent latch behavior still need to be implemented and
verified. Do not hide this gap in a hardware adapter.

## Runnable programs and tests

`apps/student_runner.cpp` runs the deliberately safe firmware stub. `apps/bench.cpp` exercises the
earlier plant/IO behavior; `apps/clock_bench.cpp` demonstrates clock/power/supervision behavior.
`tests/reference_tests.cpp` and `tests/clock_tests.cpp` are component/reference tests, not a
finished firmware grader. `tests/manual_examples.cpp` executes eight examples extracted from the P6
B8 manual.

The existing scenario JSON files are specifications, **not** an implemented general scenario-replay
CLI. Old acceptance scenarios predate the current email requirements and include retired algorithm
prescriptions. The owner crosswalk must be applied before developing a current acceptance runner.
Source inclusion is not proof that a scenario has been executed.
