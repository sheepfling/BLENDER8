# Blender-8 — firmware first, emulator second

Version 0.2.0 • 2026-10-06 • Instructor design pack and runnable C++20 reference model.

A fictional little blender with eight muxed command contacts, a separate STOP and power switch, a
32×16 pixel LCD, nonlinear PWM motor drive, tach feedback, and analog motor-temperature sensing.
Jordan writes the firmware; the instructor supplies the hardware behavior and test fixtures. Later,
Jordan can replace one peripheral implementation while keeping its datasheet and tests unchanged.

**The boundary:** firmware sees only the Blender-8 MCU SDK. Simulator-only component headers
describe nets, ports, physics and fixture controls. Neither `get_rpm()` nor `get_temperature()` is
available to firmware.

**Important:** this package contains a safe **unfinished student firmware stub**, not completed
protection firmware. The automated suite validates the reference models, SDK boundary and starter
safety. The customer-level firmware acceptance scenarios are specified separately and are not
claimed to pass.

## Read in this order

1. [Assignment and handoff](../ASSIGNMENT.md), [Architecture](../ARCHITECTURE.md) and
   [Board wiring](../BOARD_WIRING.md).
2. [Firmware requirements](../requirements/FW_REQUIREMENTS.md) and the six component manuals in
   `../datasheets/`.
3. [Test plan](../TEST_PLAN.md), [quirk catalog](../QUIRKS.md),
   [HIL migration](../HIL_MIGRATION.md) and
   [implementation status](../IMPLEMENTATION_STATUS.md).

The additions requested mid-design are integrated into this revision: AN0/ADC, linear analog
temperature sensor, case thermal dynamics, cold-stall protection, thermal trip, TS sensor/channel
faults and deliberate restart interlocks. Version 0.1 was an internal pre-thermal prototype, not a
separate deliverable you need to reconcile.

## Build and run locally

Requires a C++20-capable compiler and CMake 3.20+. Python 3.10+ is used only for local
specification/boundary checks. No downloaded C++ dependencies, GPU, game engine or CI service is
required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/blender8_bench
./build/blender8_student
```

With a multi-configuration generator, build with `--config Release`, run CTest with `-C Release`,
and use the executable in the corresponding configuration directory. The validation report states
which toolchains were actually tested; other platforms are not implicitly validated.

`blender8_bench` is an instructor signal-path demonstration. It drives registers, shows pixels and
motor/coast behavior; it is **not** a blender firmware solution or a thermal-protection
demonstration. `blender8_student` runs the safe starter for 100 ms; its unchanged output must keep
the motor off.

## Package structure

```text
sdk/include/blender8/     MCU-only firmware interface and register names
firmware/                Jordan's safe starter, entry points and wiring constants
sim/include/blender8/sim/ instructor-only component contracts and interconnects
sim/src/                 working reference component models and runtime binding
apps/                    headless instructor bench and student runner
spec/                    register map, teaching constants and requirement manifests
scenarios/               customer-acceptance scenario specifications, not an executable runner
tests/                  executable reference-model tests
tools/                  local boundary/profile/register checks
docs/                   assignment, architecture, six component manuals, requirements, tests
```

All artifacts are drafts for a fictional educational device. Do not connect this code to a mains
blender, cutting blades or a high-energy motor. A physical exercise should use a guarded low-energy
motor/indicator rig with independent power removal; see HIL-001. There is no claim of appliance
safety compliance.
