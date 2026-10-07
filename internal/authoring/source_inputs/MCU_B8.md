# B8 microcontroller / interface 03

Publication P4. The ordered register interface below and the supplier text form the current reference; interface-02 fixed-rate prose is superseded.

| Address | Register | Access | Reset | Meaning |
|---|---|---|---|---|
| `0x0000` | `SYS_ID` | R | `0xB8` | Device identifier |
| `0x0001` | `SYS_REV` | R | `0x03` | Interface revision |
| `0x0010` | `IRQ_GLOBAL` | RW | `0x00` | bit 0 global enable |
| `0x0011` | `IRQ_ENABLE` | RW | `0x00` | bits 0..4 source enables |
| `0x0012` | `IRQ_FLAGS` | W1C | `0x00` | TIMER0, TIMER1, TACH, VBLANK, ADC complete |
| `0x0020` | `GPIOA_DIR` | RW | `0x00` | 1 output, 0 input |
| `0x0021` | `GPIOA_OUT` | RW | `0x00` | Output latch |
| `0x0022` | `GPIOA_IN` | R | `0x00` | Resolved pad values; wiring determines reset read |
| `0x0028` | `GPIOB_DIR` | RW | `0x00` | 1 output, 0 input |
| `0x0029` | `GPIOB_OUT` | RW | `0x00` | Output latch |
| `0x002A` | `GPIOB_IN` | R | `0x00` | Resolved pad values; wiring determines reset read |
| `0x0040` | `T0_CTRL` | RW | `0x00` | bit 0 enable, bit 1 periodic; writes reset counter and divider |
| `0x0041` | `T0_PRESCALE` | RW | `0x00` | codes 0,1,2,3 = divide 1,8,64,256 |
| `0x0042` | `T0_COUNT_LO` | R | `0x00` | Read low to latch high |
| `0x0043` | `T0_COUNT_HI` | R | `0x00` | Latched high; high-first returns stale latch |
| `0x0044` | `T0_COMPARE_LO` | RW | `0x00` | Stage low byte while disabled |
| `0x0045` | `T0_COMPARE_HI` | RW | `0x00` | Commit staged low and written high while disabled |
| `0x0050` | `T1_CTRL` | RW | `0x00` | bit 0 enable, bit 1 periodic; writes reset counter and divider |
| `0x0051` | `T1_PRESCALE` | RW | `0x00` | codes 0,1,2,3 = divide 1,8,64,256 |
| `0x0052` | `T1_COUNT_LO` | R | `0x00` | Read low to latch high |
| `0x0053` | `T1_COUNT_HI` | R | `0x00` | Latched high; high-first returns stale latch |
| `0x0054` | `T1_COMPARE_LO` | RW | `0x00` | Stage low byte while disabled |
| `0x0055` | `T1_COMPARE_HI` | RW | `0x00` | Commit staged low and written high while disabled |
| `0x0060` | `PWM_CTRL` | RW | `0x00` | bit 0 enable; clear drives low immediately |
| `0x0061` | `PWM_DUTY` | RW | `0x00` | Shadow duty, applied at next 256-tick boundary |
| `0x0070` | `TACH_COUNT_LO` | R | `0x00` | Low read latches high |
| `0x0071` | `TACH_COUNT_HI` | R | `0x00` | Latched high |
| `0x0080` | `XBUS_REG` | RW | `0x00` | External device register address, low 2 bits |
| `0x0081` | `XBUS_DATA` | RW | `0x00` | One external byte-bus transaction |
| `0x0090` | `ADC_CTRL` | RW | `0x00` | bit0 enable; bit1 START is write-only strobe |
| `0x0091` | `ADC_CHANNEL` | RW | `0x00` | 0 AN0; 1 AN1; writes while busy ignored with ERROR |
| `0x0092` | `ADC_PRESCALE` | RW | `0x02` | codes 0..3 = div1,2,4,8; changes require idle |
| `0x0093` | `ADC_STATUS` | R/W1C | `0x00` | bit0 BUSY; bit1 READY; bit2 OVERRUN W1C; bit3 ERROR W1C |
| `0x0094` | `ADC_DATA_LO` | R | `0x00` | low read latches high and acknowledges READY |
| `0x0095` | `ADC_DATA_HI` | R | `0x00` | bits0..1 latched high; high-first is stale |
| `0x0002` | `RST_CAUSE` | W1C | `0x01` | POR/BOR/WDT/DMT/CLOCK/SOFT/EXTERNAL bits 0..6; warm-reset accumulation |
| `0x0003` | `RST_DETAIL` | W1C | `0x00` | WDT bad key, DMT bad1/bad2/early/late, WDT timeout bits 0..5 |
| `0x0004` | `SW_RESET` | W | `0x00` | B6h requests software reset |
| `0x00A0` | `CLK_SOURCE` | RW | `0x00` | Staged source: 0 FRC, 1 EC, 2 EC with PLL |
| `0x00A1` | `PLL_PREDIV` | RW | `0x02` | Staged literal divisor 1,2,4,8 |
| `0x00A2` | `PLL_MULT` | RW | `0x08` | Staged literal multiplier 4..16 |
| `0x00A3` | `PLL_POSTDIV` | RW | `0x02` | Staged literal divisor 1,2,4,8 |
| `0x00A4` | `PB_DIV` | RW | `0x04` | Staged literal divisor 1,2,4,8,16,32 |
| `0x00A5` | `CLK_STATUS` | R/W1C | `0x00` | EXT_READY bit0, PLL_LOCK bit1, BUSY bit2, ERROR W1C bit3 |
| `0x00A6` | `CLK_ACTIVE` | R | `0x00` | Actual source; not the staged request |
| `0x00A7` | `CLK_KEY` | W | `0x00` | C3h then 3Ch then COMMIT without intervening access |
| `0x00A8` | `CLK_COMMIT` | W | `0x00` | A5h commits staged tuple when unlocked and quiescent |
| `0x00A9` | `PB_ACTIVE` | R | `0x04` | Actual peripheral bus divisor |
| `0x00B0` | `WDT_CTRL` | RW | `0x01` | Fused ON bit0, one-way configuration LOCK bit7 |
| `0x00B1` | `WDT_SCALE` | RW | `0x03` | Literal code k 0..5; timeout 256*2^k independent LFRC ticks |
| `0x00B2` | `WDT_SERVICE` | W | `0x00` | A5h then 5Ah; second within fewer than 16 LFRC ticks |
| `0x00B3` | `WDT_STATUS` | R | `0x00` | First key armed bit0; configuration ERROR bit7 |
| `0x00C0` | `DMT_CTRL` | RW | `0x00` | Enable bit0; one-way LOCK bit7 |
| `0x00C1` | `DMT_LIMIT_LO` | RW | `0x00` | Staged low byte of maximum count |
| `0x00C2` | `DMT_LIMIT_HI` | RW | `0x04` | High write commits maximum count |
| `0x00C3` | `DMT_WINDOW_LO` | RW | `0x00` | Staged low byte of window-open count |
| `0x00C4` | `DMT_WINDOW_HI` | RW | `0x01` | High write commits window-open count |
| `0x00C5` | `DMT_STATUS` | R/W1C | `0x00` | OPEN bit0, ENABLED bit1, config ERROR W1C bit7 |
| `0x00C6` | `DMT_COUNT_LO` | R | `0x00` | Count low; captures high byte |
| `0x00C7` | `DMT_COUNT_HI` | R | `0x00` | Captured count high byte |
| `0x00C8` | `DMT_PRECLR` | W | `0x00` | 69h in open window; does not refresh counter |
| `0x00C9` | `DMT_CLR` | W | `0x00` | 96h after PRECLR within fewer than 16 DMT ticks |

