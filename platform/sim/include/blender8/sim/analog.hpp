#pragma once
#include <cmath>
#include <stdexcept>
namespace b8::sim {
// Single-driver voltage net; disconnect resolves to the board's diagnostic pull-up.
// This models connectivity, not input impedance, clamp current, or ESD protection.
class AnalogNet {
public:
    explicit AnalogNet(double bias_volts=3.3) : voltage_(bias_volts), bias_(bias_volts) {}
    void drive(double volts) {
        if (!std::isfinite(volts)) throw std::invalid_argument("non-finite analog voltage");
        voltage_=volts; driven_=true;
    }
    void disconnect() noexcept { driven_=false; }
    [[nodiscard]] double sample_volts() const noexcept { return driven_?voltage_:bias_; }
private:
    double voltage_,bias_; bool driven_=false;
};
struct ThermalNode { double celsius=25.0; bool available=true; }; // one plant writer, any number of sensor readers
}
