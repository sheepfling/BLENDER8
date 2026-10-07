# B8 animated C++ workbench

Platform 0.6.0 / interface 03 / chassis 04 / machine ABI 1 / scene ABI 1.

The workbench is now an application written in C++, not a JavaScript drawing of a C++ simulator.
`view/src/` owns appliance layout, hit-testing, button travel, jar placement, rotor visualization,
wire indicators, contact history, logical-time pacing, and the complete RGBA framebuffer. The
browser worker passes raw input and presents that framebuffer on its `OffscreenCanvas`. It owns no
motor, button, fault or appliance policy.

**Delivery boundary:** the same C++ application builds and runs natively and through Emscripten.
This integrated checkout passed native CTest and the compiled WebAssembly ABI, worker, scene, and
native parity gates with Emscripten 6.0.10. Generated binaries remain ignored. Browser navigation
is a separate gate; see [local build verification](../../docs/BUILD-VERIFICATION.md). The explicit
native preview is not an automatic fallback and is never labeled as Wasm.

## Start now: native execution of the same workbench

From the code directory:

```sh
python tools/b8.py build
python tools/b8.py test
python tools/b8.py animated --probe --open
```

The probe is firmware: it reads B8 registers and writes the actual LCD through the byte bus. Its
motor stays off. The rear switch initially requests ON; the page starts logical time for firmware.
RUN/PAUSE controls supply settling, reset,
and firmware. Press a speed button and observe its latch, contact history and scanned LCD pixels.

To see actual motor dynamics without supplying a finished firmware solution:

```sh
python tools/b8.py animated --bench --open
```

In a **fresh** component-bench session click **MOTOR EXHIBIT**. This activates a journaled,
finite sequence of register/fixture commands: supply qualification, STOP acknowledgment, a selected
mechanical command, changing PWM, and drive removal at two seconds. It explicitly services the WDT
during that experiment, not in normal firmware or idle bench mode. Lift the jar while running and
observe the actual independent gate and coastdown. Reseating does not re-arm the permission. The
exhibit is not customer-compliant firmware, does not implement thermal/motion diagnostics, and is
unavailable in selected-firmware or probe mode.

Use `python tools/b8.py animated --open` for the selected firmware. Change the source at build time:

```sh
python tools/b8.py --build-dir build-new-employee build --firmware ../new-employee-firmware
python tools/b8.py --build-dir build-new-employee animated --open
```

The native preview server uses one C++ process per server. Opening/reinitializing another tab
replaces that preview machine; use different ports/server processes for independent native sessions.
In the Wasm design each tab owns a separate worker/module. Both routes are local engineering tools,
not multi-user services.

## Build the intended all-browser application

Install and activate Emscripten first; see `docs/WASM.md`. Then:

```sh
python tools/b8.py build
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-test --native-build build
python tools/b8.py wasm-serve --probe --open
# For the finite motor experiment:
python tools/b8.py wasm-serve --bench --open
```

The build creates `build-wasm/site/`. Its `index.html` is the animated C++ application.
`diagnostics.html` retains the original data-centric workbench. Publish only that allowlisted site;
no server-side simulator, Python RPC, `/command` endpoint or native executable is part of the Wasm
deployment. HTTP/HTTPS static hosting is needed; `file://` is not supported.

The static release includes modular `.mjs` loaders plus `.wasm` for selected firmware, probe and
fixed starter. The deliberate hang fixture is excluded. `build-info.json` identifies and hashes the
release. Changing firmware requires a local rebuild; neither an in-browser compiler nor a fictional
CPU instruction decoder/ROM loader is supplied.

The original `6.0.11` emsdk pin remains unverified here. The local compiled gate passed with
Emscripten 6.0.10; `build-info.json` and `reports/wasm-parity.json` record compiler and module
hashes. No JavaScript module test double is substituted when compiler outputs are absent.

## Ownership and files

