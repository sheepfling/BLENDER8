#include "blender8/sim/jar_interlock.hpp"
namespace b8::sim {
JarInterlock::JarInterlock(DigitalNet& raw,DigitalNet& permit,const DigitalNet& stop_n)
 :raw_(raw),permit_(permit),stop_n_(stop_n),raw_driver_(raw.attach(Logic::low)),
  permit_driver_(permit.attach(Logic::low)){}
void JarInterlock::set_seated(bool seated,Tick now) {
    if(seated==seated_) return;
    previous_seated_=seated_;seated_=seated;changed_at_=now;transition_=true;
}
void JarInterlock::advance(Tick now,bool released) {
    bool closed=seated_;
    if(bounce_ && transition_) {
        const auto age=now-changed_at_;
        if((age>=200 && age<700)||(age>=1600 && age<2700)||(age>=3800 && age<5000))
            closed=previous_seated_;
        if(age>=5000)transition_=false;
    }
    if(fault_==JarFault::broken_wire)closed=false;
    if(fault_==JarFault::bypassed_contact)closed=true; // intentionally undetectable single-loop fault
    const bool stop=!stop_n_.sample();
    const bool fresh_stop=stop&&!previous_stop_;
    if(!closed||!released) {
        permission_=false;closed_since_=now;
    } else {
        if(!previous_raw_)closed_since_=now;
        if(fresh_stop && now-closed_since_>=20000)permission_=true;
    }
    raw_closed_=closed;previous_raw_=closed;previous_stop_=stop;
    raw_.drive(raw_driver_,closed?Logic::high:Logic::low);
    permit_.drive(permit_driver_,permission_?Logic::high:Logic::low);
}
}
