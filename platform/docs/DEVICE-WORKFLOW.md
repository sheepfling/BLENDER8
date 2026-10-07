# Choose, compile and run B8 or B16

Use the [New Employee route](../../NEW-EMPLOYEE-START.md) to install the Python tooling and generate
the manuals. Rick's handoff offers either MCU. The accepted customer behavior is the same for both.

| Choice | Firmware arithmetic                  | Peripheral interface                              | Build folders                          |
| ------ | ------------------------------------ | ------------------------------------------------- | -------------------------------------- |
| B8     | 8-bit integers; no float             | Interface 03, five vectors                        | `build/native/`, `build/wasm/`         |
| B16    | 8/16-bit integers and binary32 float | Interface 04, SRAM, DMA, pin routing, six vectors | `build/native-b16/`, `build/wasm-b16/` |

Neither firmware profile permits doubles or integer scalars/intermediates of 32 bits or more.
The source checker runs before linking. The simulator's physics uses its own host numeric types.
The `--numeric-profile` spelling remains an alias for `--device`; it now selects the matching
behavioral device too. These are C++ peripheral models, not instruction-set interpreters.

## Compile native firmware and C++ tests

From the repository root, with the Python environment activated:

```sh
python tools/project.py native --root . --device B8
python tools/project.py native --root . --device B16
python platform/tools/b8.py --root platform --build-dir ../build/native test
python platform/tools/b8.py --root platform --build-dir ../build/native-b16 test
```

Run the selected emulator interactively:

```sh
python tools/project.py run --root . --device B8
python tools/project.py run --root . --device B16
```

Type `run 100000`, `speed 4`, `snapshot`, or `quit`; each command returns JSON. `--bench` permits
explicit register writes without running firmware. The hello response identifies the actual device
and interface. Executables have the same names in separate directories, including on Windows.
The default firmware is unfinished and keeps the motor off. Passing platform tests does not mean
that the assignment is complete.

For a separate firmware directory, use the lower-level Python entry point with an explicit build
folder, for example:

```sh
python platform/tools/b8.py --root platform --build-dir ../build/x build --device B16 --firmware ../fw
```

## B16 headers and chassis connections

Include the shared `blender8/firmware.hpp`, `device.hpp`, `registers.hpp`, `numeric.hpp`, and
optionally
`access.hpp`. B16 adds `blender8/b16.hpp`. Its SRAM addresses are logical device addresses, never
native or Wasm pointers. B16's vector order is T0, T1, TACH, VBLANK, ADC, DMAC. Provide the sixth
handler even if it only acknowledges unused DMA causes. Clear DMA terminal status before clearing
IRQ controller bit 5; a still-enabled peripheral cause reasserts on the next PB clock.

| B16 bench wire       | Pad and route            | Startup direction/analog setting                    |
| -------------------- | ------------------------ | --------------------------------------------------- |
| Motor PWM            | PB5, PWM route 2         | PB5 output                                          |
| Motor tach           | PB1, TACH route 1        | PB1 input                                           |
| LCD VBLANK           | PB6, VBLANK route 1      | PB6 input                                           |
| Temperature AN0      | PA4, fixed ADC channel 0 | PA4 input, ANSELA bit 4 set                         |
| Spare AN1            | PA5, fixed ADC channel 1 | PA5 input, ANSELA bit 5 set                         |
| External DMA request | PA6 or PB7               | No chassis source; low bias until externally driven |

The remaining customer connections keep the r4 chassis wiring. B8 retains its dedicated PWM
connection. On B16, configuring PWM on PA4 does not move the motor wire away from PB5. Digital
routing resets disconnected. Disable PWM and ADC, leave DMA idle, write PIN_KEY 0x5A then 0xA5
consecutively, establish OUT and DIR, select routes, check PIN_STATUS, and write PIN_LOCK = 1
before enabling peripherals. The B16 follow-on is the register-level source for exact behavior.

## Compile WebAssembly and load it into the page

With Emscripten active in the same terminal:

