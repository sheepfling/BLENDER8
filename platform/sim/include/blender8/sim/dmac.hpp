#pragma once
#include "blender8/sim/signals.hpp"
#include <array>
namespace b8::sim {
struct DmaObservation {unsigned status,error,done,left;std::uint32_t sram_hash;};
// Interface 04 byte-cell engine. Descriptors are CPU-visible, jobs capture them at START.
class Dmac {
public:
    void reset(bool clear_memory);
    std::uint8_t read(unsigned address);
    void write(unsigned address,std::uint8_t value,std::uint8_t selector,bool locked);
    void tick(BytePeripheral& peripheral,Tick now);
    void request(unsigned trigger);
    void ownership_conflict();
    [[nodiscard]] bool busy() const noexcept {return (status_&1)!=0;}
    [[nodiscard]] bool external_busy() const noexcept {return busy()&&dst_==0x81;}
    [[nodiscard]] bool irq() const noexcept {return (status_&ien_&0x1C)!=0;}
    [[nodiscard]] DmaObservation observe() const noexcept;
private:
    void finish(std::uint8_t cause,std::uint8_t error=0);
    void start(std::uint8_t selector,bool locked);
    std::array<std::uint8_t,1024> memory_{};
    std::array<std::uint16_t,3> descriptor_{};
    std::array<std::uint8_t,3> staged_{};
    std::uint16_t src_=0,dst_=0,done_=0,left_=0;
    std::uint8_t config_=1,trigger_=0,pace_=8,status_=0,ien_=0,error_=0;
    std::uint8_t job_config_=0,job_trigger_=0,job_pace_=8,selector_=0,done_high_=0,left_high_=0;
    unsigned elapsed_=0;
    bool pending_=false,block_=false;
};
}
