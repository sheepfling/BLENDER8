# Half-A/Labs / B8 — WebAssembly route

B8 interface 03 or B16 interface 04 · chassis 04 · host bridge ABI 1.

## Animated application added in 0.6.0

The default static entry now uses `view/` (C++ layout, controls, animations and RGBA renderer) and
`webview/` (thin worker/canvas shell). The original workbench is preserved at `diagnostics.html`.
The machine ABI remains 1; the additive scene ABI is 1. `b8_view` links into each real Emscripten
image. No JS scene renderer or replacement physics is supplied. See
[ANIMATED-WORKBENCH.md](ANIMATED-WORKBENCH.md) for exact state ownership, operation and limits, and
[local build verification](../../docs/BUILD-VERIFICATION.md) for this checkout. The 0.5.0 and
0.6.0 validation records describe their original authoring environments.

## Status of this delivery

The source route, CMake targets, shared session, C bridge, worker transport, C++ scene,
OffscreenCanvas shell, static-site packager and validation commands are implemented. Compiled B8
`.wasm` and Emscripten `.mjs` binaries are generated locally and ignored. This integrated checkout
was built with Emscripten 6.0.10: compiled ABI, worker, scene, and native-versus-Wasm parity checks
passed. The original 0.6.0 delivery lacked that toolchain; its historical record is
[VALIDATION-0.6.0.md](VALIDATION-0.6.0.md). Browser navigation remains a separate check.

The JavaScript unit tests use clearly labeled module test doubles to test worker messaging and
termination. They do not implement or validate the C++ plant in WebAssembly. The separate
compiled-Wasm tests require real compiler outputs and fail when those outputs are missing; they
never substitute those doubles.

## Architecture

```text
New Employee's firmware sources + B8 register/runtime/component sources
                 |
          shared C++ Session
           /               
  native compiler      Emscripten
        |                   |
  native process     b8_student.mjs + b8_student.wasm
        |                   |
 loopback HTTP       browser module Worker
        |                   |
  NativeTransport       WasmTransport
           \               /
         shared workbench / real scanned LCD pixels
```

The browser route compiles the existing C++ machine, not a second JavaScript model of the physics or
controller. Firmware continues to include only the B8 SDK. The host-only `Session`, JSON interface
and C exports never enter its include path. The C++ source remains the native testing/HWIL route;
the browser is another execution target.

The selected firmware build runs the [B8/B16 numeric gate](B16-FOLLOW-ON.md) with the Emscripten
driver before compiling. B8 is the default. `--device B16` selects the expanded device and
its matching numeric policy. See [the two-device workflow](DEVICE-WORKFLOW.md).

There is no B8 instruction-set emulation, ROM loader, compiler inside the page, physical hardware
transport, or automatic native fallback. Change firmware locally, rebuild, then restart the browser
session, or load its matching `.wasm` and `.mjs` pair with **Load image**. The local file chooser
sends no files to a server. **Test fresh image** records a deterministic 100 ms execution smoke
check and offers a JSON result with the image hash; product acceptance remains separate. Each
module owns one machine and one firmware-global state. Independent workers have
independent module instances.

## 1. Install and activate Emscripten

Build prerequisites: CMake 3.20+, Python 3.12+, Emscripten/emsdk and its Node runtime; a native
C++20 compiler is also required for native parity testing. The helper uses no pip/npm packages. A
modern browser supporting WebAssembly and module workers is needed for the panel.

`wasm/emsdk-version.txt` records **6.0.11** as the original documented starting version. This
checkout passed its compiled checks with **6.0.10**; 6.0.11 remains unverified here. Record the
version that passes your local build and tests.