| Layer                        | Source                                             | What it owns                                                                                |
| ---------------------------- | -------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| Firmware                     | `firmware/`, `sdk/include`                         | Application logic and B8 register access only                                               |
| MCU and connected hardware   | `sim/`, `host/`                                    | Clock domains, electrical state, mechanical latches, physics, reset, component replacements |
| Read-only scene observations | `sim/scene_observation.*`, component probes        | Typed physical state; no register reads with side effects                                   |
| C++ graphical application    | `view/include`, `view/src`                         | Layout, hit-testing, control actions, pacing, visuals and pixels                            |
| Host C bridges               | `wasm/api.h`, `wasm/view_api.h`, `wasm/bridge.cpp` | Machine/scene lifecycle and bounded calls                                                   |
| Browser worker               | `webview/scene-worker.mjs`                         | Load module, serialize requests, validate ABI, present pixels                               |
| Browser shell                | `webview/scene-shell.mjs`                          | Raw events, worker timeout, focus/visibility, static page, file download                    |
| Explicit native preview      | `apps/view_host.cpp`, `tools/animated_server.py`   | Same C++ scene over local framed transport; not browser-Wasm execution                      |

`Canvas` is an original software 2D renderer: rectangles, polygons, lines, ellipses and a small
built-in glyph table. There are no bundled font files, downloaded textures, JavaScript drawing
commands, SDL dependency, or separate physics implementation. A WebGL renderer may later replace
this output implementation without changing controls, machine state or the native acceptance runner.
Software rendering at the present resolution is not a performance guarantee for every
browser/device.

## Observation, not another appliance

A pressed graphical speed button acts on the physical fixture. The component produces contact
bounce; the mux and B8 expose the resulting electrical state; firmware decides how to react. The
renderer cannot issue a matching speed command on the firmware's behalf.

Mechanical latch state is separate from electrical contact state. A stuck-closed wire can read
active without depressing its button. PULSE can remain visibly held while STOP blocks its electrical
command. Rendering only GPIO contact bits would lose those distinctions.

The motor's existing integrated phase supplies rotor visualization. At high RPM the renderer uses a
blurred representation rather than an apparently stationary, aliased blade. **The drawing is a
shaft-motion schematic, not an independent blade/coupling or fluid simulation.** Jar access,
coupling separation and guarding are not physically modeled. Raising the jar graphic eases
cosmetically; the electrical contact/gate changes immediately according to the model, not after the
drawing finishes moving.

LCD pixels are sampled from the module's visible scan output, not drawn from a host-side string. An
unfinished firmware image may leave the LCD blank. A wrong image may display the wrong thing. The
renderer never corrects it.

The chassis view uses the same machine and controls. Animated highlights are indication aids, not
measured propagation waveforms. The thermal connection is neutral rather than a fictitious digital
high. The scope samples contacts every 100 logical microseconds over a bounded 1,024-sample history.
It can therefore miss a shorter pulse visually; that does not remove such a pulse from the hardware
simulation or the conformance tests.

Some host observation capabilities are optional. A replacement motor without a phase probe shows
unavailable rotation detail; no imaginary RPM, heat or phase is synthesized. Physical voltage and
fault toggles that depend on a particular reference implementation are disabled when that
implementation is absent. All new probe declarations remain outside the firmware SDK.

## Supervision status

Open **Supervision** or press **U** for watchdog/deadman enablement, locks, service counts,
window, expiry estimates and retained reboot history. Firmware chooses enablement; a separate
development fuse permits optional watchdog operation. See [supervision configuration and reset
reasons](SUPERVISION.md).

## LCD bus and blanking

Open **LCD BUS** or press **L**. The PX32-16 accepts ordered eight-bit byte transfers from the
MCU; its VBLANK output travels back to the MCU. The view plots the last 40 logical milliseconds of
actual blanking edges: 16 ms of active scan followed by 4 ms of blanking. A second scope magnifies
the latest DATA-write burst into D7 through D0 and decodes the last 16 bytes as hexadecimal.
Pink bytes were rejected because the LCD was still BUSY. The accepted DATA-write recovery time is
8 microseconds. Writes during scanout remain visible in the capture and can cause tearing.

Instrumentation lives at the display boundary, so CPU, bench and B16 DMA traffic use the same
capture. The probe records reads, writes, register selection, the VRAM address before each access,
acceptance and the contemporaneous VBLANK level. Observation never reads MMIO, changes a selector
or advances logical time. A DMA completion counts issued bytes; the LCD capture reports whether
those bytes were accepted. A CPU access rejected by DMA ownership never reaches this boundary.

