# AVT10 analog case-temperature sensor

**Document:** TEMP-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## Why this device

AVT10 is a fictional powered linear analog temperature sensor, not an NTC thermistor. The first
analog assignment is therefore offset/slope inversion plus ADC handling, without also requiring
logarithmic resistance conversion. It uses the familiar linear-voltage-sensor style exemplified by
Analog Devices' TMP36, but its exact emulator contract and fault model are defined here, not
imported from a commercial datasheet. See SOURCES-001 for that distinction.

Connections are VDD=3.3 V, ground, VOUT→MCU AN0, and a mechanical/thermal attachment to the motor
case. The board supplies a weak diagnostic pull-up on AN0 so a disconnected sensor wire tends high
rather than silently reading room temperature. The single-driver AnalogNet model represents this
behavior without simulating resistor loading.

## Nominal transfer and lag

Nominal operating range is -20..125 C. Its linear output is:

```text
VOUT = 0.500 V + (0.010 V/C)*Ts
Ts   = (VOUT - 0.500 V)/(0.010 V/C)
dTs/dt = (Tm - Ts)/0.250 s
```

Ts is sensor package/mount temperature; Tm is motor case temperature. A sudden change of the
fixture's motor temperature does not instantly appear at AN0. The simulation initially equilibrates
both at 25 C, then evolves sensor lag even while panel power is off; ADC is unpowered then. The
always-evolving sensor state is a thermal approximation, not a powered electronic readout while off.

| Temperature | Nominal output | Ideal rounded10-bit code at 3.3 V |
| ----------- | -------------- | --------------------------------- |
| 0 C         | 0.500 V        | 155                               |
| 25 C        | 0.750 V        | 233                               |
| 65 C        | 1.150 V        | 357                               |
| 85 C        | 1.350 V        | 419                               |
| 100 C       | 1.500 V        | 465                               |

The exact half-code rounding direction follows MCU-001's mathematical transfer; floating-point test
comparisons at a half-code boundary must permit representation effects or choose values away from a
tie. The code spacing corresponds to approximately 0.3226 C per LSB. Quantized reconstructed
temperature, not simulator case-temperature truth, is the firmware's input.

## Analog channel behavior

Take fresh conversions every 10 ms or faster, waiting for conversion completion. At default ADC div4
a conversion takes 52 us and samples after 16 us. Read DATA_LO then DATA_HI; track sample freshness
from READY/completion events. A second read of an old code is not a fresh sample, even if the number
is reasonable.

Core plausibility limits are codes62..574 inclusive. Codes outside that envelope set TS. This
deliberately leaves margin outside the nominal sensor range. A valid measurement at or above 85 C is
TH after two fresh consecutive samples. Plausibility and thermal thresholds are different decisions.

## Defined fixtures

Healthy nominal mode has zero added error. The sensor fixture can add offset, fractional slope error
and bounded pseudorandom voltage noise with a recorded seed. The typical bounded teaching-noise
profile is ±2 mV with a 1-ms update; nominal tests use zero noise. Reference-voltage error is
configured on the ADC, not silently folded into sensor gain.

Open circuit resolves to the board's3.3 V pull-up; short-ground resolves0 V; short-supply
resolves3.3 V. All produce implausible rail readings. `frozen_output` preserves a plausible voltage
while the physical sensor/case continues evolving. That last fault is intentionally **not reliably
diagnosable** from one analog channel alone. Do not grade a student for infallible detection or
claim temperature freshness proves the physical sensor is healthy.

Use offset and noise corner tests to verify threshold/rearm robustness, but measure firmware
acceptance against the published acquired samples and tolerance envelope. Do not silently tighten85
C into an instantaneous true-winding-temperature guarantee. A later NTC option must specify its
divider, R25, beta/Steinhart-Hart data, Kelvin conversion, source impedance and open/short behavior
as a different part revision.
