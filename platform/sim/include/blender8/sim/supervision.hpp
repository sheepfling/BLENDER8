#pragma once
#include <cstdint>
namespace b8::sim {
enum class ResetCause:std::uint8_t {por=1,brownout=2,watchdog=4,deadman=8,clock_fail=16,software=32,external=64};
struct ResetEvent { ResetCause cause; std::uint8_t detail=0; };
class Watchdog {
public:
 void reset() noexcept {scale_=3;age_=0;key_age_=0;armed_=locked_=error_=false;}
 bool tick() noexcept {++age_;if(armed_)++key_age_;return age_ >= (256u<<scale_);}
 // false means bad key; a good first key does not refresh the counter.
 bool service(std::uint8_t v) noexcept {
   if(v==0xA5 && !armed_){armed_=true;key_age_=0;return true;}
   if(v==0x5A && armed_ && key_age_<16){age_=0;armed_=false;return true;}
   armed_=false;return false;
 }
 void configure(std::uint8_t v) noexcept {if(locked_ || v>5){error_=true;return;}scale_=v;age_=0;armed_=false;}
 void control(std::uint8_t v) noexcept {if(v&128)locked_=true;} // fused ON; cannot disable
 std::uint8_t control() const noexcept{return static_cast<std::uint8_t>(1|(locked_?128:0));}
 std::uint8_t scale() const noexcept{return scale_;}
 std::uint8_t status() const noexcept{return static_cast<std::uint8_t>((armed_?1:0)|(error_?128:0));}
private: unsigned age_=0,key_age_=0;std::uint8_t scale_=3;bool armed_=false,locked_=false,error_=false;
};
class DeadmanTimer {
public:
 void reset() noexcept;
 bool advance(double sys_hz,bool core_active) noexcept;
 std::uint8_t read(unsigned offset);
 // 0 = no reset; otherwise detail bits: BAD1=2, BAD2=4, EARLY=8, LATE=16.
 std::uint8_t write(unsigned offset,std::uint8_t value);
 bool enabled() const noexcept{return enabled_;}
 bool debug_locked() const noexcept{return locked_;}
 std::uint16_t debug_limit() const noexcept{return limit_;}
 std::uint16_t debug_window() const noexcept{return window_;}
private:
 std::uint16_t limit_=1024,window_=256,count_=0;std::uint8_t low_limit_=0,low_window_=0,high_=0;
 double phase_=0;unsigned key_count_=0;bool enabled_=false,locked_=false,armed_=false,error_=false;
};
}
