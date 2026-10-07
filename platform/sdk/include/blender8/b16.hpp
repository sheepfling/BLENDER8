#pragma once
#include "blender8/registers.hpp"
// Interface 04 extensions. The original B8 register enum remains interface 03.
namespace b16 {
inline constexpr b8::Reg SYS_CAPS=static_cast<b8::Reg>(0x05);
inline constexpr b8::Reg ANSELA=static_cast<b8::Reg>(0x23);
inline constexpr b8::Reg DMA_CMD=static_cast<b8::Reg>(0xD0), DMA_CONFIG=static_cast<b8::Reg>(0xD1);
inline constexpr b8::Reg DMA_SRC_LO=static_cast<b8::Reg>(0xD2), DMA_SRC_HI=static_cast<b8::Reg>(0xD3);
inline constexpr b8::Reg DMA_DST_LO=static_cast<b8::Reg>(0xD4), DMA_DST_HI=static_cast<b8::Reg>(0xD5);
inline constexpr b8::Reg DMA_LEN_LO=static_cast<b8::Reg>(0xD6), DMA_LEN_HI=static_cast<b8::Reg>(0xD7);
inline constexpr b8::Reg DMA_TRIGGER=static_cast<b8::Reg>(0xD8), DMA_PACE=static_cast<b8::Reg>(0xD9);
inline constexpr b8::Reg DMA_STATUS=static_cast<b8::Reg>(0xDA), DMA_IEN=static_cast<b8::Reg>(0xDB);
inline constexpr b8::Reg DMA_ERROR=static_cast<b8::Reg>(0xDC);
inline constexpr b8::Reg DMA_DONE_LO=static_cast<b8::Reg>(0xDD), DMA_DONE_HI=static_cast<b8::Reg>(0xDE);
inline constexpr b8::Reg DMA_LEFT_LO=static_cast<b8::Reg>(0xE0), DMA_LEFT_HI=static_cast<b8::Reg>(0xE1);
inline constexpr b8::Reg PIN_KEY=static_cast<b8::Reg>(0xE8), PIN_LOCK=static_cast<b8::Reg>(0xE9);
inline constexpr b8::Reg PIN_STATUS=static_cast<b8::Reg>(0xEA), PWM_ROUTE=static_cast<b8::Reg>(0xEB);
inline constexpr b8::Reg TACH_ROUTE=static_cast<b8::Reg>(0xEC), VBLANK_ROUTE=static_cast<b8::Reg>(0xED);
inline constexpr b8::Reg DREQ_ROUTE=static_cast<b8::Reg>(0xEE);
// CPU bus access to SRAM is still a byte transaction.
[[nodiscard]] constexpr b8::Reg sram(std::uint16_t address) { return static_cast<b8::Reg>(address); }
inline constexpr std::uint8_t dmac_irq_mask=0x20;
}
