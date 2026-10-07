#pragma once
#include "blender8/device.hpp"
#include <cstdint>
namespace b8 {
// These helpers do not mask interrupts, select a clock, read host time, or interpret
// board wiring. The caller must own each pair across foreground/ISR contexts.
struct ReadPair {Reg low; Reg high;};
struct WritePair {Reg low; Reg high;};
[[nodiscard]] inline std::uint16_t read_latched(ReadPair pair){
    const auto low=read8(pair.low);
    const auto high=read8(pair.high);
    return static_cast<std::uint16_t>((static_cast<unsigned>(high)<<8)|low);
}
inline void write_staged(WritePair pair,std::uint16_t value){
    write8(pair.low,static_cast<std::uint8_t>(value));
    write8(pair.high,static_cast<std::uint8_t>(value>>8));
}
inline void acknowledge(Irq irq){write8(Reg::IRQ_FLAGS,mask(irq));}
inline constexpr ReadPair timer0_count{Reg::T0_COUNT_LO,Reg::T0_COUNT_HI};
inline constexpr ReadPair timer1_count{Reg::T1_COUNT_LO,Reg::T1_COUNT_HI};
inline constexpr ReadPair tach_count{Reg::TACH_COUNT_LO,Reg::TACH_COUNT_HI};
inline constexpr ReadPair adc_result{Reg::ADC_DATA_LO,Reg::ADC_DATA_HI};
inline constexpr ReadPair deadman_count{Reg::DMT_COUNT_LO,Reg::DMT_COUNT_HI};
inline constexpr WritePair timer0_compare{Reg::T0_COMPARE_LO,Reg::T0_COMPARE_HI};
inline constexpr WritePair timer1_compare{Reg::T1_COMPARE_LO,Reg::T1_COMPARE_HI};
inline constexpr WritePair deadman_limit{Reg::DMT_LIMIT_LO,Reg::DMT_LIMIT_HI};
inline constexpr WritePair deadman_window{Reg::DMT_WINDOW_LO,Reg::DMT_WINDOW_HI};
// No generic read-modify-write helper: W1C/status/strobe registers are NOT ordinary RAM.
}