Install the SDK outside the project (shell example):

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 6.0.11
./emsdk activate 6.0.11
source ./emsdk_env.sh
emcc --version
emcmake --help
```

On Windows, use `emsdk.bat install 6.0.11`, `emsdk.bat activate 6.0.11`, then `emsdk_env.bat` in the
same Command Prompt used for the build. SDK installation needs network access and downloads
substantial compiler assets. The project does not install or activate a toolchain silently.

## 2. Compile and open the probe

Run these commands from `platform/`, the directory containing `CMakeLists.txt` and `tools/b8.py`.

```sh
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-serve --probe --open
```

The default Wasm build directory is `build-wasm/`. After a successful build, the second command
serves `build-wasm/site/` on loopback port 8088. It serves static assets only: **no native
simulation executable is launched**. Close with Ctrl-C.

The pixel probe drives the real simulated LCD through B8 register transactions. Its motor stays off.
The selected-firmware mode uses the unfinished starter until New Employee supplies his code:

```sh
python tools/b8.py wasm-serve --open
python tools/b8.py wasm-serve --bench --open
```

The first opens selected firmware; the second opens an explicit no-firmware component bench. The
bench permits register writes but does not automatically feed the watchdog. Query-string choices are
`?mode=probe`, `?mode=student`, and `?mode=bench`; changing mode creates a fresh page/session.
Direct static entry defaults to the probe.

Do not open `index.html` using `file://`. Use an HTTP static server or HTTPS static hosting. The
supplied server sets the Wasm/JavaScript MIME types, validates its asset manifest, snapshots the
complete release at startup, and permits no command POST endpoint. After rebuilding assets, restart
that server so it loads the new release.

### Select New Employee's source directory

```sh
python tools/b8.py --build-dir build-wasm-new-employee wasm-build --firmware ../new-employee-firmware
python tools/b8.py --build-dir build-wasm-new-employee wasm-serve --open
```

The chosen directory contains the project's firmware entry points and `.cpp` sources. It is selected
at compile time, not uploaded into a running page. The fixed probe and fixed starter are separate
build images. The helper explicitly selects the default source when `--firmware` is omitted rather
than inheriting an unnoticed candidate from an old cache.

### Direct CMake equivalent

```sh
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DB8_WASM_TEST_FIXTURES=ON
cmake --build build-wasm --config Release --parallel 4
```

Use separate build directories for native and Wasm compilers. The launcher rejects a detected
mixed-toolchain cache.

## 3. Build outputs and publication

```text
build-wasm/web/                  Compiler outputs
  b8_student.mjs + .wasm         Selected firmware + C++ machine
  b8_probe.mjs + .wasm           Pixel probe + same machine
  b8_starter.mjs + .wasm         Fixed starter used by parity checks
  b8_hang.mjs + .wasm            ONLY with --test-fixtures; do not publish
build-wasm/site/                 Allowlisted static release
  index.html                    Animated C++ scene
  diagnostics.html              Original register-centric UI
  scene-shell.mjs
  scene-worker.mjs
  view-client.mjs
  wasm-main.mjs
  b8-worker.mjs
  worker-host.mjs
  core-client.mjs
  transport.mjs
  workbench.mjs
  b8_student.mjs + .wasm
  b8_probe.mjs + .wasm
  b8_starter.mjs + .wasm
  build-info.json
```

The site packager validates actual Wasm headers and fails if compiler outputs are absent. It records
every published asset's SHA-256/size and the compiler version. It excludes source files, native
binaries, test fixtures, caches, and the deliberate hang image. Publish only `site/`, not the
repository or `web/`. No hosted service is deployed by these commands.

The worker loads its local module pair. The standard modularized Emscripten factory supplies the
runtime; no Embind, Asyncify, pthreads, SharedArrayBuffer, or browser filesystem is used.
`-fexceptions` is set for compilation **and linking**, because C++ hardware-reset unwinding depends
on catches. Disabling exception handling changes runtime behavior and is not an equivalent build.
Current memory settings are 32 MiB initial, 256 MiB maximum with growth, and a 1 MiB stack.

## 4. Host ABI and worker execution

`wasm/api.h` defines five host exports:

| Export                               | Meaning                                                                                      |
| ------------------------------------ | -------------------------------------------------------------------------------------------- |
| `b8_wasm_abi()`                      | Returns ABI 1.                                                                               |
| `b8_wasm_init(mode)`                 | Creates one session: selected firmware, chassis-04 bench, or legacy-02 bench for regression. |
| `b8_wasm_hello()`                    | Current protocol/firmware identity and non-invasive state snapshot.                          |
| `b8_wasm_command(text, byte_length)` | Executes one existing line-protocol command; returns JSON.                                   |
| `b8_wasm_dispose()`                  | Removes observers and inhibits/disposes the host machine.                                    |

Returned C strings belong to the module and remain valid only until the next ABI call. `ccall`
converts/copies them immediately. No C++ class layout, heap view, or firmware-global pointer crosses
into the page.

