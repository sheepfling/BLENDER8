# Firmware acceptance from the customer correspondence

`tools/acceptance.py` runs separate native processes for independent cases. Stimuli are physical
fixture operations; the harness cannot write student registers. It observes actual gate, firmware
GPIO/PWM requests, tach/physics, ADC read completion, reset state and **scanned pixels**.

Run all cases:

```sh
python tools/b8.py accept --output reports/acceptance.json
```

The supplied starter is expected to fail functional cases. A selected subset is useful during
development:

```sh
python tools/b8.py accept --case startup.external_clock --case panel.speed_1
```

A subset can return successful selected results but never sets full `accepted: true`. The output
identifies `full_suite`, `selected_passed`, counts and source message IDs. Every failure includes
reproducible fixture commands and relevant observations. `--expect-incomplete` on the lower-level
script is a **harness self-test flag**, not a product acceptance mode; it succeeds only when a
functional case fails, and does not change the report's acceptance value.

## What is checked

The inventory covers startup outputs/clock, both locked execution monitors before drive, all seven
speed targets, PULSE alone and boost/release, STOP cutoff/held-PULSE, jar loss/quick
interruption/reseating/broken lead, failed starts and running jams, speed/PULSE not renewing startup
grace, missing tach, permitted load, invalid simultaneous speed indications, thermal
trip/freshness/channel faults, deliberate recovery, missing/lost external clock, logic brownout,
retained power-cycle commands, real core halt and lost foreground with live IRQ opportunities, and
the displayed labels.

Output removal checks firmware's enable request, PWM enable **and pending duty** separately from the
independent hardware gate. Transition timestamps enforce the deadlines. Analytic physics conformance
is tested separately: firmware cannot satisfy acceptance by resetting the simulated rotor to zero.

Acceptance leaves New Employee's debounce algorithm, scan architecture, tach estimator, internal fault
representation and analog plausibility implementation open. The current message source is the
contract. Tests do not require retired fixed scan/tach/plausibility constants.

## Fonts and explicit reviews

A label cannot be recognized without defining what its pixels mean. No particular font is required.
Supply a JSON map of approved labels to one or more full 512-character `0`/`1` scanout frames
through `--glyphs`. Pixels are ordered row-major: y=0..15, x=0..31, exactly as the workbench
observes the panel. The map can contain frames for different allowed variants, but not arbitrary
wildcard output. When a template is supplied, mismatched output fails; a review cannot override that
failure.

Without an approved template, a reached display case reports `review`, not `pass`. Timed
fault-display cases record observation intervals and deadlines. The initial missing-clock
classification deadline is not more tightly invented than the correspondence; that case uses a
bounded observation and the separate no-drive check. Semantic progress authorization and retained
multi-fault priority need explicit source/trace review; valid watchdog keys alone cannot prove
useful work or the existence of subordinate software fault state.

A review record is accepted only when its executable SHA-256 matches the tested image. It requires
case name, reviewer, timezone-qualified approval timestamp and an evidence description. Example
structure (replace all placeholders with actual review):

```json
{
  "executable_sha256": "SHA256-OF-THE-TESTED-BINARY",
  "approvals": {
    "review.progress_authorization": {
      "reviewer": "Reviewer name",
      "approved_at": "2026-10-07T10:00:00-05:00",
      "evidence": "Source review and trace locations showing fresh epochs and stale completion rejection"
    }
  }
}
```

Pass that file using `--reviews`. It is a recorded human assertion, not authentication,
certification or an automatic proof. A record can resolve a pending review only; it cannot convert a
failing functional test into a pass. No approval is supplied for the unfinished starter. Full
acceptance requires all automated cases and unresolved reviews to be satisfied.

## Deliberate scope limits

Finite black-box tests cannot prove an arbitrary C++ program free of defects. Blind ISR feeding and
missing foreground work have a concrete injection, but source-level progress reasoning is still
required. Full multi-fault state retention is reviewed without dictating private fault-bit layout.
Physical HWIL timing, signal integrity, braking, guard locking, access hazards and fuse ratings are
not certified by this harness.

The acceptance runner is delivered and executed against the unfinished starter to demonstrate
rejection. No completed reference blender firmware is hidden in the emulator, and no product pass is
claimed.
