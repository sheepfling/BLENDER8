# Blender-8 document ownership audit

## Publication P3 / foundry 0.4.0 / component interface 02 retained

## Conclusion

The saved P2 supplier binder did mix application requirements and component operation in several
places. This revision separates the authority of each statement rather than merely changing titles
or moving complete sections wholesale.

The delivered set is exactly six documents: boss email with attachments; generic B8; generic mux;
generic LCD; motor plus sensor papers; Kestrel button assembly. There are seven independently
compiled source papers because the fifth handout preserves Vortek and ThermaSense authorship. The
standalone board/integration manual is retired from the active catalog. Its wiring is retained as
Attachment B of document 1.

## Findings and corrections

| P2 location                | Ownership problem                                                                                                   | P3 correction                                                                                                                                                                                     |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| B8, p. 3                   | Even its scope disclaimer named STOP, motor enable and the appliance's mux.                                         | Replaced with generic board-assignment language; no assembled-product or other part identifiers.                                                                                                  |
| B8, p. 5                   | PWM guidance reached into the external driver's shutdown sequence.                                                  | Describes only B8 shadow duty and the immediate effect of disabling its own PWM pad. Product shutdown sequence stays in the brief.                                                                |
| BA-8, p. 1                 | Required the controller to enforce a startup interlock.                                                             | States actual latch retention across power loss; startup authorization belongs to R-002/R-025.                                                                                                    |
| BA-8, p. 2                 | Hardware bounce description included instructions for software qualification.                                       | Preserves arbitrary transitions within the 5 ms envelope and independent permit behavior, without supplying a firmware debounce policy.                                                           |
| BA-8, p. 3                 | The 50 ms PULSE-recognition rule was presented inside the assembly manual.                                          | Hardware has no built-in minimum-hold timer. R-005 in the product brief owns the 50 ms acceptance rule.                                                                                           |
| BA-8, p. 3                 | Persistent invalid-input arbitration and controller rearm rules appeared as assembly guidance.                      | Defines possible electrical observations and local bench tests. IF qualification and rearm remain in the product brief.                                                                           |
| MD20, pp. 2 and 4          | Statements about fault-reset and restart policy could be read as protection supplied by the motor.                  | Explicitly states that the motor resumes responding to still-asserted ENABLE/PWM after a lock is removed. It has no obstruction-history latch or automatic thermal cutout. Firmware owns lockout. |
| MD20, p. 4                 | Detailed host PWM-disable ordering and product-independent mandatory retry policy were embedded in the motor paper. | Replaced with intrinsic enable behavior, retained momentum/heat, dead-zone loss and sensor attachment. Product output-removal and rearm rules moved to the email attachments.                     |
| AVT10, p. 2                | A B8-shaped ADC example used the product's 65/85-degree thresholds as table points.                                 | Replaced with sensor-only temperature/voltage transfer points and a converter-independent resolution relation.                                                                                    |
| AVT10, p. 3                | Explicitly assigned the sensor to B8 AN0 and repeated B8 acquisition/completion timing.                             | No controller model, channel or register sequence appears in the sensor paper. Attachment B owns VOUT-to-AN0 and the board pull-up; B8 owns converter timing.                                     |
| Board integration, pp. 2–3 | Product requirements were buried inside what looked like another component document.                                | Product requirements now lead the packet as an engineering email with controlled acceptance attachments. Wiring is another retained attachment of the same document.                              |
| LCD and mux                | These were already substantially generic.                                                                           | Kept intrinsic registers, memory mapping, settling, busy timing and generic interface checks. Removed no required behavior merely to make every document look rewritten.                          |

## The ownership test

A statement belongs to a supplier manual when it remains true of that part after installing it in
unrelated equipment. A condition imposed on the finished blender, a decision its firmware must make,
or the selection of a particular board connection belongs to Kestrel's product brief.

Examples:

- **MCU fact:** disabling the B8 PWM block drives its pad low immediately. **Product requirement:**
  remove both the firmware enable request and PWM output within 10 ms of STOP or a qualified fault.
- **Button fact:** command-contact transitions occur within a 5 ms bounce envelope. **Product
  requirement:** qualify stable observations over 16 ms and meet a 40 ms recognition deadline.
- **Motor fact:** two tach edges represent one shaft revolution. **Product requirement:** apply the
  selected observation window, startup grace and ST qualification.
- **Sensor fact:** output follows `0.500 + 0.010 * Ts` volts with a 0.250 s mounting response.
  **Product requirement:** qualify TH from acquired readings at the selected threshold; continue
  sampling while faulted.
