#include "blender8/sim/board.hpp"
#include "blender8/sim/fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace b8::sim;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void wr(Board& b,b8::Reg r,unsigned v){b.mcu().write8(r,static_cast<std::uint8_t>(v),b.now());}
void exercise(DeviceProfile device){
 Board b(BoardProfile::chassis04,{},device,false);b.advance(80000);
 auto m=b.mcu().observe();const auto initial=m.reset_serial;
 check(!m.watchdog.enabled&&!m.watchdog.fused_on&&!m.deadman.enabled,"development reset defaults");
 b.advance(300000);check(b.mcu().reset_serial()==initial,"disabled watchdog reset");
 wr(b,b8::Reg::WDT_SERVICE,0);check(b.mcu().reset_serial()==initial,"disabled service caused reset");
 wr(b,b8::Reg::WDT_CTRL,1);b.advance(10000);m=b.mcu().observe();check(m.watchdog.count>0,"firmware enable did not count");
 wr(b,b8::Reg::WDT_SERVICE,0xA5);check(b.mcu().observe().watchdog.armed,"first key observation");
 wr(b,b8::Reg::WDT_SERVICE,0x5A);check(b.mcu().observe().watchdog.services==1,"completed service observation");
 wr(b,b8::Reg::WDT_CTRL,0);b.advance(300000);check(b.mcu().reset_serial()==initial,"firmware disable ignored");
 wr(b,b8::Reg::WDT_CTRL,0x81);wr(b,b8::Reg::WDT_CTRL,0);
 check(b.mcu().observe().watchdog.enabled&&b.mcu().observe().watchdog.error,"watchdog lock bypass");
 b.advance(205000);m=b.mcu().observe();check(m.reset_serial>initial&&!m.watchdog.enabled&&!m.watchdog.locked,"reset did not restore development fuse");
 check(m.reset_history[m.reset_history_size-1].causes==4&&m.reset_history[m.reset_history_size-1].details==32,"watchdog reset event");
 b.advance(1000);wr(b,b8::Reg::DMT_CTRL,1);b.advance(70000);
 m=b.mcu().observe();check(m.deadman.enabled&&m.deadman.window_open&&m.deadman.count>=256,"deadman open window");
 wr(b,b8::Reg::DMT_PRECLR,0x69);wr(b,b8::Reg::DMT_CLR,0x96);
 check(b.mcu().observe().deadman.services==1,"deadman service observation");
 wr(b,b8::Reg::DMT_CTRL,0);b.advance(300000);check(!b.mcu().observe().deadman.enabled,"deadman disable ignored");
 wr(b,b8::Reg::DMT_CTRL,0x81);wr(b,b8::Reg::DMT_CTRL,0);
 check(b.mcu().observe().deadman.enabled&&b.mcu().observe().deadman.error,"deadman lock bypass");
 auto count=b.mcu().observe().deadman.count;b.mcu().halt_core(true);b.advance(300000);
 check(b.mcu().observe().deadman.count==count&&b.mcu().observe().watchdog.count==0,"halt or disabled watchdog counting");
 b.mcu().halt_core(false);b.advance(263000);m=b.mcu().observe();
 check(m.reset_history[m.reset_history_size-1].causes==8&&m.reset_history[m.reset_history_size-1].details==16,"deadman expiry event");
 wr(b,b8::Reg::RST_CAUSE,255);wr(b,b8::Reg::RST_DETAIL,255);m=b.mcu().observe();
 check(m.reset_causes==0&&m.reset_details==0&&m.reset_history[m.reset_history_size-1].causes==8,"W1C erased history");
 const auto before=snapshot_json(b);for(int i=0;i<20;++i)(void)b.mcu().observe();check(before==snapshot_json(b),"observer mutated supervision");
 for(unsigned i=0;i<20;++i)b.mcu().reset(static_cast<ResetCause>(4|8),32|16,b.now());
 m=b.mcu().observe();check(m.reset_history_size==16,"unbounded reset history");
 check(m.reset_history.back().serial==m.reset_serial&&m.reset_history.front().serial+15==m.reset_serial,"history dropped latest reset");
 check(reset_cause_names(4|8)=="WDT + DMT"&&reset_detail_names(32|16)=="DMT LATE + WDT TIMEOUT","combined reset decoding");
}
}
int main(){try{
 for(auto device:{DeviceProfile::b8,DeviceProfile::b16})exercise(device);
 Watchdog fused;fused.reset();fused.control(0);check(fused.observe().enabled,"production fuse disabled");
 fused.control(128);check(fused.control()==129,"production lock semantics changed");
 Watchdog development(false);development.reset();development.control(128);development.control(1);
 check(!development.observe().enabled&&development.observe().locked&&development.observe().error,"locked disabled option changed");
 std::cout<<"PASS supervision development options and retained reset observations\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
