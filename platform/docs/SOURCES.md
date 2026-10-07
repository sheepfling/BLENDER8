# External inspiration and evidence boundaries

**Document:** SOURCES-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

Blender-8, BA-8, MX8-1, PX32-16, MD20 and AVT10 are fictional teaching components. Their timings,
equations, ratings and fault thresholds are design choices in this package. No commercial supplier
has validated them. The following primary sources informed general engineering distinctions, not the
fictional specifications.

**Analog Devices, TMP36 product documentation.** Linear voltage-output temperature sensors;10 mV/C
slope and 750 mV at 25 C support the choice of a simple offset-and-slope analog exercise. AVT10 is
not a drop-in claim about TMP36 accuracy, lag, wiring-fault behavior or ADC thresholds.

<https://www.analog.com/en/products/tmp36.html>

**Analog Devices, Using Analog Temperature Sensors with ADCs.** Discusses the analog sensor/ADC
interface, source impedance, sample/hold loading and settling. The B8 source is assumed compliant;
detailed analog acquisition-error physics is not included.

<https://www.analog.com/en/resources/design-notes/2022/07/16/06/59/using-analog-temperature-sensors-with-adcs.html>

**Microchip, Analog-to-Digital Converter Acquisition Time.** Distinguishes acquisition/settling from
conversion time and explains why source impedance matters. B8's4+9-cycle timing is its own explicit
synthetic contract.

<https://developerhelp.microchip.com/xwiki/bin/view/products/data-converters/adc-specs/acquisition-time/>

**Texas Instruments, SLVAFQ3, Motor Stall Detection With DRV8213 (January 2024).** Describes
stall-detection methods and the importance of startup blanking/qualification. Its specific
implementation is current-based. Blender-8 uses tach observation and independently selected timing
thresholds; TI's numerical parameters are not imported into this assignment.

<https://www.ti.com/document-viewer/lit/html/SLVAFQ3/GUID-8EE6B044-DE93-4F14-93B1-F0C5F83980A6>

The thermal equation is a stated lumped energy balance with a first-order analytical step. The
selected heat-loss law, case capacity, cooling conductances and optional food coupling are
illustrative model assumptions. They are not measured blender performance, evidence that ice
necessarily cools a real motor, or an appliance-safety assessment.
