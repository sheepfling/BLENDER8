# Blender-8 microcontroller programmer reference

**Document:** MCU-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## Scope and identity

B8 is a fictional 8-bit **peripheral/register interface** with a 16-bit register address space and a
nominal 1 MHz peripheral clock. This revision does not define an instruction set, CPU pipeline,
program ROM, instruction encoding or cycle-accurate C++ execution. The reference executable runs
native host C++ firmware against register-level emulated hardware.

The only runtime firmware operations are `b8::read8(Reg)`, `b8::write8(Reg,uint8_t)` and
`b8::idle()`. `firmware::reset()`, bounded `firmware::step()` and a five-entry `firmware::vectors`
table are firmware entry points. The instructor runtime binds those free functions to the emulated
MCU. No simulator object is passed into firmware.

Every SDK read/write is one ordered 8-bit register transaction at current logical time, followed by
1 us of device advancement and an interrupt-dispatch opportunity. `idle()` advances 1 us. Ordinary
host arithmetic has no emulated cycle cost. Direct instructor fixture access to `Mcu::read8/write8`
does not advance time; it is not the SDK behavior. A C++ array of register bytes is not an adequate
peripheral model because reads and writes have side effects.

## Pin groups and reset

The MCU provides GPIOA[7:0], GPIOB[7:0], a dedicated PWM output, a tach input on PB1, a dedicated
LCD-byte-bus bridge, and AN0/AN1 analog inputs. GPIO DIR=0 releases a pad; DIR=1 drives the OUT
latch. OUT resets low; DIR resets input. Reading IN samples resolved pads. Board pull resistors,
peripheral drivers and contention determine the result; IN is not always equal to OUT.

The board connects PB2 to active-low STOP. This is a board wiring assignment, not an MCU-native stop
command. Power reset disables PWM, clears IRQ enables/flags, resets timers and ADC state, and
releases GPIO output pads. The board motor-enable pull-down holds the physical driver off. No safe
operating state may depend on uninitialized host memory.

## Register map

R means read-only. RW means ordinary read/write. W1C means write a one to clear just that bit;
writing zero does nothing. Unimplemented addresses and writes to read-only registers trap in this
teaching simulator. Reserved RW bits are ignored and read zero. These strict traps are diagnostic
teaching behavior, not a claim that arbitrary real hardware throws C++ exceptions.

