#include "blender8/sim/button_assembly.hpp"
#include <stdexcept>
#include <algorithm>
namespace b8::sim {
ButtonAssembly::ButtonAssembly(std::array<DigitalNet*,8> contacts, DigitalNet& permit,DigitalNet& stop_n)
    : contacts_(contacts), permit_(permit), permit_driver_(permit.attach(Logic::low)),
      stop_n_(stop_n),stop_driver_(stop_n.attach(Logic::high)) {
    faults_.fill(-1);
    for (unsigned i=0;i<8;++i) drivers_[i]=contacts_[i]->attach();
}
void ButtonAssembly::change(std::uint8_t mask, Tick now) {
    const auto changed=static_cast<std::uint8_t>(target_mask_^mask);
    for (unsigned i=0;i<8;++i) if ((changed&(1u<<i))!=0) {
        const auto bit=static_cast<std::uint8_t>(1u<<i);
        if ((target_mask_&bit)!=0) previous_mask_|=bit;
        else previous_mask_&=static_cast<std::uint8_t>(~bit);
        changed_at_[i]=now; changed_mask_|=bit;
        if(random_bounce_){
            for(auto& edge:random_edges_[i]){rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;edge=1+rng_%4999;}
            std::sort(random_edges_[i].begin(),random_edges_[i].end());
        }
    }
    target_mask_=mask;
    permit_.drive(permit_driver_,!stopped_ && mask!=0 ? Logic::high:Logic::low);
    advance(now);
}
void ButtonAssembly::press_speed(unsigned speed, Tick now) {
    if (speed<1 || speed>7) throw std::out_of_range("speed must be 1..7");
    if (stopped_) return;
    change(static_cast<std::uint8_t>((target_mask_&0x80u)|(1u<<(speed-1u))),now);
}
void ButtonAssembly::pulse(bool down, Tick now) {
    const bool rising=down && !pulse_down_; pulse_down_=down;
    if (!down) { pulse_blocked_=false; change(target_mask_&0x7fu,now); return; }
    if (stopped_) { pulse_blocked_=true; return; }
    if (rising && !pulse_blocked_) change(target_mask_|0x80u,now);
}
void ButtonAssembly::stop(bool down, Tick now) {
    stopped_=down;
    stop_n_.drive(stop_driver_,down?Logic::low:Logic::high);
    if (down) { pulse_blocked_=pulse_down_; change(0,now); }
    else change(target_mask_,now); // no auto re-latch on STOP release
}
void ButtonAssembly::contact_fault(unsigned channel, int fault) {
    if (channel>=8 || fault < -1 || fault>1) throw std::out_of_range("contact fault");
    faults_[channel]=fault;
}
void ButtonAssembly::advance(Tick now) {
    // Deterministic nominal profile: target at 0, old at 100, target at 400,
    // old at 900, final target at 1800 us. Spec allows arbitrary bounce <=5000 us.
    for (unsigned i=0;i<8;++i) {
        const auto bit=static_cast<std::uint8_t>(1u<<i);
        bool closed=(target_mask_&bit)!=0;
        if (bounce_ && (changed_mask_&bit)!=0) {
            const auto age=now-changed_at_[i];
            if(random_bounce_){
                unsigned edges=0;for(const auto edge:random_edges_[i])if(age>=edge)++edges;
                if(age<5000 && (edges&1u))closed=(previous_mask_&bit)!=0;
                if(age>=5000)changed_mask_&=static_cast<std::uint8_t>(~bit);
            }else{
            const Tick a=slow_bounce_?1000:100,b=slow_bounce_?2000:400,
                       c=slow_bounce_?3000:900,end=slow_bounce_?5000:1800;
            if ((age>=a && age<b)||(age>=c && age<end))
                closed=(previous_mask_&bit)!=0;
            if (age>=end) changed_mask_&=static_cast<std::uint8_t>(~bit);
            }
        }
        if (faults_[i]>=0) closed=faults_[i]==0;
        contacts_[i]->drive(drivers_[i],closed?Logic::low:Logic::high_z);
    }
}
}
