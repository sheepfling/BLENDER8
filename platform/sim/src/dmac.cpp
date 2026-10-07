#include "blender8/sim/dmac.hpp"
#include <stdexcept>
namespace b8::sim {
DmaObservation Dmac::observe() const noexcept {
    std::uint32_t hash=2166136261u;
    for(auto byte:memory_){hash^=byte;hash*=16777619u;}
    const auto status=busy()?1u|((elapsed_<job_pace_||(!block_&&!pending_))?2u:0u):status_;
    return {status,error_,done_,left_,hash};
}
void Dmac::reset(bool clear_memory) {
    if(clear_memory)memory_.fill(0);
    descriptor_={};staged_={};src_=dst_=done_=left_=0;
    config_=1;trigger_=0;pace_=8;status_=ien_=error_=0;
    job_config_=job_trigger_=selector_=done_high_=left_high_=0;job_pace_=8;
    elapsed_=0;pending_=block_=false;
}
void Dmac::finish(std::uint8_t cause,std::uint8_t error) {status_=cause;error_=error;pending_=block_=false;}
void Dmac::ownership_conflict(){if(external_busy())finish(8,6);}
void Dmac::start(std::uint8_t selector,bool locked) {
    status_=0;error_=0;done_=0;left_=descriptor_[2];elapsed_=0;pending_=block_=false;
    const auto length=descriptor_[2];
    const auto span=[length](unsigned address,bool increment){return address>=0x1000&&address<=0x13FF&&(!increment||address+length-1<=0x13FF);};
    if(length==0||length>1024){finish(8,3);return;}
    if(!locked||(config_&~3u)||trigger_>3||pace_==0){finish(8,4);return;}
    if(!span(descriptor_[0],(config_&1)!=0)){finish(8,1);return;}
    if(descriptor_[1]==0x81 ? (config_&2)!=0 : !span(descriptor_[1],(config_&2)!=0)){finish(8,2);return;}
    src_=descriptor_[0];dst_=descriptor_[1];job_config_=config_;job_trigger_=trigger_;job_pace_=pace_;selector_=selector;
    block_=trigger_==0;status_=block_?1:3;
}
void Dmac::request(unsigned trigger) {
    if(!busy()||trigger!=job_trigger_)return;
    if(trigger==3){block_=true;status_=1;return;}
    if(pending_){finish(8,7);return;}
    pending_=true;status_=1;
}
void Dmac::tick(BytePeripheral& peripheral,Tick now) {
    if(!busy())return;
    if(elapsed_<job_pace_)++elapsed_;
    if(elapsed_<job_pace_||(!block_&&!pending_))return;
    const auto byte=memory_[src_-0x1000];
    if(dst_==0x81){try{peripheral.bus_write(now,selector_,byte);}catch(...){finish(8,5);return;}}
    else memory_[dst_-0x1000]=byte;
    ++done_;--left_;elapsed_=0;pending_=false;
    if(left_==0){finish(4);return;}
    if(job_config_&1)++src_;
    if(job_config_&2)++dst_;
    status_=block_?1:3;
}
std::uint8_t Dmac::read(unsigned address) {
    if(address>=0x1000&&address<=0x13FF)return memory_[address-0x1000];
    if(address>=0xD2&&address<=0xD7)return static_cast<std::uint8_t>(descriptor_[(address-0xD2)/2]>>((address&1)*8));
    switch(address){
    case 0xD0:return 0;case 0xD1:return config_;case 0xD8:return trigger_;case 0xD9:return pace_;
    case 0xDA:return busy()?static_cast<std::uint8_t>(1|((elapsed_<job_pace_||(!block_&&!pending_))?2:0)):status_;
    case 0xDB:return ien_;case 0xDC:return error_;
    case 0xDD:done_high_=static_cast<std::uint8_t>(done_>>8);return static_cast<std::uint8_t>(done_);
    case 0xDE:return done_high_;
    case 0xE0:left_high_=static_cast<std::uint8_t>(left_>>8);return static_cast<std::uint8_t>(left_);
    case 0xE1:return left_high_;
    default:throw std::out_of_range("unmapped B16 DMAC read");}
}
void Dmac::write(unsigned address,std::uint8_t value,std::uint8_t selector,bool locked) {
    if(address>=0x1000&&address<=0x13FF){memory_[address-0x1000]=value;return;}
    if(address==0xDA){status_&=static_cast<std::uint8_t>(~(value&0x1C));return;}
    if(address==0xDB){ien_=value&0x1C;return;}
    if(address==0xD0&&(value&2)){if(busy())finish(16);return;}
    if(address==0xD0&&(value&~3u)){if(!busy()){done_=0;left_=descriptor_[2];}finish(8,4);return;}
    const bool descriptor=address>=0xD1&&address<=0xD9;
    if(busy()&&(descriptor||(address==0xD0&&(value&1)))){finish(8,4);return;}
    if(address==0xD0){if(value&1)start(selector,locked);return;}
    if(address>=0xD2&&address<=0xD7){
        const auto i=(address-0xD2)/2;
        if(address&1)descriptor_[i]=static_cast<std::uint16_t>((static_cast<unsigned>(value)<<8)|staged_[i]);
        else staged_[i]=value;
        return;
    }
    switch(address){case 0xD1:config_=value;return;case 0xD8:trigger_=value;return;case 0xD9:pace_=value;return;
    default:throw std::logic_error("read-only or unmapped B16 DMAC write");}
}
}
