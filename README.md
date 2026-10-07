# Blender-8 teaching and solution workspace

This is **`feature/b16-solution`**. Start with the [worked B16 teaching route](TEACHING-SOLUTION.md)
for the explicit solution build, code walkthrough, experiments and requirements discussion.
This branch stays outside `main`. The problem presentation lives on `feature/b8-b16-workbench`.

Half-A/Labs maintains a fictional B8 firmware exercise, its logical C++ device model, and a
LaTeX document foundry. The selectable MCU models are B8 interface 03 and B16 interface 04, on
chassis 04. The supplied
firmware is an unfinished safe starter. Simulation and browser builds do not establish physical
hardware behavior or customer acceptance.

`Half-A/Labs` is the visible wordmark. Existing ASCII archive identifiers retain `HalfALabs`
because `/` separates paths in ZIP files.

Start with the [New Employee route](NEW-EMPLOYEE-START.md) to install the Python tools, generate
the PDF packet, compile C++ tests, and build and serve the WebAssembly page. The
[documentation index](docs/INDEX.md) links the current guides.

See [branch purposes and update direction](docs/BRANCHES.md).

| Location                                                              | Role                                                                   |
| --------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| `platform/sdk/include/blender8/`                                      | Public B8 headers available to firmware                                |
| `platform/firmware/`                                                  | New Employee's editable firmware and chassis constants                 |
| `platform/requirements/`                                              | Accepted customer correspondence used for the exercise                 |
| `platform/sim/`, `platform/host/`, `platform/view/`, `platform/wasm/` | Device model, host binding, renderer, and browser bridge               |
| `platform/ui/`, `platform/webview/`                                   | HTML and JavaScript front ends                                         |
| `latex/`                                                              | Editable LaTeX manuals and correspondence layouts                      |
| `src/b8docs/`, `tools/`, `scripts/`                                   | Python foundry and cross-platform build commands                       |
| [`internal/`](internal/README.md)                                     | Authoring inputs, owner interpretation, provenance, and legacy records |
| `build/`, `dist/`, `generated/`                                       | Ignored build products                                                 |

The open LaTeX source tree stays at `latex/` so the document editor can keep its current file open.
The authoring catalogs, mail source, recipes, controlled original snapshots, and semantic crosswalk
are under `internal/`. That directory is for maintainers; it is outside the New Employee reading
route. `platform/requirements/` and the finished PDFs are the recipient contract.

## Build from the repository root

Install Python 3.12+, CMake 3.20+, a C++20 compiler, and Tectonic or `latexmk` with `pdflatex`.
LLVM Clang is required for the firmware numeric check when using GCC or MSVC as the compiler.
Emscripten (`emcmake` and `em++`) and Node are needed for the browser target. Then run:

```sh
python3.12 tools/bootstrap.py --root .
```

Activate `.venv` or call its Python executable, then run:

```text
python tools/project.py doctor --root .
python tools/project.py docs --root .
python tools/project.py native --root .
python tools/project.py test --root .
python tools/project.py wasm --root .
python tools/project.py wasm-test --root .
python tools/project.py serve-wasm --root . --port 8089
```

On Windows, start with `py -3.12 tools\bootstrap.py --root .` and activate
`.venv\Scripts\Activate.ps1`. The Python commands accept an explicit `--root`; none infer the
checkout from their own file path. Run `python -m scripts.ci --static` for Ruff, strict Pyright,
rumdl, mdrepo, and pre-commit configuration checks.

`docs` stores pristine PDFs and the cumulative G1 and G2 copies under `build/docs/`, then saves
the final G3 field copies, seven recipient handouts, and binder under `dist/`. Generated outputs
are ignored by Git. The original intake ZIPs remain in ignored
`.local/source-archives/original-intake/`; there is no active `INTAKE/` directory.

The [B8/B16 follow-on manual](platform/docs/B16-FOLLOW-ON.md) defines the two numeric profiles.
The selected firmware build uses B8 by default; use `--device B16` for the expanded MCU and
matching language subset. See [build, run and local Wasm
upload](platform/docs/DEVICE-WORKFLOW.md). B16 permits 16-bit integers and binary32 float;
neither profile permits doubles
or 32-bit integers. B16 implements its expanded peripheral contract in native and Wasm models;
no instruction ISA or FPU instruction timing is assigned.

The native and browser targets compile the C++ model. Firmware can include the B8 SDK and its own
files; host observations and simulator internals remain outside its interface. See
[platform architecture](platform/docs/ARCHITECTURE.md) and
[local build verification](docs/BUILD-VERIFICATION.md) for the exact evidence boundaries.
