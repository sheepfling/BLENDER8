#include "blender8/sim/runtime.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using b8::Reg;using b8::sim::Board;
namespace {
void check(bool x,const char* message){if(!x)throw std::runtime_error(message);}
void close(double a,double b,double t,const char* m){if(std::abs(a-b)>t)throw std::runtime_error(std::string(m)+": "+std::to_string(a)+" vs "+std::to_string(b));}
template<class F> void throws(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"expected exception");}
void wr(Board& b,Reg r,std::uint8_t d){b.mcu().write8(r,d,b.now());b.settle();}
std::uint8_t rd(Board& b,Reg r){return b.mcu().read8(r,b.now());}
void timer0(Board& b,std::uint16_t c,std::uint8_t scale=0,bool periodic=true){wr(b,Reg::T0_CTRL,0);wr(b,Reg::T0_PRESCALE,scale);wr(b,Reg::T0_COMPARE_LO,static_cast<std::uint8_t>(c));wr(b,Reg::T0_COMPARE_HI,static_cast<std::uint8_t>(c>>8));wr(b,Reg::T0_CTRL,periodic?3:1);}
void run_motor(Board& b,std::uint8_t code=160){b.buttons().set_bounce(false);b.buttons().press_speed(3,b.now());wr(b,Reg::GPIOB_DIR,1);wr(b,Reg::GPIOB_OUT,1);wr(b,Reg::PWM_DUTY,code);wr(b,Reg::PWM_CTRL,1);}
unsigned calls=0;std::vector<unsigned> order;
void timer_isr(){++calls;b8::write8(Reg::IRQ_FLAGS,1);}
void zero_isr(){order.push_back(0);b8::write8(Reg::IRQ_FLAGS,1);}
void one_isr(){order.push_back(1);b8::write8(Reg::IRQ_FLAGS,2);}
void reset(){Board b(b8::sim::BoardProfile::legacy02);check(rd(b,Reg::SYS_ID)==0xB8,"ID");check(rd(b,Reg::IRQ_GLOBAL)==0,"IRQ reset");check(rd(b,Reg::PWM_CTRL)==0,"PWM reset");check(!b.drive_enabled(),"drive reset");}

void registers(){Board b(b8::sim::BoardProfile::legacy02);throws([&]{wr(b,Reg::SYS_ID,0);});throws([&]{(void)rd(b,static_cast<Reg>(0x7777));});wr(b,Reg::IRQ_ENABLE,255);check(rd(b,Reg::IRQ_ENABLE)==31,"reserved bits");}

void gpio_contention(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::GPIOA_DIR,8);wr(b,Reg::GPIOA_OUT,0);throws([&]{(void)rd(b,Reg::GPIOA_IN);});}

void mux_select(){Board b(b8::sim::BoardProfile::legacy02);b.buttons().set_bounce(false);b.buttons().press_speed(6,b.now());wr(b,Reg::GPIOA_DIR,7);for(unsigned i=0;i<8;++i) {wr(b,Reg::GPIOA_OUT,static_cast<std::uint8_t>(i));b.advance(3);check(((rd(b,Reg::GPIOA_IN)&8)==0)==(i==5),"mux channel mapping");}}

void mux_settle(){Board b(b8::sim::BoardProfile::legacy02);b.buttons().set_bounce(false);b.buttons().press_speed(2,b.now());wr(b,Reg::GPIOA_DIR,7);b.advance(3);wr(b,Reg::GPIOA_OUT,1);check((rd(b,Reg::GPIOA_IN)&8)!=0,"old level before settling");b.advance(1);check((rd(b,Reg::GPIOA_IN)&8)!=0,"old at 1us");b.advance(1);check((rd(b,Reg::GPIOA_IN)&8)==0,"new at 2us");}

void button_interlock(){Board b(b8::sim::BoardProfile::legacy02);b.buttons().press_speed(1,0);check(b.buttons().ideal_mask()==1,"speed1");b.buttons().press_speed(7,0);check(b.buttons().ideal_mask()==64,"one-hot latch");throws([&]{b.buttons().press_speed(8,0);});}

