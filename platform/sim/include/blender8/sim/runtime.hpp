#pragma once
#include "blender8/device.hpp"
#include "blender8/host/backend.hpp"
#include "blender8/sim/board.hpp"
#include <exception>
#include <functional>
#include <span>
#include <stdexcept>
namespace b8::sim {
class FirmwareReset final:public std::exception {
public:const char* what() const noexcept override{return "hardware reset";}
};
// This is a HOST cooperative-call limit, never claimed to be a watchdog reset.
class FirmwareBudgetExceeded final:public std::runtime_error {
public:FirmwareBudgetExceeded():std::runtime_error("firmware callback exceeded SDK service budget (host failure, not WDT)"){}
};
struct BusTrace {Tick time_us; char operation; Reg reg; std::uint8_t value; std::uint64_t reset_serial;};
class Runtime : public host::RegisterBackend {
public:
    Runtime(Board& board,std::span<const Isr> vectors);
    ~Runtime() override = default;
    Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
    [[nodiscard]] std::uint8_t read8(Reg address) override;
    void write8(Reg address,std::uint8_t value) override;
    void idle() override;
    void dispatch();
    // Persistent reset-entry state across successive run_for calls: UI chunking is NOT reset.
    // Can overshoot requested duration by one bounded callback, reported by board.now().
    void run_for(Tick duration,void (*reset_fn)(),void (*step_fn)());
    void set_service_budget(Tick count){if(!count)throw std::invalid_argument("zero service budget");service_budget_=count;}
    void set_trace(std::function<void(const BusTrace&)> sink){trace_=std::move(sink);}
private:
    void budget();
    void await_ready();
    Board& board_;std::span<const Isr> vectors_;bool in_isr_=false,in_callback_=false,running_=false;
    std::uint64_t seen_reset_=0;
    Tick service_budget_=150000,services_=0;
    void (*reset_fn_)()=nullptr;void (*step_fn_)()=nullptr;
    std::function<void(const BusTrace&)> trace_;
    host::ScopedBinding binding_;
};
}
