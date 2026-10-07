# Watchdog, deadman and reboot status

Open **Supervision** in the animated workbench, or focus the canvas and press **U**. Both B8 and
B16 expose the same supervision registers. The tab observes the actual loaded MCU model; it
cannot enable, disable, feed or unlock a firmware monitor.

## Firmware choices

The supplied production option retains the interface-03 contract: WDT is fused on at reset,
scale 3, unlocked. DMT starts disabled and unlocked. The customer release requires both monitors
enabled and locked before drive permission. Development experiments with a monitor disabled do
not satisfy those acceptance requirements.

The separately selected **D1 development supervision option**, defined in the
[B16-004/P2 follow-on manual](../../latex/mcu/b16-follow-on.tex), replaces only the watchdog fuse.
WDT starts disabled and unlocked; `WDT_CTRL.ON` becomes writable. No addresses or numeric profiles
change. All other watchdog clock, scale, key, timeout and reset semantics remain the same.
The fuse belongs to the compiled image and survives MCU resets. Restarting a session does not
change it. The status tab identifies the selected fuse explicitly.

From `platform/`, use separate build directories for development images:

```sh
python tools/b8.py --build-dir build-dev-b16 build --device B16 --watchdog firmware
python tools/b8.py --build-dir build-dev-b16-wasm wasm-build --device B16 --watchdog firmware
```

These commands use the editable `platform/firmware/` sources. Add `--firmware PATH` to select another
source directory; relative paths are resolved from the explicit platform root.

Use `--device B8` for B8. The default `--watchdog forced` selects the original production fuse.
The equivalent CMake option is `-DB8_WATCHDOG_FUSED_ON=OFF` for D1, or `ON` for production.
Fixed conformance starter fixtures retain their production fuse independently of this choice.
Upload the matching `.mjs` and `.wasm` from the selected build's `web/` directory. A page selector
cannot override a fuse or a locked monitor in an uploaded image.

| Register   | Value  | Firmware action                                            |
| ---------- | ------ | ---------------------------------------------------------- |
| `WDT_CTRL` | `0x00` | Disable WDT on D1 while unlocked; production fuse stays on |
| `WDT_CTRL` | `0x01` | Enable WDT while unlocked                                  |
| `WDT_CTRL` | `0x81` | Enable and lock WDT until MCU reset                        |
| `DMT_CTRL` | `0x00` | Disable DMT while unlocked                                 |
| `DMT_CTRL` | `0x01` | Enable DMT with a valid configured window and limit        |
| `DMT_CTRL` | `0x81` | Enable and lock DMT until MCU reset                        |

Write these through `b8::write8` in firmware. Configure WDT scale before locking; configure DMT
limit/window while disabled and unlocked. B8 writes explicit low/high bytes; B16 may also use the
SDK's ordered byte-pair helpers. Writing CTRL
with unchanged enablement does not renew either monitor. A change of enablement resets its count
and pending key sequence. Locking a disabled D1 monitor (`0x80`) keeps it disabled until reset.
Service writes to disabled WDT set configuration ERROR without a reset. A locked D1 control change
sets ERROR and leaves its enablement unchanged. Production control writes retain their published
fused-on behavior. Hardware service timing does not establish useful application progress.

## Live instruments

Both monitors show enablement, lock, count/limit, completed service pairs since the latest MCU
reset, pending first key and configuration error. DMT also shows the service window's lower bound
and whether the window is open. The displayed time to limit is approximate, based on whole ticks
and the current actual clock; it is not a promised wall-clock deadline. When counting is paused,
that number describes remaining clocked time after resumption.

WDT uses independent LFRC, including its configured tolerance, and continues through core halt.
DMT uses SYSCLK/1024 and pauses with a halted core or stopped system clock. Both pause while reset
is asserted. Page RUN/PAUSE controls logical time for the entire machine. **CORE HALT** and **NO
FOREGROUND** remain fault fixtures; observe the enabled monitors' response and the firmware LCD.
No physical operator-held deadman switch is modeled.

## Reboot reasons and retained history

`RST_CAUSE` is an accumulating bit mask, not a mutually exclusive number. Capture all bits before
acknowledging them with write-one-to-clear (W1C).

| Hex bit | Name  | Meaning                   |
| ------- | ----- | ------------------------- |
| `01`    | POR   | Power-on reset            |
| `02`    | BOR   | Brownout                  |
| `04`    | WDT   | Watchdog                  |
| `08`    | DMT   | Deadman                   |
| `10`    | CLOCK | Clock failure             |
| `20`    | SOFT  | Firmware software reset   |
| `40`    | EXT   | External controller reset |

`RST_DETAIL` bits are `01` WDT bad key, `02` DMT bad first key, `04` DMT bad second key,
`08` DMT early service, `10` DMT late/expired, and `20` WDT timeout (all hex).

The tab shows the current register latches separately from reset events. The host records each
actual MCU reset with its serial, logical timestamp, cause mask and detail mask. W1C and POR do not
erase this diagnostic history. It retains the latest 16 events; the tab shows the newest five and
debug snapshots contain all retained events. **MCU RESET** adds an external reset event;
**Restart session** creates a new machine and history. Initial reset assertion and qualified
power-on release can produce separate POR records. Browser failures/timeouts do not become MCU
reboot reasons.

Debug logs include newly observed reset events, even when firmware cleared the cause registers
before the next frame. If more events occurred than the retained history can cover, the log
reports the unavailable count. Scene and snapshot probes never read destructive MMIO, refresh
monitors, alter key sequences or create firmware fault codes.
