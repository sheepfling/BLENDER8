#pragma once
#include "blender8/registers.hpp"
#include <array>
#include <cstdint>
namespace b8 {
using Isr = void (*)();
#if defined(B8_TARGET_B16)
using VectorTable = std::array<Isr, 6>;
#else
using VectorTable = std::array<Isr, 5>;
#endif
// One ordered 8-bit register transfer, followed by a service boundary.
// The supplied native binding advances 1 microsecond per service operation.
// Firmware never receives a backend component object.
[[nodiscard]] std::uint8_t read8(Reg address);
void write8(Reg address, std::uint8_t value);
// Advance one service interval, independent of SYSCLK/PBCLK.
// Not wall-clock sleep, a hardware low-power mode, or a clock oracle.
void idle();
} // namespace b8