## Supplier operating text


### Source page 1

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
B8
Microcontroller user manual
8-bit peripheral interface / 16-bit register addressing / selectable FRC / external clock / PLL
DEVICE ID B8h INTERFACE REVISION 03 ISSUE P4 / 06 OCT 2026
General description
The B8 provides two eight-bit GPIO ports, two independent 16-bit compare timers, a PWM output, a tachometer counter,
an external byte-interface bridge and a two-channel 10-bit ADC. Five interrupt sources share a fixed-priority vector
table. Revision 03 adds a programmable clock tree, reset-cause registers, an independent watchdog and a windowed
deadman counter.
This reference defines the register interface and peripheral timing. Instruction encoding, program storage and instruction
execution timing are outside its scope. All register accesses are ordered eight-bit transactions; multi-byte values require
the access sequences specified on the following pages.
16-BIT ADDRESS / 8-BIT REGISTER DATA
GPIO A+B Timers T0+T1 PWM + tach XBUS bridge 10-bit ADC
FIXED-PRIORITY INTERRUPT CONTROLLER / 5 VECTORS
Signal groups
Group Function
CLKIN External CMOS clock input; this issue supports EC input, not a bare resonator.
PA[7:0], PB[7:0] Independently configured digital input/output pads.
PWM Dedicated output; 256 peripheral-clock carrier period.
PB1 / TACH Rising-edge input to the 16-bit tachometer counter.
XBUS Register-select and ordered-byte transfer bridge to an external peripheral; external VBLANK input.
AN0, AN1 Single-ended analog inputs; nominal reference 3.3 V.
Reset behavior
GPIO direction and output latches reset to zero: pads are released as inputs. PWM is disabled. Timers, interrupt
enables, pending flags and ADC activity are cleared. The ADC prescaler resets to divide-by-four. Reset selects the
nominal 4 MHz FRC with PB divide-by-four; its±5% tolerance applies. The watchdog starts automatically after reset
release.
BOARD BIAS IS REQUIRED
An input-direction reset does not create a pull-up or pull-down. External bias and other connected drivers determine the level
read at a GPIO pad. Provide a board-level pull-down for any driver-enable input that must remain inactive during reset.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 1 / 11

