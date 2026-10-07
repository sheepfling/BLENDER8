#pragma once
#include "blender8/sim/button_assembly.hpp"
#include "blender8/sim/lcd32.hpp"
#include "blender8/sim/mcu.hpp"
#include "blender8/sim/motor.hpp"
#include "blender8/sim/mux8.hpp"
#include "blender8/sim/temperature_sensor.hpp"
#include "blender8/sim/power_domain.hpp"
#include "blender8/sim/clock_source.hpp"
#include "blender8/sim/jar_interlock.hpp"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
namespace b8::sim {
enum class BoardProfile { clocked03, legacy02, chassis04 };
// Nets outlive devices. Components are injected ONLY at construction; no dangling drivers,
// invisible reference device running alongside hardware, or mid-run topology replacement.
class Board {
public:
    explicit Board(BoardProfile profile=BoardProfile::chassis04,ComponentFactories factories={},DeviceProfile device=DeviceProfile::b8);
    Board(const Board&)=delete;Board& operator=(const Board&)=delete;
    void advance(Tick delta);
    void power(bool on);
    void settle();
    void emergency_inhibit() noexcept;
    // Scheduled fixture actions execute before peripherals at their exact logical timestamp.
    // Ties execute in insertion order. Actions scheduled at now execute immediately.
    // An action may not recursively advance the board or schedule other actions.
    void schedule(Tick at,std::function<void(Board&)> action);
    [[nodiscard]] std::size_t pending_events() const noexcept{return events_.size();}
    [[nodiscard]] bool ready() const noexcept{return powered_ && mcu_.ready() && !failed_;}
    [[nodiscard]] Tick now() const noexcept{return now_;}
    [[nodiscard]] bool powered() const noexcept{return powered_;}
    [[nodiscard]] bool failed() const noexcept{return failed_;}
    void set_foreground_enabled(bool enabled) noexcept{foreground_enabled_=enabled;}
    [[nodiscard]] bool foreground_enabled() const noexcept{return foreground_enabled_;}
    void set_observer(std::function<void(Board&)> observer){observer_=std::move(observer);}
    [[nodiscard]] BoardProfile profile() const noexcept{return profile_;}
    [[nodiscard]] bool drive_enabled() const{return gated_enable_.sample();}
    [[nodiscard]] Logic probe_gpio(unsigned bank,unsigned bit) const { return bank ? b_.at(bit).resolve() : a_.at(bit).resolve(); }
    [[nodiscard]] bool pwm_level() const{return pwm_.sample();}
    [[nodiscard]] bool run_permit() const{return run_permit_.sample();}
    [[nodiscard]] bool stop_asserted() const{return !b_[2].sample();}
    [[nodiscard]] std::uint8_t contact_mask() const;
    [[nodiscard]] double analog_voltage() const noexcept{return temperature_voltage_.sample_volts();}
    [[nodiscard]] const ClockNet& external_clock() const noexcept{return external_clock_;}
    // Interface-level access for host composition / observation.
    ButtonDevice& button_device() noexcept{return *buttons_;}
    MuxDevice& mux_device() noexcept{return *mux_;}
    DisplayDevice& display_device() noexcept{return *lcd_;}
    MotorDevice& motor_device() noexcept{return *motor_;}
    TemperatureDevice& sensor_device() noexcept{return *temperature_sensor_;}
    OscillatorDevice& clock_device() noexcept{return *oscillator_;}
    PowerDevice& power_device() noexcept{return *power_domain_;}
    Mcu& mcu() noexcept{return mcu_;}
    JarInterlock& jar() noexcept{return jar_;}
    // Reference-only fixture conveniences. Explicitly fail when replaced by a real adapter.
    ButtonAssembly& buttons(){return reference<ButtonAssembly>(*buttons_);}
    Lcd32& lcd(){return reference<Lcd32>(*lcd_);}
    Motor& motor(){return reference<Motor>(*motor_);}
    TemperatureSensor& temperature_sensor(){return reference<TemperatureSensor>(*temperature_sensor_);}
    CrystalOscillator& oscillator(){return reference<CrystalOscillator>(*oscillator_);}
    PowerDomain& power_domain(){return reference<PowerDomain>(*power_domain_);}
private:
    template<class T,class Base> static T& reference(Base& device){
        if(auto* p=dynamic_cast<T*>(&device))return *p;
        throw std::logic_error("reference fixture capability unavailable on replacement component");
    }
    void apply_events();
    std::array<DigitalNet,8> a_,b_,contacts_;
    DigitalNet run_permit_{Logic::low},pwm_{Logic::low},gated_enable_{Logic::low};
    Driver gate_driver_,vblank_driver_;
    ThermalNode case_temperature_;
    AnalogNet temperature_voltage_{3.3},spare_analog_{0};
    ClockNet external_clock_;
    std::unique_ptr<ButtonDevice> buttons_;
    std::unique_ptr<MuxDevice> mux_;
    std::unique_ptr<DisplayDevice> lcd_;
    std::unique_ptr<OscillatorDevice> oscillator_;
    std::unique_ptr<PowerDevice> power_domain_;
    Mcu mcu_;
    std::unique_ptr<MotorDevice> motor_;
    std::unique_ptr<TemperatureDevice> temperature_sensor_;
    JarInterlock jar_;
    std::multimap<Tick,std::function<void(Board&)>> events_;
    Tick now_=0;bool powered_=false;BoardProfile profile_;bool ever_powered_=false;
    bool failed_=false,advancing_=false,applying_=false,foreground_enabled_=true;
    std::function<void(Board&)> observer_;
};
}
