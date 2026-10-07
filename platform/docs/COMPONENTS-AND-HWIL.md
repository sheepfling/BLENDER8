# Replaceable components and hardware-in-the-loop boundary

## Host-only ports and factories

`sim/include/blender8/sim/components.hpp` defines seven construction-time slots:

| Factory    | Electrical/transaction boundary                             |
| ---------- | ----------------------------------------------------------- |
| buttons    | eight individual contact nets, STOP_N and RUN_PERMIT        |
| mux        | eight inputs, three select nets and one output              |
| display    | four-register byte bus, scan advance, reset and VBLANK      |
| motor      | PWM and ENABLE inputs, TACH output, case thermal node       |
| sensor     | thermal attachment input and voltage output                 |
| oscillator | clock-frequency/validity net and powered advance            |
| power      | qualified logic supply, motor-supply state and rail voltage |

An empty slot creates the reference implementation. A nonempty slot must return a non-null
component. Only that implementation runs; there is no hidden reference motor in parallel with a
hardware adapter. Nets are owned by Board and outlive every component. References in port bundles
are non-owning. Drivers must be attached to those nets, not passed around as high-level `set_speed`
or `get_button` functions.

The jar permission circuit is board-specific chassis behavior, not part of the generic B8 or button
component. It is implemented in `jar_interlock.hpp/.cpp` and wired in Board. Replacing the whole MCU
execution backend uses `host::RegisterBackend`, not a fake component RPM API.

```cpp
b8::sim::ComponentFactories parts;
parts.display = [] { return std::make_unique<MyDisplay>(); };
b8::sim::Board board(b8::sim::BoardProfile::chassis04, std::move(parts));
```

`examples/component_swap.cpp` is a complete compiled example. Its replacement display wraps the
reference controller with an independent write counter. The same B8 XBUS transactions produce the
specified two vertical pixels. Run `build/b8_component_swap` after building.

Reference-only fixture functions such as `board.motor().set_jammed()` are explicitly unavailable if
that component is replaced by another type. Use `motor_device()` for the stable interface and
optional `MotorProbe` for observation. This prevents a UI slider or test from pretending it
physically jammed a motor when its injected implementation cannot do so.

## Observation is not required on hardware

A replacement may implement `DisplayProbe`, `MotorProbe` or `SensorProbe`, but it need not. Snapshot
fields are null when the probe is absent. A tach pulse stream is still a valid motor interface
without direct access to a shaft-RPM truth getter. The fixed browser fixture commands target the
supplied references; unsupported operations on replacements fail explicitly. Custom hardware-control
panels need their own capability-aware actions.

## Implemented link adapter

`hwil.hpp/.cpp` provides a tested `LockstepMotorAdapter` and `MotorPinTransport`. A request contains
a logical timestamp, sequence number, PWM level and ENABLE level. The response contains the matching
timestamp/sequence, tach level and validity, with an optional independently observed/modelled case
temperature. The adapter checks those fields before accepting a sample. A stale timestamp, wrong
sequence, invalid reply or exception calls `inhibit`, latches link failure and fails the run.
Construction and destruction also request inhibition.

```cpp
std::shared_ptr<b8::sim::MotorPinTransport> link = make_my_lockstep_link();
b8::sim::ComponentFactories parts;
parts.motor = [link](b8::sim::MotorPorts pins) {
    return std::make_unique<b8::sim::LockstepMotorAdapter>(pins, link);
};
b8::sim::Board board(b8::sim::BoardProfile::chassis04, std::move(parts));
```

The supplier PWM/tach semantics remain unchanged. Tests use an explicit loopback transport with a
private reference plant, verify tach delivery through the real board counter, and inject
stale/reordered/missing responses and teardown. The loopback is a transport test double, not a
hardware demonstration.

## Why realtime transports are rejected

The current scheduler may run faster or slower than wall time. One million synchronous USB/serial
transactions per second is not a credible implicit transport assumption. Therefore the adapter
accepts only `logical_lockstep`, a one-microsecond sample step, and a declared independent-inhibit
capability. It explicitly rejects `physical_realtime`, coarse periods or missing inhibit support.

A physical implementation must add a paced scheduler and measured timing budgets. Usually the device
side should generate PWM and timestamp/capture tach locally, exchanging bounded batches rather than
one host call per edge. It also needs sequence validation, stale-data handling, a **local**
loss-of-host timeout, reset mapping, startup inhibited outputs, electrically appropriate
isolation/levels, and physical acceptance measurements. Merely declaring capabilities is not
evidence of those properties.

The independent-inhibit method must be nonthrowing and must not depend on successful receipt of
another ordinary command. A process kill cannot guarantee hardware de-energization. Real hardware
must enforce that itself. No USB/DAQ implementation or real bench validation is claimed in this
release.

## Chassis-04 jar behavior

PB3 is raw JAR_OK; PB4 is latched JAR_PERMIT. An opening clears permission immediately at the
modeled boundary and dominates any simultaneous arm. Closure alone cannot arm. After at least 20 ms
continuously closed, a **new** STOP assertion can arm; STOP already held during closure cannot.
Reset or lost qualified logic power disarms. The separate physical STOP mechanism releases command
latches, so arming is not a start request.

Bounce and one-microsecond dropouts are tested. Open wiring is nonpermissive. A bypassed/welded loop
can still appear closed: the model does not grant an unimplemented fault-tolerant safety channel. No
brake, guard lock or physical access-time certification follows from these software checks.

## Thermal coupling is an explicit dependency

A pin-only motor transport does not know its physical case temperature from PWM/tach alone. Its
thermal node is marked unavailable unless the reply supplies that observation. A reference simulated
temperature sensor cannot silently read an invented 25-degree case: unsupported coupling fails the
run and invokes transport inhibit. Alternatively replace the sensor with a real voltage-input
component; it need not use the motor thermal node. That combination is tested separately. Missing
motor/sensor probes remain null. This distinction prevents a valid motion adapter from accidentally
producing fictitious temperature-protection evidence.
