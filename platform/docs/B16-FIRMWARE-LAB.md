# B16-first firmware and subsystem path

This is a learner route for Astin, starting with the behavioral B16 target. Start with one pure
function in the [practice harness](../labs/b16/README.md), then connect what you learned to the
existing platform models and only later to `platform/firmware/firmware.cpp`. The supplied firmware
is intentionally unfinished. This guide gives inputs, checks, and expected observations without
providing a finished controller.

Read the [accepted correspondence](../requirements/correspondence.md), the relevant supplier
handouts, and the separate [B16 register addendum](B16-FOLLOW-ON.md). For the first three lessons,
use the AVT10, MD20, and BA-8 manuals linked from the practice harness. The seven shared recipient
handouts remain a separate set; the optional B16 manual is an additional 11-page manual. Do not
include `internal/owner/`, `internal/authoring/`, or the solution branch's worked controller and
lesson material in Astin's packet.

The seven shared recipient handouts are the [accepted correspondence](../requirements/correspondence.md),
[MCU-001](datasheets/MCU_B8.md), [XO8-33 sheet](../../latex/oscillator/xo8.tex), [MX8-1](datasheets/MUX8.md),
[PX32-16](datasheets/LCD32.md), the combined MD20 motor/AVT10 sensor sheet
([motor](datasheets/MOTOR.md), [sensor](datasheets/TEMPERATURE_SENSOR.md)), and
[BA-8 controls](datasheets/BUTTON_ASSEMBLY.md). The correspondence states the appliance requirements
and safety behavior. The optional [B16 follow-on](B16-FOLLOW-ON.md) adds B16 pin routing, DMA, SRAM,
vector-table, and D1 supervision details. The order below uses those sources together; it does not
imply every manual feature is needed to complete the first firmware exercise.

## 1. Establish the native loop

