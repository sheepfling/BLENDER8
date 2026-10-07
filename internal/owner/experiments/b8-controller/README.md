# Worked B8 controller

This example belongs to `feature/b16-solution`, the teaching branch, alongside the B16 example.
It stays outside `main` and the learner source handoff. Start with the
[teaching route](../../../../TEACHING-SOLUTION.md). The employee starter remains unfinished.

## Build and run

Activate the repository Python environment and, for Wasm, Emscripten. From the checkout root:

```sh
python tools/teaching.py native --root . --device B8
python tools/teaching.py test --root . --device B8
python tools/teaching.py verify-native --root . --device B8
python tools/teaching.py wasm --root . --device B8
python tools/teaching.py check-wasm --root . --device B8
python tools/teaching.py verify-wasm --root . --device B8
python tools/teaching.py serve --root . --device B8 --port 8095
```

Native and Wasm products use `build/teaching-b8-native/` and `build/teaching-b8-wasm/`.
Full reports use `reports/teaching-b8-native.json.gz` and `reports/teaching-b8-wasm.json.gz`.
The production watchdog fuse and standard memory profile are selected explicitly. To upload,
choose B8 and the matching `b8_student.mjs` and `b8_student.wasm` from this build's `web/` directory.
Use **Selected firmware**. The hosted Site remains its separately published B16 snapshot.

Allow two cool logical seconds, press STOP, release controls, then choose a speed. The workbench
controls, recovery rules, fault labels and physical experiments are the same as the B16 example.
The emulator, plant and visualization retain full precision.

## What changed from B16

| Area         | B8 implementation                                                    |
| ------------ | -------------------------------------------------------------------- |
| Arithmetic   | Byte scalars and explicit two-byte representations; no native words  |
| Timer epoch  | Carry-aware byte increment; foreground snapshot masks the ISR        |
| Long ages    | Byte carry/borrow; saturation at 30,000 ms; wrap-aware tick delta    |
| ADC and tach | Low-byte capture followed by high-byte read by one foreground owner  |
| Temperature  | Raw ADC comparisons: hot at 419+, cool at 356 or below               |
| GPIO         | Fixed interface-03 wiring and five vectors; no B16 route writes      |
| Display      | 148 font/index bytes; stream each display byte without a framebuffer |
| Diagnostics  | Three explicitly retained binding-owned bytes, cleared on POR/BOR    |
| DMA/FPU      | Neither is used or required                                          |

The nominal transfer is `T = 330*N/1023 - 50`. Therefore the first measured code at or above
85°C is 419, and the last at or below 65°C is 356. Plausibility remains codes 78–558. The B8
firmware compares high bytes first, then low bytes, with no float or wider integer intermediate.
These are nominal measured-value decisions; they do not add an installed accuracy guarantee.

Read [firmware.cpp](firmware.cpp), [byte_pair.hpp](byte_pair.hpp) and [pixels.hpp](pixels.hpp)
together. The shared [B16 design table](../b16-controller/README.md#design-choices-and-timing)
still describes the clock tuple, mux settling, debounce, motion grace, recovery and monitor margins.
B8 uses the dedicated PWM connection, rather than the B16 pin-selection peripheral.

## Carry, borrow and overflow

The public binding exposes no instruction flags register. Each helper keeps its carry or borrow
local to the operation:

- Addition computes the low byte, detects `low_result < original_low`, then adds the carry high.
- Subtraction detects `left_low < right_low`, then subtracts that borrow high.
- Counter differences wrap modulo 65,536 through two byte subtractions.
- Elapsed ages saturate when addition wraps or reaches the cap. Saturation prevents an old
  qualified condition becoming unqualified through wrap.

Unsigned carry and signed overflow are different conditions. This controller uses unsigned byte
representations. It neither consumes nor changes a hypothetical CPU status register. A future
instruction-set model would need instruction-specific flag and interrupt save/restore rules.

The timer ISR only acknowledges T0 and increments its epoch. Foreground masks global interrupts
while copying both epoch bytes, then restores the previous mask. It owns paired ADC/tach reads
and staged timer/deadman writes. The ISR never renews supervision or owns the external LCD bus.

## Reset retention boundary

The B8 manual's software-owned-state contract says static constructors are not rerun on MCU
reset. This example deliberately preserves only a magic byte, fault mask and reset detail in
ordinary binding-owned static storage. Reset entry reads causes first, clears that diagnosis
on POR/BOR, and explicitly initializes all command, timing and authorization state on every reset.
Qualified deliberate recovery clears the retained diagnosis.

This works under the supplied native/Wasm binding. It is not a newly invented B8 SRAM register
window or a guarantee about a physical C++ startup runtime. A physical port needs a documented
retained/no-init region and startup handling, with power-loss behavior and integrity checks.
See the [requirements discussion](REQUIREMENTS-AUDIT.md), especially diagnostic lifetime.

## Font and memory

The compact font holds twenty 5×7 glyphs (100 bytes) and twenty-four pairs of character indices
(48 bytes). The renderer expands one column/page byte at a time. It needs no 64-byte framebuffer.
The 1,536-byte `font.hpp` remains an independent generated reference for tests and is not included
by the firmware. All 1,536 emitted bytes are compared to that reference.

Regenerate both forms and the encoding oracle after editing `font-source.json`, then rebuild:

```sh
python tools/teaching.py font --root . --device B8
```

| Build                       | Program bytes | Static RAM bytes | Profile        |
| --------------------------- | ------------- | ---------------- | -------------- |
| Native Clang                | 5,933         | 79               | B8 standard    |
| Emscripten                  | 7,230         | 395              | B8 standard    |
| Clang address/UB sanitizers | 14,273        | 3,520            | B8 plus, debug |

Standard B8 allows 16,384 program bytes and 2,048 RAM bytes. These receipts measure isolated
object sections; runtime stack, heap and external linked helpers are unaccounted. The controller
does not allocate a heap. These are host-target section measurements, not a linked physical B8
image or measured B8 stack usage. Sanitizer instrumentation fails the standard RAM gate, so only
the instrumented debug build uses plus (4,096 RAM / 32,768 program bytes).

## Verification and limits

The [bounded verification receipt](verification.json) records source/image hashes and results:

- 61 automated correspondence passes on native Clang, GCC, address/UB sanitizers and real Wasm.
- 19 inherited additional scenarios and three characterization probes on each build. The third
  probe records the selected warm-reset versus brownout diagnostic lifetime.
- 237 native CTest checks per native toolchain configuration.
- Three helper checks: all 65,536 values with eight boundary deltas, all 1,536 display bytes,
  generated-font freshness, and actual native execution beyond the 65,536-tick wrap.
- Eleven compiled candidate workbench checks for both devices, including more than 70 logical
  seconds without a reset, followed by responsive STOP handling.

The progress-authorization and combined-fault-retention human reviews remain open. Glyph encoding
does not approve readability. The [common audit](../b16-controller/REQUIREMENTS-AUDIT.md) still has
four unresolved customer decisions. Service-boundary timing supplies no physical instruction-cost
or interrupt-latency proof. No customer correspondence or supplier requirements were weakened.
