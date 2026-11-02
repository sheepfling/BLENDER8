# Local build verification — 2026-10-07

This record applies to the integrated working tree on macOS/arm64. It records toolchain and browser
behavior, not finished firmware acceptance or physical hardware behavior.

## Junior B16 recovery check — 2026-11-02

Fresh local review work is based on problem branch `feature/b8-b16-workbench` at
`47a1fb0f6684cdadc8fe75129d025364967afbde`; it does not include the worked controller or owner-only
teaching files. The starter remains intentionally incomplete.

- The entry route is [NEW-EMPLOYEE-START.md](../NEW-EMPLOYEE-START.md). Its B16-first native loop
  separates C++ build/test from PDF and Emscripten prerequisites. A standalone host lab at
  `platform/labs/b16/` has a scripted `RegisterBackend` ADC transaction example, deterministic
  register traces, and TODO exercises for temperature conversion, motor duty, and button/mux mapping.
  The subsystem guide orders the later timer, IRQ, LCD, watchdog/deadman, DMA, and composition work.
- `python -m pytest -q`: **256 passed**. The clean native B16 build and full platform CTest passed
  **237/237**. Regressions that replace the learner's editable firmware with a compile-time error
  still built the native and Emscripten fixed-starter targets; reused native build selection also
  returned to `platform/firmware/` instead of a stale worked-controller path. Branch-policy tests
  cover both PR directions, and learner bundles omit the solution-only candidate-view test. Bundle
  metadata marks firmware as non-solution only for a byte match with the shipped safe starter;
  modified inputs are `unknown` rather than assumed to be safe or solved.
- The complete `tools/project.py test --root . --device B16` aggregate also passed: document packet
  assembly (**62 pages / 7 handouts**), manual-example check, **256 Python tests**, and **237/237**
  native B16 CTests. The strict `scripts/ci.py --static --root .` gate passed Ruff, Pyright, rumdl,
  mdrepo, and pre-commit configuration checks.
- The B16 lab configured and built. Its five ADC/fake-hardware tests passed; the temperature,
  motor, and panel TODO cases failed as expected because their learner implementations are empty.
  Those three failures are exercises, not platform failures or product acceptance results.
- Emscripten 6.0.10 rebuilt the selected B16 target and test fixtures. `wasm-test` passed **9/9
  CTests**, **15/15** compiled Node tests, ten native/Wasm conformance scenarios, two seeded 128-action
  trajectories, fifteen probe commands, and selected-B16 motion/DMA parity. This was a Node-based
  compiled-Wasm check. A local browser smoke test also loaded `b8_student.wasm` for B16, advanced
  logical time, paused the workbench, and stepped 1 ms. The debug panel reported 88 entries and no
  dropped events; repeated WDT warnings were from the unfinished starter under the fused-on default.
  This is a launch/step smoke test, not a check of every browser control or customer behavior.
- `tools/project.py docs --root .` regenerated the clean and G3 seven-handout binders at **62 pages**,
  the clean/G1/G2/G3 source-unit copies, and the optional B16 follow-on at **11 pages**. The B16 G3
  page-count metadata now records 11, matching the compiled manual. Contact sheets covered all G3
  binder pages and the B16 addendum; selected technical pages and the clean cover were rendered at
  full size and visually checked. Ignored QA renders are under `build/qa-pdf/`; PDFs are in
  `build/docs/` and `dist/`, not source changes.
- Read-only Alphus commands against the sibling checkout were:

  ```sh
  /Users/rick/GIT/sheepfling/alphus/.venv/bin/python \
    -m alphus.gates.cpp.cpp_advisory_quality style platform \
    --include '*.cpp' --include '*.hpp' --exclude 'build*' --exclude 'node_modules/**' \
    --output /tmp/blender8-alphus-style-20261102.json
  /Users/rick/GIT/sheepfling/alphus/.venv/bin/python \
    -m alphus.gates.cpp.cpp_artifact_quality scan build/astin-b16 \
    --no-sections --output /tmp/blender8-alphus-artifact-20261102.json
  ```

  The advisory style scan reported **0 provisional findings**; the artifact scan reported **27
  artifacts / 24 executables / 0 findings**. There is no BLENDER8 Alphus policy, so these results
  supplement, rather than replace, CMake/compiler, numeric-policy, or CTest gates.

