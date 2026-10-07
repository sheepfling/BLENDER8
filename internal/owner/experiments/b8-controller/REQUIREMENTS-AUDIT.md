# B8 requirements experiment

Authority remains the accepted P5 correspondence, supplier LaTeX and chassis 04. The
[common requirements audit](../b16-controller/REQUIREMENTS-AUDIT.md) applies to both controllers:
ADC/deadman interaction, deliberate CK recovery, diagnostic lifetime, and the validation envelope
still need customer decisions. B8 passing automated cases does not resolve them.

## B8 feasibility result

The B8 candidate meets the same 61 automated cases and inherited additional scenarios as B16,
under the supplied native/Wasm binding. Its source passes the strict B8 byte-only policy and its
ordinary native/Wasm sections fit standard memory. Floating point, DMA, expanded pin routing and
B16 SRAM are not needed for the defined control behavior.

Timing quantities exceed a byte, but are represented as explicit bytes with carry/borrow. That is
permitted by the follow-on manual. No native 16-bit operation, float, assembler escape, host clock,
simulator object or hidden device register is introduced into firmware.

## Diagnostic retention

The controller uses the B8 manual's explicit reset-entry binding contract: MCU reset does not
rerun C++ static constructors. Three diagnostic bytes survive warm reset, while reset entry clears
them on POR/BOR and initializes command/authorization state every time. Warm-reset TS and the
brownout characterization probe exercise the chosen lifetime.

The brownout probe documents this implementation choice; it is not a new customer grading rule.
The exact required diagnostic lifetime and service retrieval interface remain E-03. A physical
port needs a specified retained/no-init section and startup implementation. Native/Wasm static
storage behavior does not establish physical power-loss retention or protection from corruption.

## Numeric and resource evidence

Comparisons against nominal ADC codes preserve the measured-value temperature decisions without
computing degrees in firmware. Sensor/reference error and a broader noise guarantee remain E-04.
The host plant continues to use full precision.

Memory receipts count isolated object sections. They exclude stack, heap and external linked helper
costs and are not an instruction-set-specific linked image. The compact streamed font reduces
static storage. Debug sanitizer instrumentation uses the plus tier; ordinary Clang, GCC and Wasm
use standard. See [the build and evidence guide](README.md) and [image receipts](verification.json).

Human progress/retention reviews, glyph readability and physical validation remain outstanding.
