# SIL-to-HIL migration contract and limits

**Document:** HIL-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## What “same interface” means here

The firmware source continues to call the MCU-only SDK and follow the same register, timing and
interrupt contracts. It never learns whether the attached button, motor or LCD is simulated. A
backend/board-support implementation performs the binding.

Blender-8 is fictional: there is no physical B8 chip with this register map. Moving to a real MCU
requires a B8-compatible SDK backend or a deliberately revised port of the register layer. The same
**source-level contract** is the goal; the same compiled binary, timing accuracy and unchanged
numeric peripheral addresses are not promised. A true CPU emulator would be a different later
deliverable.

## Useful hardware-in-the-loop configurations

| Configuration                  | Firmware executes on       | Plant/peripherals                                                 | What it verifies                                                              |
| ------------------------------ | -------------------------- | ----------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| Native SIL baseline            | Desktop reference runtime  | All modeled                                                       | Application logic and modeled timing contracts                                |
| Partial peripheral replacement | Desktop or physical target | One physical LCD/button rig, rest modeled                         | Adapter conformance and interface discipline                                  |
| Controller HIL                 | Real MCU                   | Real-time fixture drives digital/analog inputs, observes GPIO/PWM | Actual compiled firmware, peripheral timing and electrical interface behavior |
| Guarded low-energy bench       | Real MCU                   | Small guarded motor, sensor, controls and display                 | Integration beyond the toy physics, with independent protection               |

A PC sending GPIO changes through a slow USB link is not automatically microsecond-accurate HIL.
The2-us mux delay andPWM/tach edges belong on local hardware, a real-time fixture, FPGA, or target
peripherals; a desktop UI can send coarse events but should not be the timing authority. Measure
adapter latency/jitter, input thresholds, voltage ranges and maximum event rate.

## Adapter conformance requirements

Preserve GPIO direction and resolved input semantics; use an actual mux/contact harness or an
explicitly compatible digital fixture. Preserve PWM enable polarity, carrier/duty coding or a
documented mapping, immediate disable behavior and tach counting. Convert physical ADC behavior to
the B8 sampling/result contract only with explicit calibration and timing documentation. Drive the
LCD through its specified register transactions or supply a bridge that correctly translates them; a
host `drawText()` shortcut is not equivalent.

An analog fixture must generate safe voltage levels and account for its ground/reference arrangement
and source impedance. Simulator thermal nodes are modeling constructs, not physical electrical
ports. A real motor case sensor couples through actual mounting and thermal contact, which must be
characterized rather than assumed to have the synthetic 250-ms lag.

## Power and safety boundary

Use a low-energy, guarded educational rig: an indicator or small inertial motor load rather than a
mains blender or exposed cutting blades. Provide independent power removal, appropriate current
limiting/fusing and a stop circuit that does not depend on the student process running. Do not
transplant85/65 C teaching thresholds into an actual product without characterizing its sensor
location, winding temperatures, sensor failure cases and allowable operating envelope.

The delivered reference does not include a certified safety chain, driver current limits, real motor
selection, physical wiring drawing, hardware watchdog, emergency-stop certification or appliance
approval. Before a physical experiment, separately review electrical energy, exposed rotating parts,
driver fault behavior and fail-off states. HIL timing and safety claims require physical
measurement; a passing SIL test is evidence only for the modeled contract.
