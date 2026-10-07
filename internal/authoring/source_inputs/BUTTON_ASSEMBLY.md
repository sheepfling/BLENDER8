# BA-8 interlocked button assembly manual

**Document:** BTN-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design draft


## Constructed part and controls

BA-8 is the fictional company's mechanical assembly, not a generic microcontroller peripheral. It contains seven mutually interlocked, latching speed buttons, one momentary PULSE button, a separate STOP button, and a separate panel power switch. Thus the mux has exactly eight **command** channels, not every switch on the product.

Pressing speed N releases every other speed latch. PULSE does not unlatch the selected speed. Pressing STOP releases every speed latch, releases the electrical PULSE command even when held, and independently removes RUN_PERMIT. STOP dominates simultaneous speed/PULSE action. Releasing STOP does not re-latch anything. A PULSE held through STOP remains blocked until released and freshly pressed. Pressing a speed while STOP is held is ignored.

## Connections

Eight active-low sense contacts C0..C7 go to mux X0..X7; C0..C6 are speed1..7, C7 is PULSE. Each contact closes to ground and otherwise releases to high impedance. The board supplies pull-ups. The assembly does not contain the mux address decoder.

RUN_PERMIT is high only while STOP is released and at least one physical command is active. A separate STOP_N sense contact connects directly to MCU PB2. The reference's safety link and STOP_N are idealized logical connections: STOP removes permit immediately when the board wiring settles, independent of software and debounce. The eight sense contacts bounce; the modeled independent inhibit does not. A real assembly would require separate verified electrical contacts and an actual safe driver circuit.

## Contact timing and faults

The published bounce envelope is arbitrary make/break transitions confined to 5 ms after a mechanical change, then the final state. The supplied nominal pattern changes at 0,100,400,900,1800 us. The slow pattern changes at 0,1000,2000,3000,5000 us. Those exact patterns are fixtures, not the basis for the student's debouncer. The test set must be able to choose either; the software must use the envelope and elapsed time.

A speed latch remains until another speed or STOP. Reliable PULSE recognition is required for holds of at least 50 ms; shorter taps may be missed by scanned/debounced inputs. Independent contact stuck-open/stuck-closed faults are available. A contact fault changes only its electrical sense wire, not the underlying mechanical latch or RUN_PERMIT. Multiple sensed speeds are consequently possible under faults or sequential-scan transients even though the healthy physical latch is one-hot.

## Power interaction

Panel power resets electronics, not this mechanical assembly. Speed latches may remain depressed across a power cycle. The rotor and thermal state likewise remain physical state. Firmware must refuse a power-on run until commands have been released and the startup temperature interlock has qualified.

## Simulator-only API

`button_assembly.hpp` exposes `press_speed`, `pulse`, `stop`, `contact_fault`, `set_bounce` and `set_slow_bounce` to the instructor fixture/UI. Its electrical outputs are `DigitalNet` connections. Firmware receives none of these methods and must scan actual MCU GPIO.