Captures retain 64 DATA-write attempts, 32 bus transactions and 32 blanking/reset edges. Lifetime
counters continue after older records roll off. DATA history is separate so repeated STATUS reads
cannot evict a burst. LCD power resets mark the trace and restart the scan epoch; retained traffic
from before reset is labeled. Restart session creates fresh counters and captures. A replacement
display without `DisplayBusProbe` shows unavailable bus instrumentation.

The bit lanes hold byte values between timestamped transaction markers for readability. They do
not model electrical setup/hold timing, propagation or a serial clock. Blanking edges and byte
timestamps come from the component model, independently of the 100 microsecond contact scope.
The chassis also shows the data and VBLANK connections with their directions.

## Time and animation

`Workbench::frame(elapsed_ms)` accepts finite elapsed values in [0,1000] for pacing only. The
selected presentation rate is 0.05, 0.25, 1 or 4. Each call advances at most 20,000 logical
microseconds, using the existing one-microsecond hardware scheduler. Excess wall time is not caught
up by skipping hardware cycles: a slow host runs behind realtime. Changing the display rate does not
change the oscillator, timer divisors, ADC, mux or thermal equations.

`render()` and `resize()` do not advance or mutate the machine. Button travel and jar placement use
a 45 ms cosmetic easing time. The motor phase is a physical observation. Tests compare machine
outcomes across different drawing cadences and verify that register latches, ADC freshness and clock
unlock keys survive observation unchanged.

RUN pauses/resumes automatic logical stepping. STEP advances exactly 1,000 logical microseconds
through the same simulator. Canvas focus loss releases GUI-owned momentary inputs and preserves
the RUN selection. Hidden tabs and unfocused windows suspend browser pacing. Returning resumes
pacing if RUN was selected, without catching up hidden time or unlatching speed selections.
Pointer/key ownership is independent: releasing one PULSE holder does not
release another. STOP blocks PULSE mechanically/electrically as specified. Repeat keydown is
ignored. A fresh pump generation prevents stale in-flight animation callbacks from spawning
duplicate loops after blur/focus.

## Scene ABI

The original machine ABI remains 1 and the B8 register contract remains 03. The additive scene ABI
is also numbered 1:

```text
b8_view_abi()                          -> 1
b8_view_init()                         -> status (machine must already exist)
b8_view_resize(width, height)          -> status
b8_view_frame(elapsed_ms)              -> status
b8_view_event(kind, id, x, y)           -> status
b8_view_pixels()                       -> pointer to RGBA8
b8_view_width(), b8_view_height()      -> dimensions
b8_view_status(), b8_view_journal()     -> module-owned UTF-8 JSON
```

Input kinds: 0 pointer-down, 1 pointer-up, 2 pointer-move, 3 pointer-cancel, 4 key-down, 5 key-up, 6
release GUI inputs and pause (legacy). Additive kind 7 releases GUI inputs while preserving RUN;
`view.input_release_kind` advertises it. Older uploaded images retain their documented focus-pause
behavior and emit a compatibility warning. C++ converts physical canvas coordinates to logical
1280x900
layout, including letterboxing. Resize bounds are 320..1920 by 240..1440. Pointer IDs are
nonnegative; ASCII keys use a separate internal ownership space. Invalid fields are rejected.

The RGBA pointer is valid only while the current canvas allocation remains unchanged. The host
reacquires the heap view after every call that might grow Wasm memory or resize the canvas. Returned
strings are copied before the next ABI call. Never store a borrowed Wasm pointer as permanent UI
state. One worker/module owns one session and firmware-global set.

## Failure and journals

The shell imposes a 10-second command deadline, 30 seconds for initialization. A stuck C++ callback
can block that worker's simulation and drawing. The independently running page terminates it and
reports `WASM_HOST_TIMEOUT`; it cannot invent a final frame, cleanup, or a WDT cause. Restart
session recreates the whole machine. MCU RESET applies reset/retention to the existing machine. The
native preview similarly reports a host timeout and kills its local process. Neither is a physical
HWIL inhibition mechanism.

