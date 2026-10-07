# Half-A/Labs B8 development platform

## Platform 0.6.0 • B8 register interface 03 • chassis 04 • C++20

The New Employee writes firmware against the B8 register interface. This package supplies the board,
component models, animated C++ workbench, deterministic scenario runner, conformance and stress tests,
customer-acceptance harness. The supplied firmware is deliberately unfinished and drive-off. A
passing hardware test suite is not a completed blender.

## Build and operate

For setup through PDF, native and WebAssembly builds, use [New Employee's start guide](../NEW-EMPLOYEE-START.md).
The [firmware lab](docs/FIRMWARE-LAB.md) then walks through the register exercises. The editable
starter is in [`firmware/`](firmware/README.md); the pixel probe is a separate, drive-off
demonstration.

Requirements: a C++20 compiler, CMake 3.20 or newer, Python 3.12 or newer, and a browser. The native
targets have no downloaded C++ dependencies. The workbench and test runners use Python's standard
library. No LaTeX, document-rendering packages, GPU, server account, or hosted CI is needed for code
work.

From this directory:

```sh
python tools/b8.py doctor
python tools/b8.py build
python tools/b8.py test
python tools/b8.py serve --probe --open
```

The tools use the current directory as the default platform root. From elsewhere, pass
`--root /absolute/path/to/BLENDER8/platform` before the action. Relative build directories, firmware
paths, scenario files, and report paths are resolved from that selected root. CMake also passes the
root to its Python checks explicitly.

The last command runs the **pixel probe**, not finished product firmware. Press a speed button and
watch the actual scanned LCD pixels. Its motor stays off. Close with Ctrl-C. Serve defaults to
loopback port 8088; change it with `--port`.

To run the New Employee firmware instead:

```sh
python tools/b8.py serve --open
# Or select an external directory of .cpp firmware files at build time:
python tools/b8.py --build-dir build-new-employee build --firmware ../new-employee-firmware
python tools/b8.py --build-dir build-new-employee serve --open
```

Firmware supplies `firmware::reset()`, bounded `firmware::step()`, and `firmware::vectors`; include
`blender8/firmware.hpp`. `sdk/include` and the selected firmware directory are its only project
include paths. No simulator target is linked while compiling the firmware object. The lexical
dependency checker is an engineering guardrail, not a sandbox for untrusted native code.

With a multi-configuration generator, direct commands need `--config Release` and
`ctest -C Release`. The Python wrapper handles that. Select a different compiler in a **fresh**
build directory:

```sh
python tools/b8.py --build-dir build-clang build --compiler clang++
python tools/b8.py --build-dir build-asan build --compiler clang++ --sanitize
```

## WebAssembly workbench

With Emscripten active, build the real C++ machine, firmware, and renderer as WebAssembly:

```sh
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-test --native-build build
python tools/b8.py wasm-serve --probe --open
```

The page is `build-wasm/site/index.html`. Its source lives in `ui/` and `webview/`; `view/` owns
the scene, hit testing, and RGBA pixels. The Python server delivers static files and executes no
simulation commands. The fixed hang fixture is built only for tests and excluded from the site.
See [WASM.md](docs/WASM.md) for direct CMake and publication details.

The same C++ scene runs natively:

```sh
python tools/b8.py animated --probe --open
```

The animated page shows real scanned LCD pixels from the compiled probe. The probe keeps drive
off. Bench mode offers a finite motor demonstration, recorded in its journal, without supplying
finished product firmware. See [ANIMATED-WORKBENCH.md](docs/ANIMATED-WORKBENCH.md).

## Four distinct ways to run

| Mode                  | Firmware                 | Permitted stimuli                                    | Meaning                                                   |
| --------------------- | ------------------------ | ---------------------------------------------------- | --------------------------------------------------------- |
| `b8_emulator`         | Selected firmware        | Physical controls/faults; no register pokes          | New Employee's normal development target                  |
| `b8_probe_emulator`   | Pixel bring-up probe     | Same physical controls                               | Build/display demonstration, not a product solution       |
| `b8_emulator --bench` | None                     | Physical controls and explicit `write address value` | Device and plant inspection; no implicit watchdog feeding |
| `b8_starter_emulator` | Fixed unfinished fixture | Normal controls                                      | Harness self-tests independent of selected firmware       |

The browser provides seven latching speed buttons, held PULSE and STOP, rear power, jar seating,
load, jam, core/clock/foreground failures, sensor and feedback faults, a pixel display, and
host-only signal/physics observations. It never passes those observations into firmware. The command
console is a fixture console, not a firmware back door.

## Tests and evidence

```sh
python tools/b8.py scenario scenarios/conformance/jar_dropout_memory.json --output reports/jar.json
python tools/b8.py stress --seed 4815 --episodes 128 --actions 256
python tools/b8.py accept --output reports/acceptance.json
```

**The final command is expected to fail on the unfinished starter.** It tests current outward
customer behavior, not a secretly supplied solution. Missing approved display templates and semantic
source reviews report `review`, never automatic success. `accepted` is true only for a full-suite
run with all tests and required reviews satisfied. A selected passing subset is not full acceptance.

See [TESTING.md](docs/TESTING.md), [ACCEPTANCE.md](docs/ACCEPTANCE.md), and the case inventory in
`spec/acceptance_inventory.json`. Customer decisions are preserved in
`requirements/correspondence.md` and `.json`. Retired teaching requirements and scenarios now live
under `../internal/history/platform/`; they are outside the current grading contract.

## Where the code lives

`SDK`: `sdk/include/blender8/{registers,device,access,firmware}.hpp`.

`Host binding`: `host/include/blender8/host/backend.hpp` and `host/src/backend.cpp`.

`Component contracts`: `sim/include/blender8/sim/components.hpp`; replace one or several factory
slots when constructing `Board`. Physics, digital nets, analog nodes and time remain outside the
SDK.

`Emulator`: `sim/src/runtime.cpp`, `sim/src/fixture.cpp`, `apps/emulator.cpp`, `tools/b8client.py`,
`tools/emulator.py`, and `ui/index.html`.

`Animated scene`: `view/`, `apps/view_host.cpp`, `tools/animated_server.py`, and `webview/`.

`WebAssembly bridge and static site`: `wasm/`, `cmake/B8Wasm.cmake`, `tools/wasm_site.py`,
`tools/wasm_server.py`, and `ui/wasm.html`.

[ARCHITECTURE.md](docs/ARCHITECTURE.md) documents ownership, timing and reset behavior.
[COMPONENTS-AND-HWIL.md](docs/COMPONENTS-AND-HWIL.md) gives tested substitution examples and the
physical-HWIL boundary. [PROTOCOL.md](docs/PROTOCOL.md) describes commands and traces.

## Important limits

The jar dropout latch and PB3/PB4 connections are now implemented; the prior chassis-03 executable
is no longer the default target. This is a logical, fixed-step plant/peripheral simulation, not
transistor-level or instruction-set emulation. A native callback that never returns and never calls
the SDK can stop logical time; the outer process deadline reports a **host callback timeout**, not a
watchdog reset. A separate modeled core-halt fixture lets independent hardware time continue and
really tests WDT behavior.

The transport adapter is tested with a lockstep loopback, **not physical hardware**. It refuses
unpaced wall-clock transports. No physical HWIL driver, brake, guard lock, fuse sizing, safety
certification or single-fault tolerance is supplied. A single jar loop cannot detect a welded or
bypassed contact. The application still has to clear its drive requests, display the reason and
enforce deliberate restart.
