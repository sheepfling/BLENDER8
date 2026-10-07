#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <string_view>
namespace b8::sim {
enum class ResetCause:std::uint8_t {por=1,brownout=2,watchdog=4,deadman=8,clock_fail=16,software=32,external=64};
struct ResetEvent { ResetCause cause; std::uint8_t detail=0; };
struct ResetFlag {unsigned bit;std::string_view name,description;};
inline constexpr std::array<ResetFlag,7> reset_cause_flags{{
 {1,"POR","POWER ON"},{2,"BOR","BROWNOUT"},{4,"WDT","WATCHDOG"},
 {8,"DMT","DEADMAN"},{16,"CLOCK","CLOCK FAILURE"},{32,"SOFT","SOFTWARE"},{64,"EXT","EXTERNAL"}}};
inline constexpr std::array<ResetFlag,6> reset_detail_flags{{
 {1,"WDT BAD KEY",""},{2,"DMT BAD1",""},{4,"DMT BAD2",""},
 {8,"DMT EARLY",""},{16,"DMT LATE",""},{32,"WDT TIMEOUT",""}}};
[[nodiscard]] std::string reset_cause_names(unsigned bits);
[[nodiscard]] std::string reset_detail_names(unsigned bits);
struct ResetRecord {std::uint64_t serial=0,time_us=0;unsigned causes=0,details=0;};
struct WatchdogObservation {
 bool enabled=false,fused_on=true,locked=false,armed=false,error=false;
 unsigned count=0,limit=0,key_count=0;
 std::uint64_t services=0;
};
struct DeadmanObservation {
 bool enabled=false,locked=false,armed=false,error=false,window_open=false;
 unsigned count=0,limit=0,window=0,key_count=0;
 std::uint64_t services=0;
};
class Watchdog {
public:
 explicit Watchdog(bool fused_on=true) noexcept:fused_on_(fused_on),enabled_(fused_on){}
 void reset() noexcept {scale_=3;age_=0;key_age_=0;services_=0;enabled_=fused_on_;armed_=locked_=error_=false;}
 bool tick() noexcept {if(!enabled_)return false;++age_;if(armed_)++key_age_;return age_ >= (256u<<scale_);}
 // false means bad key; a good first key does not refresh the counter.
 bool service(std::uint8_t v) noexcept {
   if(!enabled_){error_=true;return true;}
   if(v==0xA5 && !armed_){armed_=true;key_age_=0;return true;}
   if(v==0x5A && armed_ && key_age_<16){age_=0;armed_=false;++services_;return true;}
   armed_=false;return false;
 }
 void configure(std::uint8_t v) noexcept {if(locked_ || v>5){error_=true;return;}scale_=v;age_=0;armed_=false;}
 void control(std::uint8_t v) noexcept {
   if(locked_){if(!fused_on_&&(v&0x81)!=control())error_=true;return;}
   const bool on=fused_on_||(v&1);
   if(on!=enabled_){age_=key_age_=0;armed_=false;}enabled_=on;
   if(v&128)locked_=true;
 }
 std::uint8_t control() const noexcept{return static_cast<std::uint8_t>((enabled_?1:0)|(locked_?128:0));}
 std::uint8_t scale() const noexcept{return scale_;}
 std::uint8_t status() const noexcept{return static_cast<std::uint8_t>((armed_?1:0)|(error_?128:0));}
 WatchdogObservation observe() const noexcept{return {enabled_,fused_on_,locked_,armed_,error_,age_,256u<<scale_,key_age_,services_};}
private: unsigned age_=0,key_age_=0;std::uint64_t services_=0;std::uint8_t scale_=3;bool fused_on_=true,enabled_=true,armed_=false,locked_=false,error_=false;
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
 DeadmanObservation observe() const noexcept{return {enabled_,locked_,armed_,error_,enabled_&&count_>=window_,count_,limit_,window_,key_count_,services_};}
private:
 std::uint16_t limit_=1024,window_=256,count_=0;std::uint8_t low_limit_=0,low_window_=0,high_=0;
 double phase_=0;unsigned key_count_=0;std::uint64_t services_=0;bool enabled_=false,locked_=false,armed_=false,error_=false;
};
}
