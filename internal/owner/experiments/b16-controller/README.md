# B16 controller experiment

Owner material, outside the New Employee route and excluded from the source handoff bundle.
The learner starter remains unfinished. This candidate uses only the public SDK and its own
files; the plant, emulator and renderer retain their normal precision.

## Result

The candidate passes 61 automated correspondence cases and 19 additional scenarios on native
Clang, GCC, Clang with address/undefined sanitizers, and actual compiled Wasm. Two further
characterization probes document the ADC/deadman and CK recovery decisions. Two acceptance reviews
remain outstanding: progress authorization and combined fault retention. Candidate glyph templates
check encoding, rather than approve human legibility.
See [the requirements audit](REQUIREMENTS-AUDIT.md) for the remaining specification decisions.
Generated receipts live under `reports/`; `.json.gz` retains the full evidence with lossless
compression. Compiled products live under `build/` and are ignored. The bounded, checked-in
[verification receipt](verification.json) identifies the tested images and source hashes.

The real Emscripten build is available at `build/experiment-b16-wasm/site/`. Use **Selected
firmware**, rather than Component bench, to execute this controller. Component bench deliberately
runs without firmware. A local serving session is available at
[the B16 experiment](http://127.0.0.1:8094/?mode=student) while its Python server is running.

The compiled workbench is also published as a hosted
[ChatGPT Site](https://half-a-labs-blender8-workbench.rick583.chatgpt.site).
The Site defaults to **Selected firmware** and starts logical time. Pixel probe is an explicit
bring-up mode that keeps the motor off. Wait for two cool logical seconds, briefly hold STOP and
release, then select a speed. The page includes LCD fault codes and recovery instructions.
See [workbench controls and food fixtures](../../../../platform/docs/ANIMATED-WORKBENCH.md).
The Site includes the browser debug logs and local compiled-image loader. Its
[deployment receipt](sites-deployment.json) identifies the published bundle. Publishing is a
snapshot; later repository edits need a new Site deployment.

## Run it

Activate the repository Python environment first, as described in
[the installation guide](../../../../docs/START-HERE.md). Run the Python snippet from the repository root.
All tooling accepts explicit paths; no parent-directory discovery is used.

```python
import subprocess
import sys

candidate = "internal/owner/experiments/b16-controller"
native = "build/experiment-b16"
wasm = "build/experiment-b16-wasm"


def run(*arguments: str) -> None:
    subprocess.run([sys.executable, *arguments], check=True)


run("platform/tools/b8.py", "--root", "platform", "--build-dir", "../" + native,
    "build", "--firmware", "../" + candidate, "--device", "B16")
run("platform/tools/experiment.py", "--experiment", candidate,
    "--exe", native + "/b8_emulator", "--output", "reports/experiment-b16.json.gz")
run("platform/tools/b8.py", "--root", "platform", "--build-dir", "../" + wasm,
    "wasm-build", "--firmware", "../" + candidate, "--device", "B16")
run("platform/tools/experiment.py", "--experiment", candidate,
    "--wasm-module", wasm + "/site/b8_student.mjs",
    "--output", "reports/experiment-b16-wasm.json.gz")
run("platform/tools/wasm_server.py", wasm + "/site", "--port", "8094", "--mode", "student")
```

The experiment runner returns success when the automated cases pass. It explicitly reports
`accepted: false` and never supplies a human approval. Full reports include image hashes and
observed device identity. In the upload UI, choose B16 and the matching `b8_student.mjs` and
`b8_student.wasm` from the same build's `web/` directory.

In the page: press **R** to run, allow the two-second temperature qualification, press and
release **Space** for a new STOP acknowledgment, allow controls to settle, then press **1–7**.
Hold **P** for boost. **Space** stops; **J** removes/reseats the jar. **T**, **Y**, **M** and **S**
open Thermal, Truth, Motor and Systems. Jar reseating needs another deliberate STOP acknowledgment.

## Design choices and timing

| Function             | Candidate choice                                      | Reason or bound                                                           |
| -------------------- | ----------------------------------------------------- | ------------------------------------------------------------------------- |
| MCU                  | B16, standard memory                                  | Strict byte/word integers and binary32; no doubles                        |
| Clock                | P=2, M=4, Q=2, PB divider=8                           | 8 MHz SYS, 1 MHz PB; VCO=16 MHz nominal                                   |
| Clock startup        | Bounded 20,000 service-boundary status reads          | Healthy switch completes well within 100 ms                               |
| Timer                | T0 periodic, prescale 1, compare 999                  | 1 ms foreground epoch, ±50 ppm external allowance                         |
| Pins                 | PWM PB5, tach PB1, VBLANK PB6, AN0 PA4                | Chassis 04 wiring; route selection locked                                 |
| Input acquisition    | All eight mux channels each epoch; 2 us settling      | MX8-1 switching contract                                                  |
| Stable input         | Whole bitmap stable 10 ms                             | Rejects 5 ms bounce; comfortably inside 40 ms response                    |
| Invalid panel        | Two speed indications continuously 40 ms              | No arbitrary selection during changeover                                  |
| PWM                  | Rounded inverse MD20 table, 256-tick carrier          | Nominal no-load targets; no speed regulation                              |
| Motion               | 500 ms off-to-on grace; 100 ms without tach edges     | Blocked start within 750 ms, running loss within 400 ms                   |
| ADC                  | Fresh AN0 completion about every millisecond          | Completed result consumed once; also sampled while stopped                |
| Plausibility         | Codes 78–558, about 0.25–1.80 V                       | AVT10 nominal 0.30–1.75 V, with guard band; rails become TS               |
| Thermal              | Two consecutive plausible reads at least 85°C         | Case sensor estimate, including physical lag                              |
| Recovery             | Plausible fresh readings at most 65°C for two seconds | New STOP after qualification; released controls, fresh command            |
| WDT                  | Scale 3, enabled and locked                           | 227.56 ms worst case at 9 kHz LFRC; 22.44 ms margin                       |
| DMT                  | Limit 512, window 128, enabled and locked             | 65.54 ms worst case at slow approved SYS; 34.46 ms margin                 |
| Renewal              | Foreground work plus a new ADC completion token       | ISR never feeds; a completion authorizes at most one renewal              |
| Diagnostic retention | B16 SRAM 1000h–1002h                                  | Magic, fault mask, reset details; warm reset preserves subordinate causes |
| LCD                  | Independent 5×7 font, doubled to 10×14                | Busy-aware 64-byte writes; scanned labels checked by templates            |
| DMAC                 | Unused                                                | CPU byte writes meet this display's bandwidth needs                       |

Each firmware SDK access advances a service interval in this model. C++ expression execution
and interrupt instruction costs have no MCU timing model. The table therefore describes
simulation deadlines and analytic clock margins; real instruction timing remains unverified.

Fault priority is CK > WD > DM > TS > TH > IF > IL > ST. Normal STOP does not create a fault.
All three firmware drive requests are cleared on inhibit: enable, PWM enable and pending duty.
The candidate neither clears rotor momentum nor resets physical heat.

## Font source

`font-source.json` contains explicit 5×7 binary rows. `font.hpp` and `glyphs.json` are generated
from those rows with the documented PX32-16 column/page packing. Regenerate after changing rows,
then rebuild the firmware:

```python
run("platform/tools/experiment.py", "--experiment", candidate, "--generate-font")
```

The runner rejects stale font artifacts. Matching candidate templates proves the pixel encoding
and deadlines in these cases; a human must still judge readability and meaning.

## Workbench regression checks

The actual compiled candidate has additional checks for LCD STOP/PULSE behavior, physical food
loads, fault codes and continued playback. From the repository root, after the Wasm build:

```python
import os
import subprocess
from pathlib import Path

subprocess.run(
    ["node", "--test", "platform/tests/wasm/candidate-view.test.mjs"],
    env={
        **os.environ,
        "B8_CANDIDATE_WEB": str(Path("build/experiment-b16-wasm/web").resolve()),
    },
    check=True,
)
```

These tests use real scanned LCD pixels and the candidate's glyph oracle. They do not supply
human review approval or physical hardware evidence. The original verification receipt remains
historical; subsequent workbench runs are recorded separately in ignored build/report files.

## Supervision status update

The workbench now includes a read-only Supervision tab, monitor enable/lock/count/window/service
probes and a retained 16-event reboot history. Firmware acknowledgment does not erase those events.
B16-004/P2 documents the separate D1 development watchdog fuse; the production candidate retains
both enabled, locked monitors and its original acceptance obligations.

The verification receipt is `build/supervision-verification.json`: 235 checks in each Clang, GCC,
sanitizer and B8 development-fuse native build; 21 real compiled-Wasm checks; six development-Wasm
ABI checks; 68 native/Wasm paired commands; and 243 Python checks. Both selected-candidate native
and Wasm acceptance runs retain 61 automated passes, zero failures and two open human reviews.
The P2 manual was regenerated as pristine, G1, G2 and final G3, with 11 pages per version.
