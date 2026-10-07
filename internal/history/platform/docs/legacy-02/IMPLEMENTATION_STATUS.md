# Implementation scope and remaining work

**Document:** STATUS-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design
draft

## Included and exercised

Native C++20 firmware SDK and safe application stub; standalone-compiling firmware/simulator
headers; resolved digital nets and analog/thermal signal connections; interlocked buttons with
nominal/slow bounce and sense-contact faults; delayed mux; pixel-memory LCD with
busy/read/write/scanout behavior; MCU timers/interrupts/PWM/tach counter; timed10-bit ADC; nonlinear
motor mechanics, hard jam and tach faults; lumped case heating/cooling with optional food path;
sensor lag, offset/gain/seeded noise and wiring faults; board reset/power/STOP behavior; local
build, headless bench and reference test suite.

## Deliberately not supplied as the student's answer

Completed scanning/debounce firmware, a completed protection/rearm state machine, final pixel
font/UI, calibrated inverse mapping in the application, closed-loop speed regulator, or the
customer's finished firmware acceptance evidence. Those are Jordan's work. The reference plant
models do not contain a hidden ST/TH supervisor.

## Specified but not implemented as a finished feature

The38 full-firmware acceptance scenario definitions and their traces are provided, but there is no
general JSON-event replay/GUI runner in this revision. There is no graphical game-style panel; the
headless bench prints scanned pixels. No physical HIL adapter, actual MCU port, bus-strobe model,
hardware safety certification, CPU instruction decoder or cycle-accurate firmware execution is
supplied.

These are explicit limits of the delivered starter, not claims that a future result was already
tested. The architecture/spec/header task is complete as a draft package; application acceptance and
hardware migration require their own implementation and evidence.