Sanitizer builds, manual browser interaction, selected-firmware customer acceptance, and physical
hardware validation were not run in this recovery check. No result here implies the starter meets the
product requirements.

## Thermal and subsystem visualization

- The shared native/Wasm C++ renderer now offers Thermal, Truth, Motor and Systems alongside
  Appliance and Chassis. Thermal separates nominal AN0 wire estimates from actual fresh firmware
  ADC reads; Truth overlays case, lagged sensor, local air, optional food and room temperatures.
  Signed heat-flow observations and motor RPM/heating histories use existing model states.
- Histories retain nominal 100 ms observations for ten logical minutes, with actual service-boundary
  timestamps and explicit gaps. Render/resize/page changes never sample MMIO or advance the model.
  A regression exposed callbacks crossing sampling deadlines; the observer now captures actual
  reached times instead of silently dropping those samples. Missing ADC reads remain absent; reads
  hold until replaced and clear on reset. Food controls preserve case and nearby-air state.
- The Python/Markdown quality gates passed with **243 pytest cases**. B8 native CTest passed
  **227/227**. B16's focused view, peripheral, thermal and sensor gate passed **41/41**. GCC and
  Clang address/undefined sanitizer builds each passed the original **31 scene tests**, then the
  final food/deadline changes passed their **4 focused scene cases** on both compilers.
- Both B8 and B16 Emscripten builds passed **7/7 CTests** and **9/9 compiled scene/upload tests**.
  This stage did not rerun the full native/Wasm trajectory parity suite.
- Actual browser verification covered the B8 measured/truth views and a locally uploaded final B16
  image, food addition, heat injection, logical-time history, measured/truth/System navigation and
  a clean browser error log. B16 image SHA-256 was
  `9ea6700a100644db8c4ab4460d17253d6dedce27ae3625efef7fc6244705a6bd`.
  Ignored screenshots are under `build/qa-thermal/browser-*.jpg`; full C++ render proofs are in the
  same directory. Build/test receipts are `build/thermal-*.log`.
- This verifies host observation and modeled behavior, not product firmware acceptance or physical
  thermal calibration. Noise appears only at snapshot cadence; there is no fluid or blade model.

## Selectable B8/B16 devices and local Wasm images — current

This stage supersedes the earlier numeric-profile-only B16 limitation recorded below.

- Rick's handoff now offers B8 or B16, describes the B16 bench wiring and the shared customer
  behavior, and explains local native/Wasm builds and image loading. The correspondence remains
  ten pages. Pristine and cumulative G1/G2/G3 generation, pairing and publication boundaries pass;
  the seven final handouts and 62-page binder were reassembled. The updated first page and all
  final page layouts were visually checked. Only final copies are delivered under `dist/`.
- `--device B8|B16` selects the actual behavioral MCU and matching firmware numeric policy, with
  separate native/Wasm build directories. B16 implements 1 KiB SRAM, paced DMA with software,
  timer, DREQ and VBLANK requests, terminal status/IRQ, pin selection and six firmware vectors.
  B8 preserves interface 03. Host vector spans keep the five/six-vector firmware ABIs distinct.
- B16's 16 peripheral/numeric conformance cases execute natively and in Wasm. They cover source
  mutation, forward overlap, staged descriptors, validation priority, request overrun, bus ownership,
  bus failure, partial abort, pacing, pin lock, ADC qualification, reset retention, halted-core
  progress, IRQ reassertion and observable binary32 corner cases. No instruction ISA or FPU
  instruction timing is modeled; service boundaries and peripheral clocks remain the timing contract.
- Full native CTest passed **223/223** in each of the B8 Clang, B16 Clang, GCC 16.2.0, and Clang
  address/undefined-behavior sanitizer builds. Tests needing loopback sockets ran with that access.
  Legacy B8 scenarios use the fixed B8 fixture; selected-device parity separately exercises B16.
