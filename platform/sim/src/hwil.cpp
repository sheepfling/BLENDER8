#include "blender8/sim/hwil.hpp"
#include <cmath>
namespace b8::sim {
LockstepMotorAdapter::LockstepMotorAdapter(MotorPorts ports,std::shared_ptr<MotorPinTransport> link)
 :ports_(ports),link_(std::move(link)),tach_driver_(ports.tach.attach(Logic::low)){
    if(!link_)throw std::invalid_argument("null motor transport");
    const auto c=link_->capabilities();
    if(c.clock!=LinkClock::logical_lockstep||c.sample_period_us!=1||!c.independent_inhibit){
        link_->inhibit();throw std::invalid_argument("motor link requires 1us lockstep and an independent inhibit; physical realtime is not supported by this scheduler");
    }
    ports_.case_temperature.available=false;
    link_->inhibit(); // de-energized handshake
}
LockstepMotorAdapter::~LockstepMotorAdapter(){link_->inhibit();ports_.tach.drive(tach_driver_,Logic::high_z);}
void LockstepMotorAdapter::inhibit_host_failure() noexcept { healthy_=false;link_->inhibit(); }
void LockstepMotorAdapter::advance_one_us(){
    if(!healthy_)throw HardwareLinkError("motor link latched failed");
    ++time_;++sequence_;
    try{
        const auto r=link_->exchange({time_,sequence_,ports_.pwm.sample(),ports_.enable.sample()});
        if(!r.valid||r.time_us!=time_||r.sequence!=sequence_)throw HardwareLinkError("invalid/stale/reordered motor-link reply");
        if(r.case_c && !std::isfinite(*r.case_c))throw HardwareLinkError("non-finite thermal attachment reply");
        ports_.case_temperature.available=r.case_c.has_value();
        if(r.case_c)ports_.case_temperature.celsius=*r.case_c;
        ports_.tach.drive(tach_driver_,r.tach?Logic::high:Logic::low);
    }catch(...){healthy_=false;link_->inhibit();ports_.tach.drive(tach_driver_,Logic::low);throw;}
}
}