### Source page 2

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Register map: system, GPIO and timers
Addresses are hexadecimal. All entries are eight bits wide.
R = READ ONL Y RW = READ/WRITE W1C = WRITE ONE TO CLEAR
Address Register Access Reset
0000h SYS_ID R B8h
0001h SYS_REV R 03h
0010h IRQ_GLOBAL RW 00h
0011h IRQ_ENABLE RW 00h
0012h IRQ_FLAGS W1C 00h
0020h GPIOA_DIR RW 00h
0021h GPIOA_OUT RW 00h
0022h GPIOA_IN R 00h
0028h GPIOB_DIR RW 00h
0029h GPIOB_OUT RW 00h
002Ah GPIOB_IN R 00h
0040h T0_CTRL RW 00h
0041h T0_PRESCALE RW 00h
0042h T0_COUNT_LO R 00h
0043h T0_COUNT_HI R 00h
0044h T0_COMPARE_LO RW 00h
0045h T0_COMPARE_HI RW 00h
0050h T1_CTRL RW 00h
0051h T1_PRESCALE RW 00h
0052h T1_COUNT_LO R 00h
0053h T1_COUNT_HI R 00h
0054h T1_COMPARE_LO RW 00h
0055h T1_COMPARE_HI RW 00h
GPIO port operation
DIR bit 0 means input/released; DIR bit 1 enables the corresponding OUT driver. Writes to OUT update the latch
whether or not the pad is configured as an output. IN samples the resolved pad levels, not a copy of the OUT register.
The listed reset storage for GPIO IN is not a guaranteed pin reading. Board wiring may pull a released pad high. Do
not drive a peripheral output from a GPIO output at the same time. An unresolved or contended digital read is an
interface fault.
Access discipline
Unimplemented register addresses and writes to read-only registers cause an interface fault. Reserved bits in ordinary
RW registers are ignored on write and return zero. Write-one-to-clear fields clear only the bits written as one; a zero
leaves the corresponding pending condition unchanged.
The timer COUNT high byte is a read latch. The COMPARE low byte is a write staging register. Neither pair is an ordinary 16-bit memory location.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 2 / 11