- Both Emscripten 6.0.10 builds passed **7/7 CTests**, **11/11 compiled Node tests**, ten shared B8
  scenarios, two seeded 128-action trajectories and fifteen probe commands. An added selected-device
  trajectory confirms native/Wasm equality and motor motion for each MCU, plus B16 SRAM DMA.
  Receipts and module hashes are in ignored `reports/wasm-parity.json` and
  `reports/wasm-b16-parity.json`.
- The in-app browser loaded the final local B8 and B16 `.wasm`/`.mjs` pairs into a B8-built page.
  Both passed the fresh 100 ms execution smoke check and rendered the C++ scene. Selecting B8
  for a B16 image was explicitly rejected. Worker replacement and the blob-loader file resolver
  were corrected during this check. The final browser pass recorded no warnings/errors.
  JSON evidence and screenshots are under ignored `build/qa-device/`. The page exposes the complete
  result JSON as well as a download button; the in-app download event itself was not verified.
- The strict Python/Markdown gate and all **240 Python tests** passed. The developer path is
  [DEVICE-WORKFLOW.md](../platform/docs/DEVICE-WORKFLOW.md). Firmware remains an unfinished safe
  starter. These checks establish model/tooling behavior, not customer acceptance or physical
  hardware behavior. Windows was not exercised.

## Document foundry

- `tools/bootstrap.py --root .` installed the editable package and development tools into `.venv/`.
  `tools/project.py doctor --root .` found Python 3.12.14, Tectonic, CMake, a C++ compiler and
  Emscripten. The install needed package-index access for isolated build dependencies.
- `tools/project.py docs` regenerated eight pristine LaTeX PDFs, eight G1 copies, eight G2 copies,
  and eight G3 copies. Pristine/G1/G2 files and their seven-handout previews and 62-page binders are
  in `build/docs/`; only G3 PDFs, seven recipient handouts and its 62-page binder are in `dist/`.
  Source hashes, page counts, geometry, copy-chain prefixes and clean/copy pairing passed. The
  correspondence's clean first page was visually inspected after the Half-A/Labs and New Employee
  update. MD20 characteristic set 03 compiled as a four-page Issue P4 document. Other pages still
  need editorial review before publication.
- The built-in single-file editor compiler cannot resolve the repository's shared
  `latex/vendors/vendor-vortek.sty` because it does not receive project search paths. The style is
  present; the repository build supplies those paths and compiled the revised MD20 successfully.
- This integration reran the strict Python and Markdown gate: compilation, Ruff lint and format,
  strict Pyright, rumdl, mdrepo, pre-commit config, and all 206 Python tests passed.

### Correspondence presentation follow-up

- Rick's customer replies describe the team's intended implementation. The internal handoff
  addresses the firmware team; the mail no longer names the learner persona. Technical limits and
  message identifiers are preserved.
- All 36 printed messages have subjects and sender signatures. Reply subjects use `RE:`; Mara's
  21 incoming messages use `[EXTERNAL]`. Her signatures reuse the BA-8 manual's angular Kestrel K
  through a shared LaTeX identity; the Markdown copy includes an SVG rendition. Rick's signature
  remains two plain lines. Sketch captions describe their contents.
- The ten-page correspondence was rebuilt pristine and through G1/G2/G3. All ten pristine and final
  G3 pages were visually inspected. Generation pairing and the publication boundary audit passed;
  the seven handouts and 62-page binders were reassembled. Only G3 is delivered in `dist/`.
- All **212 Python tests** and the strict Python/Markdown gate passed. The Kestrel identity proof
  also compiled with the shared logo. C++ and WebAssembly were not rerun for this presentation edit.

## C++ and WebAssembly

- The Release build compiled the revised C++20 emulator, simulator, native animated renderer, host
  processes, and test executables. Native CTest passed **221/221** current checks, including the real
  local HTTP scene transport, static-site tooling and coupled thermal checks. The retired contract
  snapshot check moved to `internal/history/platform/`; its separate owner command also passed.
- Emscripten **6.0.10** built real modular `.mjs` and `.wasm` images for selected firmware, pixel
  probe, fixed starter, and the test-only hang fixture. The packaged `build/wasm/site/` contains the
  C++ scene shell and a manifest of 17 hashed static assets; it excludes the hang fixture.