| Address  | Register        | Access | Reset  | Meaning                                                        |
| -------- | --------------- | ------ | ------ | -------------------------------------------------------------- |
| `0x0000` | `SYS_ID`        | R      | `0xB8` | Device identifier                                              |
| `0x0001` | `SYS_REV`       | R      | `0x02` | Interface revision                                             |
| `0x0010` | `IRQ_GLOBAL`    | RW     | `0x00` | bit 0 global enable                                            |
| `0x0011` | `IRQ_ENABLE`    | RW     | `0x00` | bits 0..4 source enables                                       |
| `0x0012` | `IRQ_FLAGS`     | W1C    | `0x00` | TIMER0, TIMER1, TACH, VBLANK, ADC complete                     |
| `0x0020` | `GPIOA_DIR`     | RW     | `0x00` | 1 output, 0 input                                              |
| `0x0021` | `GPIOA_OUT`     | RW     | `0x00` | Output latch                                                   |
| `0x0022` | `GPIOA_IN`      | R      | `0x00` | Resolved pad values; wiring determines reset read              |
| `0x0028` | `GPIOB_DIR`     | RW     | `0x00` | 1 output, 0 input                                              |
| `0x0029` | `GPIOB_OUT`     | RW     | `0x00` | Output latch                                                   |
| `0x002A` | `GPIOB_IN`      | R      | `0x00` | Resolved pad values; wiring determines reset read              |
| `0x0040` | `T0_CTRL`       | RW     | `0x00` | bit 0 enable, bit 1 periodic; writes reset counter and divider |
| `0x0041` | `T0_PRESCALE`   | RW     | `0x00` | codes 0,1,2,3 = divide 1,8,64,256                              |
| `0x0042` | `T0_COUNT_LO`   | R      | `0x00` | Read low to latch high                                         |
| `0x0043` | `T0_COUNT_HI`   | R      | `0x00` | Latched high; high-first returns stale latch                   |
| `0x0044` | `T0_COMPARE_LO` | RW     | `0x00` | Stage low byte while disabled                                  |
| `0x0045` | `T0_COMPARE_HI` | RW     | `0x00` | Commit staged low and written high while disabled              |
| `0x0050` | `T1_CTRL`       | RW     | `0x00` | bit 0 enable, bit 1 periodic; writes reset counter and divider |
| `0x0051` | `T1_PRESCALE`   | RW     | `0x00` | codes 0,1,2,3 = divide 1,8,64,256                              |
| `0x0052` | `T1_COUNT_LO`   | R      | `0x00` | Read low to latch high                                         |
| `0x0053` | `T1_COUNT_HI`   | R      | `0x00` | Latched high; high-first returns stale latch                   |
| `0x0054` | `T1_COMPARE_LO` | RW     | `0x00` | Stage low byte while disabled                                  |
| `0x0055` | `T1_COMPARE_HI` | RW     | `0x00` | Commit staged low and written high while disabled              |
| `0x0060` | `PWM_CTRL`      | RW     | `0x00` | bit 0 enable; clear drives low immediately                     |
| `0x0061` | `PWM_DUTY`      | RW     | `0x00` | Shadow duty, applied at next 256-tick boundary                 |
| `0x0070` | `TACH_COUNT_LO` | R      | `0x00` | Low read latches high                                          |
| `0x0071` | `TACH_COUNT_HI` | R      | `0x00` | Latched high                                                   |
| `0x0080` | `XBUS_REG`      | RW     | `0x00` | External device register address, low 2 bits                   |
| `0x0081` | `XBUS_DATA`     | RW     | `0x00` | One external byte-bus transaction                              |
| `0x0090` | `ADC_CTRL`      | RW     | `0x00` | bit0 enable; bit1 START is write-only strobe                   |
| `0x0091` | `ADC_CHANNEL`   | RW     | `0x00` | 0 AN0; 1 AN1; writes while busy ignored with ERROR             |
| `0x0092` | `ADC_PRESCALE`  | RW     | `0x02` | codes 0..3 = div1,2,4,8; changes require idle                  |
| `0x0093` | `ADC_STATUS`    | R/W1C  | `0x00` | bit0 BUSY; bit1 READY; bit2 OVERRUN W1C; bit3 ERROR W1C        |
| `0x0094` | `ADC_DATA_LO`   | R      | `0x00` | low read latches high and acknowledges READY                   |
| `0x0095` | `ADC_DATA_HI`   | R      | `0x00` | bits0..1 latched high; high-first is stale                     |

## Timers

T0 and T1 are independent 16-bit compare timers. Prescaler codes 0/1/2/3 mean divisors 1/8/64/256.
CTRL bit0 enables; bit1 selects periodic reload instead of one-shot. Every CTRL write resets COUNT
and the divider, including writes that leave enable set. Program compare/prescale only while
disabled. Write COMPARE_LO to stage it; COMPARE_HI commits both bytes. Reading COMPARE before the
high-byte commit returns the prior committed value.

On each divided clock, the timer checks COUNT against COMPARE: equal generates a flag and reloads
COUNT=0, otherwise increments. Thus:

```text
period_us = (COMPARE + 1) * prescaler_divisor
COMPARE=999, divisor=1 → 1 ms
COMPARE=124, divisor=8 → 1 ms
COMPARE=65535, divisor=1 → 65.536 ms
```