### Source page 3

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Register map: output, feedback and analog
Dedicated functions retain their own side effects.
B8 / REV 03 MCU-001 / P4
Address Register Access Reset
0060h PWM_CTRL RW 00h
0061h PWM_DUTY RW 00h
0070h TACH_COUNT_LO R 00h
0071h TACH_COUNT_HI R 00h
0080h XBUS_REG RW 00h
0081h XBUS_DATA RW 00h
0090h ADC_CTRL RW 00h
0091h ADC_CHANNEL RW 00h
0092h ADC_PRESCALE RW 02h
0093h ADC_STATUS R/W1C 00h
0094h ADC_DATA_LO R 00h
0095h ADC_DATA_HI R 00h
Control-field summary
Register Implemented fields
IRQ_GLOBAL Bit 0: global interrupt enable.
IRQ_ENABLE / FLAGS Bits 0–4: T0, T1, tach rising edge, VBLANK rising edge, ADC complete.
T0/T1_CTRL Bit 0: enable. Bit 1: periodic mode. Every write resets count and divider.
PWM_CTRL Bit 0: output enable. Clearing it drives PWM low at the write.
XBUS_REG Low two bits select the external register index.
ADC_CTRL Bit 0: enable. Bit 1: START write strobe; reads zero.
ADC_STATUS Bit 0 BUSY (R), bit 1 READY (R), bit 2 OVERRUN (W1C), bit 3 ERROR (W1C).
Byte-pair ownership
Reading a COUNT or ADC low byte captures its high byte. The next high-byte read returns that capture, not the
current live high bits. A high-first read returns the previous latch value. Another low-byte read replaces the latch.
INTERRUPT INTERLEAVING
An interrupt routine that reads the same counter can replace the high-byte capture between foreground low and high reads. Use
one owner for each byte pair or exclude such interleaving for the pair. Individual eight-bit accesses do not make a multi-access
sequence atomic.
Assignments of general-purpose pads to external system functions are determined by the board design.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 3 / 11

### Source page 4

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Timers and interrupt controller
Two independent compare timers and five non-nesting interrupt sources.
Timer programming
Prescaler codes 0, 1, 2 and 3 select divisors 1, 8, 64 and 256. Disable the timer before changing its prescaler or compare
value. Write COMPARE_LO first to stage the low byte, then COMPARE_HI to commit both bytes. Until the high-byte
write, reads of COMPARE return the previously committed value.
At each divided clock, COUNT is compared with COMPARE. Equality sets the timer flag and returns COUNT to zero.
Otherwise COUNT increments. Periodic mode repeats; one-shot mode stops at the compare event.
tperiod = (COMPARE + 1)D
fPB
s
COMPARE Divisor D Period at 1 MHz Notes
999 1 1 ms 1,000 peripheral-clock ticks.
124 8 1 ms 125 divided ticks.
65535 1 65.536 ms Full 16-bit count interval.
Every CTRL write resets the count and divider, including a write that leaves enable set. Rewriting CTRL periodically
is therefore not a harmless refresh operation.
Vector order and acknowledgment
Vector / bit Source Event
0 TIMER0 T0 compare. Highest priority.
1 TIMER1 T1 compare.
2 TACH Rising edge at PB1.
3 VBLANK External VBLANK rising edge.
4 ADC Completed conversion. Lowest priority.
Pending flags are sticky even when their sources are masked. Repeated events coalesce in a single flag; they are not
queued. Delivery requires the source enable and IRQ_GLOBAL bit 0. The lowest numbered eligible vector is serviced
first.
A hardware reset abandons the interrupted execution; it does not perform interrupt return. Entry masks global delivery.
Nested interrupts are not supported. The handler returns normally; return restores the enabled global-delivery state
from entry. Disabling global delivery inside the handler does not persist across this return. Missing handlers are an
interface fault.
FLAGS ARE NOT CLEARED BY ENTRY
Acknowledge the serviced source by writing its mask to IRQ_FLAGS. Do not write back every bit from a status snapshot: that
would clear unrelated pending sources. The tach counter counts edges independently of its coalescing interrupt flag.
One pending interrupt is dispatched at an I/O service boundary. Handlers must return so that other sources and foreground work can progress.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 4 / 11

