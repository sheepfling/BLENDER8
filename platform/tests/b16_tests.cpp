#include "blender8/sim/mcu.hpp"
#include "blender8/b16.hpp"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace b8::sim;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Bus:BytePeripheral {
    std::vector<std::uint8_t> bytes;std::vector<Tick> times;std::vector<unsigned> selectors;bool fail=false;
    std::uint8_t bus_read(Tick,std::uint8_t) override{return 0;}
    void bus_write(Tick now,std::uint8_t reg,std::uint8_t byte) override{
        if(fail)throw std::runtime_error("explicit transport failure");
        bytes.push_back(byte);times.push_back(now);selectors.push_back(reg);
    }
};
struct Rig {
    std::array<DigitalNet,8> a{DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low)};
    std::array<DigitalNet,8> b{DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low),DigitalNet(Logic::low)};
    DigitalNet pwm{Logic::low};AnalogNet analog{1.65};ClockNet clock;Bus bus;
    Mcu mcu{{&a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&a[6],&a[7]},
        {&b[0],&b[1],&b[2],&b[3],&b[4],&b[5],&b[6],&b[7]},pwm,bus,{&analog,&analog},&clock,DeviceProfile::b16};
    Rig(){mcu.reset();tick(1000);}
    void tick(unsigned n){for(unsigned i=0;i<n;++i)mcu.advance_one_us();}
    void w(unsigned address,unsigned byte){mcu.write8(static_cast<b8::Reg>(address),static_cast<std::uint8_t>(byte),0);}
    unsigned r(unsigned address){return mcu.read8(static_cast<b8::Reg>(address),0);}
    void pair(unsigned address,unsigned word){w(address,word&255);w(address+1,word>>8);}
    void unlock(){w(0xE8,0x5A);w(0xE8,0xA5);require(r(0xE9)==0,"pin unlock key pair");}
    void job(unsigned source=0x1000,unsigned dest=0x1100,unsigned len=4,unsigned config=3,unsigned trigger=0,unsigned pace=8){
        pair(0xD2,source);pair(0xD4,dest);pair(0xD6,len);w(0xD1,config);w(0xD8,trigger);w(0xD9,pace);w(0xD0,1);
    }
};
void identity(){Rig f;require(f.r(0)==0x16&&f.r(1)==4&&f.r(5)==15,"B16 identity and capabilities");}
void pins(){Rig f;f.w(0xEB,2);require(f.r(0xEA)==4&&f.r(0xEB)==0,"locked route write");
    f.w(0xE8,0x5A);f.r(0);f.w(0xE8,0xA5);require(f.r(0xE9)==1,"intervening access cancels key");
    f.unlock();f.w(0xEB,2);require(f.r(0xEA)&1,"PWM needs output direction");
    f.w(0x28,0x20);f.w(0xEB,2);f.w(0x60,1);require(f.r(0x60)==0&&(f.r(0xEA)&2),"unlocked PWM rejects enable");
    f.w(0xE9,1);f.w(0x61,255);f.w(0x60,1);f.tick(256);require(f.pwm.sample(),"PB5 PWM reaches motor wire");
    f.w(0xE8,0x5A);f.w(0xE8,0xA5);require(f.r(0xE9)==1,"cannot unlock live PWM");
}
void analog(){Rig f;auto driver=f.a[4].attach(Logic::high);(void)driver;require((f.r(0x22)&0x10)==0,"ANSEL suppresses digital input");
    f.w(0x90,3);f.tick(52);require((f.r(0x93)&2)!=0,"valid analog input conversion");
    f.w(0x90,0);f.unlock();f.w(0x23,0);f.w(0xE9,1);f.w(0x90,3);require(f.r(0x93)==8,"digital pad ADC start rejects");
}
void copy(){Rig f;for(unsigned i=0;i<4;++i)f.w(0x1000+i,10+i);f.job();
    f.tick(7);require(f.r(0xDD)==0&&(f.r(0xDA)&3)==3,"first cell waits PACE");
    f.tick(1);require(f.r(0xDD)==1&&f.r(0x1100)==10,"first cell at PACE");
    f.w(0x1001,99);f.tick(24);require(f.r(0xDA)==4&&f.r(0x1101)==99&&f.r(0xE0)==0,"live source and completed block");
    f.w(0xDA,4);require(f.r(0xDA)==0,"terminal W1C");
}
void overlap(){Rig f;f.w(0x1000,7);f.w(0x1001,8);f.job(0x1000,0x1001,4);f.tick(32);require(f.r(0x1004)==7,"forward copy is not memmove");}
void repeated(){Rig f;f.w(0x1000,42);f.job(0x1000,0x1100,4,0);f.tick(32);require(f.r(0x1100)==42&&f.r(0xDD)==4&&f.r(0x1101)==0,"fixed source and destination");}
void validation(){Rig f;f.job(0,0,0,255);require(f.r(0xDC)==3,"length priority");f.job(0,0,1,255);require(f.r(0xDC)==4,"config priority");
    f.job(0,0,1);require(f.r(0xDC)==1,"source priority");f.job(0x1000,0,1);require(f.r(0xDC)==2,"destination range");
    f.job(0x13FF,0x1100,2);require(f.r(0xDC)==1,"source span boundary");f.job(0x1000,0x13FF,2);require(f.r(0xDC)==2,"dest span boundary");
    f.job(0x1000,0x81,1,3);require(f.r(0xDC)==2,"XBUS destination cannot increment");
    f.unlock();f.job();require(f.r(0xDC)==4&&(f.r(0xEA)&2),"DMA needs locked pin config");
}
void descriptors(){Rig f;f.w(0xD2,0xAB);require(f.r(0xD2)==0,"low descriptor write is staged");f.w(0xD3,0x12);require(f.r(0xD2)==0xAB,"high commits pair");
    f.job();f.w(0xD2,99);require(f.r(0xDC)==4&&f.r(0xD2)==0,"busy descriptor write aborts and is ignored");
}
void abort_job(){Rig f;f.job();f.tick(8);f.w(0xD0,3);require(f.r(0xDA)==16&&f.r(0xDD)==1&&f.r(0xE0)==3,"abort dominates start, preserves issued prefix");f.w(0xD0,2);require(f.r(0xDA)==16,"idle abort no-op");}
void bus(){Rig f;f.w(0x80,1);f.w(0x1000,17);f.job(0x1000,0x81,4,1);f.tick(32);
    require(f.bus.bytes.size()==4&&f.bus.bytes[0]==17&&f.bus.selectors[0]==1,"captured XBUS selector");
    require(f.bus.times[1]-f.bus.times[0]==8,"XBUS paced cells");
    f.job(0x1000,0x81,4,1);f.w(0x80,2);require(f.r(0xDC)==6&&f.r(0x80)==1,"CPU XBUS ownership conflict suppresses write");
    f.job(0x1000,0x81,4,1);f.r(0x81);require(f.r(0xDC)==6,"CPU XBUS read conflict");
    f.job(0x1000,0x81,4,1);f.tick(8);f.bus.fail=true;f.tick(8);require(f.r(0xDC)==5&&f.r(0xDD)==1,"explicit bus failure keeps issued prefix");
}
void requests(){Rig f;f.job(0x1000,0x1100,4,3,1);f.tick(40);require(f.r(0xDD)==0&&(f.r(0xDA)&3)==3,"armed timer waits");
    f.pair(0x44,0);f.w(0x40,3);f.tick(1);require(f.r(0xDD)==1,"timer compare triggers cell independent of IRQ mask");
    f.tick(2);require(f.r(0xDC)==7,"second pending request is overrun");
}
void edges(){Rig f;auto d=f.a[6].attach(Logic::high);f.unlock();f.w(0xEE,1);f.w(0xED,1);f.w(0xE9,1);
    f.job(0x1000,0x1100,1,3,2);f.mcu.latch_inputs(false,false);f.tick(8);require(f.r(0xDD)==0,"routing high does not synthesize DREQ edge");
    f.a[6].drive(d,Logic::low);f.mcu.latch_inputs(false,false);f.a[6].drive(d,Logic::high);f.mcu.latch_inputs(false,false);f.tick(1);require(f.r(0xDA)==4,"DREQ edge transfers cell");
    auto v=f.b[6].attach(Logic::high);f.mcu.latch_inputs(false,false);f.job(0x1000,0x1100,4,3,3);f.tick(40);require(f.r(0xDD)==0,"already-high VBLANK is not next edge");
    f.b[6].drive(v,Logic::low);f.mcu.latch_inputs(false,false);f.b[6].drive(v,Logic::high);f.mcu.latch_inputs(false,false);f.tick(1);f.b[6].drive(v,Logic::low);f.mcu.latch_inputs(false,false);f.tick(24);require(f.r(0xDD)==4,"VBLANK starts whole block beyond blank");
}
void irq(){Rig f;f.w(0xDB,4);f.w(0x11,0x20);f.w(0x10,1);f.job(0x1000,0x1100,1);f.tick(8);
    require(f.mcu.pending_irq()&&static_cast<unsigned>(*f.mcu.pending_irq())==5,"sixth IRQ vector");
    f.w(0x12,0x20);f.tick(1);require(f.r(0x12)&0x20,"terminal IRQ reasserts");
    f.w(0xDA,4);f.w(0x12,0x20);f.tick(1);require(!(f.r(0x12)&0x20),"peripheral then controller acknowledgement");
}
void reset(){Rig f;f.w(0x1000,33);f.job();f.mcu.reset(ResetCause::software);require(f.r(0x1000)==33&&f.r(0xDA)==0&&f.r(0xE9)==1,"software reset retains SRAM, cancels job and locks pins");
    f.mcu.reset(ResetCause::brownout);require(f.r(0x1000)==0,"brownout clears SRAM");
}
void halt(){Rig f;f.job();f.mcu.halt_core(true);f.tick(32);require(f.r(0xDA)==4,"DMA runs while core halted and IRQs masked");}
void binary32(){
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    volatile float one=1.0f,half=0x1p-24f,tiny=std::numeric_limits<float>::denorm_min();
    require(one+half==1.0f,"binary32 ties to even");require(tiny+tiny==0x1p-148f,"binary32 subnormal");
    volatile float zero=0.0f;require(std::isinf(one/zero)&&std::isnan(zero/zero),"binary32 infinity and NaN");
    require(std::signbit(-zero),"binary32 signed zero");
}
}
int main(){try{identity();pins();analog();copy();overlap();repeated();validation();descriptors();abort_job();bus();requests();edges();irq();reset();halt();binary32();std::cout<<"B16: 16 peripheral/numeric conformance cases passed\n";}catch(const std::exception& e){std::cerr<<"B16: "<<e.what()<<'\n';return 1;}}
