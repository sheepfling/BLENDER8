#include "firmware.hpp"
#include "blender8/sim/runtime.hpp"
#include <exception>
#include <iostream>
int main() {
    try {
        #if defined(B8_TARGET_B16)
        b8::sim::Board board(b8::sim::BoardProfile::chassis04,{},b8::sim::DeviceProfile::b16);
#else
        b8::sim::Board board;
#endif
        b8::sim::Runtime runtime(board,firmware::vectors);
        runtime.run_for(100000,firmware::reset,firmware::step);
        std::cout<<"Starter ran for "<<board.now()<<" us; motor RPM="<<board.motor().debug_rpm()
                 <<". Firmware functionality remains the student's assignment.\n";
        return board.motor().debug_rpm()==0?0:1;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 2; }
}
