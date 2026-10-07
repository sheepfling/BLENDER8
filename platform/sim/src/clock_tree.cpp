#include "blender8/sim/clock_tree.hpp"
#include <cmath>
#include <stdexcept>
namespace b8::sim {
namespace { bool pow2(unsigned v){return v && !(v&(v-1));} }
void ClockTree::reset() noexcept {staged_={};active_={};pending_={};stable_us_=lock_remaining_=missing_=0;key_=0;ready_=locked_=error_=false;}
void ClockTree::set_frc_ppm(double v){if(!(v>=-50000 && v<=50000))throw std::invalid_argument("FRC tolerance");frc_ppm_=v;}
double ClockTree::system_hz() const noexcept {
 if(active_.source==0)return 4'000'000*(1+frc_ppm_/1e6);
 const double ext=external_?external_->hz:0;
 return active_.source==1?ext:ext/active_.pre*active_.mul/active_.post;
}
bool ClockTree::stopped() const noexcept{return active_.source!=0 && (!external_ || !external_->valid);}
bool ClockTree::valid_plan(const ClockPlan& p) const noexcept {
 if(p.source>2 || !pow2(p.pb) || p.pb>32)return false;
 double sys=4'000'000; // validate nominal ranges; tolerances are separate
 if(p.source) {
   if(!ready_ || !external_)return false;
   sys=external_->hz;
   if(!std::isfinite(sys)||sys<=0)return false;
   if(p.source==2) {
     if(!pow2(p.pre)||p.pre>8||!pow2(p.post)||p.post>8||p.mul<4||p.mul>16)return false;
     const double ref=sys/p.pre,vco=ref*p.mul;
     if(ref<999'000||ref>4'004'000||vco<15'984'000||vco>64'064'000)return false;
     sys=vco/p.post;
   }
 }
 return sys>=3'996'000 && sys<=32'032'000 && sys/p.pb>=249'750 && sys/p.pb<=4'004'000;
}
bool ClockTree::advance_one_us(unsigned lf_edges) {
 if(external_ && external_->valid){if(stable_us_<10000)++stable_us_;ready_=stable_us_>=static_cast<unsigned>(std::ceil(256e6/external_->hz));}
 else {stable_us_=0;ready_=false;locked_=false;}
 if(stopped()) {missing_+=lf_edges;if(missing_>=8)return true;}
 else missing_=0;
 if(lock_remaining_) {
   if(!ready_){lock_remaining_=0;error_=true;}
   else if(--lock_remaining_==0){active_=pending_;locked_=true;}
 }
 return false;
}
std::uint8_t ClockTree::read(unsigned o){
 key_=0;
 switch(o){case 0:return staged_.source;case 1:return staged_.pre;case 2:return staged_.mul;
 case 3:return staged_.post;case 4:return staged_.pb;case 5:return static_cast<std::uint8_t>((ready_?1:0)|(locked_?2:0)|(lock_remaining_?4:0)|(error_?8:0));
 case 6:return active_.source;case 7:return 0;case 8:return 0;case 9:return active_.pb;default:throw std::out_of_range("clock register");}
}
void ClockTree::write(unsigned o,std::uint8_t v,bool quiescent){
 if(o==7){if(v==0xC3 && key_==0)key_=1;else if(v==0x3C && key_==1)key_=2;else{key_=0;error_=true;}return;}
 const bool authorized=key_==2;key_=0;
 switch(o){
 case 0:staged_.source=v;break;case 1:staged_.pre=v;break;case 2:staged_.mul=v;break;
 case 3:staged_.post=v;break;case 4:staged_.pb=v;break;case 5:if(v&8)error_=false;break;
 case 8:
   if(!authorized || v!=0xA5 || !quiescent || lock_remaining_ || !valid_plan(staged_)){error_=true;return;}
   if(staged_.source==2){pending_=staged_;lock_remaining_=250;locked_=false;}
   else{active_=staged_;locked_=false;}
   break;
 default:throw std::logic_error("read-only clock register");
 }
}
}
