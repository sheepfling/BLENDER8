#pragma once
#include "blender8/sim/analog.hpp"
#include "blender8/sim/signals.hpp"
#include <array>
#include <functional>
#include <memory>

namespace b8::sim {
struct ClockNet;
// Host-side component contracts. NONE of this directory is on the firmware target's
// include path. Factory injection occurs once at board construction, not while running.
// Port references are non-owning and live for the complete component lifetime.
struct ButtonPorts {
    std::array<DigitalNet*,8> contacts;
    DigitalNet& run_permit;
    DigitalNet& stop_n;
};
struct MuxPorts {
    std::array<DigitalNet*,8> inputs;
    std::array<DigitalNet*,3> select;
    DigitalNet& output;
};
struct MotorPorts {
    DigitalNet& pwm;
    DigitalNet& enable;
    DigitalNet& tach;
    ThermalNode& case_temperature;
};
struct TemperaturePorts {
    const ThermalNode& attachment;
    AnalogNet& voltage;
};
class ButtonDevice {
public:
    virtual ~ButtonDevice() = default;
    virtual void advance(Tick now) = 0;
};
class MuxDevice {
public:
    virtual ~MuxDevice() = default;
    virtual void advance(Tick now) = 0;
};
class DisplayDevice : public BytePeripheral {
public:
    virtual void reset(Tick now) = 0;
    virtual void advance(Tick now) = 0;
    [[nodiscard]] virtual bool vblank() const noexcept = 0;
};
class MotorDevice {
public:
    virtual ~MotorDevice() = default;
    virtual void advance_one_us() = 0;
    // Hardware adapters override this for immediate out-of-band loss-of-host inhibition.
    virtual void inhibit_host_failure() noexcept {}
};
class TemperatureDevice {
public:
    virtual ~TemperatureDevice() = default;
    virtual void advance_one_us() = 0;
};
class OscillatorDevice {
public:
    virtual ~OscillatorDevice() = default;
    virtual void advance_one_us(bool powered) = 0;
};
class PowerDevice {
public:
    virtual ~PowerDevice() = default;
    virtual void advance_one_us(Tick now) = 0;
    [[nodiscard]] virtual bool good() const noexcept = 0;
    [[nodiscard]] virtual bool motor_supply() const noexcept = 0;
    [[nodiscard]] virtual double volts() const noexcept = 0;
};
// Observation is optional; lack of hardware instrumentation must NOT manufacture plant
// truth. UI/trace users detect these capabilities with dynamic_cast and emit null if absent.
class DisplayProbe {
public:
    virtual ~DisplayProbe() = default;
    [[nodiscard]] virtual bool pixel(unsigned x, unsigned y) const = 0;
};
struct ButtonObservation {
    std::uint8_t latched_mask; bool pulse_held; bool stop_held; bool pulse_blocked;
};
class ButtonProbe {
public:
    virtual ~ButtonProbe() = default;
    [[nodiscard]] virtual ButtonObservation observe_buttons() const noexcept = 0;
};
// Optional exact shaft phase; a transport with only RPM must not invent this observation.
class RotationProbe {
public:
    virtual ~RotationProbe() = default;
    [[nodiscard]] virtual double shaft_turns() const noexcept = 0;
};
struct MotorObservation { double rpm; double effective_duty; double case_c; double loss_w; };
class MotorProbe {
public:
    virtual ~MotorProbe() = default;
    [[nodiscard]] virtual MotorObservation observe_motor() const = 0;
};
class SensorProbe {
public:
    virtual ~SensorProbe() = default;
    [[nodiscard]] virtual double debug_sensor_c() const noexcept = 0;
    [[nodiscard]] virtual bool debug_sensor_available() const noexcept { return true; }
};
// Empty entries use reference implementations. Returning nullptr is an error.
// Swapping a model never grants firmware access to that model or its fixture methods.
struct ComponentFactories {
    std::function<std::unique_ptr<ButtonDevice>(ButtonPorts)> buttons;
    std::function<std::unique_ptr<MuxDevice>(MuxPorts)> mux;
    std::function<std::unique_ptr<DisplayDevice>()> display;
    std::function<std::unique_ptr<MotorDevice>(MotorPorts)> motor;
    std::function<std::unique_ptr<TemperatureDevice>(TemperaturePorts)> sensor;
    std::function<std::unique_ptr<OscillatorDevice>(ClockNet&)> oscillator;
    std::function<std::unique_ptr<PowerDevice>()> power;
};
} // namespace b8::sim
