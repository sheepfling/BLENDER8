#pragma once
#include <cstdint>
// Chassis-04 wiring facts, not device-model accessors. See Half-A/Labs r4 drawings.
namespace board_config {
inline constexpr std::uint8_t mux_select_mask = 0x07; // PA0..PA2
inline constexpr std::uint8_t mux_input_mask = 0x08;  // PA3; low means contact closed
inline constexpr std::uint8_t motor_enable_mask = 0x01; // PB0
inline constexpr std::uint8_t tach_input_mask = 0x02; // PB1
inline constexpr std::uint8_t stop_input_mask = 0x04; // PB2; active-low STOP
inline constexpr std::uint8_t jar_ok_mask = 0x08; // PB3; high = raw closed loop
inline constexpr std::uint8_t jar_permit_mask = 0x10; // PB4; high = hardware latch armed
inline constexpr std::uint8_t motor_temperature_channel = 0; // AN0
inline constexpr std::uint8_t pulse_channel = 7;
}
