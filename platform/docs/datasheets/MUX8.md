# MX8-1 digital 8-to-1 multiplexer

**Document:** MUX-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

MX8-1 has eight digital inputs X0..X7, select inputs S0..S2, and one digital output Y.
`selected=(S0?1:0)+(S1?2:0)+(S2?4:0)`. After a select or selected-input change, the output retains
its previous resolved value for 2 us, then follows the selected input. Another change during this
interval restarts settling. This is a deliberately visible teaching delay, not a claimed real-part
propagation time.

The device does not encode button identity, debounce contacts, or remember presses. Active-low
meaning comes from the connected button assembly. The board maps PA0..PA2 to S0..S2 and Y to PA3.
Firmware drives three outputs and reads one input for the eight command contacts. STOP remains a
separate PB2 input.

The simulator net resolver supports LOW, HIGH, high impedance and UNKNOWN. Opposing driven levels
resolve UNKNOWN; sampling a floating/contended value traps. This is diagnostic behavior rather than
an analog circuit solver. Do not accidentally configure PA3 as an output against the mux driver.

The provided `mux8.hpp` constructor accepts input/select/output nets. Its `advance(Tick)` propagates
delayed changes. A future alternative implementation must satisfy the same pin/timing contract, not
merely return the selected button index through a method call.
