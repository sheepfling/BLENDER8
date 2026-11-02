# New Employee's MCU firmware route

Run these steps from the Blender-8 repository root. This is the developer route for the fictional
B8 interface 03 or B16 interface 04, chassis 04. The supplied firmware is a safe, unfinished
starter; platform tests
do not mean that its customer behavior is complete.

Use the problem presentation branch, `feature/b8-b16-workbench`, for the exercise. The separate
`feature/b16-solution` branch contains the worked teaching example. See
[branch purposes](docs/BRANCHES.md) before switching between them.

## 1. Install the tools

For the native C++ route, install Python 3.12 or newer, CMake 3.20 or newer, a C++20 compiler,
LLVM `clang++`, and `llvm-size` or the platform's `size` utility. CMake requires Clang for the
firmware numeric-width check and a size tool for the firmware memory report, even when your main
compiler is GCC or MSVC. PDF generation uses Tectonic or `latexmk` with `pdflatex`; those tools are
not needed for the first native build. The optional browser build also needs Emscripten (`emcmake`
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

The root doctor reports native, PDF and optional Emscripten tools. For a focused native check, use
`python platform/tools/b8.py --root platform doctor`; `native_ready` is true only when CMake, a C++
compiler, Clang and the firmware size tool are all available. The Python environment is ignored by
Git and can be recreated by rerunning the bootstrap.

### First native C++ session: B16 first

Run the short loop from the repository root. This uses a separate ignored build directory for the
learner image and does not need the PDF or WebAssembly toolchains:

```sh
python platform/tools/b8.py --root platform doctor
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 build --device B16
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 test
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 animated --open
```

Pass expectations: the doctor says `native_ready: true`; the B16 build completes in
`build/astin-b16/`; the platform C++ tests pass; and the animated page opens at the URL it prints.
Click **RUN** to advance simulated time. The unmodified starter keeps the motor off. Stop the page
server with Ctrl-C. This native path does not need PDF generation or Emscripten.

Before editing the application, build the isolated [B16 subsystem practice
harness](platform/labs/b16/README.md):

```sh
cmake -S platform/labs/b16 -B build/astin-b16-labs -DCMAKE_BUILD_TYPE=Debug
cmake --build build/astin-b16-labs
ctest --test-dir build/astin-b16-labs -R '^b16\.(adc|fake)' --output-on-failure
ctest --test-dir build/astin-b16-labs -R '^b16\.temperature$' --output-on-failure
```

The configure/build and five ADC fake tests should pass; `b16.temperature` should initially fail on
the TODO. Run the ADC tests with `--verbose` to see the status/low/high transaction trace. The
scripted host fake checks register sequencing; it does not simulate conversion time or replace the
platform ADC conformance tests. Implement only the temperature conversion in
`platform/labs/b16/student.cpp`, rebuild, and rerun `b16.temperature`. Pass means all three
documented nominal ADC samples are within tolerance. A compiler error fails the build; a mismatched
sample fails that test with expected and actual values. Then continue with the MD20 and BA-8 tests
in the [B16 subsystem walkthrough](platform/docs/B16-FIRMWARE-LAB.md). The practice helpers are not
connected to PWM or safety gates. Keep the supplied product firmware unfinished; its acceptance
suite is expected to fail until the learner implements requirements.

## 2. Read the contract and choose the work area

Read the [accepted customer correspondence](platform/requirements/correspondence.md), the
[approved board wiring](platform/docs/BOARD_WIRING.md), and the finished supplier handouts generated
in `dist/recipient/`. The final accepted messages and current r4 drawings govern customer
behavior. In particular, two speed indications persisting for 40 ms stop drive and show `IF`;
STOP overrides a concurrent speed or PULSE request.

After the isolated exercises, edit `platform/firmware/firmware.cpp` and local `.hpp` files. Read
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
inspect the simulated machine; firmware may only use selected-device SDK registers and its own
state.

## 3. Use the firmware headers

The firmware API uses `.hpp` files, not `.h` files. These are the project headers available to
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

Astin starts with B16, so include `blender8/b16.hpp` only for its added SRAM, DMA, and pin-selection
registers. B16 requires six vector handlers, with DMAC last. See the [SDK header map](platform/sdk/README.md)
for this interface in one place.
`platform/firmware/firmware.hpp` is the local include shim for the SDK entry-point header. CMake
compiles the selected firmware with only `platform/sdk/include/` and its own source directory on
its project include path. `platform/wasm/api.h` and `platform/wasm/view_api.h` are host bridge
headers, not firmware interfaces. The [B16-first firmware lab](platform/docs/B16-FIRMWARE-LAB.md)
walks through small isolated subsystems, then timer, interrupt, and supervision composition.

The B16 firmware checker permits 8/16-bit integers and binary32 `float`; it rejects `double` and
32-bit-or-wider integer variables or intermediates in selected firmware. Use `1.25f`, not `1.25`.
These are constraints of the fictional B16 target and its checker, not general C++ rules. The
standalone host practice harness does not apply this checker. The [B8/B16 guide](platform/docs/B16-FOLLOW-ON.md)
explains the numeric profiles and enforcement limits. B8's stricter byte-only/no-float profile is a
later stretch exercise.

## 4. Generate and inspect the learner manuals

```sh
python tools/project.py docs --root .
```

`docs` compiles pristine, searchable PDFs from LaTeX under `build/docs/clean/`. It then saves the
cumulative first and second copier generations under `build/docs/g1/` and `build/docs/g2/` for
inspection. The finished third generation goes under `dist/field/`; the seven finished handouts
are in `dist/recipient/`, with a G3 binder at `dist/Blender8-Supplier-Manuals-G3.pdf`. The build
folder also holds handout and binder previews of the pristine, G1 and G2 stages. These generated
files are Git-ignored. The repository command supplies the LaTeX search paths; the standalone
editor preview cannot resolve this multi-file project on its own.

The command also generates the optional [B8/B16 follow-on manual](platform/docs/B16-FOLLOW-ON.md)
separately. For Astin, pair its B16 addendum with relevant shared recipient handouts; it is not part
of the seven-handout binder. Clean/G1/G2/G3 copies live under `build/docs/` and `dist/field/`. Give
the learner `dist/recipient/` or its G3 binder, plus only the optional B16 addendum if useful. Other
`dist/field/`, `build/docs/`, and `internal/` files are not the learner packet. None of these PDFs
is needed for the first C++ harness or native build.

Add `--device B16` to `native`, `test`, `wasm`, `wasm-test` and `serve-wasm` for B16. The project
default is B8, so use the option for Astin's route. Project B16 builds use `build/native-b16/` and
`build/wasm-b16/`; the numeric policy always matches the selected device. The dedicated
`build/astin-b16/` directory above is his isolated firmware build.

`native` builds the C++ emulator, simulator, renderer and unit-test executables under
`build/native/`. `test` rebuilds as needed, checks the document tooling with Python tests, and runs
native CTest. To rerun only the C++/platform tests after a native build, use:

```sh
python platform/tools/b8.py --root platform --build-dir ../build/native test
```

The focused C++ test command runs the platform CTest suite and does not require generated manuals:

```sh
python platform/tools/b8.py --root platform --build-dir ../build/astin-b16 test
```

The broader repository gate is `python tools/project.py test --root .`. It also checks generated PDF
examples and the Python/documentation suite, so create the manuals first when the PDF artifacts are
not already present. The safe starter should pass platform self-tests while failing many
customer-acceptance cases. Run acceptance on the selected firmware after implementing a behavior; a
passing subset is only a local diagnostic.

## 5. Build and serve the C++ WebAssembly page

After the native and isolated subsystem loops, activate Emscripten in the same terminal as the
Python environment if the optional browser path is useful. Astin's first edit does not require it.
For B16, run:

```sh
python tools/project.py wasm --root . --device B16
python tools/project.py wasm-test --root . --device B16
python tools/project.py serve-wasm --root . --device B16 --port 8089
```

`wasm` compiles the real C++ B16 machine, selected firmware and C++ scene renderer with Emscripten.
The static page is `build/wasm-b16/site/index.html`; the serve command prints the loopback URL. Open
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
