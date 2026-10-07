#include "blender8/sim/temperature_sensor.hpp"
#include <cmath>
#include <stdexcept>
namespace b8::sim {
TemperatureSensor::TemperatureSensor(const ThermalNode& source,AnalogNet& out)
    :source_(source),output_(out),sensed_c_(source.celsius) { drive(); }
void TemperatureSensor::set_calibration(double offset,double gain) {
    if(!std::isfinite(offset)||!std::isfinite(gain)||std::abs(offset)>0.05||std::abs(gain)>0.1)
        throw std::out_of_range("sensor calibration outside fixture range");
    offset_=offset;gain_=gain;drive();
}
void TemperatureSensor::set_noise(double amplitude,std::uint32_t seed) {
    if(!std::isfinite(amplitude)||amplitude<0||amplitude>0.02)
        throw std::out_of_range("noise amplitude [0,.02] V");
    noise_amp_=amplitude;rng_=seed?seed:1;noise_=0;drive();
}
void TemperatureSensor::set_fault(TemperatureFault fault) {
    if(fault==TemperatureFault::frozen_output) frozen_volts_=output_.sample_volts();
    fault_=fault;drive();
}
void TemperatureSensor::drive() {
    if(!source_.available){output_.disconnect();return;}
    switch(fault_) {
    case TemperatureFault::open:output_.disconnect();return;
    case TemperatureFault::short_ground:output_.drive(0);return;
    case TemperatureFault::short_supply:output_.drive(3.3);return;
    case TemperatureFault::frozen_output:output_.drive(frozen_volts_);return;
    case TemperatureFault::healthy:output_.drive(0.5+offset_+0.01*(1+gain_)*sensed_c_+noise_);return;
    }
}
void TemperatureSensor::advance_one_us() {
    if(!source_.available)throw std::logic_error("reference temperature sensor requires an available thermal attachment; replace sensor or supply link case temperature");
    static const double alpha=-std::expm1(-1.0/250000.0); // 250 ms package/mount lag
    sensed_c_+=(source_.celsius-sensed_c_)*alpha;
    if(++divider_==1000) {
        divider_=0;
        rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;
        noise_=noise_amp_*(2.0*static_cast<double>(rng_)/4294967295.0-1.0);
    }
    drive();
}
}
