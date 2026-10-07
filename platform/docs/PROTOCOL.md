# Native emulator protocol and workbench

Start `b8_emulator` for newline-delimited JSON responses to one ASCII command per line. A startup
hello includes protocol 1, firmware identity, chassis, state and `ok`. Every command returns one
object. Parse errors return `ok:false` and preserve a usable process; a model/runtime failure
latches the host inhibit and cannot be cleared by pretending a normal firmware reset repaired the
host.

Use `--bench` to run without firmware and allow explicit register writes. `--legacy02` is accepted
only with bench mode. `--bus-trace file.csv` records actual firmware bus transactions. The normal
firmware mode rejects raw register commands. Command and reply sizes, event queue, run interval,
trace sample count and event count are bounded.

## Time commands

- `snapshot`: no time or register side effects.
- `run MICROSECONDS`: advance through the firmware runtime (or raw board in bench mode); report
  actual time.
- `trace MICROSECONDS SAMPLE_US`: periodic samples and transition events plus final state. Maximum
  1,000 periodic samples and 10,000 recorded transitions per call; saturation fails explicitly.
- `schedule ABSOLUTE_US COMMAND`: enqueue a validated physical/bench command at exact logical time.
  Stable insertion order for ties; past or recursive scheduling fails.
- `quit`: graceful process termination.

## Fixture commands

| Command                                                              | Allowed values                                                                 |
| -------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| `speed N`                                                            | 1..7, physical latching press                                                  |
| `pulse B`, `stop B`, `jar B`, `power B`                              | 0 or 1                                                                         |
| `jam B`, `halt B`, `clock_failed B`, `foreground B`, `adc_stalled B` | 0 or 1; distinct physical/core/execution injections                            |
| `load X`                                                             | 0..1 normalized load                                                           |
| `temperature C`                                                      | -40..150; explicit case-state injection, not natural instantaneous cooling     |
| `food TEMP G`                                                        | replace/remove food only; preserve case and air; G 0..0.2 W/K                  |
| `environment ROOM FOOD G`                                            | replace nearby air and food at these temperatures; case-food G 0..0.2          |
| `sensor MODE`                                                        | healthy, open, ground, supply, frozen                                          |
| `tach MODE`                                                          | healthy, low, high                                                             |
| `jar_fault MODE`, `jar_bounce B`                                     | healthy/open/bypassed; bounce 0/1                                              |
| `contact CHANNEL MODE`                                               | 0..7; normal/closed/open                                                       |
| `bounce MODE`                                                        | off, nominal, slow, or `random SEED`                                           |
| `food_preset NAME`                                                   | empty, water, frozen_fruit, hot_vegetables; coupled load and food thermal node |
| `noise AMPLITUDE SEED`                                               | 0..0.02 V amplitude and uint32 seed                                            |
| `calibration OFFSET GAIN`                                            | offset +/-0.05 V; gain perturbation +/-0.1                                     |
| `xo_ppm N`, `frc_ppm N`, `lfrc_ppm N`                                | +/-50, +/-50000, +/-100000 respectively                                        |
| `voltage VALUE`                                                      | auto or 0..3.6 V logic-rail override                                           |
| `fuse BRANCH B`                                                      | input/logic/motor; 1 intact, 0 open                                            |
| `reset`                                                              | external MCU reset, not a reset of every physical state                        |
| `write ADDRESS VALUE`                                                | bench only; integer decimal or 0x notation                                     |

The command parser validates all tokens/ranges before constructing the action. Some reference
fixture actions are unsupported if that component was replaced: they fail rather than falsely report
an applied hardware fault. All values and flags can also be exercised through the native host API.

## Observations

State includes logical time, actual power/rail/motor supply, reset readiness, host-failure inhibit,
physical contacts and run permission, raw/latching jar signals, PWM/gate, analog voltage, optional
motor/sensor probes, B8 register observations, ADC fresh-read evidence, per-vector delivered-IRQ
counters and a 512-bit scanned LCD image. These are **host instruments**. No extra firmware
registers are created.

`mcu.watchdog` and `mcu.deadman` report read-only enablement, locks, counters, key state,
configuration error, completed service counts and actual clock rates. `mcu.reset_history` retains
the latest 16 actual reset events with serial, logical timestamp, cause/detail masks and decoded
names; firmware W1C acknowledgment does not erase this host history. See
[supervision configuration and reset reasons](SUPERVISION.md).

`lcd_bus` is an optional, read-only display-boundary probe. It reports lifetime `reads`, `writes`,
`data_writes` (attempts) and `rejected` counts, along with the current address, control, BUSY,
ERROR, scan epoch/phase and ready timestamp. Chronological arrays retain the latest 32 `transfers`,
64 DATA-write attempts in `data` and 32 `blank_edges`. Each transfer includes its lifetime serial,
logical microsecond timestamp, direction, register, byte, VRAM address before access, acceptance
and VBLANK level. Blanking edges include the actual level and an LCD-reset marker. Captures survive
LCD resets; restarting the session clears them. Missing instrumentation returns `null`.
Standalone snapshots include these histories with `captures_included: true`. Dense trace samples
and event points carry the same counters and current status with `captures_included: false` and
omit the three capture arrays. The trace reply's final `state` includes the full histories.

The LCD bus view resolves exact byte transactions independently of the contact scope. It includes
DMA writes that reach the LCD and excludes CPU accesses rejected by DMA ownership. See
[LCD bus and blanking](ANIMATED-WORKBENCH.md#lcd-bus-and-blanking).

ADC read evidence counts only firmware DATA_LO reads that consume a fresh READY result. A new host
snapshot does not count as a new acquisition. IRQ observations are lifetime counters, not
reset-state registers. If a replaced motor or display lacks a probe, its observation is null.

## Browser service

`tools/emulator.py` wraps exactly this native process. It binds only to loopback, requires an
unpredictable per-session token and the local Origin/Host on POST, rejects arbitrary paths and
oversized bodies, and serializes commands. No third-party web assets or network API are used. UI
controls change fixture inputs. Plots/pixels are outputs, not UI simulations of the component
behavior.

The Python Engine has a wall-clock response deadline. Its expiry kills the subprocess and reports
HOST_CALLBACK_TIMEOUT; it cannot claim physical outputs were de-energized or invent a target
watchdog event. Native code is trusted code in this development setup, not a security sandbox. The
browser's start/pause controls pace requests for convenience, not realtime-HWIL timing.

Native stdout is reserved for JSON-lines replies. Firmware/debug diagnostics should use stderr or
the provided bus trace. The UI Pause button pauses logical world time; it is not a hardware halt. To
test a stopped core with clocks and watchdog continuing, inject `halt 1` and continue advancing
time.
