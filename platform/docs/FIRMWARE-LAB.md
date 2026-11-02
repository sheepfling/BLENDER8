# B8 firmware lab

This page is the later B8 stretch path. For Astin's first C++ route, begin with the
[B16-first firmware lab](B16-FIRMWARE-LAB.md) and its isolated subsystem harness.

For one setup route covering Python tools, PDFs, C++ tests, firmware headers and WebAssembly, start
with [New Employee's guide](../../NEW-EMPLOYEE-START.md). This page continues with hands-on firmware
exercises from the `platform/` directory.

This is the hands-on route for writing firmware for the fictional B8 blender. The editable image is
[`firmware/firmware.cpp`](../firmware/firmware.cpp). It starts with the motor off and the product
behavior unfinished, so every new behavior comes from the engineer's code.

The board model uses interface 03 and chassis 04. Firmware sees ordered eight-bit register
transactions through the SDK. It cannot read simulator objects, host time, motor truth, or fixture
state. The animated workbench shows what the simulated hardware does with those transactions.

## Read the starter before editing

`firmware.cpp` is the learner's unfinished TODO file. The supplied `firmware.hpp` declares the
entry points, and `board_config.hpp` holds wiring constants. In `namespace firmware`, `reset()`
initializes safe outputs, `step()` is called for bounded foreground work, and `vectors` holds
interrupt callbacks. A `b8::write8` call is one ordered byte write to an SDK register. Start by
following those calls; you do not need to add classes or redesign the starter to complete the early
labs.

The fixed `tests/fixtures/safe-starter/` is test scaffolding, not the learner file; see its
[fixture note](../tests/fixtures/safe-starter/README.md). The separate
`examples/pixel_probe/firmware.cpp` is a drive-off display reference that demonstrates the LCD bus;
it is not a product controller or a solution to the exercise. The `TODO` comments in the editable
starter remain assignments. The worked teaching firmware is kept on the separate solution branch.

## Which harness to use

The repository has several different kinds of checks. Keep their result scopes separate while
learning:

| Harness                                                 | What it exercises                                                                                                                                             | What a pass means                                                                                                                                    |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| CMake build                                             | The selected `firmware/` image, C++20 warnings, SDK boundary, numeric profile, and memory budget.                                                             | This image compiles and fits the selected fictional target profile.                                                                                  |
| Native CTest (`tools/b8.py --build-dir build-lab test`) | Existing register, peripheral, clock, simulator, view, protocol, and tool cases. `firmware.safe_starter` uses the fixed `tests/fixtures/safe-starter/` image. | The modeled platform and developer tools pass their checks. It does not test your new product behavior.                                              |
| `animated --open`                                       | The selected firmware running in the interactive workbench.                                                                                                   | You can inspect contacts, registers, LCD pixels, motor motion, and subsystem state. This is an observation surface, not an automated pass/fail test. |
| Selected acceptance (`accept --case <case>`)            | The selected firmware against one named customer behavior, such as `startup.external_clock`, `panel.speed_1`, or `jar.removal`.                               | That named behavior passed for this run. Other behavior is still unverified.                                                                         |
| Full acceptance (`accept`)                              | The selected firmware against the current correspondence.                                                                                                     | The firmware passed the listed customer checks; it still does not establish physical hardware behavior.                                              |

The CTest binaries include small model checks such as `lcd.bus_observations`, `clock.*`, and
`platform.*`. Those are harnesses for the simulated hardware, not for your firmware. The named
acceptance cases do run your image, but they are black-box customer checks rather than unit tests
for each lab step. There is not currently a separate automated learner-firmware harness for the
one-column pixel exercise; use the workbench observation specified below. No new expected product
behavior is hidden in the model tests.

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
pairs for wider peripheral values, and no float. This is a teaching constraint of the fictional B8
target and its checker, not a general rule of C++; ordinary C++ compilers support wider integer and
floating-point types. The
[B8/B16 follow-on guide](B16-FOLLOW-ON.md) explains `numeric.hpp` helpers and promoted intermediates.
Fixed probes and model tests use host-side fixture code; their wider convenience values are not
examples of firmware accepted by the strict B8 checker.

## Suggested learning path

Each stage introduces one piece of C++ and one subsystem. Reuse the same short native loop from
the first session after each edit: build, CTest, then run `animated --open` and observe the selected
firmware. CTest should remain green, but only an acceptance case that names the behavior is an
automated check of that firmware behavior.

| Stage            | C++ idea                                                                                                                                 | Small learner step in the editable firmware                                                                                      | Existing harness and success signal                                                                                                                                                                                                                                                                    |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 0: safe reset    | Translation units, headers, function calls, and initialized state. Trace `reset()`, `step()`, and the vector table before changing code. | Keep the supplied reset path safe; identify where GPIOB and PWM are set.                                                         | Run `ctest --test-dir build-lab -R firmware.safe_starter --output-on-failure`; this checks the immutable fixture. In `animated --open`, RUN the selected starter and choose a speed: drive should remain off.                                                                                          |
| 1: one pixel     | Byte values, masks, and ordered device writes.                                                                                           | Follow the LCD lab to send commands through XBUS and write `0x81` to one column.                                                 | Run `ctest --test-dir build-lab -R 'reference.lcd_pixels|lcd.bus_observations' --output-on-failure` for the model. Inspect the selected image in the workbench: exactly two expected pixels should appear and the motor should stay off. No dedicated automated pixel-exercise case exists.            |
| 2: real contacts | `if` statements, masks, persistent state, and a small debounce state machine.                                                            | Start with one GPIOA/mux input, then extend the scan to stable selections.                                                       | Run `ctest --test-dir build-lab -R 'reference.mux_select|platform.jar_gpio' --output-on-failure` for the model. Change one contact at a time in the workbench; verify the mapped observation changes without inventing additional presses. These CTests do not test learner firmware.                  |
| 3: timed output  | Bounded work in `step()`, clock/timer setup, and interrupt callbacks from `vectors`.                                                     | Observe legal clock startup before adding timer work; request PWM only after the required permission conditions are established. | Run `ctest --test-dir build-lab -R '^clock\.(startup|timer_frequency)$' --output-on-failure` for the clock model. Then use `accept --case startup.external_clock` and `accept --case panel.speed_1` on the selected image. Pass requires the agreed clock and speed, with no output before permission. |
| 4: protection    | Functions that combine fresh input samples, fault state, and explicit recovery.                                                          | Add one sensor or jar fault path at a time; keep shutdown and rearm behavior observable.                                         | Run `ctest --test-dir build-lab -R 'reference.adc_conversion|supervision.development_options|platform.jar_open_cutoff' --output-on-failure` for the model. After normal drive works, inject one fault in the workbench and run `accept --case jar.removal` or `accept --case thermal.trip`.            |
| 5: composition   | Small functions composed into one controlled loop, with traceable state transitions.                                                     | Combine panel, timed output, interlock, sensor, and display work; draw status labels only after the underlying state is correct. | Run `python tools/b8.py --build-dir build-lab accept` on the selected image and inspect the report. Full acceptance is expected to fail on the untouched safe starter; keep each unresolved case visible rather than weakening its check.                                                              |

The lab stages are exercises, not a second firmware implementation. The selected CTest suite
already has platform conformance coverage, while the one-pixel exercise currently relies on visual
inspection. Add a narrow selected-firmware test only when an exercise needs an objective signal the
existing acceptance cases and workbench cannot provide; keep its expected behavior to that single
exercise.

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
