# Half-A/Labs / Blender-8 correspondence revision

**Owner record: P5 / Foundry 0.6.0. B8 interface 03 and reference platform 0.3.0 are unchanged.
Chassis 04 interlock is specified, not implemented.**

## Authorship and distribution

Rick at Half-A/Labs collects decisions from customer Mara Ellis at Kestrel and hands the old
correspondence to the firmware team. Seven five-message exchanges follow Mara → Rick
with a
sketch → Mara → Rick's final interpretation → Mara's approval. Rick's later handoff and two retained
drawings complete the ten-page file.

These messages, dates and approvals were authored for this project. They are not retrieved/sent
email or evidence of actual customer approval. Reserved `.example` addresses in the source and
printed signatures are fictional. No mail connector was used. Keep this provenance and crosswalk OUT
of the recipient packet. Kestrel remains the customer organization and chassis/button-assembly
source; Half-A/Labs is the firmware contractor.

## Authority changes, not a hidden detailed rubric

The old formal brief is replaced, not appended. Recipient copies have no R-numbers, rubric,
acceptance matrix or project-provenance labels. The last Rick message and following Mara approval
settle each topic; later clock/watchdog messages extend the earlier fault-label priority.

The exact 8 ms scan/16 ms debounce prescription, 20 ms release/acknowledgment counters, 200 ms tach
window, 300 RPM/150 ms/200 ms stall recipe, ADC band 62..574, fault-bit assignments and report
template are **no longer acceptance requirements**. They are explicitly delegated or owner-only.
Confirmed outward timing, speed, temperature, recovery, clock and supervision limits remain. Do not
require New Employee to reproduce retired constants in a different correct design.

The previous brief is preserved under `internal/history/docs/P4/`. Controlled
`internal/authoring/source_inputs/` and the
reference-platform archive remain unchanged for provenance, not as a competing current
specification. The old platform's firmware acceptance scenarios are NOT a P5 grader; migrate those
scenarios before evaluating new firmware. No completed-firmware acceptance is claimed. Generic
supplier manuals retain their original authorship and content.

The two current r4 circuit drawings are owned by interlock-02 and approved through interlock-05. The
clock thread originally mentions r3 sheets; the forwarding email explicitly retires them. The
current drawings retain chassis-03 electrical/power facts and unreleased physical fuse ratings; they
are not fabrication or mains instructions. Schematics are authoritative for fixed wiring; inline
behavioral sketches are clarifications, not compulsory software architecture.

## Prior-clause dispositions

Machine-readable source: `internal/owner/requirements-crosswalk.json`. IDs below refer to
messages in
`internal/authoring/correspondence/archive.json`, not printed requirement numbers.

