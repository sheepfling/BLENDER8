// Host-side display replacement. The firmware still uses only the B8 SDK.
#include "blender8/access.hpp"
#include "blender8/sim/board.hpp"
#include "blender8/sim/runtime.hpp"
#include <iostream>
#include <memory>

class AuditedDisplay final : public b8::sim::DisplayDevice, public b8::sim::DisplayProbe {
public:
    void reset(b8::sim::Tick now) override { lcd_.reset(now); }
    void advance(b8::sim::Tick now) override { lcd_.advance(now); }
    bool vblank() const noexcept override { return lcd_.vblank(); }
    bool pixel(unsigned x, unsigned y) const override { return lcd_.pixel(x,y); }
    std::uint8_t bus_read(b8::sim::Tick now, std::uint8_t reg) override {
        return lcd_.bus_read(now,reg);
    }
    void bus_write(b8::sim::Tick now, std::uint8_t reg, std::uint8_t value) override {
        ++writes_; lcd_.bus_write(now,reg,value);
    }
    std::uint64_t writes() const noexcept { return writes_; }
private:
    b8::sim::Lcd32 lcd_;
    std::uint64_t writes_=0;
};

int main() {
    // Only the factory changes; the B8 register protocol and LCD timing stay identical.
    b8::sim::ComponentFactories devices;
    devices.display = [] { return std::make_unique<AuditedDisplay>(); };
    b8::sim::Board board(b8::sim::BoardProfile::chassis04, std::move(devices));
    board.advance(60000); // Fixture power/clock qualification; not a firmware delay recipe.
    b8::VectorTable vectors{};
    b8::sim::Runtime binding(board,vectors);
    b8::write8(b8::Reg::XBUS_REG,3); b8::write8(b8::Reg::XBUS_DATA,3);
    b8::write8(b8::Reg::XBUS_REG,0); b8::write8(b8::Reg::XBUS_DATA,0);
    b8::write8(b8::Reg::XBUS_REG,1); b8::write8(b8::Reg::XBUS_DATA,0x81);
    board.advance(20000);
    const auto& display = dynamic_cast<const AuditedDisplay&>(board.display_device());
    if(!display.pixel(0,0) || !display.pixel(0,7) || display.pixel(1,0)) return 1;
    std::cout << "Replacement display: " << display.writes() << " bus writes; pixel contract passed.\n";
}
