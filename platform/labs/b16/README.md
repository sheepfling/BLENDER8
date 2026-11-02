# B16 subsystem practice harness

This small native C++20 project is a learner sandbox for Astin. It compiles without building the
PDFs, simulator, WebAssembly site, or private teaching material. It contains **three TODO
exercises**, not a firmware solution:

1. Convert one valid AVT10 ADC reading to a nominal temperature estimate.
2. Convert a positive MD20 target RPM to a PWM duty fraction using the documented no-load curve.
3. Map a mux contact index to a speed number while keeping C7 (PULSE) distinct.

It also contains the first **register-driver harness**: `adc_sample.cpp` reads one completed
10-bit result through the SDK's `read8` interface. `fake_hardware.cpp` provides a strict scripted
`RegisterBackend` and transaction trace. This is a small driver example, not an ADC conversion
engine or temperature/safety policy. The driver helpers compile as their own target with only the
SDK include path; a separate host support target supplies the fake binding. The temperature
conversion remains a learner TODO.

Build the lab and run the ADC transaction harness from the repository root:

```sh
cmake -S platform/labs/b16 -B build/astin-b16-labs -DCMAKE_BUILD_TYPE=Debug
cmake --build build/astin-b16-labs
ctest --test-dir build/astin-b16-labs -R '^b16\.(adc|fake)' --output-on-failure
ctest --test-dir build/astin-b16-labs -R '^b16\.(adc|fake)' --verbose
```

All four commands should pass. The verbose form prints the deterministic register trace. Then run
the temperature conversion exercise:

```sh
ctest --test-dir build/astin-b16-labs -R '^b16\.temperature$' --output-on-failure
```

This test is **expected to fail** until the temperature TODO is implemented; its failure names the
first sample that differs. Repeat with `^b16\.motor$` and `^b16\.panel$` as those exercises are
reached. Run all lab checks with `ctest --test-dir build/astin-b16-labs --output-on-failure`.

For a ready ADC result, the trace must show status, low byte, then latched high byte:

```text
#00 @0us READ ADC_STATUS[0x0093] -> 0x02
#01 @1us READ ADC_DATA_LO[0x0094] -> 0xE9
#02 @2us READ ADC_DATA_HI[0x0095] -> 0x00
```

The fake requires explicit read scripts, fails on an unexpected extra read, and records each bus
transaction and `idle()` service boundary. Its displayed microseconds count SDK service operations;
the fake does not wait for an ADC conversion or implement W1C, sampling, electrical, or timing
behavior. A failed assertion prints the trace to help locate the first unexpected operation. The
ADC reader assumes one owner for status and the low/high pair; it does not mask interrupts or make
the pair atomic.

## Harness progression

1. **ADC result read — implemented example.** Script READY/BUSY and a latched 10-bit value; practice
   status checks, byte order, and trace reading. Tests cover ready, pending, ERROR/OVERRUN visibility,
   transaction order, and rejected overreads.
2. **Temperature conversion — TODO exercise.** Convert the acquired code into a nominal estimate;
   practice numeric types and tolerances. `b16.temperature` is expected to fail until implemented.
   Simulator sensor/ADC CTests verify device behavior separately.
3. **Startup and routes — proposed harness.** Trace safe reset/clock setup, power state, GPIO, and
   B16 pin routing before enabling peripherals. Existing clock, power-retention, pin-route, and reset
   model tests are separate evidence.
4. **Basic controls — pure mapping TODO exists.** Map mux contacts with bounds checks and keep
   no-command, STOP, and PULSE distinct. Existing model CTests cover mux selection/settling, buttons,
   bounce, and jar/STOP interlocks.
5. **Timers and IRQs — proposed harnesses.** Trace prescaler/compare writes and source-specific
   acknowledge order; practice vector tables and shared-state ownership. Existing timer and IRQ model
   tests cover timing, priority, flags, and dispatch. B16 adds DMAC as vector six.
6. **Timed controls — proposed register harness.** Script mux/GPIO service intervals and study stable
   sampling, debounce, and rearm. Model tests own elapsed-time bounce and independent hardware gates.
7. **Sensor acquisition — proposed timed harness.** Add conversion timing, freshness, and separate
   ERROR/OVERRUN handling around the result reader. Existing ADC/sensor model CTests cover conversion
   timing, latching, faults, and freshness-related behavior.
8. **Motor and tach — pure curve TODO plus proposed register harness.** Convert positive RPM to duty,
   then trace PWM writes and tach byte-pair reads. Platform model tests cover PWM, motor load/coast,
   and tach behavior.
9. **LCD and XBUS — proposed harness.** Trace register-select/data writes and status reads; then study
   busy, VRAM, and VBLANK update boundaries. Existing model tests cover pixels, wrapping, VBLANK, and
   bus observations.
10. **Supervision — proposed harness.** Exercise watchdog key order, DMT windows, reset causes, and
    foreground progress independently of timer IRQs. Simulator tests own logical time, reset, WDT,
    and DMT conformance.
11. **B16 SRAM/DMA — advanced proposed harness.** Trace descriptor writes, DREQ routing, completion
    status, and DMAC acknowledgement; B16 model tests remain the peripheral evidence.
12. **Product composition — not implemented by these labs.** Combine commands, sensors, independent
    inhibits, outputs, and supervision using selected-firmware acceptance and scenario replay. Keep
    B8 as later stretch work.

Never read a fake-harness pass as simulator conformance or product acceptance. The fake checks the
firmware's register transaction logic; platform CTests check the modeled device; selected-firmware
acceptance checks the assembled application.

The temperature samples come from the [AVT10 manual](../../docs/datasheets/TEMPERATURE_SENSOR.md).
The duty curve and samples come from the [MD20 manual](../../docs/datasheets/MOTOR.md). Contact
identity comes from the [BA-8 manual](../../docs/datasheets/BUTTON_ASSEMBLY.md). The focused
platform model checks are useful next: `reference.motor_curve`, `reference.pwm_endpoints`,
`reference.mux_select`, `reference.button_interlock`, `reference.button_bounce`, and
`reference.pulse_stop`.

The three pure-function exercises have no MMIO, simulator APIs, driver enable, safety decision,
sampling timer, debounce state, or interrupt handler. The separate ADC slice uses only the SDK
register calls behind a host-only scripted backend. Neither harness models ADC conversion timing,
physical sensor behavior, or the platform's independent STOP/jar inhibit. Fresh-sample policy,
startup/rearm behavior, watchdog and deadman semantics remain part of the full firmware design;
these exercises must never substitute for the hardware inhibit.

For the actual B16 target, later compile and run the selected firmware using the
[device workflow](../../docs/DEVICE-WORKFLOW.md). That target applies the repository's B16 numeric
checker; this isolated host harness does not. B16's byte/word/binary32 rules are constraints of this
fictional target and its checker, not general C++ rules. Keep the exercises in this sandbox until
each is understood and covered by a test.