- Compiled WASM verification passed **6/6** ABI CTests and **8/8** Node worker/scene cases. Native
  parity passed ten conformance scenarios, two seeded 128-action trajectories, and fifteen probe
  commands. The native reference was rebuilt cleanly before the final parity run. See the ignored
  `reports/wasm-parity.json` receipt for module hashes.
- The documented `serve-wasm` command served the compiled page on a temporary loopback port; an
  HTTP GET returned status 200 and `text/html`. Ctrl-C stopped the temporary server cleanly. This
  verifies static serving, not a new browser interaction pass.
- Before the thermal edit, the in-app browser loaded the static WASM page from loopback. The Run
  control advanced logical time and changed scanned LCD pixels; selecting speed 3 changed the C++
  scene; the chassis
  view rendered. The browser reported no console errors. The optional Playwright smoke suite was not
  run because its
  package is absent; this manual check does not cover every browser control or browser engine.

## Provenance and limits

### B8/B16 numeric profiles and follow-on manual

- The expanded device is now named B16. Its separate ten-page follow-on manual defines B8 byte
  integers, B16 byte/word integers and binary32 float, plus the DMAC and pin-selection contract.
  Both profiles reject doubles and integer scalars/intermediates wider than the chosen profile.
  B8 still accesses its wider peripheral values through explicit bytes.
- The selected firmware target runs the Python/Clang semantic-AST gate before native or Wasm
  compilation. `numeric.hpp` supplies width-preserving operations. Tests cover aliases, inferred
  types, local headers, macro expansion, promoted expressions, cast-after-multiply, missing tools,
  arithmetic boundaries, and CMake rejection/acceptance when switching B8/B16 profiles.
- All **240 Python tests** and the strict Python/Markdown gate passed. Native builds and CTest
  passed **222/222** with Apple Clang, GCC 16.2.0, and Clang address/undefined-behavior sanitizers.
  The initial sandbox runs failed three HTTP checks; complete reruns with loopback access passed.
- Emscripten 6.0.10 rebuilt the selected firmware with its own driver performing the numeric AST
  check. Compiled ABI CTests passed **6/6**, Node worker/scene tests **8/8**, and native/Wasm parity
  passed. The machine still implements B8 interface 03; selecting B16 numeric rules does not
  implement B16 SRAM, DMA, pin routing or FPU state/timing.
- The follow-on manual compiled through the repository LaTeX build with ten pages and no layout
  overflow. Pristine and final G3 pages were visually inspected. G1/G2/G3 pairing and cumulative
  chains passed. The optional manual remains separate from the seven handouts/62-page binder.
  Superseded B8P outputs moved into ignored build history. The standalone editor still cannot
  resolve the shared vendor styles; its preview failure is separate from the successful build.

### Original intake and acceptance boundaries

- A second `HalfALabs-Blender8` dump arrived in `INTAKE` on 2026-10-07. Its original inventoried
  files were verified before migration and retained under ignored
  `.local/source-archives/original-intake/HalfALabs-Blender8-redump-2026-10-07/`. The active LaTeX
  tree remains at `latex/`; the correspondence styles now use the Half-A/Labs wordmark and MD20
  retains its intentional thermal revision. `INTAKE` is absent again.
- The 0.6.0 intake contained 262 files. Its archived ZIP passed CRC and byte-for-byte comparison
  with the intake tree. The archive SHA-256 is
  `616e9fe221f7057d7b3d7af6b0830ff36bc2be29ebc75fe9ca450f29bca257fc`. The original
  manifest and inventory are tracked under `internal/authoring/source_inputs/platform-0.6.0/`.
  `INTAKE` is absent.
- The earlier full product acceptance run on the supplied unfinished starter recorded 2 pass,
  56 fail, 5 review, and `accepted=false`; it was not repeated for this integration. No physical
  HWIL, electrical, safety, or production acceptance was performed. Windows was not tested.

Local versions: Python 3.12.14; CMake 4.4.4; Apple Clang 21.0.0; Emscripten 6.0.10; Tectonic
0.17.0. The native HTTP tests required permission to bind to `127.0.0.1`.