### Source page 5

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
PWM, tachometer and external bus
Waveform generation, input-edge counting and external register transfers.
PWM timing and coding
The carrier period is256/fPB seconds and frequency isfPB/256. AtfPB = 1 MHz these are 256µs and 3,906.25 Hz.
For duty codes 0–254, the active high fraction is code/256. Code 255 is the special full-on endpoint.
d(c) =
{
c/256, 0 ≤ c≤ 254,
1, c = 255.
PWM_DUTY writes the shadow value. It transfers to the active value at the next carrier boundary. Enabling the
output partway through a period does not restart the carrier. Clearing PWM_CTRL.EN drives the pin low immediately
at the register write.
DISABLE IS NOT A ZERO-DUTY WRITE
Clearing only PWM_DUTY can leave the old active waveform present until a carrier boundary. Clearing PWM_CTRL.EN forces
the B8 PWM pad low at the register write. No other external signal is changed by that write.
Tachometer counter
PB1 rising edges increment a 16-bit counter modulo 65,536. It runs while the controller is powered, independently of the
state of the PWM generator. Reset is the only counter-clear operation.
Read TACH_COUNT_LO, then TACH_COUNT_HI for a coherent value. For successive unsigned readingsC0,C1,
use ∆C = (C1 − C0) mod 65536. The observation interval must be short enough to exclude an unobserved complete
wrap. The relationship between input edges and an external physical quantity is defined by the connected source.
External register bridge
XBUS_REG selects one of four external registers using its low two bits. Each read or write of XBUS_DATA performs
one corresponding external byte transaction. The selected external device supplies the DATA behavior; the B8 does not
interpret the meaning of external register contents.
XBUS_REG is shared state. Coordinate access when foreground code and an interrupt handler both use the bridge.
Device-local status acknowledgment is separate from MCU IRQ_FLAGS acknowledgment.
Software access binding
The controller binding providesb8::read8(Reg), b8::write8(Reg,uint8_t) and b8::idle(). One ordered read or
write occurs at the current service boundary and is followed by one 1-µs service interval and an interrupt-service
opportunity. This binding interval is independent of SYSCLK and PBCLK; it is not an instruction-cycle duration. An
idle operation advances one service interval without an I/O transfer; it does not enter a low-power hardware state.
Firmware entry points arefirmware::reset(), boundedfirmware::step() and a five-entry vector table of functions
returning void. Arithmetic and instruction timing outside these I/O boundaries are not specified by this peripheral
interface.
External peripheral recovery periods are independent of the binding service interval and the selected PBCLK.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 5 / 11

### Source page 6

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Analog-to-digital converter
Acquisition, conversion completion and result ownership are distinct.
2 INPUTS 10 BITS NOMINAL VREF = 3.3 V
Transfer characteristic
For input voltageV and the reference sampled for that conversion, the converter uses the following endpoint-rounded
transfer:
N = clamp
(⌊
1023 V
VREF
+ 1
2
⌋
,0,1023
)
.
The result occupies DATA_LO and bits 1:0 of the captured DATA_HI. Reference error and sensor error are separate
contributors; conversion results alone do not identify which source caused an offset.
Acquisition and conversion timing
ADC prescaler codes 0–3 select divisors 1, 2, 4 and 8. Each operation has four acquisition cycles followed by nine
conversion cycles. At divide-by-four andfPB = 1 MHz, acquisition ends at 16µs and completion occurs at 52µs. In
general, tsample = 4DA/fPB and tcomplete = 13DA/fPB.
Acquire Convert held sample
START
0
SAMPLE
16
READY + IRQ
52
µs
Voltage and reference are captured at the end of acquisition. Changes after that instant affect a later conversion, not the
held result.
Starting, reading and errors
Write 03h to ADC_CTRL to enable and start. START is a write strobe and reads zero. Writing START alone writes
EN=0: it disables the ADC and cannot initiate a conversion. Disable aborts an active conversion, clears READY and
retains the prior result.
Condition Result
START while busy ERROR set; active conversion continues, not restarted.
START while disabled ERROR set; no conversion starts.
Change channel/prescaler while busy Write ignored; ERROR set.
Completion over unread READY OVERRUN set; newest result replaces old.
Read DATA_LO Capture high bits and acknowledge READY.
Read DATA_HI first Return previous high-byte latch; no fresh snapshot.
Reading data does not acknowledge the ADC interrupt. ERROR and OVERRUN are cleared independently with W1C
status writes. Re-reading an old result is not a completed conversion.
Drive AN0/AN1 from a source that settles within acquisition. This interface issue does not assign a general high-impedance source-settling guarantee.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 6 / 11

### Source page 7

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Clock sources and frequency tree
A staged configuration is not the active clock.
FRC 4 MHz ±5% EC / PLL INDEPENDENT LFRC 10 kHz ±10%
CLKIN / EC ÷P PLL ×M ÷Q
Source selectFRC ÷BSYSCLK
PBCLK
LFRC watchdog / missing-clock detector
fREF = fEC/P, f VCO = fREFM, f SYS = fVCO /Q, f PB = fSYS /B.
Element Supported selection / nominal operating range
Source 0: internal FRC; 1: direct EC; 2: EC through PLL.
PLL input dividerP Literal 1, 2, 4 or 8; PLL input 1–4 MHz.
PLL multiplierM Literal integer 4–16; VCO 16–64 MHz.
PLL output dividerQ Literal 1, 2, 4 or 8; SYSCLK 4–32 MHz.
Peripheral dividerB Literal 1, 2, 4, 8, 16 or 32; PBCLK 0.25–4 MHz.
External qualification 256 consecutive valid input periods.
PLL qualification 250 µs after an accepted PLL commit with valid EC.
PLL and external-source nominal limits permit±0.1% source-frequency deviation. Divider register values are literal
divisors, not logarithmic encodings. The FRC is a separate source with its own wider accuracy envelope. Multiplication
does not remove the source’s relative frequency error.
INDEPENDENT TIMEBASES
The LFRC continues while the core or its selected high-frequency source is halted. PB-driven timers and the ADC change rate
when PBCLK changes. An external peripheral’s local timing changes only if its own clock input changes.
The 1 MHz numerical examples earlier in this manual are conditional examples. No external frequency or divider plan is selected for the application by this manual.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 7 / 11

### Source page 8

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Clock configuration and switching
Protected commit, active status and missing-clock response.
Address Register Access Reset Function
00A0h CLK_SOURCE RW 00h Staged source code 0, 1, 2.
00A1h PLL_PREDIV RW 02h Staged P.
00A2h PLL_MULT RW 08h Staged M.
00A3h PLL_POSTDIV RW 02h Staged Q.
00A4h PB_DIV RW 04h Staged B.
00A5h CLK_STATUS R/W1C 00h READY[0], LOCK[1], BUSY[2], ERROR[3].
00A6h CLK_ACTIVE R 00h Current source, not requested source.
00A7h CLK_KEY W 00h Protected-write sequence.
00A8h CLK_COMMIT W 00h A5h submits the staged tuple.
00A9h PB_ACTIVE R 04h Current peripheral divisor.
Programming sequence
Stage the source and division/multiplication values. For an EC-based request, first establish EXT_READY. Disable
both compare timers and PWM, complete or abort any ADC conversion, and disable DMT before committing. Keep
these functions quiescent until BUSY clears.
Write C3h, then 3Ch to CLK_KEY, followed by A5h to CLK_COMMIT. No other register access may intervene,
including a status read or an interrupt handler’s access. Each stage consumes the previous unlock state. Invalid keys, an
invalid frequency plan, a busy switch, a nonquiescent peripheral or an unready source leave the active clock unchanged
and set ERROR.
Direct-source commits take effect at the accepted write. A PLL request sets BUSY and retains the old active source for
the qualification interval. It then selects the committed tuple and sets LOCK. If EC becomes invalid while pending,
the request is abandoned and ERROR is set. Read CLK_ACTIVE and PB_ACTIVE after BUSY clears. ERROR is
cleared by writing bit 3 as one.
Clock interruption
When an active external source loses valid edges, its clock-driven functions stop. A separate LFRC detector requests
reset after eight LFRC ticks of continuous absence. No pending maskable interrupt is required. Reset returns selection
to FRC and records CLOCK in RST_CAUSE. A short interruption that ends before the detector expires resumes the
current source; it is not a promise of undisturbed application timing.
NO SCHEDULE CONTINUITY GUARANTEE
Changing clock rate does not rescale already interpreted deadlines or application counters. Review timer compare values,
ADC timing, PWM carrier and DMT settings against the new clock plan. Status bits do not measure absolute frequency for the
application.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 8 / 11

### Source page 9

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Independent watchdog timer
Elapsed-time supervision, separate from SYSCLK and interrupt delivery.
Address Register Access Reset Function
00B0h WDT_CTRL RW 01h Fused ON[0], one-way LOCK[7].
00B1h WDT_SCALE RW 03h Code k= 0,1,2,3,4,5.
00B2h WDT_SERVICE W 00h A5h then 5Ah clear sequence.
00B3h WDT_STATUS R 00h First-key ARMED[0], config ERROR[7].
NW = 256 2k, t W = NW/fLF, f LF = 10 000(1 +ϵLF) Hz.
Code LFRC ticks Nominal time Tolerance note
0 256 25.6 ms All times use the independent LFRC.
1 512 51.2 ms Fast LFRC shortens the timeout.
2 1,024 102.4 ms Slow LFRC lengthens it.
3 2,048 204.8 ms Reset selection.
4 4,096 409.6 ms Not a SYSCLK-derived interval.
5 8,192 819.2 ms Maximum code.
The watchdog begins counting at reset release. ON is fixed high in this device option; writing zero does not disable it.
SCALE can be changed before LOCK is set. An accepted scale change clears the count. Once locked, scale changes are
ignored and set configuration ERROR; LOCK cannot be cleared before reset. Rewriting CTRL does not refresh the
count.
Service protocol
Write A5h followed by 5Ah. Fewer than 16 LFRC ticks may elapse between the writes. The first write arms the key
sequence but does not clear the watchdog. The second valid write clears it. A wrong value, duplicate first key or a
second key without a valid, unexpired first key requests a watchdog reset with BAD_KEY detail. Omitting the second
key leaves the original watchdog deadline running; the shorter key window is checked on a service attempt. A watchdog
timeout records TIMEOUT detail. No software interrupt acknowledgment can suppress these events.
Uncertainty
For the reset code and the stated±10% LFRC range, the nominal limits are approximately 186.18–227.56 ms. Service
relative to the fastest permitted LFRC, and bound fault detection using the slowest. The free-running LFRC phase can
shorten the first post-service interval by less than one LFRC period.
The watchdog validates elapsed service time, not the correctness of the code writing its service keys. No application-health interpretation is performed by this block.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 9 / 11

### Source page 10

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Windowed deadman timer
Core-clock interval supervision with a two-stage service sequence.
Address Register Access Reset Function
00C0h DMT_CTRL RW 00h Enable[0], one-way LOCK[7].
00C1h–C2h DMT_LIMIT_LO/HI RW 0400h Low staged; high commits limit.
00C3h–C4h DMT_WINDOW_LO/HI RW 0100h Low staged; high commits opening count.
00C5h DMT_STATUS R/W1C 00h OPEN[0], ENABLED[1], ERROR[7].
00C6h–C7h DMT_COUNT_LO/HI R 0000h Low read captures high byte.
00C8h DMT_PRECLR W 00h First key 69h.
00C9h DMT_CLR W 00h Second key 96h.
fD = fSYS /1024, t open = 1024Nopen/fSYS , t limit = 1024Nlimit/fSYS .
The counter advances from SYSCLK divided by 1,024 while the core is active. It is not driven by LFRC and it does
not count instruction fetches. A core halt pauses it. A selected-clock failure also pauses it until reset or recovery. The
independent watchdog is unaffected by that pause.
Program the two count pairs while disabled and unlocked. Valid limits satisfy0 ≤ Nopen <Nlimit ≤ 65,535. Enabling
with an invalid window leaves the timer disabled and sets ERROR. Set LOCK to freeze enable and configuration until
reset. Writing the same enabled CTRL value does not refresh the count.
Window and keys
The service window isNopen ≤ COUNT <N limit. Write 69h to PRECLR in that window, then 96h to CLR before 16
further DMT ticks elapse and before expiry. The first key does not refresh the deadline. A successful second key clears
count and fractional divider phase. Reads do not substitute for either write.
Early service, a duplicate/wrong first key, an absent/wrong/late second key, or maximum count each requests a DMT
reset. The detail flag is retained. With no second write, the original maximum-count deadline remains in force; the
key window is checked at the attempted write. At an exact expiry boundary, the hardware expiry precedes a later bus
service. With DMT disabled, key writes set configuration ERROR rather than resetting the device.
A WINDOW IS NOT PROOF OF USEFUL WORK
A repeating sequence of correct key writes inside the window is acceptable to this hardware even if other application work
is wrong. Service eligibility and the evidence authorizing service are software responsibilities. This block is not an operator-
presence switch.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 10 / 11

### Source page 11

Northstar Micro Devices B8 / REV 03 / MCU-001 / P4
NORTHSTARMICRO DEVICES
EMBEDDED SYSTEMS DIVISION
MCU-001
Reset causes and retained state
Hardware reset terminates the current execution context.
Address Register Access Function
0002h RST_CAUSE R/W1C Bits 0..6: POR, BOR, WDT, DMT, CLOCK, SOFT, EXTERNAL.
0003h RST_DETAIL R/W1C Bits 0..5: WDT bad key, DMT bad first key, DMT bad second key, DMT early,
DMT late, WDT timeout.
0004h SW_RESET W B6h requests software reset; other values ignored.
Warm-reset causes and details accumulate until written as one to clear. POR establishes a new cause set. More than
one cause can be present; software must not assume one-hot status. Read causes before clearing them. The current
instruction context and any interrupt-return context are abandoned on reset.
Reset effects
State Result
GPIO / PWM GPIO pads released; output latches zero; PWM output low; active and pending
duty zero.
Timers / ADC / IRQ Counts, enables, captures, acquisition and pending flags cleared.
Clock configuration FRC selected; PB divide-by-four; no pending PLL switch.
WDT / DMT Watchdog reset code and unlocked state; DMT disabled and unlocked.
Reset release Local reset holds execution for 1 ms after a request and while external reset is
asserted. WDT counting starts on release.
External parts No implied reset of any external memory, display, mechanical latch, rotating mass
or thermal state.
Power and external reset
The supply supervisor and its thresholds belong to the board. The B8 cannot perform firmware initialization while
supply qualification or reset holds it inactive. Released GPIO pads require external bias appropriate to the connected
circuits. A clock source that starts separately from the supply may still be unavailable after reset release.
Software service binding
The published C++ binding uses bounded reset and step entry points and the existing five-entry interrupt vector table.
Reset entry must initialize all firmware-owned state required for a fresh execution. The service binding advances external
time in 1µs quanta; it is not an instruction-set or instruction-fetch timing interface. Peripheral events within a quantum
are quantized to that boundary.
Reset causes are device observations. What the surrounding equipment displays, when it may restart, and which faults require acknowledgment are separate system
requirements.
Northstar Micro Devices • TECHNICAL DOCUMENTATION • ISSUE P4 11 / 11
