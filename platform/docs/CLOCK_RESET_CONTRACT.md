# Blender-8 clock, execution supervision and power revision

> Historical interface-03 design record. For platform 0.4.0/chassis04 execution, replacement
> contracts and current acceptance, read README.md, ARCHITECTURE.md, COMPONENTS-AND-HWIL.md and
> ACCEPTANCE.md first. Historical algorithm choices are not reinstated.

**Owner engineering record — B8 interface 03; reference platform 0.3.0; document foundry 0.5.0;
publication P4.**

## Release and ownership

This is a behavior revision, not just a new photocopy appearance. The prior B8 interface 02 assumed
a fixed 1 MHz peripheral clock and had no device watchdog. Interface 03 boots from an internal FRC,
exposes a clock tree and hardware reset causes, starts a fused-on independent watchdog, and supplies
an optional-to-enable windowed deadman timer. Final Kestrel acceptance requires both monitors to be
configured and locked before running.

The recipient packet now has seven documents: Kestrel email/attachments (9 pages), generic B8 (11),
mux (3), LCD (5), motor with sensor papers (7), button assembly (3), external oscillator (2). Total:
40 pages. The motor and sensor remain separate suppliers within a single handout. No independent
board manual was reintroduced: circuit ownership stays in the email attachments. No application pin
assignments, fault names or chosen divider tuple belong in the MCU or oscillator manual.

Meridion Frequency Products is the added supplier. XO8-33 is a powered 3.3 V, nominal 8 MHz
crystal-controlled CMOS oscillator module, not a bare resonator. EC receives that logic-level
waveform at CLKIN. A bare crystal would require an MCU oscillator amplifier/pins and specified
load-capacitance/drive conditions; that circuit has not been invented implicitly. Its two-page sheet
includes operating frequency, combined tolerance, startup, OE, signal terminals, bypass and
placement/load guidance.

These are deliberately defined in-universe component interfaces. Commercial PIC32 documentation
informed the concepts, not the numerical limits or register encoding. There is no claim of PIC32
register, oscillator, instruction-set or safety-equivalence.

## Clock contract and designer freedom

The reference fixture supplies XO8-33 at 8,000,000 Hz, adjustable within ±50 ppm, with 1–5,000
microseconds startup (default 5,000). Its frequency-domain net provides Hz and validity to the
simulator; firmware cannot read this net. Supplier output duty/load/bypass specifications are
documentary electrical constraints, not analog signal-integrity simulation.

The B8 boots from FRC at nominal 4 MHz (fixture tolerance ±5%), PB divider 4. The independent LFRC
is nominal 10 kHz (fixture tolerance ±10%). A system-clock failure or core halt does not stop this
LFRC while qualified MCU power remains present. Clock failure is not the same thing as loss of
electrical supply.

For external PLL mode:

```text
f_reference = f_external / P
f_vco       = f_reference * M
f_system    = f_vco / Q
f_peripheral = f_system / B
```

Literal P and Q are 1,2,4,8; M is integer 4..16; B is 1,2,4,8,16,32. Reference range 1..4 MHz, VCO
16..64 MHz, SYS 4..32 MHz, PB 0.25..4 MHz. Nominal external operating bounds admit ±0.1% source
deviation, separately from FRC's wider tolerance. A multiplied clock retains source-relative
frequency error; this model adds no PLL jitter.

Kestrel requires external PLL mode, nominal SYS 8..32 MHz and PB 1.000 MHz ±0.1%. There are multiple
valid solutions. New Employee submits his tuple, every intermediate frequency, range checks, timer
comparisons, ADC times and monitor windows. The handout does not supply a solved tuple. Owner/test
example only: 8 MHz /2 ×8 /2 = 16 MHz SYS; /16 = 1 MHz PB.

EXT_READY requires 256 continuously valid input periods, rounded to the 1-us event quantum. PLL
commits require an already qualified external source and 250 us further lock time. Pending PLL
configuration retains the old active clock. Direct FRC/EC switches are effective at an accepted
commit; PLL switches occur after qualification. A missing source during pending lock abandons the
request and sets ERROR without silently selecting it.

