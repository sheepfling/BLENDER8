# MD20 motor-and-driver module

**Document:** MOT-001 · **Revision:** 0.3.0 · **Date:** 2026-10-07 · **Status:** teaching-design
draft

## Boundary and scope

MD20 includes the modeled driver and motor. Inputs are logic PWM and logic ENABLE; output is a
2-pulse/revolution tach signal. Its mechanical/thermal case node couples to a separate analog
temperature sensor. The core interface is never `set_rpm` or `get_temperature` for firmware.
Instructor instrumentation may inspect truth, set load, jam the shaft or change thermal boundaries.

All ratings, curves and constants below are invented for education. This is an averaged drive/speed
model, not an electrical motor design, current-regulation simulation, winding-temperature model or
validated real blender.

## Nominal nonlinear speed curve

For effective PWM duty d∈[0,1] and normalized load L∈\[0,1\]:

```text
u = clamp((d - 0.12) / 0.88, 0, 1)
n_inf(d,L) = 20000 * u^1.6 * (1 - 0.5 L)       [RPM]
```

Duty up to12% lies in the dead zone. L=0 is the nominal no-load calibration; L=1 halves the
steady-state speed. A hard jam is a separate constraint forcing n=0, not just the L=1 setting.

For a positive no-load target n\*, inversion is:

```text
d* = 0.12 + 0.88 * (n*/20000)^(1/1.6)
```

A zero target means disable the drive, not merely command the dead-zone duty. Quantize using
MCU-001's duty coding and assess the resulting speed error. A requested 10% of rated RPM is **not**
10% PWM. Targets are nominal; load sag is allowed without PID in the core assignment.

| Speed | Target RPM | Rated fraction | Inverse duty | Example nearest code | Predicted no-load RPM |
| ----- | ---------- | -------------- | ------------ | -------------------- | --------------------- |
| 1     | 3,000      | 15%            | 0.388868     | 100                  | 3031.4                |
| 2     | 5,500      | 27.5%          | 0.512704     | 131                  | 5477.9                |
| 3     | 8,000      | 40%            | 0.616330     | 158                  | 8022.1                |
| 4     | 10,500     | 52.5%          | 0.708277     | 181                  | 10464.4               |
| 5     | 13,000     | 65%            | 0.792285     | 203                  | 13021.2               |
| 6     | 15,500     | 77.5%          | 0.870406     | 223                  | 15522.7               |
| 7     | 18,000     | 90%            | 0.943918     | 242                  | 18048.8               |

The table is instructor guidance; New Employee should derive/test the inverse function rather than receive
a prefilled application lookup table as his only work.

## Dynamics, PWM observation and tach

The model averages the physical PWM input over 256 us windows. While enabled, speed approaches its
current loaded target with a 120 ms time constant. Disabled, it coasts toward zero with a 350 ms
time constant. The update is exponential per1-us step, with a trapezoidal phase integral. A hard jam
forces speed to zero and prevents phase advancement. Clearing a jam does not clear firmware faults.

Tach is a 50%-duty square wave with two rising edges per revolution. At rated20,000 RPM its
rising-edge interval is 1.5 ms. At standalone PULSE's nominal2000 RPM it is 15 ms. Tach can continue
after commanded shutdown while the rotor coasts. Stuck-low and stuck-high fault fixtures suppress
continuing edge information even when the rotor is rotating.

A tach-only no-motion alarm cannot distinguish an actual obstruction from missing drive power, a
disconnected tach, or certain tach failures. The UI label ST therefore means commanded motion was
not verified. Current sensing or another independent observation would be a separate hardware
extension.

## Coupled thermal model

The reference plant solves motor case `Tm`, nearby air `Ta`, and optional food `Tf`. Room temperature
`Tr` is a prescribed external boundary. Sensor package temperature `Ts` remains a separate lag
(TEMP-001), not a winding node. Setting `Gmf=0` removes food from the heat exchange.

```text
Cm*dTm/dt = Qloss - Gma*(Tm-Ta) - Gmf*(Tm-Tf)
Ca*dTa/dt = Gma*(Tm-Ta) + Gfa*(Tf-Ta) - Gar*(Ta-Tr)
Cf*dTf/dt = Gmf*(Tm-Tf) - Gfa*(Tf-Ta)
Cm, Ca, Cf = 60, 200, 1200 J/K
Gma = 0.6 + 0.8*clamp(n/20000,0,1) W/K
Gmf = 0 by default; optional fixture range 0..0.2 W/K
Gfa = 0.12 W/K when food is present, otherwise 0
Gar = 20 W/K
```

The `Ca` node is an effective nearby-air and housing heat capacity, not a specified air volume. The
1200 J/K food capacity is a simple teaching mass. These values are synthetic and not derived from a
physical appliance. Heat crossing an internal path leaves one node and enters another; ventilation
exchanges heat with the fixed room boundary.

Define motion ratio r=`clamp(n/max(n_inf(d,0),1),0,1)` when nominal no-load speed exceeds 1 RPM;
otherwise r=0. The deliberately simple loss law is:

```text
Iload = 10*d*sqrt(1-r) [A]                   if ENABLE is true, else 0
Qloss = 0                                    if ENABLE is false
Qloss = 8*d + 22*d^2 + (1 ohm)*Iload^2 [W]   if ENABLE is true
```

Thus full-duty healthy no-load motion dissipates 30 W; a full-duty hard jam dissipates 130 W. `Iload`
is a loss-equivalent current proxy that makes the speed-dependent heating explicit. It is not a
measured supply or winding current and does not add a firmware current sensor.
Losses continue if faulty firmware keeps commanding drive into a jam; the plant does not silently
rescue it.

The reference uses fourth-order Runge-Kutta for the three temperatures with a maximum 0.1 s thermal
step. The board calls it every 1 ms with endpoint-frozen drive/RPM. The resulting motor/thermal
integration is an approximation, not an exact solution of the whole machine. Power cycling preserves
case, nearby-air and food heat as well as rotor momentum. With no food, constant speed and constant
loss, the equilibrium is `Ta = Tr + Qloss/Gar` and `Tm = Ta + Qloss/Gma`.

Cold food is an **optional weak thermal path**, e.g. initial `Tf=0 C` and `Gmf=0.15 W/K`. Its
effect depends on load-related heating at the same time; ice is not a magic motor-cooling switch.
Food temperature changes during the run. `environment Tr Tf Gmf` is a fixture replacement action:
it sets the room boundary and resets nearby-air and food initial temperatures. Melting/latent heat,
evaporation, detailed shaft conduction and full appliance energy accounting are outside this model.
Firmware never receives food or air temperature or the fixture load value.

## Thermal limits and required protections

The assignment trips at two measured samples ≥85 C; rearm requires measured ≤65 C for 2 s and
deliberate acknowledgment. Use 100 C case temperature as a **teaching out-of-envelope marker**, not
a validated safe material rating. There is no automatic motor thermal cutout in the reference model:
student protection is tested separately. Sensor lag, tolerance and case-to-winding gradients mean a
case reading is not winding-hotspot truth. Real hardware would need characterized limits,
independent protective hardware and a safety review.

No automatic fault clearing, reverse kick or jam retry is implemented in the plant. Follow REQ-FW
for behavior. Manual fixture changes in initial temperature are test setup or explicit fault
injection, not realistic instantaneous cooling during a normal experiment.
