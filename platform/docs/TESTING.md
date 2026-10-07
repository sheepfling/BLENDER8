# Test layers and reproducible stress

## Conformance, acceptance and stress are different results

**CTest** validates the B8/model/platform contract and tool behavior. It includes original
register/peripheral/clock checks, compiled supplier-manual examples, standalone header compilation,
new jar and replacement-interface cases, protocol/server/oracle tests, analytic physics checks,
replay scenarios, and a bounded seeded stress job.

**Customer acceptance** runs the selected firmware through current correspondence behavior. A
drive-off stub can pass safe-start checks but must fail speed, response, fault and recovery cases.
The harness contains real precondition witnesses: it requires observed normal running before testing
a running jam or STOP. "The motor never moved" cannot make every shutdown test pass.

**Stress** is a perturbation/invariant test, not customer firmware approval. Its fixture
deliberately requests raw drive and services the watchdog so it can exercise gate/plant combinations
without supplying a firmware solution. This action is explicit in the stress source and never occurs
in normal emulator or product-acceptance mode.

## Current conformance coverage

| Area                | Evidence                                                                                                                                                                     |
| ------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Digital connections | all mux selections, settling and restarted delay, contention/floating reads, physical contact interlocking, pulse/STOP, held controls                                        |
| B8 data semantics   | all register snapshots, W1C, staged writes, low/high captures, masked/coalesced IRQs, priorities and reset inside ISR                                                        |
| Clock/reliability   | source qualification, legal/illegal PLL plans, protected keys, PB-dependent timing, independent WDT, DMT windows, clock halt/failure, power branches                         |
| Analog/thermal      | acquisition/hold/completion, fresh-read ownership, overrun, reference variation, sensor lag/noise/faults, retained temperature                                               |
| Jar latch           | absent startup, immediate dropout, one-microsecond interruption, bounce, broken wire, held STOP, exact closure boundary, clear dominance, reset and GPIO mapping             |
| Runtime             | persistent chunks, zero run, actual reset re-entry, transient source/core pauses, service budget, timestamped callbacks, event ordering and recursion rejection              |
| Substitution        | all factory slots invoked, replacement display through unchanged B8, loopback motor pins, absent probes, null factories, invalid/stale/reordered link samples                |
| Physics             | analytic nonlinear curve sweep, first-order run-up/coast/load step, thermal step subdivision and equilibrium, process heat path                                              |
| Tooling             | JSON protocol errors, no firmware-mode pokes, nonmutating snapshots, real host timeout, CSV trace, local server origin/token/size limits, deliberately wrong scenario oracle |

The historical interface02/03 tests explicitly select those profiles. Production/default Board uses
chassis04. `history.contract_snapshot` verifies retained source history; it does not reimpose
retired application constants. Tool self-tests use `b8_starter_emulator`, independent of the
firmware selected for `b8_emulator`.

## Scenario replay

Ten JSON scenarios are in `scenarios/conformance`. Each states `mode`, profile, timestamped
physical/register events and observations. Bench register writes are explicit. There is no blanket
auto-feed or simulated application controller hiding in the runner.

```sh
python tools/scenario.py scenarios/conformance/jar_dropout_memory.json \
  --exe build/b8_emulator --output reports/jar.json
```

Absolute timestamps are logical microseconds. Past/negative times, empty assertion sets, unknown
fields and unbounded jobs are rejected. Reports retain input hash, requested and actual sample
times, assertions and outcomes. Numeric int/float equality is accepted, but booleans do not
masquerade as integers. Failed comparisons return nonzero.

## Seeded stress

```sh
build/b8_stress --seed 4815 --episodes 128 --actions 256
```

Each episode runs **twice** from the same seed and compares a digest of the physical trajectory. The
action mix includes speeds, PULSE/STOP, jar motion, jams, load, contact faults, sensor/tach faults,
brownout, branch fuses, reset, jar wiring faults, environment and rear power. Contact bounce is
independently seeded within its published envelope.

Every action advances 1,000 one-microsecond steps. At every step, checks enforce finite bounded
RPM/temperature, physical speed one-hot behavior, STOP dominance, and the complete
power/reset/jar/run-permit enable chain. On failure the executable prints seed, episode, action
history and timestamp. The same seed and tested build can replay the case; floating-point trajectory
hashes are not promised identical across compilers/platforms.

For 128 episodes, 256 actions and two replays, a run checks 65,536,000 microsteps. It is not a
year-long endurance test or stochastic proof of all reachable states. Thermal long-horizon checks
use the analytic subsystem, rather than calling a very short mixed-event stress run a complete
heat-soak campaign.

## Isolation and diagnostic evidence

Native tests are separate processes with CTest timeouts. The deliberately nonreturning fixture
proves a true host timeout is distinguishable from emulated WDT. ASan/UBSan builds exercise the same
targets when enabled. The physical-link test is loopback only.

Evidence is recorded under `evidence/release-0.4.0/` in the delivered source. Platform CTest passes,
product acceptance counts, document-renderer tests, UI browser checks and unperformed physical
testing are reported separately. A finite suite can expose errors but cannot certify a safety
function.
