# HalfALabs / B8 Platform 0.5.0 — release verification

**2026-10-07 · B8 register interface 03 · chassis 04 · host bridge ABI 1. Owner evidence, not
customer firmware acceptance.**

## Critical distinction

**The Emscripten build and actual WebAssembly execution were NOT run. No compiled B8 `.wasm`
binaries are included.** Neither `emcc` nor `emcmake` is installed. SDK/package downloads failed in
this environment. `wasm-build` fails explicitly with the activation instruction; `wasm-test` fails
without a compiler-produced test build. This release delivers a source-level route plus tested
native/shared-host and worker-transport code, not a browser-execution acceptance claim.

The suggested emsdk starting version, 6.0.11, is documented but unverified here. Actual compilation
can still expose compiler/linker or browser-specific issues. Do not release the static site as
validated until the documented actual-Wasm gates pass.

## Implemented changes

- Shared host-only C++ Session used by the native command process and Wasm C bridge; no second
  physics implementation.
- Emscripten CMake targets for selected firmware, pixel probe, fixed starter, and an optional
  excluded-from-publication hang fixture.
- Explicit exception handling, modular ES-module output, ABI 1, bounded memory growth, and no
  virtual filesystem/pthreads/Asyncify.
- Browser module worker with ordered sequence-tagged commands, explicit lifecycle, host
  timeout/termination, and no invented WDT result or automatic restart.
- Shared native/Wasm workbench, static asset manifest/packager/server, Node adapter, and
  compiler-output-dependent parity/worker tests.
- Separate native/Wasm build caches and explicit firmware selection.
- The existing SDK, host binding, firmware, requirements/scenarios, core components and
  model/runtime sources were preserved: **58 existing files byte-identical**. New Session files are
  host adapters, not altered B8 registers or component physics.

## Executed checks

| Check                                                                        | Actual outcome                                                               |
| ---------------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| Original 0.4.0 baseline / GCC                                                | 185/185 CTests passed before refactoring                                     |
| Current 0.5.0 / GCC 14.2 Release                                             | **192/192 passed**                                                           |
| Current 0.5.0 / Clang 17 Release                                             | **192/192 passed**                                                           |
| Clang ASan + UBSan, leak detection; all except the default long stress CTest | **191/191 passed**                                                           |
| Small independent ASan stress                                                | **2 episodes, 2 replays each, 16 actions, 64,000 checked microsteps passed** |
| C bridge compiled natively                                                   | 5 ABI/lifecycle/error/reset checks passed; included in the CTest totals      |
| JavaScript bridge/worker tests under Node 22.16                              | **15 tests passed**, included in one CTest wrapper                           |
| Static-site packaging/manifest/HTTP/comparison tools                         | **7 unittest methods passed**, included in one CTest wrapper                 |
| Pre-refactor versus post-refactor **native** session                         | Ten conformance scenarios and two seeded 128-action trajectories passed      |
| Python and JavaScript syntax                                                 | compileall / node --check passed                                             |
| Fresh extracted source ZIP                                                   | Source hashes verified; new GCC Release build and **192/192 CTests passed**  |
| Actual Emscripten compilation                                                | **NOT RUN — compiler unavailable**                                           |
| Actual compiled-Wasm ABI/parity/hang tests                                   | **NOT RUN — no compiled products**                                           |
| Browser C++/Wasm execution                                                   | **NOT RUN**                                                                  |
| Physical HWIL, Windows/macOS builds                                          | **NOT RUN**                                                                  |
| Finished firmware/customer acceptance                                        | Not claimed; supplied firmware remains unfinished                            |

The 192 count includes the JS and Python wrappers; do not add the 15 and 7 methods to that count.
The five C-bridge tests are native compilation of the same bridge source, not WebAssembly execution.
The JS worker tests use an explicit module test double inside real Node worker threads, including a
real nonreturning JavaScript callback; that checks timeout/termination and ordering, not compiled
C++ behavior. Temporary empty Wasm-header fixtures test file handling only and are never delivered
as project binaries.

The full ASan run exceeded this environment's command timeout while the default stress case was
still running; 191 other cases had completed. It is **not** recorded as a full sanitizer-suite pass.
The separately rerun 191-case suite and small sanitizer stress passed. The ordinary default stress
passed in both full Release suites. Cross-target native/Wasm parity remains pending; the
native-before/after comparison does not establish it.

Logs are under `evidence/release-0.5.0/`. Earlier release evidence is historical, not a current Wasm
claim.

## Required remaining gate

With emsdk activated, run from the code-platform directory:

```sh
python tools/b8.py build
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-test --native-build build
python tools/b8.py wasm-serve --probe --open
```

The compiled gate requires five C ABI checks, ten conformance scenarios, two seeded physical
trajectories, probe commands, and three actual compiled-module worker tests. Missing products cause
failure, not a skip. Review actual browser loading, the scanned LCD, independent host timeout versus
MCU reset, and the absence of a native `/command` endpoint. Record compiler/browser versions and
module hashes. Passing this gate validates the platform port, not Jordon's product firmware.

## Retained boundaries

The native and Wasm variants are register-level environments, not instruction-set emulators. A
nonreturning callback without SDK accesses can stall logical time; host termination is a separate
failed run. Browser session restart recreates the whole machine, while the MCU reset command
preserves external mechanical/thermal state according to the existing contract. The browser route is
not a hardware driver and cannot substitute for measured physical loss-of-host inhibition. No
appliance-safety certification or new application requirement is created by this release.
