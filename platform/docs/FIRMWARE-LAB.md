# B8 firmware lab

For one setup route covering Python tools, PDFs, C++ tests, firmware headers and WebAssembly, start
with [New Employee's guide](../../NEW-EMPLOYEE-START.md). This page continues with hands-on firmware
exercises from the `platform/` directory.

This is the hands-on route for writing firmware for the fictional B8 blender. The editable image is
[`firmware/firmware.cpp`](../firmware/firmware.cpp). It starts with the motor off and the product
behavior unfinished, so every new behavior comes from the engineer's code.

The board model uses interface 03 and chassis 04. Firmware sees ordered eight-bit register
transactions through the SDK. It cannot read simulator objects, host time, motor truth, or fixture
state. The animated workbench shows what the simulated hardware does with those transactions.

## First session

Install Python 3.12+, CMake 3.20+, and a C++20 compiler. GCC/MSVC builds also need LLVM Clang for
the numeric-policy check. Run these commands from `platform/` with
your chosen Python environment active:

```text
python tools/b8.py doctor
python tools/b8.py --build-dir build-lab build
python tools/b8.py --build-dir build-lab test
python tools/b8.py --build-dir build-lab animated --open
```

The doctor prints the compiler and tool paths it actually found. `build-lab/` is ignored; the
firmware source is not. The native suite checks the hardware model and tools. It does not certify
the unfinished firmware. In the animated page, click RUN to advance the logical clock. The starter
should keep drive disabled even when a speed is selected. Try the jar, STOP, power switch, and
chassis view; compare physical contacts with register and LCD observations.

For a known visual reference, run the fixed pixel probe in the same build:

```text
python tools/b8.py --build-dir build-lab animated --probe --open
```

The probe is deliberately drive-off. It reads muxed contacts through B8 registers and writes LCD
pixels through the external byte bus. It demonstrates the path from code to pins to pixels, not a
finished appliance controller. Its source is
[`examples/pixel_probe/firmware.cpp`](../examples/pixel_probe/firmware.cpp).

From another directory, pass an explicit platform root before the action. For example:

```text
python /absolute/path/to/BLENDER8/platform/tools/b8.py \
  --root /absolute/path/to/BLENDER8/platform doctor
```

No script infers the repository root from its own file location. On Windows, use the Python
executable in `.venv\Scripts\`; on macOS/Linux, use `.venv/bin/python` if the environment is not
activated.

## Edit, build, observe

1. Edit `firmware/firmware.cpp` and keep `firmware/firmware.hpp` plus
   `firmware/board_config.hpp` beside it. `reset()` establishes safe outputs; `step()` performs
   bounded foreground work; `vectors` holds interrupt callbacks.
2. Rebuild with `python tools/b8.py --build-dir build-lab build`.
3. Open `python tools/b8.py --build-dir build-lab animated --open` to run the selected image. Each
   process starts a fresh simulated machine; reloading a page is not a firmware update.
4. Use `python tools/b8.py --build-dir build-lab accept --case panel.speed_1` when that behavior is
   implemented. A passing subset is only a local diagnostic. A full run without `--case` is the
   customer-behavior check and will initially fail on the safe starter.
5. Keep the source, command, selected executable, report, and relevant trace together when asking
   for review. Reports can be written under ignored `reports/`.

Compile commands are emitted at `build-lab/compile_commands.json` for editors that support them.
The selected firmware object is compiled with only `sdk/include/` and its own source directory.
The build and boundary checker reject simulator or host headers in that target; this is a code
boundary, not a security sandbox for untrusted C++.

The selected image uses the strict B8 numeric profile by default: byte integers, explicit byte
pairs for wider peripheral values, and no float. The
[B8/B16 follow-on guide](B16-FOLLOW-ON.md) explains `numeric.hpp` helpers and promoted intermediates.
Fixed probes and model tests use host-side fixture code; their wider convenience values are not
examples of firmware accepted by the strict B8 checker.

## Suggested learning path

| Lab              | Change in the editable firmware                                      | What to inspect                                                    |
| ---------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------ |
| 0: safe reset    | Trace GPIOB output and PWM reset state. Keep drive off.              | Reset, power settling, and the independent jar gate.               |
| 1: one pixel     | Send LCD commands through XBUS and write `0x81` to one column.       | The two lit bits and byte/column orientation.                      |
| 2: real contacts | Configure GPIOA, scan the mux, and debounce stable selections.       | Contact bounce, multiple indications, PULSE, and STOP.             |
| 3: timed output  | Choose legal clock/timer settings; request PWM only with permission. | PB clock, duty shadow, run-up, coastdown, and tach count.          |
| 4: protection    | Sample ADC, classify faults, and require deliberate rearm.           | Freshness, hot motor, jar dropout, reset causes, WDT/DMT progress. |
| 5: explain it    | Draw labels on the LCD and run full acceptance.                      | Source-to-observation trace, remaining failures, human reviews.    |

The [register manual](datasheets/MCU_B8.md), [board wiring](BOARD_WIRING.md), and
[current correspondence](../requirements/correspondence.md) are the source material. Use
[TESTING.md](TESTING.md) to distinguish conformance, stress, and
acceptance. Historical firmware requirements are useful background; the correspondence is the
current customer contract.

Start with one observable change at a time. For example, write one LCD byte before implementing a
font, or record one mux contact before adding debounce. The simulator's bench mode can expose
component behavior, but firmware mode cannot make raw register pokes. STOP can remove drive while
the rotor coasts; zero RPM is a separate physical observation.

## Optional WebAssembly page

With Emscripten and Node installed, compile the selected C++ image and the C++ renderer into a
static browser page:

```text
python tools/b8.py --build-dir build-wasm-lab wasm-build --test-fixtures
python tools/b8.py --build-dir build-wasm-lab wasm-test --native-build build-lab
python tools/b8.py --build-dir build-wasm-lab wasm-serve --open
```

The compiled parity gate checks the platform against fixed native fixtures; it does not prove the
student's product logic meets the correspondence. Rebuild and restart the browser session after
changing firmware. The selected image, fixed pixel probe, and test-only hang image remain separate.
See [WASM.md](WASM.md) for target and publication detail.

No result in this lab establishes physical hardware behavior or appliance safety. There is no
physical B8 board in this repository.
