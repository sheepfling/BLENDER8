#include "blender8/sim/runtime.hpp"
#include <limits>
namespace b8::sim {
Runtime::Runtime(Board& board,std::span<const Isr> vectors):board_(board),vectors_(vectors),binding_(*this){}
void Runtime::budget(){
    if(in_callback_ && ++services_>service_budget_){board_.emergency_inhibit();throw FirmwareBudgetExceeded{};}
}
void Runtime::await_ready(){
    if(board_.ready())return;
    if(!in_callback_)throw std::logic_error("B8 access while controller unavailable");
    const auto serial=board_.mcu().reset_serial();
    while(!board_.ready()){
        if(board_.failed())throw std::runtime_error("board host-failure inhibit");
        board_.advance(1);
        if(board_.mcu().reset_serial()!=serial)throw FirmwareReset{};
    }
    // A short clock interruption or externally released core halt is NOT a reset.
}
void Runtime::dispatch(){
    if(in_isr_||!board_.ready())return;
    const auto irq=board_.mcu().pending_irq();if(!irq)return;
    const auto index=static_cast<unsigned>(*irq);
    if(index>=vectors_.size())throw std::logic_error("missing interrupt vector");
    const auto fn=vectors_[index];
    if(!fn)throw std::logic_error("unhandled interrupt");
    const auto serial=board_.mcu().reset_serial();
    in_isr_=true;board_.mcu().set_global(false);board_.mcu().note_irq_delivery(*irq);
    try{fn();}catch(...){
        if(board_.mcu().reset_serial()==serial)board_.mcu().set_global(true);
        in_isr_=false;throw;
    }
    in_isr_=false;
    if(board_.mcu().reset_serial()!=serial)throw FirmwareReset{};
    board_.mcu().set_global(true);
}
std::uint8_t Runtime::read8(Reg reg){
    budget();
    await_ready();
    const auto serial=board_.mcu().reset_serial();
    const auto result=board_.mcu().read8(reg,board_.now());
    if(trace_)trace_({board_.now(),'R',reg,result,serial});
    board_.advance(1);
    if(serial!=board_.mcu().reset_serial())throw FirmwareReset{};
    dispatch();return result;
}
void Runtime::write8(Reg reg,std::uint8_t value){
    budget();
    await_ready();
    const auto serial=board_.mcu().reset_serial();
    if(trace_)trace_({board_.now(),'W',reg,value,serial});
    board_.mcu().write8(reg,value,board_.now());board_.settle();board_.advance(1);
    if(serial!=board_.mcu().reset_serial())throw FirmwareReset{};
    dispatch();
}
void Runtime::idle(){
    if(in_callback_)await_ready();
    budget();const auto serial=board_.mcu().reset_serial();board_.advance(1);
    if(serial!=board_.mcu().reset_serial())throw FirmwareReset{};
    dispatch();
}
void Runtime::run_for(Tick duration,void (*reset_fn)(),void (*step_fn)()){
    if(!reset_fn||!step_fn||running_)throw std::invalid_argument("invalid or reentrant firmware run");
    if(reset_fn_ && (reset_fn_!=reset_fn||step_fn_!=step_fn))throw std::logic_error("cannot change firmware callbacks in a live runtime");
    if(duration>std::numeric_limits<Tick>::max()-board_.now())throw std::overflow_error("run duration");
    reset_fn_=reset_fn;step_fn_=step_fn;const auto until=board_.now()+duration;
    running_=true;
    try {
        while(board_.now()<until){
            if(board_.failed())throw std::runtime_error("board latched in host-failure inhibit");
            if(!board_.ready()){board_.advance(1);continue;}
            try {
                in_callback_=true;services_=0;
                if(seen_reset_!=board_.mcu().reset_serial()){
                    seen_reset_=board_.mcu().reset_serial();reset_fn();
                }else if(board_.foreground_enabled())step_fn();
                idle();in_callback_=false;
            }catch(const FirmwareReset&){in_callback_=false;seen_reset_=0;}
        }
        running_=false;
    }catch(...){in_callback_=false;running_=false;board_.emergency_inhibit();throw;}
}
}