void button_bounce(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::GPIOA_DIR,7);b.buttons().press_speed(1,b.now());b.settle();b.advance(2);check((rd(b,Reg::GPIOA_IN)&8)==0,"first make");b.advance(100);check((rd(b,Reg::GPIOA_IN)&8)!=0,"bounce opens");b.advance(1800);check((rd(b,Reg::GPIOA_IN)&8)==0,"settled make");b.buttons().stop(true,b.now());b.settle();b.advance(2);check((rd(b,Reg::GPIOA_IN)&8)!=0,"break");b.advance(100);check((rd(b,Reg::GPIOA_IN)&8)==0,"release bounce");b.advance(1800);check((rd(b,Reg::GPIOA_IN)&8)!=0,"settled release");}

void pulse_stop(){Board b(b8::sim::BoardProfile::legacy02);b.buttons().press_speed(2,0);b.buttons().pulse(true,0);check(b.buttons().ideal_mask()==130,"pulse retains speed");b.buttons().stop(true,0);check(b.buttons().ideal_mask()==0,"stop releases");b.buttons().press_speed(4,0);check(b.buttons().ideal_mask()==0,"stop dominant");b.buttons().stop(false,0);b.buttons().pulse(true,0);check(b.buttons().ideal_mask()==0,"held pulse blocked");b.buttons().pulse(false,0);b.buttons().pulse(true,0);check(b.buttons().ideal_mask()==128,"fresh pulse");}

void stop_gate(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(500000);check(b.drive_enabled(),"drive on");b.buttons().stop(true,b.now());b.settle();check(!b.drive_enabled(),"stop gate without firmware");check(rd(b,Reg::PWM_DUTY)==255,"stale command retained");b.buttons().stop(false,b.now());b.settle();check(!b.drive_enabled(),"no restart on stop release");}

void power_retention(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(200000);const auto rpm=b.motor().debug_rpm();b.power(false);check(!b.drive_enabled(),"power gate");check(b.buttons().ideal_mask()==4,"latch retained");b.advance(1000);check(b.motor().debug_rpm()<rpm && b.motor().debug_rpm()>0,"rotor coasts");b.power(true);check(rd(b,Reg::PWM_CTRL)==0 && !b.drive_enabled(),"safe reset");}

void lcd_pixels(){Board b(b8::sim::BoardProfile::legacy02);auto& l=b.lcd();l.bus_write(0,3,3);l.bus_write(0,0,0);l.bus_write(0,1,0x81);b.advance(40000);check(l.pixel(0,0)&&l.pixel(0,7),"vertical byte packing");check(!l.pixel(0,1)&&!l.pixel(1,0),"no extra pixels");}

void lcd_busy(){Board b(b8::sim::BoardProfile::legacy02);auto& l=b.lcd();l.bus_write(0,1,0x11);l.bus_write(1,1,0x22);check((l.bus_read(1,2)&5)==5,"busy and error");check(l.bus_read(1,0)==1,"dropped byte does not increment");l.bus_write(8,1,0x33);check(l.debug_vram()[1]==0x33,"expiry accepted");l.bus_write(8,2,4);check((l.bus_read(8,2)&4)==0,"W1C");}

void lcd_wrap(){Board b(b8::sim::BoardProfile::legacy02);auto& l=b.lcd();l.bus_write(0,0,63);l.bus_write(0,1,0xAA);check(l.bus_read(0,0)==0,"wrap");l.bus_write(8,1,0x55);check(l.debug_vram()[0]==0x55,"wrapped data");}

void lcd_vblank(){Board b(b8::sim::BoardProfile::legacy02);b.advance(15999);check(!b.lcd().vblank(),"active");b.advance(1);check(b.lcd().vblank(),"blank at16ms");check((rd(b,Reg::IRQ_FLAGS)&8)!=0,"vblank IRQ");b.advance(4000);check(!b.lcd().vblank(),"new frame");}

