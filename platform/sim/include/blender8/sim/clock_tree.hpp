#pragma once
#include "blender8/sim/clock_source.hpp"
#include <array>
#include <cstdint>
namespace b8::sim {
struct ClockPlan { std::uint8_t source=0,pre=2,mul=8,post=2,pb=4; };
class ClockTree {
public:
 explicit ClockTree(const ClockNet* external=nullptr):external_(external){}
 void reset() noexcept;
 // Returns true after eight independent LFRC ticks with an absent active source.
 bool advance_one_us(unsigned lf_edges);
 std::uint8_t read(unsigned offset);
 void write(unsigned offset,std::uint8_t value,bool quiescent);
 void other_access() noexcept { key_=0; }
 void set_frc_ppm(double ppm);
 [[nodiscard]] double system_hz() const noexcept;
 [[nodiscard]] double peripheral_hz() const noexcept {return system_hz()/active_.pb;}
 [[nodiscard]] bool stopped() const noexcept;
[[nodiscard]] ClockPlan active_plan() const noexcept{return active_;}
 [[nodiscard]] std::uint8_t debug_status() const noexcept{return static_cast<std::uint8_t>((ready_?1:0)|(locked_?2:0)|(lock_remaining_?4:0)|(error_?8:0));}
private:
 [[nodiscard]] bool valid_plan(const ClockPlan& p) const noexcept;
 const ClockNet* external_; ClockPlan staged_{},active_{},pending_{};
 double frc_ppm_=0; unsigned stable_us_=0,lock_remaining_=0,missing_=0;
 std::uint8_t key_=0; bool ready_=false,locked_=false,error_=false;
};
}
