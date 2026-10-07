#pragma once
#include "blender8/sim/analog.hpp"
#include <array>
#include <cstdint>
namespace b8::sim {
class Adc {
public:
    explicit Adc(std::array<const AnalogNet*,2> inputs) : inputs_(inputs) {}
    void reset() noexcept;
    void reject() noexcept {error_=true;}
    void set_stalled(bool v) noexcept {stalled_=v;} // fixture: suppress conversion-clock progress
    [[nodiscard]] bool stalled() const noexcept{return stalled_;}
    [[nodiscard]] bool busy() const noexcept{return remaining_!=0;}
    [[nodiscard]] bool advance_clock_tick(); // true only on a conversion-complete edge
    [[nodiscard]] std::uint8_t read(unsigned offset);
    void write(unsigned offset,std::uint8_t value);
    void set_reference_volts(double volts); // fixture-only reference tolerance
[[nodiscard]] unsigned debug_status() const noexcept{return (remaining_?1u:0u)|(ready_?2u:0u)|(overrun_?4u:0u)|(error_?8u:0u);}
    [[nodiscard]] std::uint16_t debug_result() const noexcept{return result_;}
    [[nodiscard]] unsigned debug_result_channel() const noexcept{return result_channel_;}
    [[nodiscard]] bool debug_enabled() const noexcept{return enabled_;}
private:
    std::array<const AnalogNet*,2> inputs_;
    double reference_=3.3,held_=0,held_reference_=3.3;
    std::uint8_t channel_=0,scale_=2,high_=0;
    std::uint16_t result_=0;
    unsigned result_channel_=0;
    unsigned remaining_=0,divisor_=4;
    bool stalled_=false;
    bool enabled_=false,ready_=false,overrun_=false,error_=false;
};
}
