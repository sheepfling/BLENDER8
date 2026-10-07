#pragma once
#include "blender8/sim/components.hpp"
#include <cstdint>
#include <stdexcept>
namespace b8::sim {
// Simulator-only frequency-domain clock connection; never exposed to firmware.
struct ClockNet { double hz=8'000'000; bool valid=false; };
class CrystalOscillator : public OscillatorDevice {
public:
 explicit CrystalOscillator(ClockNet& out):out_(out){}
 void advance_one_us(bool supply_and_enable) noexcept override {
   if(!supply_and_enable) elapsed_=0;
   else if(elapsed_<startup_us_) ++elapsed_;
   out_.hz=8'000'000*(1+ppm_/1e6);
   out_.valid=supply_and_enable && elapsed_>=startup_us_ && !failed_;
 }
 [[nodiscard]] bool failed() const noexcept { return failed_; }
 void set_failed(bool v) noexcept { failed_=v; if(v) out_.valid=false; }
 void set_ppm(double p) { if(!(p>=-50 && p<=50)) throw std::invalid_argument("XO tolerance"); ppm_=p; }
 void set_startup_us(unsigned n) {if(n<1 || n>5000)throw std::invalid_argument("XO startup");startup_us_=n;}
private:
 ClockNet& out_; double ppm_=0; unsigned elapsed_=0,startup_us_=5000; bool failed_=false;
};
}
