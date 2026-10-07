# B8 and B16 follow-on manual

The family has two choices: B8 with 8-bit integer arithmetic, and B16 with 8/16-bit integer
arithmetic, a binary32 FPU and the expanded peripherals. B16 replaces the earlier B8P name.
The source is [`latex/mcu/b16-follow-on.tex`](../../latex/mcu/b16-follow-on.tex), B16-004/P2.
Read it with the interface-03 B8 manual for behavior it does not amend.

P2 adds the separately selected D1 watchdog fuse option for both MCUs. The production watchdog
remains fused on. See [supervision configuration and reboot status](SUPERVISION.md).

## Numeric profiles

| Firmware data or operation | B8             | B16                       |
| -------------------------- | -------------- | ------------------------- |
| Signed/unsigned integers   | 8-bit          | 8-bit and 16-bit          |
| `float`                    | Rejected       | IEEE 754 binary32         |
| `double`, `long double`    | Rejected       | Rejected                  |
| Integers of 32+ bits       | Rejected       | Rejected                  |
| Wider peripheral values    | Explicit bytes | Byte bus; word arithmetic |
| Large byte buffers         | Permitted      | Permitted                 |

An 8-bit processor can emulate wider arithmetic in software. These are deliberate firmware
profiles, rather than a claim that an 8-bit CPU cannot compute a float. C++ also promotes byte
arithmetic to `int`; limiting variable declarations alone does not limit intermediate operations.
See the [C++ integral-promotion rules](https://eel.is/c++draft/conv.prom).

The build runs [`check_numeric.py`](../tools/check_numeric.py) before compiling the selected
firmware object, for both native and Emscripten builds. It uses Clang's semantic AST, checking
firmware declarations, local headers, aliases, inferred types, macro expansions, casts and
promoted expressions. Missing Clang or an analysis failure stops the build. LLVM Clang is required
for this check when compiling with GCC or MSVC; Clang/Emscripten builds use their selected driver.

The SDK and standard-library implementation are trusted. Their host intermediates, pointers,
register-address enums, compile-time assertions and array extents do not define the firmware
integer ALU width. A library call exposing an unsupported scalar in firmware is still rejected.
This is a source-policy guard, not a security sandbox, instruction emulator or proof that arbitrary
byte-array algorithms never implement wider arithmetic.

## Write width-preserving arithmetic

Include [`blender8/numeric.hpp`](../sdk/include/blender8/numeric.hpp). For example:

```cpp
std::uint8_t a = 250;
std::uint8_t b = 10;
a = b8::numeric::add(a, b); // 4, with no wider firmware expression
```

The helpers also provide subtract, multiply, bit operations, logical shifts, equality, ordering
and division. They return the selected integer width. Add/subtract/multiply wrap modulo that
width, including signed bit patterns. An oversized shift returns zero. Division returns quotient,
remainder, divide-by-zero and overflow fields; it does not throw. Check those fields explicitly.

B16 uses the same helpers with 16-bit arguments and can use `float` expressions directly. Use
`1.25f`, not `1.25`: the unsuffixed literal is a forbidden `double`. Float-to-integer conversions
still require finite/range checks. The checker does not prove those checks or detect every possible
runtime overflow. Neither profile permits a four-byte integer used to reinterpret float bits.

B8 must handle ADC results, timer counts and long application counters as explicit byte values
with deliberate carry/borrow. `access.hpp` paired-value helpers expose `uint16_t`, so the strict
B8 profile rejects their use; read/write the low and high registers separately. The underlying
interface-03 register semantics are unchanged. Fixed diagnostic probes and platform tests remain
host-side fixtures, separate from the checked New Employee firmware target.

## Select and check a profile

From the repository root, with the Python environment active:

```sh
python platform/tools/check_numeric.py --root platform --profile B8
python platform/tools/check_numeric.py --root platform --profile B16
python tools/project.py native --root . --numeric-profile B8
python tools/project.py wasm --root . --numeric-profile B8
```

`--device B8|B16` selects the MCU and matching numeric policy. The previous `--numeric-profile`
spelling remains an alias. The CMake setting is `B8_NUMERIC_PROFILE=B8|B16`; `B8_POLICY_CLANG`
can select an explicit Clang driver. Native and Wasm build directories are separate for each MCU.

## Peripheral implementation boundary

Both native and Wasm builds implement the selected behavioral device: B8 interface 03 keeps its
62 addresses and five vectors; B16 interface 04 adds SRAM, DMAC, pin routing and a sixth vector.
The shared host runtime takes a vector span, so B8 and B16 firmware retain their own table sizes.
B16 `float` operations execute as binary32 in the compiler target; conformance checks cover rounding,
subnormals, signed zero, NaN and infinity. No instruction ISA, FPU register state, instruction timing
or cycle cost is modeled. Logical SDK service timing and peripheral clocks remain the timing contract.

B16 identifies as `SYS_ID=16h`, `SYS_REV=04h`; capabilities read `0Fh` only on that device. It adds
1 KiB SRAM, one DMA channel, software/timer/DREQ/VBLANK requests, pacing, terminal status and
constrained pin routing. The complete pin matrix is on sheet 4. The expanded peripheral contract
retains byte-wide register transfers and DMA cells, despite the wider core arithmetic.

Rick offers either MCU for the exercise. The B16 follow-on stays separate from the seven shared
handouts and binder. Include `blender8/b16.hpp` for its added addresses, and provide six handlers in
`firmware::vectors` (DMAC is last). The B16 bench wires PWM to PB5, TACH to PB1, VBLANK to PB6,
and the temperature sensor to PA4/AN0. DREQ pads have low bias and no chassis request source.
Configure inactive peripherals, unlock with the consecutive key pair, set directions, select routes,
then lock before enabling them. Pin selection never moves board wires. See
[the device workflow](DEVICE-WORKFLOW.md) for builds, upload and tests.

## Generate the manual

```sh
python -m b8docs build --doc mcu_b16 --root .
python tools/project.py docs --root .
```

The complete command also generates the separate follow-on manual. Pristine, G1 and G2 stay
under `build/docs/`; the final copy is `dist/field/NMD-B16-Follow-On.field.pdf`. The cumulative
three-generation recipe and manifests follow the other manuals. Generated files remain ignored.
The optional manual is included once in a source bundle. The superseded B8P outputs are retained
under the ignored build folder, outside delivery.

The repository compiler resolves shared LaTeX styles; the native standalone editor cannot resolve
this multi-file project on its own. References for the peripheral design:

- [PIC32 DMA controller family reference](https://ww1.microchip.com/downloads/en/DeviceDoc/60001117H.pdf)
- [Microchip peripheral pin selection overview](https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/16bit-mcu/peripherals/digital-io/)
