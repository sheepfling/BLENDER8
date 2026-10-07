# B8 firmware interface

The New Employee firmware target sees these public `.hpp` headers under
`sdk/include/blender8/`:

| Header          | Purpose                                                 |
| --------------- | ------------------------------------------------------- |
| `firmware.hpp`  | Firmware entry points and interrupt vector table        |
| `device.hpp`    | Ordered register reads, writes, and logical idling      |
| `registers.hpp` | Interface 03 register addresses, masks, interrupts      |
| `access.hpp`    | Paired word values (B16 profile) and IRQ acknowledgment |
| `numeric.hpp`   | Integer operations preserving the selected width        |
| `b16.hpp`       | B16-only SRAM, DMA and pin-selection register addresses |

The firmware directory also provides `board_config.hpp` for chassis 04 wiring and a local
`firmware.hpp` compatibility shim. The simulator, host binding, renderer, and WebAssembly bridge
are build and observation tools. Their headers are not part of the firmware interface. CMake and
the boundary check keep their include directories out of the selected firmware target.

Follow the [New Employee route](../../NEW-EMPLOYEE-START.md) for the first build and the
[architecture guide](../docs/ARCHITECTURE.md) for the ownership boundary.

The [B8/B16 follow-on manual](../docs/B16-FOLLOW-ON.md) defines the enforced numeric profiles.
The selected firmware is checked as B8 by default. Its byte arithmetic uses `numeric.hpp` when
built-in C++ operators would promote the result. B16 additionally permits word integers and float;
both profiles reject doubles and 32-bit integers. B16 uses `b16.hpp` for the expanded register
map. Both native and Wasm implement its DMA, SRAM,
pin selection and sixth interrupt. Binary32 arithmetic is compiler-backed; no instruction ISA or
FPU instruction timing is modeled.