void timer_period(){Board b(b8::sim::BoardProfile::legacy02);timer0(b,999);b.advance(999);check((rd(b,Reg::IRQ_FLAGS)&1)==0,"not early");b.advance(1);check((rd(b,Reg::IRQ_FLAGS)&1)!=0,"1000us event");check(rd(b,Reg::T0_COUNT_LO)==0,"reload");}

void timer_prescale(){Board b(b8::sim::BoardProfile::legacy02);timer0(b,124,1);b.advance(999);check((rd(b,Reg::IRQ_FLAGS)&1)==0,"not early");b.advance(1);check((rd(b,Reg::IRQ_FLAGS)&1)!=0,"8*(124+1)");throws([&]{wr(b,Reg::T0_PRESCALE,2);});}

void timer_latch(){Board b(b8::sim::BoardProfile::legacy02);timer0(b,65535);b.advance(255);const auto low=rd(b,Reg::T0_COUNT_LO);b.advance(1);check(rd(b,Reg::T0_COUNT_HI)==0 && low==255,"coherent snapshot");check(rd(b,Reg::T0_COUNT_LO)==0,"new low");check(rd(b,Reg::T0_COUNT_HI)==1,"new high");}

void timer_write_order(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::T0_COMPARE_LO,0x34);check(rd(b,Reg::T0_COMPARE_LO)==0,"staged low");wr(b,Reg::T0_COMPARE_HI,0x12);check(rd(b,Reg::T0_COMPARE_LO)==0x34 && rd(b,Reg::T0_COMPARE_HI)==0x12,"commit pair");}

void timer_one_shot(){Board b(b8::sim::BoardProfile::legacy02);timer0(b,9,0,false);b.advance(10);check(rd(b,Reg::T0_CTRL)==0,"one shot disable");wr(b,Reg::IRQ_FLAGS,1);b.advance(100);check((rd(b,Reg::IRQ_FLAGS)&1)==0,"no repeats");}

void irq_w1c(){Board b(b8::sim::BoardProfile::legacy02);timer0(b,9);b.advance(20);wr(b,Reg::IRQ_FLAGS,0);check((rd(b,Reg::IRQ_FLAGS)&1)!=0,"zero not clear");wr(b,Reg::IRQ_FLAGS,2);check((rd(b,Reg::IRQ_FLAGS)&1)!=0,"other bit not clear");wr(b,Reg::IRQ_FLAGS,1);check((rd(b,Reg::IRQ_FLAGS)&1)==0,"one clears");}

void irq_mask(){Board b(b8::sim::BoardProfile::legacy02);timer0(b,9);b.advance(20);check(!b.mcu().pending_irq(),"global off");wr(b,Reg::IRQ_GLOBAL,1);check(!b.mcu().pending_irq(),"source off");wr(b,Reg::IRQ_ENABLE,1);check(b.mcu().pending_irq()==b8::Irq::timer0,"pending retained");}

void irq_dispatch(){Board b(b8::sim::BoardProfile::legacy02);calls=0;const b8::VectorTable v{timer_isr,nullptr,nullptr,nullptr};b8::sim::Runtime rt(b,v);timer0(b,99);wr(b,Reg::IRQ_ENABLE,1);wr(b,Reg::IRQ_GLOBAL,1);b.advance(100);rt.dispatch();check(calls==1,"ISR once");check((rd(b,Reg::IRQ_FLAGS)&1)==0,"acknowledged");check(rd(b,Reg::IRQ_GLOBAL)==1,"GIE restored");}

void irq_priority(){Board b(b8::sim::BoardProfile::legacy02);order.clear();const b8::VectorTable v{zero_isr,one_isr,nullptr,nullptr};b8::sim::Runtime rt(b,v);timer0(b,99);wr(b,Reg::T1_COMPARE_LO,99);wr(b,Reg::T1_COMPARE_HI,0);wr(b,Reg::T1_CTRL,3);wr(b,Reg::IRQ_ENABLE,3);wr(b,Reg::IRQ_GLOBAL,1);b.advance(100);rt.dispatch();rt.dispatch();check(order==std::vector<unsigned>{0,1},"priority no nesting");}

