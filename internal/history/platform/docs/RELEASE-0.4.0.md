# HalfALabs B8 Platform 0.4.0 — validation record

**B8 interface03; default chassis04. Owner package0.8.0.**

## Delivered

Firmware-only B8 SDK plus typed byte-pair/IRQ helpers; host backend; seven typed component
factories; implemented jar loop/dropout latch; persistent native firmware runtime; timestamped
fixture queue; observation and trace; graphical browser workbench; physical/fault console; scenario,
stress and customer-acceptance runners; tested lockstep motor-link protocol and example display
replacement.

The 62-register supplier contract is unchanged. The current correspondence still owns product
policy. The old jar-interlock implementation gap is closed for the simulator, not certified as
physical guarding. The firmware starter remains intentionally incomplete. The pixel probe is only a
peripheral bring-up image.

## Executed results

| Validation                                      | Result                                                                                                                 |
| ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| Original baseline before refactor               | 116/116 passed                                                                                                         |
| Final platform / GCC14.2                        | 185/185 CTest checks passed                                                                                            |
| Final platform / Clang17 Release                | 185/185 passed                                                                                                         |
| Clang Debug with ASan, UBSan and leak detection | 185/185 passed; no sanitizer diagnostic                                                                                |
| Python native protocol/server/oracle tests      | 22 unittest subcases, included in one CTest job                                                                        |
| Document/correspondence/renderer preservation   | 203 Python tests passed                                                                                                |
| Canonical document/renderer/source inputs       | 102 selected files byte-unchanged from owner0.7.0                                                                      |
| Extended stress                                 | Two seeds,128 episodes each,two identical replays per episode,256 actions;131,072,000 checked one-microsecond steps    |
| Selected external firmware directory            | Built and ran pixel probe as selected firmware; compilation project includes remained SDK-only                         |
| Customer acceptance on unfinished starter       | 63 cases:2 pass,56 fail,5 review; correctly NOT accepted                                                               |
| Visual workbench                                | Desktop/responsive Chromium rendering and native-backed interactions passed;60 lit probe pixels;zero JavaScript errors |
| Physical HWIL / completed product firmware      | Not run / not supplied                                                                                                 |
| Ruff,Pyright,clang-format;Windows/macOS         | Not run                                                                                                                |

Test and process logs are in `evidence/release-0.4.0/`. Runtime/model source changes are exercised
by native tests, not counted as verified merely because document tests passed. Stress is bounded
randomized testing, not a proof of all reachable states or a lifetime endurance test.

The browser was rendered through Playwright's in-memory content with a real native-command bridge
because this environment's managed Chromium policy blocks all URL navigation. The real loopback HTTP
service, authentication/origin/path checks and protocol were tested independently. This is not
represented as an unrestricted end-to-end browser navigation test.

The unoptimized GCC verification followed a compiler-cache reconfiguration; the fresh
extracted-package smoke build uses explicit Release. Sanitizer host timeouts are longer to allow
instrumented debug execution; simulated timing requirements are unchanged. Earlier development runs
found/fixed a stale numeric scenario comparison and a self-test accidentally tied to the selected
firmware image. Final conformance assertions were not relaxed to make model faults pass.

## Remaining boundaries

Physical realtime transport is explicitly rejected by the unpaced logical scheduler. Actual USB/DAQ
adapters, target-side PWM/edge capture, local loss-of-host inhibition and timing measurements remain
required for physical HWIL. The loopback adapter is a protocol check, not hardware validation.
Missing thermal coupling is not converted into imaginary ambient measurements: supply case
observations or replace the sensor component.

The native runtime is cooperative. Nonreturning code with no SDK accesses causes an explicit host
timeout, not an invented WDT reset. Modeled core halt and lost foreground with live IRQ delivery are
separate, tested injections.

Customer acceptance permits any valid font and firmware structure. Missing approved glyphs and
semantic review are reported as review; explicit human approvals are bound to the tested executable
hash. Failed behavior cannot be overridden by review. A subset cannot be reported as full
acceptance. No positive approval record is included for the unfinished image.

The single jar contact does not prove detection of welded/bypassed contacts. No brake, guard lock,
physical fuse ratings, single-fault tolerance or safety certification is asserted. Reset/drive
removal are not equivalent to a stationary shaft.

## Fresh archive smoke test

The native ZIP was extracted into a new directory. Every listed source hash matched. With a fresh
CMake cache and explicit GCC Release configuration, all targets built and all **185 CTest entries
passed** again. This validates included source files, local relative paths, the selected starter
fixture and Python launchers independently of the working build cache. Only validation records were
added after that smoke run; model, SDK, UI and runner code were unchanged.
