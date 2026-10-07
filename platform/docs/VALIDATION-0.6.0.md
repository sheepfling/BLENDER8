# Half-A/Labs B8 0.6.0 — animated C++ workbench verification

> Historical evidence from the original authoring environment. The integrated checkout has newer
> local native and Emscripten results in [local build verification](../../docs/BUILD-VERIFICATION.md).

**Owner package 0.10.0 / B8 interface 03 / chassis 04 / host machine ABI 1 / scene ABI 1.** This is
implementation evidence, not approval of finished appliance firmware or physical hardware.

## What is delivered

The C++ scene owns appliance/chassis layouts, hit-testing, physical control actions, button travel,
jar placement, shaft-phase visualization, real scanned LCD pixels, host instruments, contact
history, logical-time pacing and the entire RGBA frame. It connects to the same
Session/MCU/component graph as the existing native and Wasm routes. Emscripten target definitions
now link the scene. The browser's worker only loads the module, validates ABI calls, forwards raw
input and presents the C++ pixels on OffscreenCanvas.

The explicit native animated route executes this same C++ code through a framed local transport. It
is labeled native, not selected automatically as a Wasm fallback. A finite bench experiment shows
actual motor response/coastdown without supplying a product-firmware solution. The safe probe
remains drive-off.

**No compiled B8 `.wasm` or Emscripten `.mjs` is included. Actual Emscripten linking, compiled-Wasm
tests, native/Wasm parity and browser-Wasm execution were NOT run.** Neither emcc nor emcmake is
installed. Official SDK reachability/download attempts failed (DNS/network); the evidence records
the failures. The emsdk pin is an unverified starting point. Build/compiler/browser-specific defects
may still be found by the mandatory real port gates.

## Executed verification

| Check                                                                   | Actual result                                                                                                                                              |
| ----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| GCC 14.2 / Release                                                      | **222 / 222 CTest checks passed**                                                                                                                          |
| Clang 17 / Release                                                      | **222 / 222 CTest checks passed**                                                                                                                          |
| Clang Debug / address and undefined-behavior sanitizers, leak detection | **221 / 221 non-stress CTests passed**, default long stress deliberately excluded                                                                          |
| Separate sanitizer stress                                               | **2 episodes, two replays per episode, 16 actions; 64,000 checked microsteps passed**                                                                      |
| Added graphical conformance                                             | **27 view tests**, included in CTest totals                                                                                                                |
| Host C ABI                                                              | Six bridge cases including scene initialization, frame storage, non-invasive render, input and disposal; **native compilation**, not Wasm                  |
| New JS view-client tests                                                | Eight explicitly identified module-test-double tests; **not C++/Wasm execution**                                                                           |
| New native scene/HTTP tests                                             | Seven Python unittest methods in one CTest wrapper; actual C++ process, RGBA framing, inputs, resize, validation, token/origin/Host and allowlisted routes |
| Static-site tests                                                       | Real packager/manifest/server validation with intentionally labeled temporary header fixtures; fake headers are not delivered binaries                     |
| Document-production regression                                          | **203 pytest tests passed**, no skips after assembling the included unchanged masters                                                                      |
| Native visual output                                                    | Four full-size C++ scene proofs plus a 140-frame native sequence; reviewed as images                                                                       |
| Gesture stress                                                          | 250 seeded input actions exercise bounded ownership, rendering and hardware stepping; included in a scene test                                             |
| Fresh extracted code archive                                            | Manifest hashes verified; fresh GCC Release build and **222 / 222 CTests passed**                                                                          |
| Actual browser navigation                                               | Native loopback URL blocked by managed Chromium (`ERR_BLOCKED_BY_ADMINISTRATOR`); **not a successful end-to-end browser test**                             |
| Real WASM build/test commands                                           | Fail explicitly without Emscripten/real outputs; no skip or model-double fallback                                                                          |
| Physical HWIL / customer firmware acceptance                            | Not run; no completed product firmware supplied                                                                                                            |
| Ruff / Pyright / clang-format / Windows / macOS                         | Not run                                                                                                                                                    |

The 222 count includes wrappers; do not add their individual Python/JavaScript methods to the total.
The 221 sanitizer count excludes one named default stress test; it is not a claim of the complete
222-test sanitizer run. An earlier combined configure/build/test shell exceeded its execution time
budget; the non-stress sanitizer suite was subsequently run separately. Final logs, toolchain
versions, source comparison, loopback/browser limitations and animated sequence data are in
`evidence/release-0.6.0/`.

The release retains all SDK, selected starter, customer correspondence, scenario and numerical
specification files from platform 0.5.0 byte-for-byte. Existing component physics implementations
are unchanged. Optional probe/getter interfaces and the Session host observation/advance facade were
added; native regressions exercise the changes. The full owner archive retains 112 checked canonical
supplier-PDF, correspondence, LaTeX, document-renderer, recipe and controlled-input files
byte-for-byte. The preservation record enumerates the exact checked files.

## What the scene tests actually check

Rendering, resizing, view switching and status queries do not alter the machine, consume an ADC
result, replace a byte latch, acknowledge an interrupt or break the protected clock-key sequence.
Identical timed inputs at different rendering cadences have identical machine outcomes. A contact
fault does not invent a mechanical latch. Held PULSE and blocked PULSE are separate observations.
Multiple input holders, key repeats, release outside a control, pointer cancellation, blur and
bounded input maps are exercised. GUI blur does not release a STOP asserted independently by a
fixture.

The motor exhibit must achieve actual motor speed and drive before jar removal is tested. Opening
cuts the real independent gate without instantly erasing momentum. Subsequent frames show coastdown;
reseating alone does not restore permission. The experiment and every successful fixture operation
are journaled. The renderer never creates motor operation for unfinished firmware.

The native sequence runs 20 logical milliseconds per captured frame and is intentionally played back
at 40 milliseconds per GIF frame. It shows native C++ output, not a browser-Wasm screenshot. Rotor
art is a shaft-motion schematic; separate blade/coupling separation and fluid dynamics are not
modeled. The 100-microsecond scope sampling is an observation rate, not the hardware stepping rate
or a guarantee that every shorter edge appears visibly.

## Required actual-Wasm release gate

With Emscripten active, from the code directory:

```sh
python tools/b8.py build
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-test --native-build build
python tests/browser/animated_smoke.py --site build-wasm/site
```

The compiled gate requires six ABI cases, existing native/Wasm scenario/trajectory comparisons,
compiled probe/worker-isolation/nonreturning fixtures, and five added real compiled-scene tests. The
optional real-browser gate requires Playwright and Chromium and rejects missing compiler products.
It checks static loading, actual drawn frames, controls, focus/resize and absence of command POSTs.
Neither gate was passed here. Missing binaries fail rather than invoking a JavaScript simulation or
native process.

## Runtime and safety boundaries

The native runtime and Wasm design remain cooperative, not instruction-set emulators. Pure
nonreturning firmware can stop logical time and drawing in its worker. The outer page reports a host
timeout and terminates that worker; this is not a WDT event, cleanup promise or hardware inhibition
mechanism. MCU reset and new session are different operations. Focus pause is not STOP. Physical
HWIL remains a separately paced/validated native route with local loss-of-host behavior; no
graphical feature creates hardware safety certification or authorizes unattended operation.

The fresh-extract evidence covers the packaged source code and relative build paths. Final
repackaging adds only this record, the completed logs and checksums, not new implementation changes.
It does not establish actual Wasm execution or another operating system.
