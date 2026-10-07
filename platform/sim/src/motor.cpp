#include "blender8/sim/motor.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace b8::sim {
Motor::Motor(DigitalNet& pwm,DigitalNet& enable,DigitalNet& tach,ThermalNode& node)
    :pwm_(pwm),enable_(enable),tach_(tach),tach_driver_(tach.attach(Logic::low)),thermal_(node) {}
double Motor::steady_rpm(double duty,double load) {
    if (!std::isfinite(duty)||!std::isfinite(load)) throw std::invalid_argument("non-finite motor parameter");
    const auto u=std::clamp((duty-0.12)/0.88,0.0,1.0);
    return 20000*std::pow(u,1.6)*(1.0-0.5*std::clamp(load,0.0,1.0));
}
void Motor::set_load(double load) {
    if (!std::isfinite(load)||load<0||load>1) throw std::out_of_range("load must be [0,1]");
    load_=load; target_=steady_rpm(duty_,load_);
}
void Motor::advance_one_us() {
    const bool enabled=enable_.sample();
    high_ticks_+=static_cast<unsigned>(pwm_.sample());
    if (++window_==256) {
        duty_=static_cast<double>(high_ticks_)/256.0;
        window_=0;high_ticks_=0;target_=steady_rpm(duty_,load_);
    }
    // Physics is a deliberately simple averaged-drive teaching model, not SPICE.
    const double target=enabled?target_:0.0;
    static const double alpha_run=-std::expm1(-1.0/120000.0);
    static const double alpha_coast=-std::expm1(-1.0/350000.0);
    const auto previous=rpm_;
    if(jammed_) rpm_=0;
    else rpm_+=(target-rpm_)*((enabled && duty_>0)?alpha_run:alpha_coast);
    revolutions_+=(previous+rpm_)*0.5/60'000'000.0;
    // Bound phase rather than accumulate a lifetime revolution counter.
    revolutions_=std::fmod(revolutions_,1.0);
    bool high=static_cast<unsigned>(revolutions_*4.0)%2u!=0;
    if(tach_fault_==TachFault::stuck_low) high=false;
    if(tach_fault_==TachFault::stuck_high) high=true;
    tach_.drive(tach_driver_,high?Logic::high:Logic::low);
    if(++thermal_divider_==1000) {
        thermal_divider_=0;
        thermal_.advance(0.001,duty_,rpm_,steady_rpm(duty_,0),enabled);
    }
}
}
