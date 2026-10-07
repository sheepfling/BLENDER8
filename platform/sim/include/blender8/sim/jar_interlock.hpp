#pragma once
#include "blender8/sim/signals.hpp"
#include <array>
namespace b8::sim {
enum class JarFault { healthy, broken_wire, bypassed_contact };
// Chassis-04 S2 + U_IL: not an MCU feature or a firmware policy shortcut.
// Open/reset clears dominate set. Only a NEW physical STOP edge after 20ms continuous
// closure can set permission. STOP already held while qualifying is not an edge.
class JarInterlock {
public:
    JarInterlock(DigitalNet& raw, DigitalNet& permit, const DigitalNet& stop_n);
    void set_seated(bool seated, Tick now);
    void set_fault(JarFault fault) noexcept { fault_=fault; }
    void set_bounce(bool enabled) noexcept { bounce_=enabled; }
    // Raw contact injection is a fixture operation, NOT a firmware method.
    void advance(Tick now, bool reset_released);
    [[nodiscard]] bool seated() const noexcept {return seated_;}
    [[nodiscard]] bool raw_closed() const noexcept {return raw_closed_;}
    [[nodiscard]] bool permitted() const noexcept {return permission_;}
private:
    DigitalNet& raw_; DigitalNet& permit_; const DigitalNet& stop_n_;
    Driver raw_driver_, permit_driver_;
    bool seated_=true, previous_seated_=true, bounce_=true, transition_=false;
    bool raw_closed_=false, previous_raw_=false, permission_=false, previous_stop_=false;
    Tick changed_at_=0, closed_since_=0;
    JarFault fault_=JarFault::healthy;
};
}