Save journal exports accepted fixture commands and elapsed logical run intervals, not mouse
coordinates or render frequency. Adjacent runs are coalesced without reordering physical events. At
20,000 records the journal explicitly marks itself incomplete. A complete journal can be applied to
a fresh session with the same firmware and bench mode; the scene test exercises this. It is not an
authenticated customer approval or a self-contained firmware binary. Use the separate release
manifest/executable hashes when comparing builds.

## Browser debug logs

Both `index.html` and `diagnostics.html` have a **Debug logs** button. The panel combines page and
worker records and also sends them to the browser console with a `[Half-A/Labs]` prefix. Use the
console's severity filter or search for an event such as `host.failure` or `wasm.abort`.

| Level          | Output                                                         |
| -------------- | -------------------------------------------------------------- |
| Info (default) | Image identity, startup, observed machine changes and failures |
| Debug          | Also request IDs, queue/response durations and input events    |
| Trace          | Also every frame or high-frequency snapshot/run request        |
| Warn / Error   | Only the selected severity and higher                          |

Each record identifies its source, session, sequence, wall timestamp and host elapsed time. Page
observations also carry the last observed logical microsecond count. These clocks are separate:
host timing is diagnostic context and does not drive firmware. Image loading records include the
Wasm SHA-256, ABI, device and memory-policy result. Worker exceptions retain their JavaScript stack
when available; Emscripten stdout, stderr and abort messages use the same log. Host timeouts report
the outstanding request and explicitly remain distinct from observed MCU resets.

**Save debug log** downloads `mcu-debug-log.json` with the structured records and context. **View
JSON** exposes a selectable export snapshot, including in embedded browsers without file downloads.
The panel retains at most 1,000 entries and reports evicted entries. Scene restart preserves the existing
history with a new session ID; page reload clears it. **Clear log** clears the retained history.
Local loader source, binary contents and native preview capability tokens are omitted or redacted.
The log has no network collector or persistent browser storage.

Logs observe replies already produced by the C++ machine. They do not read registers, advance
logical time, consume firmware RAM or replace the fixture journal. State-change records can miss
transitions between replies. Trace can slow the host, so enable it only for a bounded investigation.
Full C++ source stacks require a Wasm build containing suitable debug information; JavaScript
logging cannot recover symbols absent from the compiled image.

## Verification gates

```text
python tools/b8.py test
python tools/b8.py wasm-build --test-fixtures
python tools/b8.py wasm-test --native-build build
python tests/browser/animated_smoke.py --site build-wasm/site
```

The last command requires the optional Playwright package and its Chromium installation. It loads
actual compiler outputs via a static server and checks drawing, controls, focus/resize, and no
command POSTs. It was not run in this checkout. Native model parity passed through the separate
`wasm-test` gate, including five real compiled-scene checks. Missing binaries fail instead of
skipping.

For a local render proof without a browser (optional Pillow):

```text
python tools/capture_animated.py --build build --output reports/native-animation
```

The GIF uses real C++ frames: 20 ms logical steps played at 40 ms per frame, explicitly half speed.
It is not a Wasm screenshot or a graphical simulation substitute.

Official context: Emscripten's build/exception mechanisms and MDN's OffscreenCanvas API explain the
platform bindings, not proof that this particular port has compiled. See the primary-source links in
`docs/WASM.md`.

## Thermal, motor and subsystem views

The canvas navigation offers Appliance, Chassis, Thermal + Truth, Motor and Systems. They observe
one machine and share its controls and history; changing pages never advances logical time.
T and Y both open the combined thermal page; M opens Motor and S opens Systems.

- **Thermal + Truth** overlays the nominal AN0 wire estimate and actual firmware ADC reads with
  case, lagged sensor, local air, optional food and room boundary temperatures. ADC values are held
  between reads and absent before a fresh read. No conversion or MMIO read is initiated by viewing
  the page. Signed heat flows show case/air/food/room conduction and ventilation in watts.
- **Motor** plots shaft RPM and generated heat over time, with load current and effective duty.
  Load, jam, PWM, power, STOP and jar permission act on the existing motor model.
- **Systems** shows the button assembly and mux, jar gate, power/reset, WDT/DMT, clock/core/timers,
  motor/tach/PWM, thermal network, sensor/ADC, LCD/XBUS, GPIO/interrupts and B16 pin routes/DMAC.
  B8 explicitly marks its absent DMAC. Missing component probes remain unavailable.

