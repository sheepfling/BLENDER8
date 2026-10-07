# Hardware quirks, failure injections and grading limits

> **Interface-03 revision:** Clock setup, watchdog/deadman progress, reset re-entry and qualified
> power behavior are added by [CLOCK_RESET_CONTRACT.md](CLOCK_RESET_CONTRACT.md). The following
> preserves the original peripheral/behavior exercise; fixed-clock assumptions apply only after
> establishing the approved peripheral rate.

**Document:** QUIRK-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

| Effect                         | Supplied reference behavior                             | Expected lesson                                       | Tier                                          |
| ------------------------------ | ------------------------------------------------------- | ----------------------------------------------------- | --------------------------------------------- |
| Active-low contacts            | Pull-up when open, ground when closed                   | Separate electrical level from pressed meaning        | Core                                          |
| Contact bounce                 | Nominal1.8-ms and slow5-ms deterministic profiles       | Debounce elapsed time, not one known waveform         | Core                                          |
| Mux settling                   | Old output retained2 us after selection/input change    | Select and sample are distinct operations             | Core                                          |
| Scanned-state skew             | Eight contacts are observed at different instants       | Avoid invented atomic snapshots/false illegal states  | Core                                          |
| STOP mechanical linkage        | Releases commands and inhibits drive independently      | Firmware correctness is not the only physical defense | Core                                          |
| Held PULSE through STOP        | Requires release and a fresh press                      | No latent command restart                             | Core                                          |
| Input pin configured as output | Contention becomes UNKNOWN and traps on sampling        | GPIO direction matters                                | Core                                          |
| Timer compare staging          | LO stage, HI commit; disabled-only writes               | Register-order semantics                              | Core                                          |
| Timer/tach read latches        | Low read snapshots high                                 | Prevent torn multi-byte observations                  | Core                                          |
| Shared latch ownership         | A second reader can replace the snapshot                | ISR/foreground coordination                           | Core                                          |
| IRQ W1C                        | Clear only serviced bits                                | Avoid read-modify-write acknowledgment bugs           | Core                                          |
| IRQ coalescing                 | Repeated events produce one pending bit                 | A flag is not a queue or elapsed-time counter         | Core                                          |
| PWM shadow update              | New duty at carrier boundary, disable immediate         | Distinguish requested and active duty                 | Core                                          |
| Dead-zone nonlinear motor      | d≤.12 gives no steady motion                            | Invert desired RPM rather than using percent as duty  | Core                                          |
| Motor inertia/coast            | 120-ms run,350-ms coast constants                       | Drive-off is not rotor-stopped                        | Core                                          |
| Tach quantization/wrap         | 2 pulses/rev;16-bit counter                             | Window sizing and modulo differences                  | Core                                          |
| LCD packing                    | Vertical bits,64-byte address space                     | Pixel memory layout rather than strings               | Core                                          |
| LCD BUSY                       | Premature DATA write is dropped and flags ERROR         | Respect peripheral readiness                          | Core                                          |
| LCD scanout                    | Row-by-row copy,20-ms frame                             | VRAM writes and visible pixels have different timing  | Core                                          |
| LCD pointer wrap               | 63→0 on auto-increment                                  | Bound framebuffer transfers                           | Core                                          |
| ADC acquisition/hold           | Sample after 16 us; finish at 52 us at div4             | Input sampling time differs from result availability  | Core                                          |
| ADC BUSY/overrun               | Bad writes flag ERROR; unread result can be overwritten | Nonblocking driver and explicit acknowledgments       | Core                                          |
| ADC stale result               | Re-reading a number is not a new conversion             | Track sample age/completion                           | Core                                          |
| Sensor offset/gain/noise       | Fixtures with bounded voltage error and recorded seed   | Unit conversion, tolerance, hysteresis                | Challenge                                     |
| Temperature lag                | 250-ms package lag, even if case is already hot         | Sensor reading is not immediate plant truth           | Core                                          |
| Cold no-motion                 | Jam can be cold at first                                | Do not wait for overtemperature before stopping       | Core                                          |
| Hot rotating motor             | Load can heat motor while tach remains healthy          | Independent fault predicates                          | Core                                          |
| Food thermal path              | Optional weak prescribed conductance                    | Thermal cooling competes with load heating            | Challenge                                     |
| Heat retained on power cycle   | Motor/sensor state is physical, MCU state resets        | No warm restart bypass                                | Core                                          |
| Stuck/open contact             | Electrical state can disagree with mechanics            | Invalid combination detection                         | Fault challenge                               |
| Missing tach signal            | Same no-edge signature as a jam                         | Diagnose missing verified motion honestly             | Fault challenge                               |
| Temperature open/short         | Board pull-up or rails produce implausible code         | Fail off on sensor/channel failure                    | Core                                          |
| Plausible frozen temperature   | Can remain in range indefinitely                        | A single sensor cannot prove its own correctness      | Limit demonstration, not guaranteed detection |
| Host infinite loop             | No SDK boundary means no emulated progress              | Harness timeout, not a fictional CPU watchdog         | Harness constraint                            |

## Not modeled or not guaranteed

No analog contact thresholds, capacitance, metastability, EMI, supply brownout analog transient,
driver short-circuit protection, armature current, winding hotspot, load-torque mechanics, blade
injury dynamics, finite food thermal mass or ice melting. No pin-level parallel bus strobes or CPU
instruction timing. No hardware ADC source-impedance error beyond the documented compliant-source
assumption.

No hidden requirement may assume those models exist. A fault injected outside the operating envelope
must be labeled as such and graded against an explicit diagnostic requirement, not an unannounced
“realism” trick. For HIL, unmodeled electrical and timing effects become engineering work that
cannot be wished away by reusing header names.

## Short PULSE holds and the stall-grace limit

Stall timing guarantees apply while drive remains continuously commanded. A short PULSE that ends
before grace and confirmation have elapsed can end without ST; its drive has already been removed by
the operator. Repeated short manual pulses are not guaranteed to produce a cold-stall diagnosis in
this baseline. Thermal/ADC protection remains active throughout. A cumulative
energized-without-motion budget or current-sense limit is an explicit later requirement, not an
unimplemented guarantee.
