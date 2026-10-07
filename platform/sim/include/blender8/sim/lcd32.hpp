#pragma once
#include "blender8/sim/components.hpp"
#include "blender8/sim/signals.hpp"
#include <array>
#include <cstdint>
namespace b8::sim {
class Lcd32 final : public DisplayDevice, public DisplayProbe, public DisplayBusProbe {
public:
    void reset(Tick now) override;
    void advance(Tick now) override;
    [[nodiscard]] std::uint8_t bus_read(Tick now, std::uint8_t reg) override;
    void bus_write(Tick now, std::uint8_t reg, std::uint8_t data) override;
    // Instrumentation only; GUI renders scanned pixels, not application strings.
    [[nodiscard]] bool pixel(unsigned x,unsigned y) const override;
    [[nodiscard]] bool vblank() const noexcept override { return vblank_; }
    [[nodiscard]] const std::array<std::uint8_t,64>& debug_vram() const noexcept { return vram_; }
    [[nodiscard]] DisplayBusObservation observe_display_bus(Tick now) const override;
private:
    void increment() noexcept;
    void record(Tick now, std::uint8_t reg, std::uint8_t data, std::uint8_t address, bool write, bool accepted);
    std::array<std::uint8_t,64> vram_{};
    std::array<bool,512> scanout_{};
    std::uint8_t address_=0,control_=2;
    bool error_=false,vblank_=false;
    Tick ready_at_=0,epoch_=0;
    DisplayBusObservation bus_{};
    unsigned transfer_next_=0,data_next_=0,blank_next_=0;
};
}