The real scanned LCD remains visible on every page. Its contents come exclusively from firmware
and the LCD component. The host does not replace them with an inferred label. The appliance lamps
indicate motor power and jar permission; they are not buttons. JAM SHAFT belongs to fault injection.
The case temperature controls add or subtract 20 degrees C, bounded to -40..150 degrees C, without
resetting the sensor or air. Sensor lag remains visible.

## Food fixtures and animations

Food buttons atomically replace mechanical load and the food thermal node. They preserve motor
case, local air, rotor momentum, MCU state and logical time, and are replayed as
`food_preset NAME`. Manual load or legacy food commands mark the contents as custom. Empty removes
food conduction and mechanical load.

| Preset             | Load | Initial temperature | Case conductance | Food heat capacity |
| ------------------ | ---- | ------------------- | ---------------- | ------------------ |
| `water`            | 15%  | 25 degrees C        | 0.15 W/K         | 1,200 J/K          |
| `frozen_fruit`     | 80%  | -10 degrees C       | 0.06 W/K         | 600 J/K            |
| `hot_vegetables`   | 45%  | 80 degrees C        | 0.18 W/K         | 900 J/K            |
| `empty`            | 0%   | absent              | 0 W/K            | inactive           |

These are fictional teaching parameters. Food heat capacity participates in time integration;
mechanical load causes speed sag, increased equivalent current and increased motor heating.
The C++ drop animation and rotating contents illustrate the fixture action. They are not fluid,
blade-contact, rheology or ice phase-change simulations, and do not advance logical time.

## Playback and visible contact bounce

The page defaults to Selected firmware and starts pacing for firmware modes. A fresh Component
bench stays paused for its explicit finite exhibit. A selected unfinished starter still needs the
employee's controller. For the owner candidate: wait for two cool logical seconds, briefly hold
STOP and release, then select a speed. Faults latch; remove their cause, qualify cooling, tap STOP,
release commands and make a new selection. CK additionally requires a controller reset after clock
restoration. The page includes the LCD code legend and recovery instructions.

There is no session duration limit. Ten minutes bounds the plot history, not execution. Explicit
PAUSE freezes physics and the LCD. Background suspension releases momentary controls and resumes
on return, preserving an explicit user pause. The finite bench exhibit removes drive at two seconds;
this is an electrical test sequence, not a simulation time limit.

A GUI STOP click completes a 50 ms physical stroke in logical time, so a press/release between
browser frames is sampled by firmware. Holding STOP keeps it asserted longer. Cancel or focus loss
releases GUI ownership immediately. The release is journaled at its actual logical time; raw
`stop 1` / `stop 0` fixtures retain unrestricted timing. A paused scene needs RUN to complete the
stroke and update the LCD.

The contact display retains the last change plus ten milliseconds, with a twenty-millisecond zoom
sampled at 100 microseconds. It stays visible after the contacts settle. BOUNCE cycles nominal
1.8 ms, 5 ms and off; `bounce random SEED` remains available through fixtures. This visualization
preserves the original microsecond mechanical/electrical model and firmware debounce behavior.

Temperature estimates use the nominal 3.3 V ADC reference and AVT10 transfer of 0.5 V + 10 mV/C.
Calibration, reference error, noise, open/short faults and sensor lag can separate them from truth;
no hidden calibration correction or physical-temperature clamp conceals that difference.

Plant histories retain up to 6,001 observations and ten minutes of logical time. They are sampled
at nominal 100 ms deadlines during graphical execution, using the actual time reached by a firmware
service boundary. Rendering and wall-clock pauses add no samples. Explicit fixture commands also
record their endpoint; skipped intervals longer than 120 ms are drawn as gaps. Contact history uses
its separate nominal 100 us deadlines. These snapshots do not resolve every 1 ms noise update or
contact transition. History buffers belong to the host and do not consume firmware memory budgets.

The thermal plant remains the existing lumped case/air/food conduction model with room ventilation,
speed-dependent case-to-air conductance and speed/current-dependent motor heat. The page does not
add fluid dynamics, blade loading, contact-force physics or MCU instruction timing.
