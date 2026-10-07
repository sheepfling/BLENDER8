#include "blender8/sim/board.hpp"
#include <iostream>
#include <exception>
int main() {
 try {
  b8::sim::Board board(b8::sim::BoardProfile::clocked03);
  while (!board.ready() && board.now()<100000) board.advance(1);
  std::cout << "B8 reset released at world_us=" << board.now() << "\n";
  while (!(board.mcu().read8(b8::Reg::CLK_STATUS,board.now())&1) && board.now()<100000) board.advance(1);
  std::cout << "external qualified at world_us=" << board.now() << "\n";
  std::cout << "boot SYS_Hz=" << board.mcu().clocks().system_hz() << " PB_Hz=" << board.mcu().clocks().peripheral_hz() << "\n";
  const auto serial=board.mcu().reset_serial();
  while(board.mcu().reset_serial()==serial && board.now()<400000) board.advance(1);
  std::cout << "unserviced watchdog reset at world_us=" << board.now() << " cause=" << unsigned(board.mcu().read8(b8::Reg::RST_CAUSE,board.now())) << " drive=" << board.drive_enabled() << "\n";
  return board.mcu().reset_serial()>serial ? 0 : 1;
 } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
