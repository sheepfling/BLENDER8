#include "blender8/sim/lcd32.hpp"
#include <stdexcept>
namespace b8::sim {
void Lcd32::reset(Tick now) {
    vram_.fill(0);scanout_.fill(false);address_=0;control_=2;
    error_=false;vblank_=false;ready_at_=now;epoch_=now;
    bus_.epoch_us=now;bus_.phase_us=0;
    bus_.blank_edges[blank_next_]={now,false,true};
    blank_next_=(blank_next_+1)%bus_.blank_edges.size();
    if(bus_.blank_size<bus_.blank_edges.size())++bus_.blank_size;
}
void Lcd32::increment() noexcept { if ((control_&2u)!=0) address_=(address_+1u)&63u; }
std::uint8_t Lcd32::bus_read(Tick now,std::uint8_t reg) {
    const auto address=address_;std::uint8_t data=0;
    switch(reg&3u) {
    case 0:data=address_;break;
    case 1:data=vram_[address_];increment();break;
    case 2:data=static_cast<std::uint8_t>((now<ready_at_?1:0)|(vblank_?2:0)|(error_?4:0));break;
    default:data=control_;break;
    }
    record(now,reg,data,address,false,true);return data;
}
void Lcd32::bus_write(Tick now,std::uint8_t reg,std::uint8_t data) {
    const auto address=address_;bool accepted=true;
    switch(reg&3u) {
    case 0:address_=data&63u;break;
    case 1:
        if (now<ready_at_) { error_=true;accepted=false;break; }
        vram_[address_]=data;increment();ready_at_=now+8;break;
    case 2:if ((data&4u)!=0) error_=false;break;
    default:control_=data&3u;break;
    }
    record(now,reg,data,address,true,accepted);
}
void Lcd32::advance(Tick now) {
    const auto phase=(now-epoch_)%20000;
    bus_.phase_us=phase;
    if(vblank_!=(phase>=16000)){
        bus_.blank_edges[blank_next_]={now,phase>=16000,false};
        blank_next_=(blank_next_+1)%bus_.blank_edges.size();
        if(bus_.blank_size<bus_.blank_edges.size())++bus_.blank_size;
    }
    vblank_=phase>=16000;
    if (phase<16000 && phase%1000==0) {
        const auto y=static_cast<unsigned>(phase/1000);
        for (unsigned x=0;x<32;++x)
            scanout_[y*32+x]=(vram_[(y/8)*32+x]&(1u<<(y%8)))!=0;
    }
}
void Lcd32::record(Tick now,std::uint8_t reg,std::uint8_t data,std::uint8_t address,bool write,bool accepted){
    if(write)++bus_.writes;else ++bus_.reads;
    DisplayTransfer e{now,bus_.reads+bus_.writes,static_cast<std::uint8_t>(reg&3u),data,address,write,accepted,vblank_};
    bus_.transfers[transfer_next_]=e;transfer_next_=(transfer_next_+1)%bus_.transfers.size();
    if(bus_.transfer_size<bus_.transfers.size())++bus_.transfer_size;
    if(write&&(reg&3u)==1){
        ++bus_.data_writes;if(!accepted)++bus_.rejected;
        bus_.data[data_next_]=e;data_next_=(data_next_+1)%bus_.data.size();
        if(bus_.data_size<bus_.data.size())++bus_.data_size;
    }
}
DisplayBusObservation Lcd32::observe_display_bus(Tick now) const {
    auto out=bus_;out.address=address_;out.control=control_;out.busy=now<ready_at_;
    out.error=error_;out.ready_at_us=ready_at_;
    for(unsigned i=0;i<bus_.transfer_size;++i)out.transfers[i]=bus_.transfers[(transfer_next_+bus_.transfers.size()-bus_.transfer_size+i)%bus_.transfers.size()];
    for(unsigned i=0;i<bus_.data_size;++i)out.data[i]=bus_.data[(data_next_+bus_.data.size()-bus_.data_size+i)%bus_.data.size()];
    for(unsigned i=0;i<bus_.blank_size;++i)out.blank_edges[i]=bus_.blank_edges[(blank_next_+bus_.blank_edges.size()-bus_.blank_size+i)%bus_.blank_edges.size()];
    return out;
}
bool Lcd32::pixel(unsigned x,unsigned y) const {
    if (x>=32 || y>=16) throw std::out_of_range("LCD pixel");
    return (control_&1u)!=0 && scanout_[y*32+x];
}
}
