// Learner exercise file. Implement one pure subsystem helper at a time.
#include "student.hpp"

namespace astin {

float temperature_celsius(std::uint16_t adc_code) {
    // TODO Lab 1: invert the AVT10/ADC transfer from the temperature manual.
    (void)adc_code;
    return -9999.0F;
}

float motor_duty_fraction(std::uint16_t target_rpm) {
    // TODO Lab 2: derive PWM duty from the MD20 no-load curve for a positive target.
    // This helper never enables the driver and does not decide whether motion is permitted.
    (void)target_rpm;
    return 0.0F;
}

std::uint8_t speed_for_contact(std::uint8_t channel) {
    // TODO Lab 3: map speed contacts C0..C6 to speed numbers 1..7.
    // C7 is PULSE, not a speed; return zero when channel is not a speed contact.
    (void)channel;
    return 0;
}

}  // namespace astin