| Prior | Current messages                           | Treatment | Disposition                                                                                                                                                 |
| ----- | ------------------------------------------ | --------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| R-001 | handoff-01                                 | retained  | B8-only access is a Half-A/Labs handoff constraint.                                                                                                         |
| R-002 | stop-04, temperature-04                    | mixed     | Fail-off startup retained; the former before-interrupt ordering is no longer a customer prescription.                                                       |
| R-003 | stop-04, temperature-04                    | mixed     | Cool two-second startup and released controls retained; exact 20 ms release counter delegated.                                                              |
| R-004 | panel-04, handoff-01                       | delegated | Exact 8 ms scan and 16 ms debounce algorithm retired; meet outward response and supplier contracts.                                                         |
| R-005 | panel-04                                   | retained  | 40 ms after final stabilization; guaranteed PULSE holds at least 50 ms.                                                                                     |
| R-006 | panel-04                                   | retained  | Two speed indications persisting for 40 ms stop with IF; no arbitrary transient winner.                                                                     |
| R-007 | stop-04                                    | retained  | 10 ms STOP-to-firmware-output removal; no release or held-PULSE restart.                                                                                    |
| R-008 | panel-04                                   | retained  | Seven equal RPM steps 3000..18000; at one second no-load error max(100 RPM,2%); loaded sag allowed.                                                         |
| R-009 | panel-04                                   | retained  | PULSE +2000 RPM, capped 20000; 2000 from idle; restore selection on release.                                                                                |
| R-010 | stop-04                                    | retained  | Firmware output removal separate from the independent hardware gate.                                                                                        |
| R-011 | panel-04, temperature-04                   | retained  | 0, 1..7, P, 1P..7P and fault display timing; boot artwork not required.                                                                                     |
| R-012 | handoff-01, temperature-04, supervision-04 | mixed     | Target-bound execution and outward deadlines retained; a global 10 ms supervisor polling architecture is not required.                                      |
| R-013 | handoff-01                                 | supplier  | Coherent register access, acknowledgment and target timing belong to the B8 contract, not a customer coding recipe.                                         |
| R-014 | motion-04                                  | delegated | Fixed rolling 200 ms tach window and last-change estimator explicitly delegated.                                                                            |
| R-015 | temperature-04                             | retained  | Fresh completed temperature readings at least every 10 ms.                                                                                                  |
| R-016 | temperature-02, temperature-04             | supplier  | ADC and sensor transfers remain supplier facts; code representation and conversion implementation are open.                                                 |
| R-017 | temperature-04                             | retained  | Two successive plausible fresh readings >=85 C qualify TH; drive off within 10 ms thereafter.                                                               |
| R-018 | temperature-04                             | mixed     | Disconnected/grounded/railed channel and >100 ms without fresh sample stop with TS. Exact old ADC band 62..574 delegated, not hidden grading.               |
| R-019 | motion-04                                  | retained  | 500 ms ordinary run-up only on actual commanded off-to-on; speed/PULSE does not renew it.                                                                   |
| R-020 | motion-04                                  | mixed     | 750 ms blocked-start and 400 ms running-lock deadlines retained; 300 RPM/150 ms/200 ms estimator recipe delegated.                                          |
| R-021 | motion-04, temperature-04                  | retained  | Cold jam and hot rotation are independent; intentional coastdown is not ST.                                                                                 |
| R-022 | temperature-04, supervision-04             | mixed     | Keep qualified causes and agreed priority; old numeric bit assignments are implementation detail.                                                           |
| R-023 | temperature-04, supervision-04             | retained  | 100 ms visible fault response; reset labels within 100 ms after reset release. Later thread extends priority to CK WD DM TS TH IF ST.                       |
| R-024 | temperature-04, temperature-05             | mixed     | \<=65 C for two seconds, NEW STOP, release all, NEW selection. Fixed 20 ms acknowledgment/release counters delegated.                                       |
| R-025 | stop-04, supervision-04                    | retained  | All resets requalify; latches, heat, motion and warm LCD state are not presumed erased.                                                                     |
| R-026 | motion-04, temperature-04                  | retained  | No automatic retry, kick or restart from cooling, repairs or obstruction removal.                                                                           |
| R-027 | stop-04, supervision-04                    | mixed     | Output/gate/coast distinction retained; exact trace-column format remains owner tooling.                                                                    |
| R-028 | handoff-01                                 | owner     | Repeatable evidence requested; exhaustive perturbation and seed protocol is owner conformance work, not secret customer criteria.                           |
| R-029 | stop-04                                    | owner     | Independent plant behavior remains an owner architecture invariant; firmware-output evidence is in the agreed STOP response.                                |
| R-030 | handoff-01, supervision-04                 | mixed     | Source, local build/test command, clock plan and awkward-case traces; prior report template no longer prescribed.                                           |
| R-031 | clock-04                                   | retained  | XO8 external PLL, SYSCLK 8..32 MHz, PBCLK 1 MHz +/-0.1%; New Employee chooses legal factors.                                                                |
| R-032 | clock-04, handoff-01                       | mixed     | Clock calculation/margins and nominal PWM 3906.25 Hz retained; prescribed form and mandatory rejected-tuple example become owner suggestions.               |
| R-033 | clock-04                                   | retained  | Healthy run clock verified within 100 ms of first reset entry; drive off during settling; absent source CK/off; internal clock only startup/fault handling. |
| R-034 | clock-04                                   | retained  | Active external clock loss inhibits within 1 ms; no automatic restart; local device timings not rescaled.                                                   |
| R-035 | clock-04, stop-04, supervision-04          | retained  | Chassis-03 signal, power, switch, fuse-position and reset facts retained in two approved drawings.                                                          |
| R-036 | supervision-04                             | retained  | WDT configured/locked before drive; slowest timeout \<=250 ms; qualified useful progress, not unconditional interrupt feed.                                 |
| R-037 | supervision-02, supervision-04             | mixed     | Fresh acquisition/protection/output evidence and no double-use retained; token structure and scheduler order not prescribed by the sketch.                  |
| R-038 | supervision-04, supervision-05             | retained  | DMT configured/locked before drive; slowest-clock expiry \<=100 ms; engineer chooses window. Detailed key fault injections are owner component tests.       |
| R-039 | supervision-04                             | retained  | WD DM CK indication, full priority, capture causes, bounded display after release and deliberate recovery.                                                  |
| R-040 | supervision-04, handoff-01                 | mixed     | Agreed live-tick/lost-foreground, clock-loss and logic-brownout demonstrations. Other old edge cases remain owner verification ideas.                       |

