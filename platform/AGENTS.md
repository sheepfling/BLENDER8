# B8 platform engineering rules

Preserve B8 interface 03 and its 62-register semantics unless a separately versioned device change
is intentional. Firmware may include only the SDK and its own files. Simulator classes, debug probes
and host clocks must never leak into that target.

Selected firmware uses the B8 numeric policy by default: byte integers and no float. The B16
policy permits byte/word integers and binary32 float. Neither allows doubles or 32-bit integer
data/intermediates. Use the Clang AST gate and `numeric.hpp` helpers; do not apply these restrictions
to plant physics or trusted host code. --device selects B8/interface03 or B16/interface04 and the
matching numeric policy. B16 implements SRAM, DMA, pin selection and six IRQ vectors. Binary32
behavior is compiler-backed; neither model implements an instruction ISA or FPU instruction timing.
Keep those evidence boundaries explicit. The B16 chassis uses PB5 PWM, PB1 TACH, PB6 VBLANK and
PA4/AN0 temperature; route selection never moves physical board wires.

Current product requirements: `requirements/correspondence.json` / `.md` (P5, r4 drawings).
Historical specs are source history, not a secret grader. Engineer is New Employee; contractor
is Rick at
Half-A/Labs; customer is Mara.

Default Board is chassis04, with implemented jar latch and PB3/PB4 mapping. Legacy profiles exist
only for regression. Do not hide application protection in the plant. Missing physical probes return
no data, not fabricated truth. HIL transports must declare timing and fail inhibited; the current
adapter rejects physical realtime under the unpaced scheduler.

Keep conformance, stress, product acceptance, human review and physical testing separate. Never
claim an unfinished starter passed product acceptance. Keep seeded replay and analytic physics
tests. Build GCC/Clang; run CTest; use sanitizer configuration. Run `tools/acceptance.py` on the
actual selected executable and identify reviews/failed preconditions.

A cooperative native callback with no SDK access can block logical time. A process timeout is not a
watchdog reset. Never solve that by secretly feeding watchdogs, suppressing resets, jumping physics
time or pretending unexecuted instructions were counted.