```sh
python tools/project.py wasm --root . --device B8
python tools/project.py wasm --root . --device B16
python tools/project.py wasm-test --root . --device B8
python tools/project.py wasm-test --root . --device B16
python tools/project.py serve-wasm --root . --device B8 --port 8089
```

Either compiled site can load either device. In the page:

1. Select **Selected firmware** for your firmware, or **Component bench** for manual experiments.
2. Select B8 or B16 to match your compiled image.
3. Choose **both** `b8_student.wasm` and `b8_student.mjs` from that device's `build/.../web/`
folder.
4. Click **Load image**. The page shows the actual MCU, image name and SHA-256 fingerprint.
5. Click **Test fresh image** to recreate the module and execute 100 ms of logical machine time.
6. Save the JSON test result, exercise the controls, and save the input journal.

The `.wasm` contains the selected firmware, emulator, physics and C++ renderer. The matching
Emscripten `.mjs` supplies its runtime imports; files from different builds must not be mixed.
See the [Emscripten Wasm runtime documentation](https://emscripten.org/docs/compiling/WebAssembly.html).
The file chooser sends bytes directly to a fresh local worker; the static server receives no upload.
Restart retains the chosen local image while the page remains open. Reloading the page clears it.
A wrong MCU choice, invalid binary or unsupported ABI produces an explicit error. An incompatible
loader fails initialization; always choose the matching pair from one build. The page also exposes
the complete test result JSON for copying when browser downloads are unavailable.
A blocked callback terminates the worker after the host timeout; it is not reported as an MCU reset.

**Test fresh image** is an execution smoke check with image identity and initial/final observations.
It does not claim customer acceptance. The legacy B8 CTest scenarios run on the fixed B8 starter
fixture, including when built beside B16. B16 peripheral conformance and selected-device parity
checks exercise B16 separately. Run the existing acceptance/scenario tools against your
selected native executable and inspect traces for the accepted requirements. The compiled Wasm
checks compare native and Wasm observations and exercise real C++ rendering. B16 peripheral tests
also execute in Wasm. Binary32 checks qualify observable arithmetic on the compiler target; there
is no assigned FPU instruction latency, opcode execution or hardware measurement.

## Firmware memory tiers

Select `--memory-profile standard`, `plus`, or `free` on native and Wasm build commands.
The default is standard. Both devices retain their numeric restrictions in free mode.

| Device | Standard RAM / program | Plus RAM / program | Free      |
| ------ | ---------------------- | ------------------ | --------- |
| B8     | 2 KiB / 16 KiB         | 4 KiB / 32 KiB     | Unlimited |
| B16    | 8 KiB / 64 KiB         | 16 KiB / 128 KiB   | Unlimited |

Capacities are configurable in `platform/memory-profiles.json`. CMake also accepts
`-DB8_MEMORY_PROFILE=plus` and an explicit `-DB8_SIZE_TOOL=/path/to/llvm-size`.
Each build writes `firmware-memory.json` in its build directory. Program usage is object
text plus initialized data; RAM usage is initialized data plus BSS. B16 also reserves its
existing 1 KiB DMA SRAM within the RAM budget; its register map does not change.

These are isolated firmware object section budgets, not physical MCU flash measurements.
Runtime stack, heap and externally linked helper code are not measured or bounded yet.
A passing report therefore does not prove a hard total RAM limit. Different compiler targets
produce different section sizes. Emulator, physics, renderer and trusted host code retain their
existing full precision and host memory allocations, including the larger Wasm runtime memory.

The page offers memory tier and warn/strict/off controls. Strict rejects missing reports and
measured overages before execution; warn displays them. Free disables capacity limits while
retaining the accounting boundary warning. The selected firmware Wasm contains a custom memory
report section. It is self-declared metadata, not authenticated proof; manually altered images
can lie. Whole Wasm download size is a separate host safety cap, never MCU program usage.
Probe and fixed starter images currently have no firmware memory report and warn by default.
