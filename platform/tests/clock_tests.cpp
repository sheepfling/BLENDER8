#include "blender8/sim/runtime.hpp"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
using b8::Reg;using b8::sim::Board;
namespace {
void ck(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void near(double a,double b,double d,const char*s){ck(std::abs(a-b)<=d,s);}
void wr(Board& b,Reg r,unsigned v){b.mcu().write8(r,static_cast<std::uint8_t>(v),b.now());b.settle();}
unsigned rd(Board&b,Reg r){return b.mcu().read8(r,b.now());}
void boot(Board&b){b.advance(45000);ck(b.ready(),"ready");ck(rd(b,Reg::SYS_REV)==3,"rev03");}
void feed(Board&b){wr(b,Reg::WDT_SERVICE,0xA5);wr(b,Reg::WDT_SERVICE,0x5A);}
void commit(Board&b){wr(b,Reg::CLK_KEY,0xC3);wr(b,Reg::CLK_KEY,0x3C);wr(b,Reg::CLK_COMMIT,0xA5);}
void plan(Board&b,unsigned pre=2,unsigned mul=8,unsigned post=2,unsigned pb=16){wr(b,Reg::CLK_SOURCE,2);wr(b,Reg::PLL_PREDIV,pre);wr(b,Reg::PLL_MULT,mul);wr(b,Reg::PLL_POSTDIV,post);wr(b,Reg::PB_DIV,pb);commit(b);}
void dmt(Board&b,unsigned limit=1024,unsigned window=256){wr(b,Reg::DMT_LIMIT_LO,limit);wr(b,Reg::DMT_LIMIT_HI,limit>>8);wr(b,Reg::DMT_WINDOW_LO,window);wr(b,Reg::DMT_WINDOW_HI,window>>8);wr(b,Reg::DMT_CTRL,0x81);}
unsigned count(Board&b){unsigned l=rd(b,Reg::DMT_COUNT_LO);return l|(rd(b,Reg::DMT_COUNT_HI)<<8);}
void drive(Board&b){b.buttons().set_bounce(false);b.buttons().press_speed(1,b.now());wr(b,Reg::GPIOB_DIR,1);wr(b,Reg::GPIOB_OUT,1);wr(b,Reg::PWM_DUTY,255);wr(b,Reg::PWM_CTRL,1);}
void startup(){Board b(b8::sim::BoardProfile::clocked03);ck(!b.ready()&&!b.drive_enabled(),"initial reset");b.advance(10000);ck(!b.ready(),"ramp blocks boot");b.advance(35000);ck(b.ready(),"released");ck(rd(b,Reg::RST_CAUSE)&1,"POR cause");}
void oscillator_startup(){b8::sim::ClockNet n;b8::sim::CrystalOscillator o(n);for(int i=0;i<4999;++i)o.advance_one_us(true);ck(!n.valid,"no early stable clock");o.advance_one_us(true);ck(n.valid,"XO stable at5000");o.advance_one_us(false);ck(!n.valid,"supply removal");}
void oscillator_ppm(){Board b(b8::sim::BoardProfile::clocked03);b.oscillator().set_ppm(50);boot(b);plan(b);b.advance(250);near(b.mcu().clocks().peripheral_hz(),1000050,.01,"XO tolerance propagates");}
void pll_lock(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);ck(rd(b,Reg::CLK_ACTIVE)==0,"pending not active");ck(rd(b,Reg::CLK_STATUS)&4,"busy");b.advance(249);ck(rd(b,Reg::CLK_ACTIVE)==0,"not early");b.advance(1);ck(rd(b,Reg::CLK_ACTIVE)==2,"selected");near(b.mcu().clocks().system_hz(),16000000,.01,"system clock");near(b.mcu().clocks().peripheral_hz(),1e6,.01,"PBclock");}
void invalid_ref(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,1,4,2,16);ck(rd(b,Reg::CLK_STATUS)&8,"8MHz ref rejected");}
void invalid_vco(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,8,4,1,4);ck(rd(b,Reg::CLK_STATUS)&8,"4MHz VCO rejected");}
void invalid_pb(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,2,8,2,1);ck(rd(b,Reg::CLK_STATUS)&8,"16MHz PB rejected");}
void invalid_code(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,3,8,2,16);ck(rd(b,Reg::CLK_STATUS)&8,"unsupported divider");}
void key_sequence(){Board b(b8::sim::BoardProfile::clocked03);boot(b);wr(b,Reg::CLK_COMMIT,0xA5);ck(rd(b,Reg::CLK_STATUS)&8,"unkeyed write");wr(b,Reg::CLK_STATUS,8);wr(b,Reg::CLK_KEY,0xC3);wr(b,Reg::CLK_KEY,0x3C);(void)rd(b,Reg::SYS_ID);wr(b,Reg::CLK_COMMIT,0xA5);ck(rd(b,Reg::CLK_STATUS)&8,"read invalidates unlock");}
void change_busy_timer(){Board b(b8::sim::BoardProfile::clocked03);boot(b);wr(b,Reg::T0_CTRL,1);plan(b);ck(rd(b,Reg::CLK_STATUS)&8,"active timer rejects switch");}
void change_busy_adc(){Board b(b8::sim::BoardProfile::clocked03);boot(b);wr(b,Reg::ADC_CTRL,3);plan(b);ck(rd(b,Reg::CLK_STATUS)&8,"busy ADC rejects switch");}
void timer_frequency(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,2,8,2,8);b.advance(250);wr(b,Reg::T0_COMPARE_LO,999&255);wr(b,Reg::T0_COMPARE_HI,999>>8);wr(b,Reg::T0_CTRL,3);b.advance(499);ck(!(rd(b,Reg::IRQ_FLAGS)&1),"timer before500");b.advance(1);ck(rd(b,Reg::IRQ_FLAGS)&1,"2MHz means500us");}
void adc_frequency(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,2,8,2,8);b.advance(250);wr(b,Reg::ADC_CTRL,3);b.advance(25);ck(rd(b,Reg::ADC_STATUS)&1,"ADC not done25us");b.advance(1);ck(rd(b,Reg::ADC_STATUS)&2,"ADC done26us at2MHz");}
void mux_independent(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b,2,8,2,8);b.advance(250);b.buttons().set_bounce(false);b.buttons().press_speed(2,b.now());wr(b,Reg::GPIOA_DIR,7);b.advance(3);wr(b,Reg::GPIOA_OUT,1);b.advance(1);ck(rd(b,Reg::GPIOA_IN)&8,"mux still2us");b.advance(1);ck(!(rd(b,Reg::GPIOA_IN)&8),"mux settled");}
void frc_error(){Board b(b8::sim::BoardProfile::clocked03);b.mcu().clocks().set_frc_ppm(-50000);boot(b);near(b.mcu().clocks().peripheral_hz(),950000,.01,"internal RC tolerance");}
void wdt_timeout(){Board b(b8::sim::BoardProfile::clocked03);boot(b);feed(b);wr(b,Reg::IRQ_GLOBAL,0);auto s=b.mcu().reset_serial();b.advance(204700);ck(b.mcu().reset_serial()==s,"not early");b.advance(200);ck(b.mcu().reset_serial()>s,"WDT timeout masked IRQ");ck(rd(b,Reg::RST_CAUSE)&4,"WDT cause");}
void wdt_good_feed(){Board b(b8::sim::BoardProfile::clocked03);boot(b);auto s=b.mcu().reset_serial();for(int i=0;i<10;++i){feed(b);b.advance(40000);}ck(b.mcu().reset_serial()==s,"valid feed");}
void wdt_first_key_only(){Board b(b8::sim::BoardProfile::clocked03);boot(b);feed(b);auto s=b.mcu().reset_serial();b.advance(180000);wr(b,Reg::WDT_SERVICE,0xA5);b.advance(25000);ck(b.mcu().reset_serial()>s,"first key cannot feed");}
void wdt_bad_key(){Board b(b8::sim::BoardProfile::clocked03);boot(b);auto s=b.mcu().reset_serial();wr(b,Reg::WDT_SERVICE,0);ck(b.mcu().reset_serial()>s,"bad feed reset");ck(rd(b,Reg::RST_DETAIL)&1,"bad key detail");}
void wdt_key_timeout(){Board b(b8::sim::BoardProfile::clocked03);boot(b);wr(b,Reg::WDT_SERVICE,0xA5);b.advance(1700);wr(b,Reg::WDT_SERVICE,0x5A);ck(rd(b,Reg::RST_CAUSE)&4,"late second key");}
void wdt_lock(){Board b(b8::sim::BoardProfile::clocked03);boot(b);wr(b,Reg::WDT_CTRL,128);wr(b,Reg::WDT_SCALE,5);ck(rd(b,Reg::WDT_SCALE)==3,"locked prescale");wr(b,Reg::WDT_CTRL,0);ck(rd(b,Reg::WDT_CTRL)==129,"cannot disable fuse/lock");ck(rd(b,Reg::WDT_STATUS)&128,"config error");}
void lfrc_fast(){Board b(b8::sim::BoardProfile::clocked03);b.mcu().set_lfrc_ppm(100000);boot(b);feed(b);auto s=b.mcu().reset_serial();b.advance(186300);ck(b.mcu().reset_serial()>s,"fast LFRC deadline");}
void lfrc_slow(){Board b(b8::sim::BoardProfile::clocked03);b.mcu().set_lfrc_ppm(-100000);boot(b);feed(b);auto s=b.mcu().reset_serial();b.advance(220000);ck(b.mcu().reset_serial()==s,"slow not nominal");b.advance(8000);ck(b.mcu().reset_serial()>s,"slow deadline");}
void cause_latches(){Board b(b8::sim::BoardProfile::clocked03);boot(b);wr(b,Reg::WDT_SERVICE,0);ck(rd(b,Reg::RST_CAUSE)==5,"POR+WDT retained");wr(b,Reg::RST_CAUSE,1);ck(rd(b,Reg::RST_CAUSE)==4,"W1C select");b.advance(1000);wr(b,Reg::SW_RESET,0xB6);ck(rd(b,Reg::RST_CAUSE)==36,"software cause accumulates");}
void dmt_good(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);b.advance(20000);ck(rd(b,Reg::DMT_STATUS)&1,"window open");wr(b,Reg::DMT_PRECLR,0x69);wr(b,Reg::DMT_CLR,0x96);ck(count(b)==0,"DMT cleared");}
void dmt_early(){Board b(b8::sim::BoardProfile::clocked03);boot(b);dmt(b);wr(b,Reg::DMT_PRECLR,0x69);ck(rd(b,Reg::RST_CAUSE)&8,"early DMT reset");ck(rd(b,Reg::RST_DETAIL)&8,"early detail");}
void dmt_bad1(){Board b(b8::sim::BoardProfile::clocked03);boot(b);dmt(b);b.advance(70000);wr(b,Reg::DMT_PRECLR,0);ck(rd(b,Reg::RST_DETAIL)&2,"bad first key");}
void dmt_bad2(){Board b(b8::sim::BoardProfile::clocked03);boot(b);dmt(b);b.advance(70000);wr(b,Reg::DMT_CLR,0x96);ck(rd(b,Reg::RST_DETAIL)&4,"second key without first");}
void dmt_expire(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);feed(b);b.advance(66000);ck(rd(b,Reg::RST_CAUSE)&8,"DMT timeout");}
void dmt_window_invalid(){Board b(b8::sim::BoardProfile::clocked03);boot(b);dmt(b,100,100);ck(!(rd(b,Reg::DMT_CTRL)&1),"bad config not enabled");ck(rd(b,Reg::DMT_STATUS)&128,"bad window");}
void dmt_cannot_refresh_ctrl(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);b.advance(20000);auto c=count(b);wr(b,Reg::DMT_CTRL,129);ck(count(b)==c,"CTRL no refresh");}
void dmt_clock_scale(){Board a(b8::sim::BoardProfile::clocked03);boot(a);dmt(a);a.advance(10000);auto slow=count(a);Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);b.advance(10000);auto fast=count(b);ck(fast>=slow*4-3 && fast<=slow*4+3,"DMT scales with SYS notPB");}
void blind_feed_does_not_feed_dmt(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);for(int i=0;i<8;++i){feed(b);b.advance(10000);}ck(rd(b,Reg::RST_CAUSE)&8,"WDT happy, DMT still expires");}
void core_halt(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);b.mcu().halt_core(true);auto c=count(b);feed(b);b.advance(100000);ck(count(b)==c,"DMT paused core halt");b.advance(110000);ck(rd(b,Reg::RST_CAUSE)&4,"independent WDT still runs");}
void clock_fail(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);drive(b);b.advance(1000);b.oscillator().set_failed(true);b.advance(1000);ck(rd(b,Reg::RST_CAUSE)&16,"clock fail reset");ck(!b.drive_enabled(),"clock failure disables drive");ck(rd(b,Reg::CLK_ACTIVE)==0,"fallback internal");}
void missing_xo_boot(){Board b(b8::sim::BoardProfile::clocked03);b.oscillator().set_failed(true);boot(b);ck(!(rd(b,Reg::CLK_STATUS)&1),"missingXO notready");plan(b);ck(rd(b,Reg::CLK_STATUS)&8,"reject absent source");ck(rd(b,Reg::CLK_ACTIVE)==0,"remainsFRC");}
void warm_reset_retention(){Board b(b8::sim::BoardProfile::clocked03);boot(b);drive(b);b.advance(50000);auto rpm=b.motor().debug_rpm();b.lcd().bus_write(b.now(),0,0);b.lcd().bus_write(b.now(),1,0x81);wr(b,Reg::SW_RESET,0xB6);ck(b.lcd().debug_vram()[0]==0x81,"warm reset not LCD reset");ck(b.buttons().ideal_mask()==1,"mechanical retention");ck(b.motor().debug_rpm()==rpm,"momentum retained");ck(!b.drive_enabled(),"outputs reset");}
void logic_brownout(){Board b(b8::sim::BoardProfile::clocked03);boot(b);drive(b);b.advance(1000);b.power_domain().set_voltage_override(2.8);b.advance(1);ck(!b.drive_enabled()&&!b.ready(),"brown immediate inhibit");ck(rd(b,Reg::RST_CAUSE)&2,"BOR cause");b.power_domain().set_voltage_override(3.3);b.advance(9999);ck(!b.ready(),"stable interval");b.advance(1002);ck(b.ready(),"recovery reset hold");}
void fuse_motor(){Board b(b8::sim::BoardProfile::clocked03);boot(b);drive(b);b.advance(1000);b.power_domain().set_fuse(b8::sim::Fuse::motor,false);b.advance(1);ck(b.ready()&&!b.drive_enabled(),"motor fuse only branch");}
void fuse_logic(){Board b(b8::sim::BoardProfile::clocked03);boot(b);drive(b);b.power_domain().set_fuse(b8::sim::Fuse::logic,false);b.advance(1);ck(!b.ready()&&!b.drive_enabled(),"logic fuse gate");}
void fuse_input(){Board b(b8::sim::BoardProfile::clocked03);boot(b);b.power_domain().set_fuse(b8::sim::Fuse::input,false);b.advance(1);ck(!b.ready()&&!b.power_domain().motor_supply(),"input fuse allbranches");}
void rear_switch(){Board b(b8::sim::BoardProfile::clocked03);boot(b);drive(b);b.power(false);ck(!b.drive_enabled(),"switch off synchronous inhibit");b.advance(10000);b.power(true);b.advance(5000);ck(!b.ready(),"switch bounce notready");b.advance(40000);ck(b.ready()&&!b.drive_enabled(),"reboot no drive request");}
void dmt_duplicate_first(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);b.advance(20000);wr(b,Reg::DMT_PRECLR,0x69);wr(b,Reg::DMT_PRECLR,0x69);ck(rd(b,Reg::RST_DETAIL)&2,"duplicate preclear");}
void dmt_late_second(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(250);dmt(b);b.advance(20000);wr(b,Reg::DMT_PRECLR,0x69);b.advance(1200);wr(b,Reg::DMT_CLR,0x96);ck(rd(b,Reg::RST_DETAIL)&4,"late clear");}
void pending_clock_lost(){Board b(b8::sim::BoardProfile::clocked03);boot(b);plan(b);b.advance(100);b.oscillator().set_failed(true);b.advance(1);ck(rd(b,Reg::CLK_ACTIVE)==0,"pending source never selected");ck(rd(b,Reg::CLK_STATUS)&8,"abandoned lock");}
void generic_ec_source(){b8::sim::ClockNet net{12000000,true};b8::sim::ClockTree clock(&net);for(int i=0;i<100;++i)clock.advance_one_us(0);clock.write(0,1,true);clock.write(4,4,true);clock.write(7,0xC3,true);clock.write(7,0x3C,true);clock.write(8,0xA5,true);near(clock.peripheral_hz(),3000000,.1,"generic12MHz EC");}
unsigned boots=0,steps=0;
void reboot(){++boots;steps=0;}
void step(){++steps;b8::idle();}
void runner_reset(){Board b(b8::sim::BoardProfile::clocked03);const b8::VectorTable v{};b8::sim::Runtime rt(b,v);boots=0;rt.run_for(500000,reboot,step);ck(boots>=3,"runtime restarts firmware reset entry");ck(!b.drive_enabled(),"safe rebooted stub");}
void reset_isr(){b8::write8(Reg::SW_RESET,0xB6);}
void isr_reset_unwind(){Board b(b8::sim::BoardProfile::clocked03);boot(b);const b8::VectorTable v{reset_isr,nullptr,nullptr,nullptr,nullptr};b8::sim::Runtime rt(b,v);wr(b,Reg::T0_CTRL,1);wr(b,Reg::IRQ_ENABLE,1);wr(b,Reg::IRQ_GLOBAL,1);b.advance(1);bool caught=false;try{rt.dispatch();}catch(const b8::sim::FirmwareReset&){caught=true;}ck(caught,"reset unwindsISR");ck(rd(b,Reg::IRQ_GLOBAL)==0,"oldISR cannot restoreGIE");}
}
int main(int argc,char**argv){const std::vector<std::pair<std::string,std::function<void()>>> cases={
#define C(x) {#x,x}
 C(dmt_duplicate_first),C(dmt_late_second),C(pending_clock_lost),C(generic_ec_source),C(startup),C(oscillator_startup),C(oscillator_ppm),C(pll_lock),C(invalid_ref),C(invalid_vco),C(invalid_pb),C(invalid_code),C(key_sequence),C(change_busy_timer),C(change_busy_adc),C(timer_frequency),C(adc_frequency),C(mux_independent),C(frc_error),C(wdt_timeout),C(wdt_good_feed),C(wdt_first_key_only),C(wdt_bad_key),C(wdt_key_timeout),C(wdt_lock),C(lfrc_fast),C(lfrc_slow),C(cause_latches),C(dmt_good),C(dmt_early),C(dmt_bad1),C(dmt_bad2),C(dmt_expire),C(dmt_window_invalid),C(dmt_cannot_refresh_ctrl),C(dmt_clock_scale),C(blind_feed_does_not_feed_dmt),C(core_halt),C(clock_fail),C(missing_xo_boot),C(warm_reset_retention),C(logic_brownout),C(fuse_motor),C(fuse_logic),C(fuse_input),C(rear_switch),C(runner_reset),C(isr_reset_unwind)
#undef C
};unsigned ran=0;for(const auto&[name,fn]:cases){if(argc>1 && name!=argv[1])continue;try{fn();++ran;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';return 1;}}return ran?0:2;}
