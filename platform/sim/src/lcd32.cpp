#include "blender8/sim/lcd32.hpp"
#include <stdexcept>
namespace b8::sim {
void Lcd32::reset(Tick now) {
    vram_.fill(0);scanout_.fill(false);address_=0;control_=2;
    error_=false;vblank_=false;ready_at_=now;epoch_=now;
}
void Lcd32::increment() noexcept { if ((control_&2u)!=0) address_=(address_+1u)&63u; }
std::uint8_t Lcd32::bus_read(Tick now,std::uint8_t reg) {
    switch(reg&3u) {
    case 0:return address_;
    case 1: { const auto data=vram_[address_];increment();return data; }
    case 2:return static_cast<std::uint8_t>((now<ready_at_?1:0)|(vblank_?2:0)|(error_?4:0));
    default:return control_;
    }
}
void Lcd32::bus_write(Tick now,std::uint8_t reg,std::uint8_t data) {
    switch(reg&3u) {
    case 0:address_=data&63u;break;
    case 1:
        if (now<ready_at_) { error_=true;break; }
        vram_[address_]=data;increment();ready_at_=now+8;break;
    case 2:if ((data&4u)!=0) error_=false;break;
    default:control_=data&3u;break;
    }
}
void Lcd32::advance(Tick now) {
    const auto phase=(now-epoch_)%20000;
    vblank_=phase>=16000;
    if (phase<16000 && phase%1000==0) {
        const auto y=static_cast<unsigned>(phase/1000);
        for (unsigned x=0;x<32;++x)
            scanout_[y*32+x]=(vram_[(y/8)*32+x]&(1u<<(y%8)))!=0;
    }
}
bool Lcd32::pixel(unsigned x,unsigned y) const {
    if (x>=32 || y>=16) throw std::out_of_range("LCD pixel");
    return (control_&1u)!=0 && scanout_[y*32+x];
}
}