## Late jar interlock: proposed chassis 04

The user added the jar-engagement interlock during this correspondence revision. The authored late
thread (interlock-01..05) asks whether it is jar or lid, discusses what reseating should do, and
records the proposed jar-only answer and accepted restart sequence. This scope is a **draft
assumption**, not independently obtained customer evidence. The lid, guard locking, an electrical
brake and access-time validation are not included.

S2 is a low-energy loop closed only by the fully seated/locked jar. A broken loop is low. It feeds
the independent enable chain and clears the reset-dominant U_IL permission latch. PB3 is raw JAR_OK;
PB4 is latched JAR_PERMIT. U_IL is disarmed by opening, power loss or controller reset; it cannot
automatically re-arm on closure. A fresh STOP assertion after 20 ms continuously closed can set the
latch; a STOP already held before qualification cannot. STOP's independent command-release/inhibit
mechanism ensures that setting this latch does not itself energize the drive. Clearing dominates
simultaneous setting. Every open in the specified 5 ms contact-transition envelope drops the latch.

This extra hardware memory is deliberate: a bare series contact can close again before the firmware
observes an opening. A diagram that only adds an AND input cannot justify the stronger no-restart
promise for a very brief interruption. The circuit and observation paths must be explicit, not
hidden inside a fixture convenience. The 20 ms hardware closure qualifier is new chassis behavior,
not the retired blanket firmware-debounce recipe.

The proposed outcome limits are 1 ms maximum hardware inhibit after opening; 10 ms maximum
firmware-request removal after trip; IL indication within 100 ms, subject to the full CK, WD, DM,
TS, TH, IF, IL, ST priority. IL cannot create a false ST during intentional coastdown. Cooling and
other faults continue to be evaluated. All starts, including after reset, now require the jar
condition, existing thermal readiness, a fresh STOP, released controls and a later new command.

**Runtime implementation is pending.** The bundled C++ reference is the unchanged v0.3.0 chassis-03
baseline. It has no S2/U_IL, PB3/PB4 interlock mapping, dropout latch or jar injection methods. The
Python tests validate document structure and rendering, NOT those new electrical behaviors. Do not
distribute this old executable as a conforming chassis-04 target or claim these new cases pass.
`spec/jar-interlock-cases.json` is a pending test/design record, not an implemented scenario player.
The reference SDK need not change: the added signals use ordinary GPIO. Board composition, component
model, fault injection and conformance tests do need changes.

## Safety limits and source context (owner only)

The single loop treats an open circuit conservatively; it cannot distinguish a correctly closed
switch from a welded/bypassed contact or a short to the permissive level. No single-fault tolerance,
Performance Level, SIL rating, appliance certification, real switch part number or physical
construction adequacy is asserted. The functional drawing has no released motor-current switch, lock
actuator or brake. Motor coastdown still exists; an access review is explicitly outside the
customer's firmware acceptance in the authored exchange.

Primary context: Pilz, *Safety switches with guard locking*,
<https://www.pilz.com/en-INT/products/sensor-technology/safety-switches-with-guard-locking>
(accessed during this revision). Its distinction between interlocking, hazardous overrun and guard
locking motivates keeping the access review separate. It does not validate this circuit or transfer
any Pilz product certification to Blender-8. The 1/10/20/100 ms values here are project choices, not
values sourced from an appliance safety standard.
