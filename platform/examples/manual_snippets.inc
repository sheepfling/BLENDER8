// Generated from MCU-001 examples by tools/extract_manual_examples.py.
// The final API declaration block is already compiled via the real SDK.
#include "blender8/device.hpp"
#include <optional>

namespace nmd_examples {
void acknowledge_timer0() {
// Acknowledge only TIMER0; do not echo the whole FLAGS register.
b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::timer0));
}

void configure_pad5() {
// Sole owner of port A; example uses only pad 5.
constexpr std::uint8_t pad = 0x20;
auto out = b8::read8(b8::Reg::GPIOA_OUT);
b8::write8(b8::Reg::GPIOA_OUT,
           static_cast<std::uint8_t>(out & ~pad));
auto dir = b8::read8(b8::Reg::GPIOA_DIR);
b8::write8(b8::Reg::GPIOA_DIR,
           static_cast<std::uint8_t>(dir | pad));
}

void timer0_handler() {
    b8::write8(b8::Reg::IRQ_FLAGS,
               b8::mask(b8::Irq::timer0));
    // Record bounded work for foreground ownership.
}

void configure_timer1(std::uint8_t prescale_code, std::uint16_t compare) {
// Generic T1 example: a 16-bit compare supplied by the caller.
b8::write8(b8::Reg::T1_CTRL, 0);
b8::write8(b8::Reg::T1_PRESCALE, prescale_code);
b8::write8(b8::Reg::T1_COMPARE_LO,
           static_cast<std::uint8_t>(compare));
b8::write8(b8::Reg::T1_COMPARE_HI,
           static_cast<std::uint8_t>(compare >> 8));
b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::timer1));
b8::write8(b8::Reg::T1_CTRL, 3);
}

std::uint16_t read_pair(b8::Reg low, b8::Reg high) {
    const auto saved = b8::read8(b8::Reg::IRQ_GLOBAL);
    b8::write8(b8::Reg::IRQ_GLOBAL, 0);
    const auto lo = b8::read8(low);
    const auto hi = b8::read8(high);
    b8::write8(b8::Reg::IRQ_GLOBAL, saved);
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(lo) |
        (static_cast<unsigned>(hi) << 8));
}

std::uint8_t read_external(std::uint8_t local_index) {
// Local index supplied by the external device's own manual.
b8::write8(b8::Reg::XBUS_REG, local_index);
const auto value = b8::read8(b8::Reg::XBUS_DATA);
return value;
}

std::optional<std::uint16_t> collect_adc() {
// Caller owns ADC access and excludes an ISR reading ADC DATA.
if ((b8::read8(b8::Reg::ADC_STATUS) & 0x02u) != 0) {
    const auto lo = b8::read8(b8::Reg::ADC_DATA_LO);
    const auto hi = b8::read8(b8::Reg::ADC_DATA_HI);
    const auto code = static_cast<std::uint16_t>(
        static_cast<unsigned>(lo) |
        ((static_cast<unsigned>(hi) & 0x03u) << 8));
    return code; // Test wrapper consumes the printed local result.
}
return std::nullopt;
}

void commit_clock() {
const auto saved = b8::read8(b8::Reg::IRQ_GLOBAL);
b8::write8(b8::Reg::IRQ_GLOBAL, 0);
// Staging and preconditions have already been established.
b8::write8(b8::Reg::CLK_KEY,    0xC3);
b8::write8(b8::Reg::CLK_KEY,    0x3C);
b8::write8(b8::Reg::CLK_COMMIT, 0xA5);
b8::write8(b8::Reg::IRQ_GLOBAL, saved);
}
} // namespace nmd_examples
