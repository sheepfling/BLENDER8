#include "student.hpp"
#include "adc_sample.hpp"
#include "fake_hardware.hpp"

#include "blender8/device.hpp"
#include "blender8/host/backend.hpp"
#include "blender8/registers.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect_near(float actual, float expected, float tolerance, std::string_view label) {
    if (!std::isfinite(actual) || std::fabs(actual - expected) > tolerance) {
        std::cerr << label << ": expected " << expected << " +/- " << tolerance
                  << ", got " << actual << '\n';
        std::exit(1);
    }
}

void expect_equal(unsigned actual, unsigned expected, std::string_view label) {
    if (actual != expected) {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

[[noreturn]] void fail_with_trace(std::string_view label, std::string_view detail,
                                  const astin::lab::FakeHardware& hardware) {
    std::cerr << label << ": " << detail << '\n' << "Register trace:\n" << hardware.trace();
    std::exit(1);
}

void expect(bool condition, std::string_view label,
            const astin::lab::FakeHardware& hardware) {
    if (!condition) fail_with_trace(label, "expectation failed", hardware);
}

void expect_equal(std::string_view actual, std::string_view expected, std::string_view label,
                  const astin::lab::FakeHardware& hardware) {
    if (actual != expected) {
        const auto detail = std::string("expected trace:\n") + std::string(expected) +
                            "actual trace:\n" + std::string(actual);
        fail_with_trace(label, detail, hardware);
    }
}

void expect_equal(unsigned actual, unsigned expected, std::string_view label,
                  const astin::lab::FakeHardware& hardware) {
    if (actual != expected) {
        const auto detail = std::string("expected ") + std::to_string(expected) + ", got " +
                            std::to_string(actual);
        fail_with_trace(label, detail, hardware);
    }
}

void require_scripts_consumed(const astin::lab::FakeHardware& hardware) {
    try {
        hardware.require_scripts_consumed();
    } catch (const std::exception& error) {
        fail_with_trace("read script", error.what(), hardware);
    }
}

template <typename Operation>
auto run_with_hardware(astin::lab::FakeHardware& hardware, Operation&& operation) {
    try {
        b8::host::ScopedBinding binding(hardware);
        return operation();
    } catch (const std::exception& error) {
        fail_with_trace("register operation", error.what(), hardware);
    }
}

void temperature() {
    // TEMP-001 nominal 3.3 V ADC examples: codes 155, 233, and 419 are
    // approximately 0 C, 25 C, and 85 C. These are analog estimates only.
    expect_near(astin::temperature_celsius(155), 0.0F, 0.35F, "code 155");
    expect_near(astin::temperature_celsius(233), 25.0F, 0.35F, "code 233");
    expect_near(astin::temperature_celsius(419), 85.0F, 0.35F, "code 419");
}

void motor() {
    // MOT-001 gives the nonlinear no-load inverse. Test values are duty fractions,
    // not output-register bytes or permission to start the motor.
    expect_near(astin::motor_duty_fraction(3000), 0.388868F, 0.00002F, "3000 RPM");
    expect_near(astin::motor_duty_fraction(8000), 0.616330F, 0.00002F, "8000 RPM");
    expect_near(astin::motor_duty_fraction(18000), 0.943918F, 0.00002F, "18000 RPM");
}

void panel() {
    for (std::uint8_t channel = 0; channel < 7; ++channel) {
        expect_equal(astin::speed_for_contact(channel), static_cast<unsigned>(channel + 1),
                     "speed contact");
    }
    expect_equal(astin::speed_for_contact(7), 0, "C7 is PULSE, not a speed");
    expect_equal(astin::speed_for_contact(8), 0, "out-of-range channel");
    expect_equal(astin::speed_for_contact(255), 0, "largest invalid channel");
}

void adc_ready() {
    astin::lab::FakeHardware hardware;
    hardware.script_reads(b8::Reg::ADC_STATUS, {0x02});
    hardware.script_reads(b8::Reg::ADC_DATA_LO, {0xe9});
    hardware.script_reads(b8::Reg::ADC_DATA_HI, {0x00});

    const auto sample = run_with_hardware(hardware, [] { return astin::read_adc_if_ready(); });

    require_scripts_consumed(hardware);
    expect(sample.fresh, "READY produces a fresh sample", hardware);
    expect_equal(sample.code, 233, "latched 10-bit ADC code", hardware);
    expect_equal(sample.status, 0x02, "status snapshot", hardware);
    expect_equal(
        hardware.trace(),
        "#00 @0us READ ADC_STATUS[0x0093] -> 0x02\n"
        "#01 @1us READ ADC_DATA_LO[0x0094] -> 0xE9\n"
        "#02 @2us READ ADC_DATA_HI[0x0095] -> 0x00\n",
        "ADC byte order", hardware);
    std::cout << hardware.trace();
}

void adc_pending() {
    astin::lab::FakeHardware hardware;
    hardware.script_reads(b8::Reg::ADC_STATUS, {0x01});  // BUSY, no READY result yet.

    const auto sample = run_with_hardware(hardware, [] { return astin::read_adc_if_ready(); });

    require_scripts_consumed(hardware);
    expect(!sample.fresh, "BUSY is not a fresh sample", hardware);
    expect_equal(sample.code, 0, "no stale data returned", hardware);
    expect_equal(sample.status, 0x01, "BUSY status is preserved", hardware);
    expect(hardware.events().size() == 1, "pending path reads status only", hardware);
    expect_equal(hardware.trace(), "#00 @0us READ ADC_STATUS[0x0093] -> 0x01\n",
                 "pending ADC trace", hardware);
    std::cout << hardware.trace();
}

void adc_status_flags() {
    astin::lab::FakeHardware hardware;
    hardware.script_reads(b8::Reg::ADC_STATUS, {0x0e});  // READY, OVERRUN, and ERROR.
    hardware.script_reads(b8::Reg::ADC_DATA_LO, {0xe9});
    hardware.script_reads(b8::Reg::ADC_DATA_HI, {0x00});

    const auto sample = run_with_hardware(hardware, [] { return astin::read_adc_if_ready(); });

    require_scripts_consumed(hardware);
    expect(sample.fresh, "READY means a result was consumed", hardware);
    expect_equal(sample.code, 233, "sample code", hardware);
    expect_equal(sample.status, 0x0e, "diagnostic flags remain visible", hardware);
    expect(hardware.trace().find("WRITE") == std::string::npos,
           "reader leaves W1C policy to its caller", hardware);
    std::cout << hardware.trace();
}

void fake_trace() {
    astin::lab::FakeHardware hardware;
    hardware.set_read_value(b8::Reg::ADC_STATUS, 0x01);
    run_with_hardware(hardware, [&] {
        (void)b8::read8(b8::Reg::ADC_STATUS);
        b8::idle();
        b8::write8(b8::Reg::PWM_DUTY, 0x4a);
    });

    expect_equal(hardware.trace(),
                 "#00 @0us READ ADC_STATUS[0x0093] -> 0x01\n"
                 "#01 @1us IDLE\n"
                 "#02 @2us WRITE PWM_DUTY[0x0061] <- 0x4A\n",
                 "fake backend trace", hardware);
    std::cout << hardware.trace();
}

void fake_overread() {
    astin::lab::FakeHardware hardware;
    hardware.script_reads(b8::Reg::ADC_STATUS, {0x00});
    bool exhausted = false;
    std::string error_message;
    {
        b8::host::ScopedBinding binding(hardware);
        (void)b8::read8(b8::Reg::ADC_STATUS);
        try {
            (void)b8::read8(b8::Reg::ADC_STATUS);
        } catch (const std::logic_error& error) {
            exhausted = true;
            error_message = error.what();
        }
    }

    expect(exhausted, "an unscripted extra read fails", hardware);
    expect(hardware.events().size() == 1, "failed read is not logged as a bus transfer",
           hardware);
    std::cout << "Expected fake failure: " << error_message << '\n';
    std::cout << hardware.trace();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "choose one test: temperature, motor, panel, adc-ready, adc-pending, "
                     "adc-status-flags, fake-trace, or fake-overread\n";
        return 2;
    }
    const std::string_view selected(argv[1]);
    if (selected == "temperature") temperature();
    else if (selected == "motor") motor();
    else if (selected == "panel") panel();
    else if (selected == "adc-ready") adc_ready();
    else if (selected == "adc-pending") adc_pending();
    else if (selected == "adc-status-flags") adc_status_flags();
    else if (selected == "fake-trace") fake_trace();
    else if (selected == "fake-overread") fake_overread();
    else {
        std::cerr << "unknown test: " << selected << '\n';
        return 2;
    }
    std::cout << selected << ": pass\n";
}
