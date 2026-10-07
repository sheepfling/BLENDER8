#pragma once
#include "blender8/sim/components.hpp"
#include "blender8/sim/analog.hpp"
#include <cstdint>
namespace b8::sim {
enum class TemperatureFault { healthy, open, short_ground, short_supply, frozen_output };
// Temperature in through a thermal attachment; voltage out through an analog wire.
class TemperatureSensor : public TemperatureDevice, public SensorProbe {
public:
    TemperatureSensor(const ThermalNode& source,AnalogNet& output);
    void advance_one_us() override;
    void set_fault(TemperatureFault fault);
    [[nodiscard]] TemperatureFault fault() const noexcept { return fault_; }
    void set_calibration(double offset_volts,double fractional_gain_error);
    void set_noise(double amplitude_volts,std::uint32_t seed=1);
    [[nodiscard]] bool debug_sensor_available() const noexcept override { return source_.available; }
    [[nodiscard]] double debug_sensor_c() const noexcept override { return sensed_c_; }
private:
    void drive();
    const ThermalNode& source_; AnalogNet& output_;
    TemperatureFault fault_=TemperatureFault::healthy;
    double sensed_c_,offset_=0,gain_=0,noise_amp_=0,noise_=0,frozen_volts_=0;
    std::uint32_t rng_=1; unsigned divider_=0;
};
}
