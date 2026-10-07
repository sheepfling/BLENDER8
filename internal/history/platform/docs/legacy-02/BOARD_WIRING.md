# Blender-8 board wiring and signal ownership

**Document:** BOARD-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## Wiring contract

| Connection           | Source                       | Destination                    | Meaning                                  |
| -------------------- | ---------------------------- | ------------------------------ | ---------------------------------------- |
| Command contact0..6  | BA-8 speed1..7 contacts      | MX8 X0..X6 with board pull-ups | Active-low, bouncy                       |
| Command contact7     | BA-8 PULSE                   | MX8 X7 with pull-up            | Active-low, momentary                    |
| MUX select0..2       | MCU PA0..PA2                 | MX8 S0..S2                     | Three digital outputs                    |
| MUX return           | MX8 Y                        | MCU PA3                        | One digital input                        |
| STOP sense           | BA-8 STOP_N                  | MCU PB2                        | Dedicated active-low input               |
| Motor request        | MCU PB0                      | enable AND gate                | Firmware-owned MOTOR_EN, board pull-down |
| Run permission       | BA-8 RUN_PERMIT              | enable AND gate                | Independent mechanical inhibit           |
| Panel power          | panel switch                 | electronics and enable gate    | External reset/power control             |
| Actual driver enable | power AND RUN_PERMIT AND PB0 | MD20 ENABLE                    | Not a firmware-call bypass               |
| Motor drive          | MCU dedicated PWM pad        | MD20 PWM                       | Physical digital waveform                |
| Rotation feedback    | MD20 TACH                    | MCU PB1/tach counter           | Two rising edges/revolution              |
| LCD bus              | MCU XBUS bridge              | PX32 registers0..3             | Ordered byte transactions                |
| Case attachment      | motor ThermalNode            | AVT10 thermal attachment       | Simulator plant connection only          |
| Temperature voltage  | AVT10 VOUT                   | MCU AN0                        | Analog voltage, board diagnostic pull-up |
| Spare analog         | board ground                 | MCU AN1                        | Grounded spare channel for tests         |

The component API contains pins, nets and fixture methods; the firmware API contains MCU register
transactions only. A button assembly has contacts; it does **not** also have the mux's select lines.
The mux is an independent connected part.

## Composition view

```text
scenario/UI → button mechanics → contact nets → 8:1 mux → MCU GPIO input
                                           MCU GPIO output → mux selects

MCU PWM output ────────────────────────────────────────→ motor driver
MCU PB0 ─┐
STOP/command mechanical permit ─┼─ AND with panel power → motor ENABLE
panel power ───────────────────┘

motor rotor → tach wire → MCU PB1/counter → firmware observation
motor case → sensor thermal lag → analog voltage → AN0/ADC → firmware observation
MCU external byte-bus bridge → LCD VRAM → row scanout → host pixel preview
```

No firmware line connects directly to rotor RPM, case temperature, a contact-state array, simulator
time or LCD debug memory. `Board` is the simulator composition root; `board_config.hpp` gives
firmware only wiring constants.

## Physical realization limit

These interfaces preserve semantics across implementations, not necessarily a literal physical
package pinout. The LCD bus is transaction-level in this revision; GPIO, PWM, tach and analog
voltage use explicit signal connections. A future physical B8-compatible backend must map these
operations to real GPIO, ADC, PWM/timer and LCD-bus hardware and meet their timing contracts. The
fictional MCU has no off-the-shelf chip to buy.
