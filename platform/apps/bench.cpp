#include "blender8/sim/runtime.hpp"
#include <exception>
#include <iomanip>
#include <iostream>
int main() {
    try {
        using namespace b8;
        sim::Board board(b8::sim::BoardProfile::legacy02);const VectorTable no_interrupts{};sim::Runtime rt(board,no_interrupts);
        // Instructor bench stimulus, not a reference blender application.
        board.buttons().set_bounce(false);board.buttons().press_speed(3,board.now());
        write8(Reg::GPIOA_DIR,0x07);write8(Reg::GPIOA_OUT,2);idle();idle();
        std::cout<<"Selected channel 2 active-low read: "<<unsigned(read8(Reg::GPIOA_IN)&8u)<<'\n';
        write8(Reg::GPIOB_DIR,1);write8(Reg::GPIOB_OUT,1);
        write8(Reg::PWM_DUTY,160);write8(Reg::PWM_CTRL,1);
        write8(Reg::XBUS_REG,3);write8(Reg::XBUS_DATA,3); // display on, auto-increment
        write8(Reg::XBUS_REG,0);write8(Reg::XBUS_DATA,0);write8(Reg::XBUS_REG,1);
        for(unsigned x=0;x<32;++x) {
            write8(Reg::XBUS_DATA,static_cast<std::uint8_t>(1u<<(x%8)));
            for(unsigned i=0;i<8;++i) idle();
        }
        board.advance(1000000);
        std::cout<<std::fixed<<std::setprecision(1)<<"No-load RPM at code 160: "<<board.motor().debug_rpm()<<'\n';
        for(unsigned y=0;y<16;++y) {
            for(unsigned x=0;x<32;++x) std::cout<<(board.lcd().pixel(x,y)?"##":"  ");
            std::cout<<'\n';
        }
        board.buttons().stop(true,board.now());board.settle();board.advance(350000);
        std::cout<<"350 ms after STOP: drive="<<board.drive_enabled()<<", coasting RPM="<<board.motor().debug_rpm()<<'\n';
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
