#pragma once
#include "blender8/sim/components.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include "blender8/sim/signals.hpp"
namespace b8::sim {
enum class Fuse {input,logic,motor};
class PowerDomain : public PowerDevice {
public:
 void request(bool on,Tick now) noexcept {requested_=on;epoch_=now;good_=false;qualified_=0;}
 void set_fuse(Fuse f,bool intact) noexcept {if(f==Fuse::input)input_=intact;else if(f==Fuse::logic)logic_=intact;else motor_=intact;}
 void set_voltage_override(std::optional<double> v){if(v && (!std::isfinite(*v)||*v<0||*v>3.6))throw std::invalid_argument("logic rail");override_=v;}
 void advance_one_us(Tick now) noexcept override {
  const auto elapsed=now-epoch_;
  // Reproducible representative make/break sequence; five-ms guaranteed envelope.
  const bool bounced=elapsed<5000 && ((elapsed>=200 && elapsed<650)||(elapsed>=1300 && elapsed<2100)||(elapsed>=3300 && elapsed<3800));
  raw_=requested_?!bounced:bounced;
  const bool source=raw_&&input_&&logic_;
  voltage_=std::clamp(voltage_+(source?3.3/20000.0:-3.3/5000.0),0.0,3.3);
  const double v=override_.value_or(voltage_);
  if(!requested_||!raw_||!input_||!logic_||v<2.9){good_=false;qualified_=0;}
  else if(!good_){if(v>=3.0){if(++qualified_>=10000)good_=true;}else qualified_=0;}
 }
 bool good() const noexcept override {return good_;}
 bool motor_supply() const noexcept override {return requested_&&raw_&&input_&&motor_;}
 [[nodiscard]] std::optional<double> override_voltage() const noexcept { return override_; }
 bool requested() const noexcept{return requested_;}
 bool raw_switch() const noexcept{return raw_;}
 double volts() const noexcept override {return override_.value_or(voltage_);}
private:
 bool requested_=true,raw_=true,input_=true,logic_=true,motor_=true,good_=false;
 double voltage_=0;unsigned qualified_=0;Tick epoch_=0;std::optional<double> override_;
};
}
