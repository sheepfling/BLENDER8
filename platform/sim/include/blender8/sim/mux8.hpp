#pragma once
#include "blender8/sim/components.hpp"
#include "blender8/sim/signals.hpp"
#include <array>
namespace b8::sim {
class Mux8 : public MuxDevice {
public:
    Mux8(std::array<DigitalNet*,8> inputs, std::array<DigitalNet*,3> select,
         DigitalNet& output);
    void advance(Tick now) override;
private:
    std::array<DigitalNet*,8> inputs_;
    std::array<DigitalNet*,3> select_;
    DigitalNet& output_; Driver driver_;
    unsigned selected_=0;
    Logic candidate_=Logic::high;
    Tick stable_at_=2;
};
}