One-shot stops at compare. Reading COUNT_LO snapshots COUNT_HI; read HI second. A new low read
overwrites the latch. An ISR that reads the same counter between foreground low/high reads can
disturb the snapshot; give the register one owner or protect the pair.

## Interrupts

Vector indexes/flag bits are 0=T0, 1=T1, 2=tach rising edge, 3=LCD VBLANK rising edge, 4=ADC
completion. Lower index wins among pending enabled sources. Each source has a sticky pending bit
even when masked; repeated events coalesce rather than queue. IRQ_GLOBAL bit0 gates delivery, and
IRQ_ENABLE gates individual sources.

Entry masks global delivery; nesting is not supported. The handler is a normal C++ function
returning void. Return restores global delivery to its enabled pre-entry state; a write disabling
global IRQ inside an ISR does not persist across the modeled return. Missing vectors trap. Firmware
explicitly W1C-acknowledges serviced IRQ_FLAGS; entry does not automatically clear them. At most one
pending source is dispatched at each SDK boundary. The hardware tach counter, unlike its one-bit IRQ
flag, counts every modeled rising edge.

## PWM

The carrier is 256 ticks (3,906.25 Hz). For codes 0..254, duty is code/256; code 255 is specially
100%. The DUTY register is a shadow. The next carrier boundary copies it to active duty. Enabling
mid-period does not reset the carrier. Clearing PWM_CTRL.EN drives the pin low immediately at the
register write; clearing the shadow duty alone may leave old active pulses until a boundary.

## Tach counter

TACH_COUNT counts PB1 rising edges modulo65536; it continues while the CPU is powered even if PWM is
off and the rotor is coasting. Low read latches high. There is no count-clear register except reset.
Use `(uint16_t)(new_count-old_count)` under a bounded observation interval. At 20,000 RPM and 2
pulses/revolution, the nominal maximum is 666.67 rising edges/second, far below one full wrap per
200 ms observation window.

## ADC

AN0/AN1 are 10-bit single-ended inputs with nominal 3.3 V reference. In this fictional design the
defined transfer is `N=clamp(floor(1023*V/Vref+0.5),0,1023)`. This explicit endpoint-rounded
convention avoids ambiguity; do not assume it describes every commercial ADC.

ADC_CTRL bit0 enables; writing bit1 starts one conversion. The START bit reads zero. To enable and
start, write 0x03; writing START alone writes EN=0 and therefore disables/aborts the converter.
Prescaler codes 0..3 mean divide1/2/4/8. A conversion comprises 4 acquisition cycles plus 9
conversion cycles: default div4 takes 52 us. Voltage and reference are sampled after acquisition (16
us at div4), then held until completion. Source-voltage changes after sampling affect the next
conversion, not the current result.

Starting while busy or disabled sets ERROR and does not restart the active conversion.
Channel/prescaler writes while busy are ignored and set ERROR. Disable aborts the conversion and
clears READY but retains the previous result. Completion sets READY and the ADC IRQ flag. Completing
over an unread READY result sets OVERRUN and replaces it with the latest result. ADC_DATA_LO
snapshots high and clears READY; HI returns the snapshot. Reading the result does not acknowledge
IRQ_FLAGS. STATUS.ERROR and OVERRUN are W1C; BUSY/READY are read-only.

The low-output-impedance voltage sensor meets this fictional ADC's acquisition envelope.
Source-impedance-dependent acquisition error, analog clamp currents and actual ADC INL/DNL are not
modeled. Reference-voltage variation, quantization, sensor offset/gain/noise, busy misuse and stale
data are distinct available effects.

## External byte bus

XBUS_REG holds the 2-bit external device register number; each XBUS_DATA read/write issues one
transaction to the LCD. Firmware sees MCU bridge registers, not an `Lcd32` class. The reference
models transaction semantics, not every RD/WR/CS setup/hold transition on a physical parallel bus.
Pin-accurate bus signaling is a later fidelity level.