void pwm_endpoints(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,0);b.advance(1000);close(b.motor().debug_duty(),0,0,"zero");wr(b,Reg::PWM_DUTY,255);b.advance(1000);close(b.motor().debug_duty(),1,0,"full");wr(b,Reg::PWM_DUTY,128);b.advance(1000);close(b.motor().debug_duty(),.5,0,"half");}

void pwm_latch(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,20);b.advance(1);check(b.mcu().debug_active_duty()==20,"first latch");wr(b,Reg::PWM_DUTY,200);b.advance(200);check(b.mcu().debug_active_duty()==20,"deferred write");b.advance(56);check(b.mcu().debug_active_duty()==200,"boundary latch");}

void motor_curve(){using b8::sim::Motor;close(Motor::steady_rpm(.12,0),0,0,"dead zone");close(Motor::steady_rpm(1,0),20000,1e-9,"rated");const double duty=.12+.88*std::pow(.5,1.0/1.6);close(Motor::steady_rpm(duty,0),10000,1e-8,"inverse");check(Motor::steady_rpm(.5,0)<10000,"nonlinear");throws([]{(void)Motor::steady_rpm(NAN,0);});}

void motor_load(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(1000000);close(b.motor().debug_rpm(),20000,10,"no load");b.motor().set_load(1);b.advance(1000000);close(b.motor().debug_rpm(),10000,10,"sag");}

void motor_coast(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(1000000);const auto rpm=b.motor().debug_rpm();b.buttons().stop(true,b.now());b.settle();b.advance(350000);close(b.motor().debug_rpm(),rpm/std::exp(1.0),.1,"coast tau");}

void tach_count(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(2000000);const auto count=static_cast<unsigned>(rd(b,Reg::TACH_COUNT_LO))|(static_cast<unsigned>(rd(b,Reg::TACH_COUNT_HI))<<8);check(count>1200 && count<1350,"2 rising pulses/rev");check((rd(b,Reg::IRQ_FLAGS)&4)!=0,"tach IRQ");}

void wiring(){Board b(b8::sim::BoardProfile::legacy02);b.buttons().set_bounce(false);b.buttons().press_speed(5,b.now());const b8::VectorTable v{};b8::sim::Runtime rt(b,v);b8::write8(Reg::GPIOA_DIR,7);b8::write8(Reg::GPIOA_OUT,4);b8::idle();b8::idle();check((b8::read8(Reg::GPIOA_IN)&8)==0,"GPIO-mux-input");b8::write8(Reg::XBUS_REG,0);b8::write8(Reg::XBUS_DATA,17);b8::write8(Reg::XBUS_REG,1);b8::write8(Reg::XBUS_DATA,0x81);check(b.lcd().debug_vram()[17]==0x81,"external bus to VRAM");}

