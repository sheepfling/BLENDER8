# PX32-16 monochrome LCD module

**Document:** LCD-001 · **Revision:** 0.2.0 · **Date:** 2026-10-06 · **Status:** teaching-design draft


## Raster and memory

The panel is 32×16 monochrome pixels, backed by 64 bytes. Each byte is a **vertical eight-pixel column**:

```text
address = (y / 8) * 32 + x
bit     = y % 8
pixel   = (VRAM[address] >> bit) & 1
```

Origin is top-left; x increases right; y increases downward; bit0 is the upper pixel of each page. Writing 0x81 to address 0 lights (0,0) and (0,7), not two horizontal pixels. Firmware authors their own small font and pixel operations. A RAM shadow framebuffer is permitted; a display text API or simulator framebuffer accessor is not.

## Four byte-bus registers

| Index | Name | Behavior |
|---|---|---|
| 0 | ADDRESS | RW low6 bits, selects byte 0..63 |
| 1 | DATA | Read/write VRAM at ADDRESS; optional auto-increment |
| 2 | STATUS | bit0 BUSY, bit1 VBLANK, bit2 ERROR (W1C) |
| 3 | CONTROL | bit0 display enable, bit1 auto-increment |

Reset: VRAM and scanned pixels zero, address 0, control 2 (blanked, auto-increment enabled), no busy/error. An accepted DATA write is busy for 8 us. A DATA write during BUSY is dropped, sets ERROR, and does not increment ADDRESS. Address/control/status operations and DATA reads remain available during busy. DATA reads also auto-increment when enabled. Auto-increment wraps from 63 to0. STATUS.ERROR clears only by writing bit2=1.

## Scanout and timing

The frame is 20 ms (50 Hz). Active rows are sampled at phases 0,1000,...15000 us. VBLANK is phase 16000..19999 us. Rows are copied from VRAM at their individual scan times, so writing mid-frame can tear. The GUI/terminal preview reads **scanned pixels**, not the raw framebuffer. At initial reset the first active instant is already blank; row0 is next refreshed at the following frame boundary.

Firmware may keep a private 64-byte shadow framebuffer and copy it during the 4 ms blanking interval while obeying 8 us DATA-write spacing. There is no hardware text mode, font ROM, sprite engine, double-buffer swap, DMA or automatic character rendering in the core assignment.

## Ownership and interrupts

MCU XBUS_REG and LCD ADDRESS are shared state. A foreground writer interrupted by another LCD writer can corrupt addressing even though individual transactions are atomic. Give the LCD one owner or protect multi-transaction sequences. VBLANK enters the MCU interrupt controller as a rising-edge source; acknowledge MCU flags separately from LCD ERROR.

## Firmware display vocabulary

The requirements call for `0`, `1`..`7`, `P`, `1P`..`7P`, `ST`, `TH`, `TS` and `IF`. These are application conventions, not strings understood by this generic LCD component. TH denotes full thermal shutdown in this revision; gradual thermal throttling is optional future behavior and needs its own indicator/specification.
