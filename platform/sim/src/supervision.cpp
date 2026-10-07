#include "blender8/sim/supervision.hpp"
#include <stdexcept>
namespace b8::sim {
namespace {
template<std::size_t N> std::string names(unsigned bits,const std::array<ResetFlag,N>& flags){
 std::string out;unsigned known=0;
 for(const auto& f:flags){known|=f.bit;if(bits&f.bit){if(!out.empty())out+=" + ";out+=f.name;}}
 if(bits&~known){if(!out.empty())out+=" + ";out+="UNKNOWN";}
 return out.empty()?"NONE":out;
}
}
std::string reset_cause_names(unsigned bits){return names(bits,reset_cause_flags);}
std::string reset_detail_names(unsigned bits){return names(bits,reset_detail_flags);}
void DeadmanTimer::reset() noexcept{limit_=1024;window_=256;count_=0;low_limit_=low_window_=high_=0;phase_=0;key_count_=0;services_=0;enabled_=locked_=armed_=error_=false;}
bool DeadmanTimer::advance(double hz,bool core_active) noexcept {
 if(!enabled_ || !core_active)return false;
 phase_+=hz/(1024.0*1e6);
 while(phase_>=1){phase_-=1;++count_;if(armed_)++key_count_;if(count_>=limit_)return true;}
 return false;
}
std::uint8_t DeadmanTimer::read(unsigned o){switch(o){
 case 0:return static_cast<std::uint8_t>((enabled_?1:0)|(locked_?128:0));
 case 1:return static_cast<std::uint8_t>(limit_);case 2:return static_cast<std::uint8_t>(limit_>>8);
 case 3:return static_cast<std::uint8_t>(window_);case 4:return static_cast<std::uint8_t>(window_>>8);
 case 5:return static_cast<std::uint8_t>((enabled_&&count_>=window_?1:0)|(enabled_?2:0)|(error_?128:0));
 case 6:high_=static_cast<std::uint8_t>(count_>>8);return static_cast<std::uint8_t>(count_);
 case 7:return high_;case 8:case 9:return 0;default:throw std::out_of_range("DMT register");}}
std::uint8_t DeadmanTimer::write(unsigned o,std::uint8_t v){
 if(o==0){
  if(locked_){if((v&0x81)!=read(0))error_=true;return 0;}
  const bool on=(v&1)!=0;
  if(on && (!limit_ || window_>=limit_)){error_=true;return 0;}
  if(on!=enabled_){count_=0;phase_=0;armed_=false;}enabled_=on;
  if(v&128)locked_=true;
  return 0;
 }
 if(o>=1 && o<=4){
  if(enabled_ || locked_){error_=true;return 0;}
  if(o==1)low_limit_=v;else if(o==2)limit_=static_cast<std::uint16_t>((v<<8)|low_limit_);
  else if(o==3)low_window_=v;else window_=static_cast<std::uint16_t>((v<<8)|low_window_);
  return 0;
 }
 if(o==5){if(v&128)error_=false;return 0;}
 if(o==8 || o==9){
  if(!enabled_){error_=true;return 0;}
  if(o==8){if(v!=0x69 || armed_)return 2;if(count_<window_)return 8;armed_=true;key_count_=0;return 0;}
  if(v!=0x96 || !armed_ || key_count_>=16)return 4;
  if(count_<window_)return 8;
  count_=0;phase_=0;armed_=false;++services_;return 0;
 }
 throw std::logic_error("read-only DMT register");
}
}
