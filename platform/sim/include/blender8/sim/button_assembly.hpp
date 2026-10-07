#pragma once
#include "blender8/sim/components.hpp"
#include "blender8/sim/signals.hpp"
#include <array>
#include <cstdint>
namespace b8::sim {
// Fixture/HMI controls are NOT firmware API. Outputs are physical sense contacts.
class ButtonAssembly : public ButtonDevice, public ButtonProbe {
public:
    ButtonAssembly(std::array<DigitalNet*,8> contacts, DigitalNet& run_permit, DigitalNet& stop_n);
    void press_speed(unsigned speed, Tick now); // 1..7, interlocked latch
    void pulse(bool down, Tick now);           // momentary, preserves speed latch
    void stop(bool down, Tick now);            // release latch and inhibit PULSE
    void advance(Tick now) override;
    void set_bounce(bool enabled) noexcept { bounce_=enabled; }
    void set_slow_bounce(bool enabled) noexcept { slow_bounce_=enabled;random_bounce_=false; }
    void set_random_bounce(std::uint32_t seed) noexcept {rng_=seed?seed:1;random_bounce_=true;bounce_=true;}
    [[nodiscard]] unsigned bounce_mode() const noexcept { return !bounce_?0:random_bounce_?3:slow_bounce_?2:1; }
    // Fault fixture only: -1 normal; 0 stuck closed; 1 stuck open.
    void contact_fault(unsigned channel, int fault);
    [[nodiscard]] std::uint8_t ideal_mask() const noexcept { return target_mask_; }
    [[nodiscard]] ButtonObservation observe_buttons() const noexcept override {
        return {static_cast<std::uint8_t>(target_mask_ & 0x7f), pulse_down_, stopped_, pulse_blocked_};
    }
private:
    void change(std::uint8_t mask, Tick now);
    std::array<DigitalNet*,8> contacts_;
    std::array<Driver,8> drivers_{};
    DigitalNet& permit_;
    Driver permit_driver_;
    DigitalNet& stop_n_; Driver stop_driver_;
    std::array<Tick,8> changed_at_{};
    std::array<std::array<Tick,6>,8> random_edges_{};
    std::uint32_t rng_=1;bool random_bounce_=false;
    std::array<int,8> faults_{};
    std::uint8_t target_mask_=0, previous_mask_=0, changed_mask_=0;
    bool stopped_=false, pulse_down_=false, pulse_blocked_=false, bounce_=true, slow_bounce_=false;
};
}
