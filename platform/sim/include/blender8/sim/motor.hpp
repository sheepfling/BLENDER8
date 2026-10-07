#pragma once
#include "blender8/sim/components.hpp"
#include "blender8/sim/signals.hpp"
#include "blender8/sim/thermal_model.hpp"
namespace b8::sim {
enum class TachFault { healthy, stuck_low, stuck_high };
class Motor : public MotorDevice, public MotorProbe, public RotationProbe {
public:
    Motor(DigitalNet& pwm,DigitalNet& enable,DigitalNet& tach,ThermalNode& case_temperature);
    void advance_one_us() override;
    void set_load(double normalized_load); // fixture-only, [0,1]
    void set_jammed(bool jammed) noexcept { jammed_=jammed; if(jammed_) rpm_=0; }
    void set_tach_fault(TachFault fault) noexcept { tach_fault_=fault; }
    LumpedThermal& thermal() noexcept { return thermal_; } // fixture-only
    [[nodiscard]] MotorObservation observe_motor() const override { return {rpm_, duty_, thermal_.temperature_c(), thermal_.last_heat_w()}; }
    [[nodiscard]] double debug_rpm() const noexcept { return rpm_; }
    [[nodiscard]] double debug_duty() const noexcept { return duty_; }
    [[nodiscard]] bool debug_energized() const { return enable_.sample() && duty_>0; }
    [[nodiscard]] static double steady_rpm(double duty,double load);
    [[nodiscard]] double shaft_turns() const noexcept override { return revolutions_; }
    [[nodiscard]] double load() const noexcept { return load_; }
    [[nodiscard]] bool jammed() const noexcept { return jammed_; }
private:
    DigitalNet& pwm_;DigitalNet& enable_;DigitalNet& tach_;Driver tach_driver_;
    unsigned window_=0,high_ticks_=0;
    double duty_=0,load_=0,rpm_=0,revolutions_=0,target_=0;
    bool jammed_=false; TachFault tach_fault_=TachFault::healthy;
    LumpedThermal thermal_; unsigned thermal_divider_=0;
};
}