Requests carry monotonic sequence IDs and execute serially. The original diagnostic UI sends bounded
10,000-microsecond run chunks. The animated C++ scene caps each paced frame at 20,000 logical
microseconds and owns its own stepping; see the scene guide. The simulation's time remains logical
time, not `Date.now()` or animation-frame time. Browser speed changes responsiveness, not modeled
peripheral frequencies. A hidden tab pauses automatic stepping; resuming does not catch up to
elapsed wall time. In-flight work is not an electrical STOP.

Input limits remain one command of at most 4,096 UTF-8 bytes. Browser scheduled timestamps are
restricted to exactly representable JavaScript integers. Trace responses retain the native limits
(at most 1,000 samples and 10,000 transition observations). Avoid pathological traces: they can use
substantial memory.

### Watchdog versus a stuck host

A simulated core-halt/clock-loss injection still advances modeled hardware time and can produce a
real emulated watchdog reset. In contrast, a C++ callback that loops forever without returning or
using the SDK blocks the worker's event loop. The page remains independent, but cannot interrupt
that C++ callback cooperatively.

After a 10-second command deadline (30 seconds for initialization), the host terminates the worker
and marks **WASM_HOST_TIMEOUT**. It rejects queued commands, does not invent final state or a WDT
reason, and does not resume automatically. An explicit **Restart browser session** creates a fresh
worker/module. That recreates the entire machine; it is different from the normal `reset` fixture
command, which applies B8 reset/retention behavior to the existing machine. Worker termination
cannot run destructors. This browser route therefore must not be treated as a physical HWIL shutdown
mechanism.

A time-consuming healthy job can also hit a host deadline on a slow device. Use shorter commands;
never reinterpret that result as successful simulated protection.

## 5. Verification on a machine with Emscripten

Build the native reference too, then run the actual-Wasm checks:

```sh
python tools/b8.py build
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-test --native-build build
```

`wasm-test` first runs the six C-ABI checks as compiled Wasm through Node. It then compares real
Wasm against the fixed native starter/probe: ten existing conformance scenarios, two seeded
128-action physical trajectories, and fifteen probe commands. Discrete state, GPIO, timestamps,
counters and pixels compare exactly. Only specified continuous numeric fields have narrow tolerances
(relative 1e-9, absolute 1e-7).

Five additional real compiled-scene tests cover RGBA output, non-invasive render/resize, real probe
pixels, held-PULSE/STOP behavior and independent scene modules. These are required in addition to
native/Wasm model parity.

The final three Node worker tests load real compiler outputs: actual scanned probe pixels, isolated
module instances, and a deliberately nonreturning compiled C++ callback. The hang image is mandatory
for this gate but never copied into the static release. A missing module fails the command instead
of skipping or running a JavaScript substitute. Success writes `reports/wasm-parity.json` with
module hashes. This is platform equivalence, **not product acceptance of New Employee's unfinished
firmware**.

The optional `python tests/browser/animated_smoke.py --site build-wasm/site` gate requires
Playwright and a real compiled site. It was not run here.

Then inspect the real browser route: confirm `WASM / FIRMWARE EXECUTION`, select the probe, step
time, press a speed and see the LCD pixels change; confirm no motor drive, inspect mode restart
versus MCU reset, and review the console for load/runtime errors. The browser network tab should
show only the static local assets—no `/command` requests. Browser-specific smoke testing remains
required even after Node parity passes.

Native regression remains:

```sh
python tools/b8.py test
python tools/b8.py stress --seed 4815 --episodes 128 --actions 256
python tools/b8.py accept --output reports/acceptance.json
```

The supplied starter is expected to fail firmware acceptance. Native sanitizers and actual HWIL
remain native responsibilities; no browser cross-compilation turns the lockstep test adapter into
physical hardware.

## Official technical references

Consulted 2026-10-07; these explain tool behavior, not proof that this project's uncompiled target
works:

- Emscripten CMake integration and output:
  <https://emscripten.org/docs/compiling/Building-Projects.html>
- Modularized ES-module factories: <https://emscripten.org/docs/compiling/Modularized-Output.html>
- Explicit exception support: <https://emscripten.org/docs/porting/exceptions.html>
- SDK installation: <https://emscripten.org/docs/getting_started/downloads.html>
- Worker termination (no cleanup opportunity):
  <https://developer.mozilla.org/en-US/docs/Web/API/Worker/terminate>