void adc_reset(){Board b(b8::sim::BoardProfile::legacy02);check(rd(b,Reg::ADC_CTRL)==0 && rd(b,Reg::ADC_STATUS)==0,"ADC reset");check(rd(b,Reg::ADC_PRESCALE)==2,"div4 default");}
unsigned adc_result(Board& b){return unsigned(rd(b,Reg::ADC_DATA_LO))|(unsigned(rd(b,Reg::ADC_DATA_HI))<<8);}
void adc_conversion(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_CTRL,3);check(rd(b,Reg::ADC_CTRL)==1,"START self-clears");b.advance(51);check(rd(b,Reg::ADC_STATUS)==1,"busy until52us");b.advance(1);check((rd(b,Reg::ADC_STATUS)&3)==2,"ready at52us");check(adc_result(b)==233,"25 C ADC code");check((rd(b,Reg::ADC_STATUS)&2)==0,"low read acks READY");}
void adc_scalers(){for(unsigned s=0;s<4;++s){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_PRESCALE,static_cast<std::uint8_t>(s));wr(b,Reg::ADC_CTRL,3);b.advance((13u<<s)-1);check(rd(b,Reg::ADC_STATUS)&1,"busy before deadline");b.advance(1);check(rd(b,Reg::ADC_STATUS)&2,"complete at deadline");}}
void adc_busy_error(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_CTRL,3);b.advance(10);wr(b,Reg::ADC_CTRL,3);wr(b,Reg::ADC_CHANNEL,1);check(rd(b,Reg::ADC_CHANNEL)==0,"busy channel write ignored");check(rd(b,Reg::ADC_STATUS)&8,"busy write error");b.advance(42);check(adc_result(b)==233,"original conversion preserved");wr(b,Reg::ADC_STATUS,8);check(!(rd(b,Reg::ADC_STATUS)&8),"error W1C");}
void adc_sample_hold(){b8::sim::AnalogNet x(0),y(0);b8::sim::Adc a({&x,&y});x.drive(0.75);a.write(0,3);for(unsigned i=0;i<16;++i)(void)a.advance_clock_tick();x.drive(2.0);for(unsigned i=16;i<52;++i)(void)a.advance_clock_tick();const unsigned value=unsigned(a.read(4))|(unsigned(a.read(5))<<8);check(value==233,"hold at end of acquisition, not end of conversion");}
void adc_result_latch(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_CTRL,3);b.advance(52);const unsigned low=rd(b,Reg::ADC_DATA_LO);b.temperature_sensor().set_fault(b8::sim::TemperatureFault::short_supply);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(rd(b,Reg::ADC_DATA_HI)==0 && low==233,"high remains old snapshot");check(adc_result(b)==1023,"new read sees latest result");}
void adc_overrun(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_CTRL,3);b.advance(52);wr(b,Reg::ADC_CHANNEL,1);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(rd(b,Reg::ADC_STATUS)&4,"unread overwrite");check(adc_result(b)==0,"latest conversion wins");wr(b,Reg::ADC_STATUS,4);check(!(rd(b,Reg::ADC_STATUS)&4),"overrun W1C");}
void adc_channels(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_CHANNEL,1);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(adc_result(b)==0,"AN1 spare ground");wr(b,Reg::ADC_CHANNEL,0);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(adc_result(b)==233,"AN0 sensor");}
void adc_irq(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::IRQ_ENABLE,16);wr(b,Reg::IRQ_GLOBAL,1);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(b.mcu().pending_irq()==b8::Irq::adc,"ADC vector");(void)adc_result(b);check(rd(b,Reg::IRQ_FLAGS)&16,"read does not clear IRQ");wr(b,Reg::IRQ_FLAGS,16);check(!b.mcu().pending_irq(),"IRQ ack");}
void adc_abort(){Board b(b8::sim::BoardProfile::legacy02);wr(b,Reg::ADC_CTRL,3);b.advance(25);wr(b,Reg::ADC_CTRL,0);b.advance(100);check(!(rd(b,Reg::IRQ_FLAGS)&16),"aborted conversion no IRQ");check(!(rd(b,Reg::ADC_STATUS)&3),"aborted conversion not READY");}
void adc_reference(){Board b(b8::sim::BoardProfile::legacy02);b.mcu().adc().set_reference_volts(3.6);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(adc_result(b)==213,"reference error changes conversion");}
void adc_fault_rails(){using F=b8::sim::TemperatureFault;Board b(b8::sim::BoardProfile::legacy02);for(const auto f:{F::open,F::short_ground,F::short_supply}){b.temperature_sensor().set_fault(f);wr(b,Reg::ADC_CTRL,3);b.advance(52);check(adc_result(b)==(f==F::short_ground?0:1023),"open/short rail reading");}}
void sensor_linear(){b8::sim::ThermalNode node{85};b8::sim::AnalogNet wire;b8::sim::TemperatureSensor s(node,wire);close(wire.sample_volts(),1.35,1e-12,"linear sensor output");s.set_calibration(.01,.01);close(wire.sample_volts(),1.3685,1e-12,"offset and slope errors");}
void sensor_lag(){b8::sim::ThermalNode node{25};b8::sim::AnalogNet wire;b8::sim::TemperatureSensor s(node,wire);node.celsius=85;for(unsigned i=0;i<250000;++i)s.advance_one_us();close(s.debug_sensor_c(),85-60/std::exp(1.0),1e-7,"sensor mount time constant");}
void sensor_seed(){b8::sim::ThermalNode n{25};b8::sim::AnalogNet a,b;b8::sim::TemperatureSensor x(n,a),y(n,b);x.set_noise(.002,71);y.set_noise(.002,71);for(unsigned i=0;i<10000;++i){x.advance_one_us();y.advance_one_us();close(a.sample_volts(),b.sample_volts(),0,"seed deterministic");check(std::abs(a.sample_volts()-.75)<=.002,"noise bounded");}}
void sensor_frozen(){b8::sim::ThermalNode n{25};b8::sim::AnalogNet a;b8::sim::TemperatureSensor s(n,a);s.set_fault(b8::sim::TemperatureFault::frozen_output);n.celsius=100;for(unsigned i=0;i<500000;++i)s.advance_one_us();close(a.sample_volts(),.75,0,"plausible frozen voltage: not diagnosable from a single sensor alone");}
void motor_jam(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(500000);b.motor().set_jammed(true);b.advance(500000);check(b.motor().debug_rpm()==0,"jam forces no motion");check(b.motor().debug_energized(),"physics does NOT secretly shut down firmware drive");check(b.motor().thermal().last_heat_w()>100,"jam still heats");}
void tach_no_motion_signature(){Board b(b8::sim::BoardProfile::legacy02);run_motor(b,255);b.advance(300000);b.motor().set_tach_fault(b8::sim::TachFault::stuck_low);b.advance(2);const unsigned before=unsigned(rd(b,Reg::TACH_COUNT_LO))|(unsigned(rd(b,Reg::TACH_COUNT_HI))<<8);b.advance(300000);const unsigned after=unsigned(rd(b,Reg::TACH_COUNT_LO))|(unsigned(rd(b,Reg::TACH_COUNT_HI))<<8);check(before==after,"failed tach yields no edges");check(b.motor().debug_rpm()>10000,"rotor can actually be spinning");}
void thermal_loss(){using T=b8::sim::LumpedThermal;close(T::heat_w(1,20000,20000,true),30,0,"no-load loss");close(T::heat_w(1,0,20000,true),130,0,"stall loss");close(T::heat_w(1,0,20000,false),0,0,"disabled loss");}
void thermal_cooling(){b8::sim::ThermalNode n{100};b8::sim::LumpedThermal t(n);t.advance(100,0,0,0,false);check(n.celsius>25&&n.celsius<100,"case cools toward room");check(t.air_temperature_c()>25&&t.air_temperature_c()<n.celsius,"nearby air receives case heat");}
void thermal_food(){b8::sim::ThermalNode a{70},b{70};b8::sim::LumpedThermal dry(a),cold(b);cold.set_environment(25,0,.15);dry.advance(10,1,0,20000,true);cold.advance(10,1,0,20000,true);check(b.celsius<a.celsius,"cold food path reduces heat");check(b.celsius>70,"cold food does not magically prevent stall heating");}
void thermal_overheat(){b8::sim::ThermalNode n{25};b8::sim::LumpedThermal t(n);t.advance(120,1,10000,20000,true);check(n.celsius>85 && n.celsius<105,"sustained load can overheat without stall");}
void thermal_power_retention(){Board b(b8::sim::BoardProfile::legacy02);b.motor().thermal().set_initial_temperature(80);b.power(false);b.advance(1000000);check(b.motor().thermal().temperature_c()>79,"power cycle does not erase heat");b.power(true);check(b.motor().thermal().temperature_c()>79,"power-on retains heat");check(!b.drive_enabled(),"power-on still cuts drive");}
void stop_sense(){Board b(b8::sim::BoardProfile::legacy02);check(rd(b,Reg::GPIOB_IN)&4,"STOP initially released");b.buttons().stop(true,b.now());b.settle();check(!(rd(b,Reg::GPIOB_IN)&4),"direct active-low STOP input");b.buttons().stop(false,b.now());b.settle();check(rd(b,Reg::GPIOB_IN)&4,"STOP release sensed");}
void slow_bounce(){Board b(b8::sim::BoardProfile::legacy02);b.buttons().set_slow_bounce(true);wr(b,Reg::GPIOA_DIR,7);b.buttons().press_speed(1,0);b.settle();b.advance(3500);check(rd(b,Reg::GPIOA_IN)&8,"still bouncing at3.5ms");b.advance(1502);check(!(rd(b,Reg::GPIOA_IN)&8),"final at5ms plus mux settle");}
void tach_wrap(){Board b(b8::sim::BoardProfile::legacy02);for(unsigned i=0;i<65536+7;++i){b.mcu().latch_inputs(false,false);b.mcu().latch_inputs(true,false);}check(rd(b,Reg::TACH_COUNT_LO)==7 && rd(b,Reg::TACH_COUNT_HI)==0,"16-bit tach wraps modulo65536");}

}
int main(int argc,char** argv){
using Case=std::pair<std::string,std::function<void()>>;
const std::vector<Case> tests{
{"reset",reset},{"registers",registers},{"gpio_contention",gpio_contention},{"mux_select",mux_select},{"mux_settle",mux_settle},{"button_interlock",button_interlock},{"button_bounce",button_bounce},{"pulse_stop",pulse_stop},{"stop_gate",stop_gate},{"power_retention",power_retention},{"lcd_pixels",lcd_pixels},{"lcd_busy",lcd_busy},{"lcd_wrap",lcd_wrap},{"lcd_vblank",lcd_vblank},{"timer_period",timer_period},{"timer_prescale",timer_prescale},{"timer_latch",timer_latch},{"timer_write_order",timer_write_order},{"timer_one_shot",timer_one_shot},{"irq_w1c",irq_w1c},{"irq_mask",irq_mask},{"irq_dispatch",irq_dispatch},{"irq_priority",irq_priority},{"pwm_endpoints",pwm_endpoints},{"pwm_latch",pwm_latch},{"motor_curve",motor_curve},{"motor_load",motor_load},{"motor_coast",motor_coast},{"tach_count",tach_count},{"wiring",wiring},{"adc_reset",adc_reset},{"adc_conversion",adc_conversion},{"adc_scalers",adc_scalers},{"adc_busy_error",adc_busy_error},{"adc_sample_hold",adc_sample_hold},{"adc_result_latch",adc_result_latch},{"adc_overrun",adc_overrun},{"adc_channels",adc_channels},{"adc_irq",adc_irq},{"adc_abort",adc_abort},{"adc_reference",adc_reference},{"adc_fault_rails",adc_fault_rails},{"sensor_linear",sensor_linear},{"sensor_lag",sensor_lag},{"sensor_seed",sensor_seed},{"sensor_frozen",sensor_frozen},{"motor_jam",motor_jam},{"tach_no_motion_signature",tach_no_motion_signature},{"thermal_loss",thermal_loss},{"thermal_cooling",thermal_cooling},{"thermal_food",thermal_food},{"thermal_overheat",thermal_overheat},{"thermal_power_retention",thermal_power_retention},{"stop_sense",stop_sense},{"slow_bounce",slow_bounce},{"tach_wrap",tach_wrap}};
try{if(argc!=2)throw std::invalid_argument("provide test name");for(const auto& [n,f]:tests)if(n==argv[1]){f();std::cout<<"PASS "<<n<<'\n';return 0;}throw std::invalid_argument("unknown test");}
catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