Disable both timers and PWM, finish/abort ADC conversion and disable DMT before changing clocks.
Maintain quiescence until BUSY clears. Stage source and divisors; write CLK_KEY=C3 then 3C and
CLK_COMMIT=A5 without any intervening register access, including an ISR or status read. Invalid
keys, illegal plans, missing source, an active switch or nonquiescent hardware reject the commit and
leave the old clock. Reads of CLK_ACTIVE and PB_ACTIVE identify applied source and PB divisor; they
are not a frequency measurement.

A selected external clock outage stops SYS/PB-driven activity. The independent missing-clock
detector resets after 8 LFRC ticks of continuous absence. At the slow LFRC extreme, this is at most
about 0.889 ms plus 1-us scheduling quantization. The reset returns to FRC and immediately removes
pad-drive authorization. A shorter outage can resume the selected clock with discontinuous
peripheral timing; Kestrel's 1-ms inhibition guarantee applies to continuous absence, not to
arbitrary jitter or intermittent bursts that defeat that detector.

## Timing-domain inventory

SYS drives the DMT divider while the modeled core is active. PB drives the two compare timers, PWM
and ADC acquisition/conversion cycles. LFRC drives WDT and missing-clock detection. External tach
edges and VBLANK inputs are sampled separately by the fixture at world-time boundaries. Mux settling
remains 2 us, LCD row scan remains 20 ms/frame, motor averaging/inertia and thermal/sensor states
evolve in world time. No common phase reference has been wired to the LCD. A shared nominal
frequency is not a shared clock.

```text
timer period = (COMPARE+1)*prescaler / f_PB
PWM frequency = f_PB/256
ADC sample time = 4*ADC_divisor/f_PB
ADC completion time = 13*ADC_divisor/f_PB
```

At 1 MHz PB the old numeric peripheral examples remain usable, but boot FRC tolerance means software
may not assume exact microseconds before external-clock qualification. At 2 MHz PB, a 999/divide-1
compare fires at 500 us and a divide-4 ADC completes at 26 us; these are tested changes. Mux
settlement does not halve.

The native SDK uses ordered read8/write8/idle service boundaries, each advancing one world
microsecond. Arithmetic outside those calls is not timed as target instructions. Multiple PB ticks
may occur in one world quantum, and intermediate sub-microsecond PWM edges are not exposed to the
motor's 1-us sampler. The supported product configuration uses 1 MHz PB; high-rate carrier waveform
fidelity above that is limited, although timer/ADC counts advance at their configured rate.
Fractional clocks use deterministic floating accumulators. This is not picosecond edge simulation,
PLL phase-noise simulation or CPU throughput benchmarking.

## Independent watchdog

WDT runs automatically after local reset release, even with global interrupts masked. WDT_CTRL.ON is
fixed 1 by the selected device option; this is a configuration fuse, not a current-protection fuse.
SCALE k=0..5 gives N=256\*2^k LFRC ticks. Default k=3 gives nominal 204.8 ms; fastest/slowest
specified oscillator gives approximately 186.18..227.56 ms. A free-running LFRC phase can shorten a
post-service interval by less than one LF tick. World-time quantization adds at most the stated
simulator quantum.

A scale write before lock restarts the watchdog age. CTRL bit7 locks configuration until hardware
reset. Writing zero cannot clear ON or LOCK, and rewriting CTRL does not feed. A locked SCALE write
is ignored and sets ERROR.

Service is A5 then 5A, with fewer than 16 LFRC ticks between. The first key does not renew the
overall deadline. An invalid value, duplicate first key, or missing/late first-key context detected
on the second write resets with a bad-key detail. Simply never supplying the second key causes the
original watchdog period to expire; it does not automatically reset exactly when the shorter key
window ends. The key window is checked on the attempted second service.

Kestrel requires a locked period whose slowest timeout is no more than 250 ms. Default code 3 meets
that maximum; smaller codes are allowed when software can demonstrate margin at the fastest LFRC. A
long period chosen merely to hide blocked firmware does not meet requirements.

## Windowed deadman