Install Python 3.12+, CMake 3.20+, a C++20 compiler, LLVM `clang++`, and `llvm-size` or `size`. The
PDF tools and Emscripten are optional for this native route. From the repository root, bootstrap the
Python tools as described in [New Employee's route](../../NEW-EMPLOYEE-START.md), then run:

```sh
python platform/tools/b8.py --root platform doctor
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 build --device B16
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 test
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 animated --open
```

Pass signals: `native_ready` is true; CMake configures the B16 profile; the selected firmware,
simulator, and C++ tests compile; CTest passes; the workbench opens and **RUN** advances logical
time. The untouched starter keeps the motor off. Platform CTest is platform/model evidence, not
proof that the learner's product behaviors pass. The default fused-on watchdog is not serviced by
the starter; if the page keeps running, periodic WDT reset warnings are expected while drive remains
off. This is a useful supervision observation, not evidence of product acceptance. Stop the workbench
server with Ctrl-C.

## 2. Small isolated exercises

The [practice harness](../labs/b16/README.md) is a standalone host C++ project. Build once, then
run only the case currently being studied:

```sh
cmake -S platform/labs/b16 -B build/astin-b16-labs -DCMAKE_BUILD_TYPE=Debug
cmake --build build/astin-b16-labs
ctest --test-dir build/astin-b16-labs -R '^b16\.temperature$' --output-on-failure
```

The first two commands should pass. Each test initially fails against its TODO stub; that is the
expected baseline. Implement one helper, rerun its named test, and keep the other named failures
visible until those exercises are reached.

| Step               | C++ idea and small change                                                                                                                                | Input, observation, and expected result                                                                                                                                                                                 | Existing model checks after the exercise                                                                                                                                                       |
| ------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1. Temperature     | `std::uint16_t`, `float`, units, a pure calculation. Implement the nominal AVT10 code-to-Celsius conversion.                                             | Codes 155, 233, and 419 should estimate about 0, 25, and 85 C within the stated tolerance. A failing sample prints expected and actual values. This estimate is not a fresh ADC read or a physical winding temperature. | `ctest --test-dir build/astin-b16 -R 'reference\.(adc_conversion|sensor_linear)$' --output-on-failure`; inspect Sensor/ADC in the workbench.                                                   |
| 2. Motor curve     | Function inputs/outputs, `float` constants with `F`, `std::pow`, dimensions, and curve inversion. Implement only the pure positive-target duty fraction. | Targets 3000, 8000, and 18000 RPM should produce approximately 0.388868, 0.616330, and 0.943918. The helper never writes PWM or grants motion.                                                                          | `ctest --test-dir build/astin-b16 -R 'reference\.(motor_curve|pwm_endpoints|motor_load|motor_coast)$' --output-on-failure`; use the model's run-up/coast observations.                         |
| 3. Buttons and mux | Small integer ranges, array indexing, input validation. Map C0..C6 to speed 1..7; keep C7 (PULSE) distinct.                                              | Channels 0..6 map to 1..7; C7 and out-of-range input return “no speed.” A passing unit harness checks every channel.                                                                                                    | `ctest --test-dir build/astin-b16 -R 'reference\.(mux_select|mux_settle|button_interlock|button_bounce|pulse_stop)$' --output-on-failure`; inspect latched command versus electrical contacts. |

These three harness tests cover only pure helper functions. The existing model tests exercise
contacts, PWM, motor physics and ADC plumbing independently of learner firmware. They do not prove
that a function was integrated into the application correctly.

## 3. Dependency-ordered subsystem walkthrough

After the pure helpers, trace the supplied `reset()`, `step()`, and six-entry B16 vector table.
Use `DEVICE-WORKFLOW.md` for pins and commands, `CLOCK_RESET_CONTRACT.md` for startup/reset/power,
`SUPERVISION.md` for watchdogs, and `ANIMATED-WORKBENCH.md` for observations. In each rung,
“proposed” means a useful next harness that has **not** been implemented; do not run it as if it
exists.

### 4. MCU profile, numeric types, and register boundary

- **Depends on and source:** the native loop; `MCU_B8` and the B16 follow-on. Identify the selected
  profile and allowed SDK/register headers before touching peripherals.
- **C++ and harness:** fixed-width types, casts/promotions, `constexpr`, enums, headers, and small
  functions. `b16.peripherals`, `manual.gpio`, and `manual.ack` are existing platform examples, not
  a learner exercise. A register-boundary learner harness is **proposed, not implemented**.
- **Observe and compose:** selected firmware builds and keeps its SDK-only include boundary. B16's
  8/16-bit integer and binary32 `float` rules, plus rejection of `double` and 32-bit integer
  intermediates, are fictional target rules, not general C++ rules. Trace one documented register
  access from a helper into `step()`; no model pass establishes product acceptance.

### 5. XO8 clock, startup, reset, and power

- **Depends on and source:** MCU register knowledge; XO8 and `CLOCK_RESET_CONTRACT.md`. Establish a
  valid clock before interpreting timer or conversion rates.
- **C++ and harness:** initialization order, named constants/enums, bounded polling, explicit error
  returns, and distinguishing reset cause from ordinary state. Existing `clock.startup`,
  `clock.oscillator_startup`, `clock.pll_lock`, `clock.logic_brownout`, and
  `clock.warm_reset_retention` test the model. A learner startup-sequence harness is **proposed, not
  implemented**.
- **Observe and compose:** missing external clock produces a visible startup/reset outcome, not
  nominal operation. Keep drive disabled during power settling, reset, and clock switching; the
  correspondence requires run-clock verification within 100 ms, drive inhibit within 1 ms of
  external-clock loss, and no auto-start when it recovers. Initialize time-based peripherals only
  after clock success. A warm reset does not erase rotor momentum, temperature, or every external
  latch. Check the selected-firmware acceptance case `startup.external_clock` after setup.

### 6. B16 pin routes, GPIO, vectors, and interrupt causes

- **Depends on and source:** MCU and B16 follow-on pin/vector tables; a known profile and clock.
  Configure routes before enabling a peripheral/vector.
- **C++ and harness:** masks/shifts, `constexpr` pin maps, function pointers in
  `firmware::vectors`, one owner for shared state, and ordered acknowledgement of peripheral and
  controller flags. B16 has a sixth DMAC vector even if this exercise does not use DMA. Existing
  `b16.peripherals`, `reference.irq_w1c`, `irq_mask`, `irq_dispatch`, `irq_priority`, and
  `manual.handler` are platform evidence; a small GPIO/vector learner harness is **proposed, not
  implemented**. The bench connects PWM to PB5, tach to PB1, LCD VBLANK to PB6, and the AVT10 input
  to PA4/AN0. DREQ pads PA6/PB7 have no chassis request source.
- **Observe and compose:** configure while peripherals are inactive: unlock with PIN_KEY `0x5A`
  then `0xA5`, set directions/routes, and lock before enabling. The intended IRQ cause clears once
  without immediate retrigger; acknowledge the device cause before its IRQ flag, including DMAC
  terminal status before vector bit 5. Route one input and one output, then exercise IRQ dispatch;
  an interrupt alone must not enable output.

### 7. Timer, tick scheduling, and bounded foreground work

- **Depends on and source:** MCU timer notes and `CLOCK_RESET_CONTRACT.md`; valid clock and clear
  pin/vector ownership. Establish a tick before using elapsed time for sampling, debounce, or
  control.
- **C++ and harness:** integer period/count arithmetic, named units, bounded functions, and avoiding
  long blocking work. MCU prescalers are divisors 1, 8, 64, and 256; disable a timer before changing
  prescaler/compare, stage compare low then commit with high, and account for every control write
  resetting count/divider. Existing `reference.timer_period`, `timer_prescale`, `timer_latch`,
  `timer_write_order`, `timer_one_shot`, `clock.timer_frequency`, and `clock.change_busy_timer` are
  model tests. A learner tick/period harness is **proposed, not implemented**.
- **Observe and compose:** check the expected period and that a clock change while the timer is busy
  is rejected. Use one tick for one bounded sample or display update, then rerun the named timer
  tests before combining more work.

### 8. MX8-1 mux, BA-8 buttons, and STOP/jar interlocks

- **Depends on and source:** MUX8 and BUTTON_ASSEMBLY plus correspondence safety requirements;
  depends on the pin routes. Mux select, settle, and sample are separate actions. RUN_PERMIT/STOP
  and the chassis jar latch are independent hardware-model inhibits.
- **C++ and harness:** arrays, bounds checks, debounce state, explicit “no command” values, and
  separating command from permission. `b16.panel` is the existing learner scaffold for contact to
  speed mapping only; it does not scan/debounce. Existing `reference.mux_select`, `mux_settle`,
  `button_interlock`, `button_bounce`, `pulse_stop`, and `platform.jar_*` test the model. A combined
  learner input harness is **proposed, not implemented**.
- **Observe and compose:** opening STOP/jar cuts motion immediately; release/reseat does not
  auto-restart. Parse only a stable command while retaining independent inhibits. Later check
  `accept --case stop.cutoff_and_release` and `accept --case jar.removal`; the untouched starter
  should fail product acceptance.

### 9. AVT10 and ADC sampling

- **Depends on and source:** TEMPERATURE_SENSOR and MCU ADC notes; valid clock, analog pin route, and
  timer/sample timing. Keep acquisition separate from the step 1 conversion.
- **C++ and harness:** units, validity, finite-width results, freshness, and reporting stale/invalid
  readings without treating them as a temperature. `b16.adc-ready`, `b16.adc-pending`, and
  `b16.adc-status-flags` use a scripted host fake to verify READY handling, result byte order, and
  visible flags; they do not simulate conversion time. `b16.temperature` remains a pure nominal
  conversion TODO. `reference.adc_*`, `reference.sensor_linear`, `sensor_lag`, `sensor_frozen`, and
  `platform.adc_probe_channel` test the model. A timed acquisition/freshness harness is still
  **proposed, not implemented**. At default divide-by-four, acquisition samples after 16 us and
  conversion completes after 52 us.
- **Observe and compose:** take completed samples at least every 10 ms. Read `ADC_DATA_LO` before
  `ADC_DATA_HI`: the low-byte read captures high bits and acknowledges READY; a high-first or repeat
  read is not a fresh conversion. The fake trace demonstrates this transaction order. Track
  completion time, overruns, and distinct ERROR/OVERRUN W1C flags. Compare valid conversion with
  stale/frozen, open/grounded/railed sensor, and fault behavior in the model; the fake helper alone
  cannot see those conditions. Connect a valid sample to conversion, then pass validity explicitly
  into control; check `accept --case thermal.trip` only once selected firmware implements that
  behavior.

### 10. MD20 motor, PWM, and tachometer

- **Depends on and source:** MOTOR and MCU timer/pin notes; valid command, PWM route, timer, and tach
  route. Requested duty remains separate from output permission.
- **C++ and harness:** curve inversion, units, range checks, sample counters, and comparing
  requested with measured speed. `b16.motor` tests only the pure positive-target curve; existing
  `reference.motor_curve`, `pwm_endpoints`, `motor_load`, `motor_coast`, `tach_count`, `tach_wrap`,
  and `tach_no_motion_signature` exercise model behavior. A learner PWM/tach integration harness is
  **proposed, not implemented**. At 1 MHz PBCLK, PWM is 3,906.25 Hz; duty writes are shadowed to a
  carrier boundary (codes 0-254 map to code/256; 255 is full-on), but clearing PWM enable drives
  the output low immediately. Tach is a 16-bit
  wrapping count of two rising edges per revolution; read low before high for a coherent pair.
- **Observe and compose:** inspect ramp-up, coast-down, load/jam, and missing-motion signatures;
  tach-only evidence cannot distinguish a jam from missing/disconnected tach. Those model checks do
  not prove learner code drives the plant correctly. Later connect command -> PWM -> tach as
  separate stages after input and safety permission; `accept --case panel.speed_1` and
  `thermal.trip` intentionally fail on the untouched starter.

### 11. PX32-16 LCD and XBUS

- **Depends on and source:** LCD32 and B16 bus/pin notes; pin routes and, for synchronized updates,
  timer/VBLANK ownership. Display reporting must not control motion.
- **C++ and harness:** `std::array`, loops, masks, packed pixels, byte order, bounded buffers, and
  bounded busy polling. The 32x16 panel has 64-byte vertical-page VRAM; a DATA write occupies BUSY
  for 8 us, auto-increment wraps at byte 63, and each frame scans over 20 ms with a 4 ms VBLANK.
  XBUS register selection and data access share state; give multi-access display operations one
  owner or protect them from interrupt interleaving. Existing `reference.lcd_pixels`, `lcd_busy`,
  `lcd_wrap`, `lcd_vblank`,
  `lcd.bus_observations`, `manual.external`, and `manual.pair` are model/example checks. A learner
  byte-pack/XBUS harness is **proposed, not implemented**.
- **Observe and compose:** check pixel/byte placement, wrap, and busy behavior in existing tests.
  Preview scanned pixels, not just raw VRAM; writes during BUSY are dropped and set ERROR, and
  mid-frame writes can tear. Render one small status field after reading controls and verify display
  changes cannot assert motor output; preserve the product starter's one-column display TODO.

### 12. Fault state, safe shutdown, and subsystem composition

- **Depends on and source:** correspondence, BUTTON_ASSEMBLY, TEMPERATURE_SENSOR, MOTOR,
  `CLOCK_RESET_CONTRACT.md`, and `BOARD_WIRING.md`; individual input, sensor, motor, and clock
  lessons. STOP and jar remain independent hardware protections; firmware still applies its own
  checks.
- **C++ and harness:** enums/state machines, transition tables, explicit valid vs stale/fault data,
  and “cut off, latch, require a fresh command.” Existing `platform.jar_*`, thermal, power-retention,
  scenario, and named acceptance checks are model or selected-image tests. A learner
  state-transition harness is **proposed, not implemented**.
- **Observe and compose:** immediate independent cutoff, no automatic restart after release/reseat,
  and visible failure/reset cause are expected. STOP must remove the firmware enable request, PWM
  enable, and pending drive within 10 ms. A jar opening clears permission immediately; rearm requires
  stable closure, a fresh STOP acknowledgment, released controls, and a later new command.
  Temperature must be sampled at least every 10 ms; two fresh plausible readings at/above 85 C trip
  TH, implausible rails or over 100 ms without a fresh result trip TS, and both shut down. A frozen
  but plausible sensor output is not guaranteed to be detected by reading it repeatedly. At
  startup/recovery, fresh plausible readings at/below 65 C must remain qualified for two seconds;
  cooling alone never clears a latched fault. Integrate one input, sensor, output, and fault
  transition at a time; retain every cause and the documented display priority
  `CK > WD > DM > TS > TH > IF > ST`. Verify with named `accept --case ...` and workbench traces. Do
  not weaken protections to pass.

### 13. Watchdog and DMT supervision

- **Depends on and source:** B16 follow-on D1 plus `SUPERVISION.md`; bounded foreground loop and
  understood reset causes. The elapsed-time watchdog and foreground-progress DMT are different
  protections. Configure and lock both before drive; correspondence sets a maximum 250 ms watchdog
  timeout and 100 ms DMT expiry at the slowest approved clock. D1 is an optional watchdog-fuse
  setting for development; production keeps the watchdog fused on.
- **C++ and harness:** bounded work, service points, monotonic counters/epochs, and not mistaking an
  interrupt for foreground progress. Existing `supervision.development_options`, `clock.wdt_*`, and
  `clock.dmt_*` are configuration/model checks. A learner service-epoch harness is **proposed, not
  implemented**.
- **Observe and compose:** a stuck foreground produces the documented supervision outcome; blind
  timer feeding does not hide it. First prove bounded loop progress, then add each supervisor and
  inspect the reset cause.

### 14. B16 SRAM and DMAC extension (advanced and optional)

- **Depends on and source:** B16 follow-on; take this after CPU-driven bus/display and interrupt
  ownership. CPU writes are sufficient for the core LCD exercise; DMA is not required for the first
  working loop. The addendum provides 1 KiB logical SRAM and one DMA channel with software, timer,
  DREQ, and VBLANK requests/pacing. DMA cells remain byte-wide; DREQ pads are low-biased and have no
  chassis request source. Select routes only while peripherals are disabled, then lock the pin
  configuration.
- **C++ and harness:** bounded memory ranges, 16-bit addresses/counts, ownership, pair writes, and
  asynchronous completion state. `b16.peripherals` checks platform assumptions. No learner DMA/SRAM
  harness exists; a bounded transfer/range-check harness is **proposed, not implemented**. SRAM
  addresses are logical B16 device addresses, never native or Wasm pointers.
- **Observe and compose:** require an in-range transfer and one completion cause; reject invalid
  routing/range according to the selected exercise. Add DMA only after the equivalent CPU transfer
  is observable, and keep completion/error handling separate from display/control state.

### 15. Integration, observation, and replay

- **Depends on:** the previous basic rungs—clock, routes, stable controls, fresh sensing, output
  mapping, display, independent protection, and bounded supervision. SRAM/DMAC remains optional.
- **Harness and observation:** `accept --case ...` runs the selected `b8_emulator` image. Workbench
  **RUN** advances logical time; inspect GPIO/PWM requests, tach/physics, ADC completion, reset
  cause, and scanned LCD pixels. Platform `scenario.*` CTests use the separate fixed starter fixture
  and are conformance/replay evidence, not selected-image acceptance. Run them with
  `ctest --test-dir build/astin-b16 -R '^scenario\.' --output-on-failure`. The ten timestamped
  JSON scenarios and deterministic seeded stress run are documented in `TESTING.md`.
- **Compose and expected result:** establish a normal-run precondition before checking a fault, then
  replay the same input and inspect recorded samples/outcomes. A model scenario may pass while the
  selected application fails; acceptance failures such as `panel.speed_1`, `jar.removal`,
  `thermal.trip`, or `stop.cutoff_and_release` remain visible until Astin's implementation passes.
  Do not count untouched starter/model tests as completed product behaviors.

To check selected firmware, name one behavior and keep its result separate from model tests. For
example:

```sh
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 accept --case panel.speed_1
```

This runs the actual selected B16 image. `panel.speed_1` should fail on the untouched starter; it
passes only after that image produces the documented command and measured response. Other useful
cases include `startup.external_clock`, `stop.cutoff_and_release`, `jar.removal`, and
`thermal.trip`. An unrun case remains unverified.

The BA-8 manual's RUN_PERMIT/STOP contact and the chassis jar latch are independent hardware-model
inhibits. Keep their immediate cutoff and no-automatic-restart behavior intact. Firmware must still
apply its own software checks. The three-function sandbox above is not a safety controller, and a
passing sandbox test never authorizes a physical output.

After this path, [the B8 firmware lab](FIRMWARE-LAB.md) is a later stretch exercise. B8's 8-bit
numeric restrictions are teaching rules for that fictional profile, not general C++ rules; B16
allows 8/16-bit integer operations and binary32 `float`, while its checker still rejects `double`
and 32-bit integer intermediates in selected firmware.

## Evidence labels

- **Learner scaffolding:** `platform/labs/b16/` contains TODO functions and small expected-value
  tests; it is not a completed solution.
- **Model harnesses:** `platform/tests/` and the simulator exercise platform behavior. Their
  successful result is not learner-firmware acceptance.
- **Supplied starter:** `platform/firmware/firmware.cpp` is incomplete and should keep the drive
  off at reset.
- **Worked reference:** the complete controller and additional course material are private owner
  work on the separate solution branch. They are not part of the learner route.
- **Unverified behavior:** any requirement without a passing named selected-firmware acceptance
  result remains unverified. A model test, TODO exercise, or old build record does not change that.
