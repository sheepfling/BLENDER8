#include "blender8/sim/mux8.hpp"
namespace b8::sim {
Mux8::Mux8(std::array<DigitalNet*,8> in, std::array<DigitalNet*,3> sel, DigitalNet& out)
    : inputs_(in),select_(sel),output_(out),driver_(out.attach(Logic::high)) {}
void Mux8::advance(Tick now) {
    unsigned selection=0;
    for (unsigned i=0;i<3;++i) selection|=static_cast<unsigned>(select_[i]->sample())<<i;
    const auto value=inputs_[selection]->resolve();
    if (selection!=selected_ || value!=candidate_) {
        selected_=selection; candidate_=value; stable_at_=now+2;
    }
    if (now>=stable_at_) output_.drive(driver_,candidate_);
}
}
