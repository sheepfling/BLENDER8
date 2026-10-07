# New Employee's MCU firmware route

Run these steps from the Blender-8 repository root. This is the developer route for the fictional
B8 interface 03 or B16 interface 04, chassis 04. The supplied firmware is a safe, unfinished
starter; platform tests
do not mean that its customer behavior is complete.

## 1. Install the tools

Install Python 3.12 or newer, CMake 3.20 or newer, and a C++20 compiler. LLVM Clang supplies the
firmware numeric check when compiling with GCC or MSVC. PDF generation also needs Tectonic or
`latexmk` with `pdflatex`. The browser build additionally needs Emscripten (`emcmake`
and `em++`) and Node; follow the [Emscripten
setup](platform/docs/WASM.md#1-install-and-activate-emscripten).

The Python bootstrap creates `.venv/` and installs this checkout plus its development tools from
`pyproject.toml`. The first installation needs access to your configured Python package index. On
macOS or Linux:

```sh
python3.12 tools/bootstrap.py --root .
source .venv/bin/activate
```

On Windows PowerShell:

```powershell
py -3.12 tools\bootstrap.py --root .
.venv\Scripts\Activate.ps1
```

Use the activated environment's `python` in the commands below. Activation is optional if you call
`.venv/bin/python` on macOS/Linux or `.venv\Scripts\python.exe` on Windows. The Python scripts use
the working directory as their default root; pass `--root` when running from somewhere else. They
do not infer a root from their own file path.

```sh
python tools/project.py doctor --root .
```

The doctor reports the Python, LaTeX, CMake, C++ and optional Emscripten tools it can find. The
Python environment is ignored by Git and can be recreated by rerunning the bootstrap.

## 2. Read the contract and choose the work area

Read the [accepted customer correspondence](platform/requirements/correspondence.md), the
[approved board wiring](platform/docs/BOARD_WIRING.md), and the finished supplier handouts generated
in `dist/recipient/`. The final accepted messages and current r4 drawings govern customer
behavior. In particular, two speed indications persisting for 40 ms stop drive and show `IF`;
STOP overrides a concurrent speed or PULSE request.

Edit `platform/firmware/firmware.cpp` and local `.hpp` files. Read
`platform/firmware/board_config.hpp` for chassis wiring constants; update it only with an approved
wiring change. Include the public interface from `platform/sdk/include/blender8/`.

Leave the maintainer and historical material under `internal/` out of the exercise:

- `internal/owner/` contains the semantic crosswalk, editorial decisions, and owner interpretation.
  Do not consult or deliver it with the New Employee packet.
- `internal/authoring/` and `internal/history/` contain the PDF construction inputs, controlled
  original snapshots, retired teaching requirements, and earlier revisions. They are maintainer
  material, not a second contract.

Build and run the tools in `platform/sim/`, `platform/host/`, `platform/view/`, `platform/wasm/`,
and `platform/tests/`. Their headers and private observations are not firmware interfaces.

The local `internal/owner/private/` list is Git-ignored. If a New Employee works in the same
checkout as the owner, it remains owner material, not an extra requirement. The test harness may
inspect the simulated machine; firmware may only use B8 registers and its own state.

## 3. Use the firmware headers

The B8 firmware API uses `.hpp` files, not `.h` files. These are the project headers available to
the firmware target:

| Include                             | Purpose                                                                          |
| ----------------------------------- | -------------------------------------------------------------------------------- |
| `#include "blender8/firmware.hpp"`  | Declares `firmware::reset()`, `firmware::step()` and the interrupt vector table. |
| `#include "blender8/device.hpp"`    | Declares ordered `read8`, `write8` and `idle` operations.                        |
| `#include "blender8/registers.hpp"` | Defines register addresses, interrupt names and masks.                           |
| `#include "blender8/access.hpp"`    | Optional helpers for latched register pairs and interrupt acknowledgment.        |
| `#include "blender8/numeric.hpp"`   | Width-preserving integer arithmetic for the selected B8/B16 numeric profile.     |
| `#include "blender8/b16.hpp"`       | Added B16 SRAM, DMA and pin-selection addresses.                                 |
| `#include "board_config.hpp"`       | Local chassis pin and channel constants beside the firmware source.              |

See the [SDK header map](platform/sdk/README.md) for this interface in one place.
`platform/firmware/firmware.hpp` is the local include shim for the SDK entry-point header. CMake
compiles the selected firmware with only `platform/sdk/include/` and its own source directory on
its project include path. `platform/wasm/api.h` and `platform/wasm/view_api.h` are host bridge
headers, not firmware interfaces. The [firmware lab](platform/docs/FIRMWARE-LAB.md) walks through
small register and LCD exercises.

The build checks selected firmware with the B8 numeric profile: 8-bit integer data and operations,
with no floating point. Use `numeric.hpp` helpers when C++ would promote an expression to a wider
`int`. For 10-bit ADC and 16-bit timer values, use explicit low/high bytes and carry/borrow.
The `access.hpp` helpers that return `uint16_t` are outside the strict B8 profile; its interrupt
acknowledgment helper remains available. The [B8/B16 guide](platform/docs/B16-FOLLOW-ON.md)
explains the numeric profiles, examples and enforcement limits.

## 4. Generate the manuals and build native C++

```sh
python tools/project.py docs --root .
python tools/project.py native --root .
python tools/project.py test --root .
```

`docs` compiles pristine, searchable PDFs from LaTeX under `build/docs/clean/`. It then saves the
cumulative first and second copier generations under `build/docs/g1/` and `build/docs/g2/` for
inspection. The finished third generation goes under `dist/field/`; the seven finished handouts
are in `dist/recipient/`, with a G3 binder at `dist/Blender8-Supplier-Manuals-G3.pdf`. The build
folder also holds handout and binder previews of the pristine, G1 and G2 stages. These generated
files are Git-ignored. The repository command supplies the LaTeX search paths; the standalone
editor preview cannot resolve this multi-file project on its own.

The command also generates the [B8/B16 follow-on manual](platform/docs/B16-FOLLOW-ON.md) separately
under `dist/field/`. Choose B8 or B16; both have native and Wasm behavioral models. B16 adds SRAM,
DMA, pin selection, six vectors and binary32 arithmetic. The seven shared handouts and their binder
retain their existing membership. Follow [the two-device workflow](platform/docs/DEVICE-WORKFLOW.md)
for the pin wiring, exact build directories and local image upload.

Add `--device B16` to `native`, `test`, `wasm`, `wasm-test` and `serve-wasm` for B16. The default
is B8. B16 builds use `build/native-b16/` and `build/wasm-b16/`; the numeric policy always matches
the selected device.

`native` builds the C++ emulator, simulator, renderer and unit-test executables under
`build/native/`. `test` rebuilds as needed, checks the document tooling with Python tests, and runs
native CTest. To rerun only the C++/platform tests after a native build, use:

```sh
python platform/tools/b8.py --root platform --build-dir ../build/native test
```

The safe starter should pass platform self-tests while failing many customer-acceptance cases. Run
acceptance on the selected firmware after implementing a behavior; a passing subset is
only a local diagnostic.

## 5. Build and serve the C++ WebAssembly page

Activate the Emscripten environment in the same terminal as the Python environment, then run:

```sh
python tools/project.py wasm --root .
python tools/project.py wasm-test --root .
python tools/project.py serve-wasm --root . --port 8089
```

`wasm` compiles the real C++ B8 machine, selected firmware and C++ scene renderer with Emscripten.
The static page is `build/wasm/site/index.html`; the serve command prints the loopback URL. Open
that URL in a browser. Choose the matching `.wasm` and `.mjs` from either build's `web/` directory,
select its MCU and click **Load image**. **Test fresh image** recreates the module and runs a
100 ms execution smoke check; **Save test result** records the image hash and observations.
Use the controls to exercise the loaded firmware and **Save journal** for its input history.
Do not open the HTML through `file://`. Stop the server with Ctrl-C and
restart it after rebuilding so it serves the new asset manifest.

`wasm-test` runs compiled Wasm ABI and worker tests plus native/Wasm parity. It requires the native
build from step 4. A browser page rendering correctly, platform parity passing, and product
firmware acceptance are three separate results. For custom firmware directories and direct CMake
commands, see the [Wasm guide](platform/docs/WASM.md).

## Watchdog and deadman experiments

The workbench's **Supervision** tab (keyboard **U**) shows monitor enablement, timing and reboot
reasons. See [firmware supervision options](platform/docs/SUPERVISION.md) for the production fuse
and optional development fuse, native/Wasm build commands, firmware controls and reset history.
