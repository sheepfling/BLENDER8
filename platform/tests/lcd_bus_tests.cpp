#include "blender8/sim/board.hpp"
#include "blender8/sim/fixture.hpp"
#include "blender8/sim/scene_observation.hpp"
#include <iostream>
#include <stdexcept>
using namespace b8::sim;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void transfers(){
 Lcd32 lcd;lcd.reset(0);lcd.bus_write(0,0,63);lcd.bus_write(0,1,0x81);
 lcd.bus_write(1,1,0x22);auto b=lcd.observe_display_bus(1);
 check(b.data_size==2&&b.data[0].accepted&&!b.data[1].accepted,"accepted and dropped transfers indistinguishable");
 check(b.data[0].address==63&&b.data[1].address==0&&b.address==0,"address-before-transfer or wrap incorrect");
 check(b.data_writes==2&&b.rejected==1&&b.busy&&b.error,"bus counters or busy/error observation");
 lcd.bus_write(8,1,0x33);check(lcd.debug_vram()[0]==0x33,"observer altered busy boundary");
 lcd.bus_write(8,0,63);check(lcd.bus_read(8,1)==0x81,"DATA read value");
 b=lcd.observe_display_bus(8);check(b.address==0&&b.transfers[b.transfer_size-1].address==63,"DATA read increment not captured");
 check(!b.transfers[b.transfer_size-1].write&&b.transfers[b.transfer_size-1].data==0x81,"readback not captured");
 for(unsigned i=0;i<200;++i)(void)lcd.bus_read(9,2);
 b=lcd.observe_display_bus(9);check(b.transfer_size==32&&b.data_size==3&&b.reads==201,"STATUS polling evicted data or history unbounded");
 for(unsigned i=0;i<100;++i)lcd.bus_write(16+i*8,1,static_cast<std::uint8_t>(i));
 b=lcd.observe_display_bus(1000);check(b.data_size==64&&b.data[0].data==36&&b.data[63].data==99,"DATA ring chronology");
 check(b.data_writes==103&&b.writes+b.reads==b.transfers[31].serial,"lifetime counters or transaction serial");
}
void blanking(){
 Lcd32 lcd;lcd.reset(0);for(Tick t=1;t<=40000;++t)lcd.advance(t);
 auto b=lcd.observe_display_bus(40000);
 check(b.blank_size==5&&b.blank_edges[0].reset,"initial reset marker");
 for(unsigned i=1;i<5;++i){const auto& e=b.blank_edges[i];
  check(e.time_us==(i%2?16000+(i/2)*20000:(i/2)*20000)&&e.high==(i%2!=0),"VBLANK edge not at actual transition");}
 lcd.advance(56000);lcd.bus_write(56000,1,0xAA);check(lcd.observe_display_bus(56000).data[0].vblank,"write blanking state missing");
 lcd.reset(57000);b=lcd.observe_display_bus(57000);
 check(!lcd.vblank()&&b.epoch_us==57000&&b.blank_edges[b.blank_size-1].reset,"reset phase and marker");
 check(b.data_size==1&&b.data[0].time_us==56000,"reset silently erased retained capture");
 for(Tick t=57001;t<=800000;++t)lcd.advance(t);
 b=lcd.observe_display_bus(800000);check(b.blank_size==32&&b.blank_edges[0].time_us<b.blank_edges[31].time_us,"blanking ring bounded chronology");
}
void dma(){
 Board board(BoardProfile::chassis04,{},DeviceProfile::b16,false);board.advance(80000);
 auto write=[&](unsigned reg,unsigned data){board.mcu().write8(static_cast<b8::Reg>(reg),static_cast<std::uint8_t>(data),board.now());};
 auto pair=[&](unsigned reg,unsigned data){write(reg,data&255);write(reg+1,data>>8);};
 write(0x80,1);for(unsigned i=0;i<4;++i)write(0x1000+i,0xA0+i);
 pair(0xD2,0x1000);pair(0xD4,0x81);pair(0xD6,4);write(0xD1,1);write(0xD9,8);write(0xD0,1);
 board.advance(32);auto b=board.lcd().observe_display_bus(board.now());
 check(b.data_size==4&&b.rejected==0&&board.mcu().observe_dma().done==4,"DMA writes missing from LCD probe");
 for(unsigned i=0;i<4;++i)check(b.data[i].data==0xA0+i&&b.data[i].time_us==80008+i*8,"DMA bytes or exact timestamps");
 write(0xD0,1);const auto before=board.lcd().observe_display_bus(board.now()).reads;
 (void)board.mcu().read8(static_cast<b8::Reg>(0x81),board.now());
 check(board.lcd().observe_display_bus(board.now()).reads==before,"CPU ownership conflict falsely reached display");
 const auto snapshot=snapshot_json(board);
 for(unsigned i=0;i<20;++i){(void)observe_scene(board);(void)board.lcd().observe_display_bus(board.now());}
 check(snapshot==snapshot_json(board),"observations changed machine or recorded synthetic bus reads");
 const auto compact=snapshot_json(board,false);
 check(compact.find("\"captures_included\":false")!=std::string::npos&&compact.find("\"transfers\":")==std::string::npos,"trace point repeated full bus capture");
}
struct Uninstrumented final:DisplayDevice {
 Lcd32 lcd;
 void reset(Tick t)override{lcd.reset(t);}void advance(Tick t)override{lcd.advance(t);}
 bool vblank()const noexcept override{return lcd.vblank();}
 std::uint8_t bus_read(Tick t,std::uint8_t r)override{return lcd.bus_read(t,r);}
 void bus_write(Tick t,std::uint8_t r,std::uint8_t d)override{lcd.bus_write(t,r,d);}
};
void missing(){ComponentFactories f;f.display=[](){return std::make_unique<Uninstrumented>();};Board b(BoardProfile::chassis04,f);
 check(!observe_scene(b).lcd_bus&&snapshot_json(b).find("\"lcd_bus\":null")!=std::string::npos,"missing probe fabricated bus truth");}
}
int main(){try{transfers();blanking();dma();missing();std::cout<<"PASS LCD bus, DMA, VBLANK and observational purity\n";}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
