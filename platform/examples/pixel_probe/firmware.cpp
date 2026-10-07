#include "blender8/firmware.hpp"
#if defined(B8_TARGET_B16)
#include "blender8/b16.hpp"
#endif
#include "blender8/access.hpp"
#include <cstdint>
// Bring-up demonstration, NOT the blender solution: no motor request, no speed policy,
// no thermal supervisor, no application PLL choice. Draws raw contact state as pixels.
namespace firmware {
const b8::VectorTable vectors{};
namespace {unsigned scan=0;std::uint8_t contacts=0;std::uint16_t previous=0;}
void reset(){
#if defined(B8_TARGET_B16)
    b8::write8(b16::PIN_KEY,0x5A);b8::write8(b16::PIN_KEY,0xA5);
    b8::write8(b16::TACH_ROUTE,1);b8::write8(b16::VBLANK_ROUTE,1);
    b8::write8(b16::PIN_LOCK,1);
#endif
    using b8::Reg;
    scan=0;contacts=0;previous=0;
    b8::write8(Reg::GPIOB_OUT,0);b8::write8(Reg::GPIOB_DIR,1);b8::write8(Reg::PWM_CTRL,0);
    b8::write8(Reg::GPIOA_DIR,7);b8::write8(Reg::GPIOA_OUT,0);
    b8::write_staged(b8::timer0_compare,65535);b8::write8(Reg::T0_CTRL,3);
    b8::write8(Reg::XBUS_REG,3);b8::write8(Reg::XBUS_DATA,3);
}
void step(){
    using b8::Reg;
    const auto time=b8::read_latched(b8::timer0_count);
    if(static_cast<std::uint16_t>(time-previous)<1000){b8::idle();return;}
    previous=time;
    const auto input=b8::read8(Reg::GPIOA_IN);
    const auto bit=static_cast<std::uint8_t>(1u<<scan);
    if((input&8)==0)contacts|=bit;else contacts&=static_cast<std::uint8_t>(~bit);
    scan=(scan+1)%8;b8::write8(Reg::GPIOA_OUT,static_cast<std::uint8_t>(scan));
    b8::write8(Reg::XBUS_REG,0);b8::write8(Reg::XBUS_DATA,0);
    for(unsigned x=0;x<32;++x){
        b8::write8(Reg::XBUS_REG,2);
        while(b8::read8(Reg::XBUS_DATA)&1)b8::idle();
        b8::write8(Reg::XBUS_REG,1);
        const auto column=static_cast<std::uint8_t>((x%4==3)?0:((contacts&(1u<<(x/4)))?0x7e:0x42));
        b8::write8(Reg::XBUS_DATA,column);
    }
    // Deliberately simple bring-up service. Do not copy as product health authorization.
    b8::write8(Reg::WDT_SERVICE,0xA5);b8::write8(Reg::WDT_SERVICE,0x5A);
}
}