- **Wiring fact of this board:** VOUT is connected to AN0 with a diagnostic pull-up. **Not a sensor
  promise:** an open output always reports a high value on every possible board.

Component-use instructions remain appropriate where they follow directly from the interface: wait
for the mux's settling interval; do not issue LCD DATA writes while BUSY; read the B8 byte pairs
coherently. These are not the same as prescribing an application state machine.

## Six-document inventory

| Document                                       |  Pages | Authority                                                                                               |
| ---------------------------------------------- | -----: | ------------------------------------------------------------------------------------------------------- |
| 01 Requirements email and retained attachments |      6 | Kestrel product behavior, acceptance limits, firmware delivery and specific chassis wiring.             |
| 02 B8 microcontroller user manual              |      6 | Generic Northstar registers, reset, timers, interrupt arbitration, PWM, edge counter, byte bus and ADC. |
| 03 MX8-1 multiplexer                           |      3 | Generic TriAxis selection, settling and signal interpretation.                                          |
| 04 PX32-16 LCD                                 |      5 | Generic PixelRiver pixel packing, local registers, write recovery and scanout.                          |
| 05 MD20 and AVT10 papers                       |      7 | Vortek drive/thermal plant (4 pages), followed by ThermaSense sensor characteristics (3 pages).         |
| 06 BA-8 button assembly                        |      3 | Kestrel's manufactured mechanical/electrical assembly and local operation.                              |
| **Total**                                      | **30** | Six handouts; no separately issued wiring or sensor handout.                                            |

## Requirement traceability

All original requirement identifiers 001–030 remain represented as R-001–R-030 in the email
attachments. The original hash-checked `source_inputs/FW_REQUIREMENTS.md` is retained as an owner
reference. It is not distributed as an extra requirements document.

| Original IDs | Recipient location | Treatment                                                                                                         |
| ------------ | ------------------ | ----------------------------------------------------------------------------------------------------------------- |
| 001          | A4                 | MCU-only firmware boundary, no bench/plant/workstation shortcuts.                                                 |
| 002–003      | A1                 | Fail-off initialization and cold/released startup.                                                                |
| 004–006      | A1                 | Acquisition, stable recognition and conflict qualification.                                                       |
| 007, 010     | A1                 | Independent firmware STOP response and drive removal; B8-specific waveform details remain in MCU manual.          |
| 008–009      | A1                 | Seven nominal targets, allowed error, momentary boost and restoration.                                            |
| 011, 027     | A1 and A3          | Pixel indications, response timing and measurement boundaries.                                                    |
| 012–013      | A4                 | Scheduling, bounded execution and correct use of register/interrupt semantics.                                    |
| 014–016      | A2                 | Tach observation, fresh analog acquisition and transfer conversion; hardware equations live with their suppliers. |
| 017–021      | A2                 | Thermal/signal faults, startup grace, no-motion qualification and independent causes.                             |
| 022–026      | A3                 | Retained bits, priority, acknowledgment, power cycling and no autonomous recovery.                                |
| 028–030      | A4                 | Reproducibility, independent fixture evidence, tests and delivery.                                                |

There is no intentional change to the speed targets, acquisition budgets, protective thresholds,
debounce envelope, qualification periods, diagnostic priority or rearm sequence. This is a
documentation-ownership revision, not a new physical part or firmware implementation.

## Sensor name and packaging

The current part is AVT10, a powered linear analog voltage-output temperature sensor. Calling it a
thermocouple would imply a different interface. The revision bundles the existing sensor paper with
the motor paper; it does not silently introduce thermocouple conditioning or change the ADC
connection. B8 remains the MCU identifier; BA-8 remains the assembly identifier.

## Automated safeguards and limits

`b8-docs audit` checks declared lexical ownership boundaries and six-document membership before
`packet` publishes. Unit tests deliberately reintroduce a B8/AN0 instruction into the sensor sheet
and require rejection. Other tests check original source hashes, requirement-ID coverage, critical
product limits, absence of an extra board handout, vendor identities within the motor packet, six
top-level binder sections, and exactly six raster-only recipient PDFs.

These are regression guards, not a semantic theorem or proof of firmware acceptance. See
`QA-RESULTS.md` for this run's measured checks. No C++ model, HIL adapter, component rating or
real-appliance safety certification is created by this revision. The source provenance, audit and
instructor-facing caveats remain outside the six in-universe documents.
