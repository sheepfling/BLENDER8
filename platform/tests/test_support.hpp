#pragma once
#include "blender8/sim/board.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
namespace b8::test {
inline void require(bool condition,const std::string& why){if(!condition)throw std::runtime_error(why);}
inline void near(double a,double b,double epsilon,const std::string& why){require(std::isfinite(a)&&std::abs(a-b)<=epsilon,why);}
template<class F> inline void rejects(F f,const std::string& why){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,why);}
inline void write(sim::Board& b,Reg r,std::uint8_t v){b.mcu().write8(r,v,b.now());b.settle();}
inline void service(sim::Board& b){if(b.ready()){write(b,Reg::WDT_SERVICE,0xA5);write(b,Reg::WDT_SERVICE,0x5A);}}
inline void advance_serviced(sim::Board& b,sim::Tick duration){
    while(duration){const auto chunk=std::min<sim::Tick>(duration,100000);service(b);b.advance(chunk);duration-=chunk;}
}
inline void boot(sim::Board& b){b.buttons().set_bounce(false);b.jar().set_bounce(false);b.advance(60000);require(b.ready(),"board must boot");}
inline void arm(sim::Board& b){b.buttons().stop(true,b.now());b.settle();b.buttons().stop(false,b.now());b.settle();require(b.jar().permitted(),"fresh STOP arms jar latch");}
inline void drive(sim::Board& b,std::uint8_t duty=255){b.buttons().press_speed(1,b.now());write(b,Reg::GPIOB_DIR,1);write(b,Reg::GPIOB_OUT,1);write(b,Reg::PWM_DUTY,duty);write(b,Reg::PWM_CTRL,1);}
}
