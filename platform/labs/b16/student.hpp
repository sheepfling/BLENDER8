#pragma once

#include <cstdint>

namespace astin {

// Pure helpers for the staged learner harness; these are not firmware interfaces.
float temperature_celsius(std::uint16_t adc_code);
float motor_duty_fraction(std::uint16_t target_rpm);
std::uint8_t speed_for_contact(std::uint8_t channel);

}  // namespace astin