The B8 DMT counts SYSCLK/1024 while the core is active. It intentionally does NOT count actual
instruction fetches. PIC32 families with DMT describe an instruction-fetch-counted mechanism, but
this native C++ runtime has no instruction-fetch stream. An ISA backend could implement a different
fidelity profile later; this release explicitly defines its simpler behavior rather than claiming
hardware equivalence.

DMT is reset-disabled. Configure a 16-bit limit and opening count while disabled/unlocked; low
stages and high commits each pair. Valid 0 \<= opening < limit \<= 65535. Default opening/limit
256/1024. Set enable then lock (bit7). A locked control change is rejected; repeating the same
enabled value does not reset age. Reads of the counter use low-then-high snapshot semantics. Status
OPEN[0], ENABLED[1], ERROR[7].

```text
t_open = 1024*opening/f_SYS
t_limit = 1024*limit/f_SYS
```

In the open window write PRECLR=69, then CLR=96 within fewer than 16 further DMT ticks and before
limit. A good second key resets count and fractional phase. Early service, bad or duplicated first
key, missing/wrong/late second key and expiry produce reset/detail conditions. Like WDT, merely
omitting the second key does not create an autonomous 16-tick timer reset; limit expiry still
applies. Exact expiry wins over a later bus write. Writes while disabled set config ERROR instead of
a reset. Configuration reads/writes themselves are not feeds.

DMT pauses on explicit core halt or selected-source failure. PB peripherals can continue during core
halt; the independent WDT still expires. Thus DMT supplements rather than replaces WDT. An
uncontrolled loop can feed a plain watchdog; a window may catch too-fast DMT service. A loop that
deliberately issues both correct sequences at valid intervals can satisfy both monitors while
accomplishing nothing useful. Hardware keys do not prove software progress.

Require one new epoch of input acquisition, fresh ADC acquisition, protection evaluation and output
application before the service owner renews either monitor. Do not service unconditionally in a tick
ISR. Startup and faulted states need their own bounded, meaningful progress accounting; waiting for
an absent oscillator must not hide a stalled startup. Do not count one completed epoch multiple
times. The reference models do not supply that firmware solution.

No physical operator-held deadman switch was added. Doing so would change the control assembly and
product interaction. The term DMT here means execution supervision only.

## Reset, power, retention and fusing

Hardware reset releases GPIO directions, zeros output latches, drives PWM low, resets
timers/ADC/IRQ, selects FRC/PB4, restores default fused-on WDT and disables/unlocks DMT. Local reset
remains asserted for 1 ms; WDT counting begins after release. RST_CAUSE accumulates
POR/BOR/WDT/DMT/CLOCK/SOFT/EXTERNAL until W1C; POR initializes a new cause set. RST_DETAIL records
service/timeout details. SW_RESET=B6 requests software reset. Causes collected in the same 1-us
event are ORed before a single reset.

Reset invalidates foreground/ISR return context. Runtime::run_for catches FirmwareReset and
re-enters firmware::reset after release. An ISR interrupted by reset cannot restore its previous
global-interrupt enable. The reset entry must explicitly reinitialize all C++ application state;
host global/static constructors are not automatically re-run. Warm B8 reset does not reset the
external LCD, rotor, case/sensor temperature or mechanical latches. Those are external state.
Reinitialize the LCD interface, especially current address and auto-increment, rather than assuming
RAM has cleared.

Power topology is a functional low-voltage bench profile, not a mains appliance schematic:
isolated/current-limited 24 V DC entry J1 -> input fuse F1 -> rear switch SW1 -> F2 motor branch and
F3 3.3 V logic regulator branch. The regulator supplies B8, mux, LCD, sensor, bias and oscillator.
No signal conductor or common return has a series fuse in this layout. Motor return and analog/logic
return use separate branches to the entry return. No selected connector, PCB trace width, fuse
ampere/interrupt/time-current rating, motor bus rating or regulator load rating has been released
here. Actual safe hardware requires that engineering. Never interpret functional nets as
construction approval.

