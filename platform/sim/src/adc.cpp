#include "blender8/sim/adc.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace b8::sim {
void Adc::reset() noexcept {
    result_channel_=0;channel_=0;scale_=2;high_=0;result_=0;remaining_=0;divisor_=4;
    enabled_=ready_=overrun_=error_=false;held_=0;held_reference_=reference_;
}
void Adc::set_reference_volts(double v) {
    if(!std::isfinite(v)||v<3.0||v>3.6) throw std::out_of_range("ADC reference [3.0,3.6] V");
    reference_=v;
}
bool Adc::advance_clock_tick() {
    if(stalled_) return false;
    if(remaining_==0) return false;
    --remaining_;
    if(remaining_==9*divisor_) { held_=inputs_[channel_]->sample_volts();held_reference_=reference_; }
    if(remaining_!=0) return false;
    if(ready_) overrun_=true;
    // Explicit Blender-8 endpoint-rounded teaching transfer, not a universal ADC formula.
    const double fraction=std::clamp(held_/held_reference_,0.0,1.0);
    result_=static_cast<std::uint16_t>(std::floor(1023.0*fraction+0.5));
    result_channel_=channel_;ready_=true;return true;
}
std::uint8_t Adc::read(unsigned offset) {
    switch(offset) {
    case 0:return enabled_?1:0;
    case 1:return channel_;
    case 2:return scale_;
    case 3:return static_cast<std::uint8_t>((remaining_?1:0)|(ready_?2:0)|(overrun_?4:0)|(error_?8:0));
    case 4:high_=static_cast<std::uint8_t>(result_>>8);ready_=false;return static_cast<std::uint8_t>(result_);
    case 5:return high_;
    default:throw std::out_of_range("ADC register offset");
    }
}
void Adc::write(unsigned offset,std::uint8_t value) {
    switch(offset) {
    case 0:
        enabled_=(value&1u)!=0;
        if(!enabled_) { remaining_=0;ready_=false; }
        if((value&2u)!=0) {
            if(!enabled_||remaining_) {error_=true;return;}
            divisor_=1u<<scale_;remaining_=13*divisor_;
        }
        return;
    case 1:if(remaining_) error_=true;else channel_=value&1u;return;
    case 2:if(remaining_) error_=true;else scale_=value&3u;return;
    case 3:if(value&4u) overrun_=false;if(value&8u) error_=false;return;
    case 4:case 5:throw std::logic_error("read-only ADC register");
    default:throw std::out_of_range("ADC register offset");
    }
}
}