PowerDomain provides a deterministic representative 5-ms contact-bounce history, a 20-ms linear
nominal rail rise and 5-ms discharge. An auxiliary switch/inhibit qualification path prevents an
interrupted contact from authorizing the motor. PGOOD drops for rear-off/lost input/lost logic
branch or rail below 2.9 V; it asserts only after voltage >=3.0 V continuously for 10 ms, followed
by B8's local reset hold. XO OE is PGOOD and its own startup begins separately. MCU can boot on FRC
before the external source is ready.

```text
actual motor enable = MOTOR_SUPPLY AND PGOOD AND RESET_RELEASED
                      AND physical RUN_PERMIT AND PB0_request
```

PB0 has a released-state pull-down. Opening the motor fuse leaves logic alive but removes motor
permission. Opening logic fuse removes permission even when motor rail remains energized. Input fuse
removes both. Fault injection may open each fuse independently; no current, I-squared-t or
spontaneous fuse-trip calculation is implemented. A fuse is not a substitute for stalled-rotor,
thermal or software supervision. Mechanical command and thermal/momentum state survive power
interruption; semiconductor supply/reset behavior is separately modeled.

## Simulator APIs and isolation

The firmware SDK exposes only Reg, Irq, read8/write8/idle and vector signatures. The simulator alone
sees ClockNet, CrystalOscillator, ClockTree, Watchdog, DeadmanTimer, PowerDomain and reset/core-halt
fixtures. A real adapter must preserve observable reset, clock, GPIO and transaction semantics; it
is not necessarily a drop-in subclass. BoardProfile::clocked03 is default; legacy02 exists for the
previous component regression suite and the historical direct-register bench. Never deliver legacy02
as evidence of the new startup/reliability behavior.

**Execution limit:** a C++ function that never returns, yields, or calls the SDK blocks this
cooperative process. The simulator cannot advance its hardware clocks behind that host loop and
claim a WDT recovery. CTest timeouts detect host nontermination as a failed test, not a target
reset. Explicit halt fixtures and bounded busy-loop fixtures let world time advance and test
hardware behavior. Preemptive instruction execution or a target MCU would be required to reproduce
arbitrary uninstrumented CPU hangs faithfully.

The safe firmware stub does not solve clock initialization, health-epoch policy, supervision or
fault recovery. After enough time its unserviced watchdog legitimately resets it. The test bench is
the supplier/reference oracle, not finished appliance firmware. No new HIL or electrical safety
certification was performed.

## Acceptance and regression evidence

New reference cases cover power qualification, oscillator startup/tolerance, PLL locking and invalid
plans, protected keys, quiescent configuration, timer/ADC scaling, independent mux delay, FRC error,
WDT deadlines/keys/lock with interrupts masked, LFRC extremes, reset causes, DMT
window/keys/expiry/clock scaling, WDT-versus-DMT independence, core halt, absent/lost oscillator,
retained LCD/mechanics, power branches, rear switching and runtime ISR unwind. Existing component
checks run with explicit legacy02 to distinguish unchanged peripheral regression from new-interface
proof.

Product R-001..R-030 are retained; R-031..R-040 add clock selection, rate evidence, startup, clock
loss, reset, independent WDT, meaningful progress, windowed DMT, CK/WD/DM diagnostics and
reliability evidence. CK/WD/DM rank before prior TS/TH/IF/ST; startup/rearm remains deliberate.
These acceptance requirements are specified for New Employee, not claimed passing in the
starter. See the
run-specific VALIDATION.md for actual toolchains/counts.

## Source provenance (owner only)

Microchip Primary Oscillator Mode:
<https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/PIC32/mx-arch-cpu-overview/oscillator/posc/>

Microchip Oscillator overview:
<https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/PIC32/mx-arch-cpu-overview/oscillator/>

Microchip Two-Speed Startup:
<https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/PIC32/oscillator/two-speed-startup/>

Microchip PIC32 Family Reference Manual, Watchdog Timer and Deadman Timer, DS60001114H, especially
DMT functional section and clock-source distinctions:
<https://ww1.microchip.com/downloads/en/DeviceDoc/60001114H.pdf>

PIC32 family variants differ in WDT clock options, NMI/reset behavior and DMT implementation. The B8
is its own specified device. Supplier labels, numbers and numerical operating limits in these
artifacts are authored for this project and are not Microchip specifications.
