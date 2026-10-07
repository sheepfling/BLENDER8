#pragma once
#include "blender8/registers.hpp"
#include "blender8/sim/signals.hpp"
#include "blender8/sim/adc.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include "blender8/sim/clock_tree.hpp"
#include "blender8/sim/supervision.hpp"
#include "blender8/sim/dmac.hpp"
namespace b8::sim {
enum class DeviceProfile { b8, b16 };
struct McuObservation {
    std::uint64_t reset_serial;
    unsigned reset_causes,reset_details,gpioa_dir,gpioa_out,gpiob_dir,gpiob_out;
    unsigned irq_enable,irq_flags,pwm_enabled,pwm_shadow,pwm_active,tach_count;
    unsigned clock_source,clock_status,pb_div,wdt_control,wdt_scale,dmt_enabled,dmt_locked,dmt_limit,dmt_window;
    unsigned adc_status,adc_result,adc_enabled;
    double system_hz,peripheral_hz;
    std::uint64_t adc_fresh_reads,last_adc_read_us;
    unsigned last_adc_read_code,last_adc_read_channel;
    std::array<std::uint64_t,6> irq_deliveries;
    std::optional<unsigned> temperature_adc_code;
    Tick temperature_adc_us=0;
    std::array<unsigned,2> timer_counts{},timer_compare{};
    std::array<unsigned,4> pin_routes{};
    bool pin_locked=true;
    WatchdogObservation watchdog{};
    DeadmanObservation deadman{};
    double lfrc_hz=10000;
    bool supervision_available=false,reset_released=false,clock_stopped=false;
    std::array<ResetRecord,16> reset_history{};
    unsigned reset_history_size=0;
};
class Mcu {
public:
    Mcu(std::array<DigitalNet*,8> a,std::array<DigitalNet*,8> b,DigitalNet& pwm,BytePeripheral& external,
        std::array<const AnalogNet*,2> analog,const ClockNet* clock=nullptr,DeviceProfile device=DeviceProfile::b8,bool watchdog_fused_on=true);
    [[nodiscard]] DeviceProfile device() const noexcept {return device_;}
    [[nodiscard]] DmaObservation observe_dma() const noexcept {return dma_.observe();}
    void reset(ResetCause cause=ResetCause::por,std::uint8_t detail=0,std::optional<Tick> at=std::nullopt);
    void hold_in_reset(bool held) noexcept { external_hold_=held; }
    [[nodiscard]] bool ready() const noexcept {return !external_hold_ && reset_hold_==0 && !clock_.stopped() && !core_halted_;}
    [[nodiscard]] bool reset_released() const noexcept {return !external_hold_ && reset_hold_==0;}
    [[nodiscard]] std::uint64_t reset_serial() const noexcept{return reset_serial_;}
    [[nodiscard]] bool core_halted() const noexcept { return core_halted_; }
    void halt_core(bool v) noexcept {core_halted_=v;}
    void set_lfrc_ppm(double v);
    ClockTree& clocks() noexcept {return clock_;}

    Adc& adc() noexcept { return adc_; } // instructor fixtures only
    void advance_one_us(Tick now=0);
    void latch_inputs(bool tach,bool vblank);
    [[nodiscard]] std::uint8_t read8(Reg reg,Tick now);
    void write8(Reg reg,std::uint8_t data,Tick now);
    [[nodiscard]] std::optional<Irq> pending_irq() const noexcept;
    [[nodiscard]] bool global_enabled() const noexcept { return global_; }
    // Observer counters are monotonic across resets and are not firmware registers.
    void note_irq_delivery(Irq irq) noexcept { ++irq_deliveries_[static_cast<unsigned>(irq)]; }
    void set_global(bool enabled) noexcept { global_=enabled; }
    [[nodiscard]] std::uint8_t debug_active_duty() const noexcept { return active_duty_; }
[[nodiscard]] McuObservation observe() const noexcept;
private:
    struct Timer {
        std::uint8_t ctrl=0,scale=0,staged_low=0,latched_high=0;
        std::uint16_t count=0,compare=0,divider=0;
    };
    void update_gpio();
    void peripheral_tick();
    void pin_transaction(unsigned address,bool write,std::uint8_t value=0);
    bool pin_change_allowed();
    bool routed_input(unsigned role) const;
    void update_pwm();
    [[nodiscard]] bool quiescent() const noexcept;
    std::array<DigitalNet*,8> a_,b_;
    std::array<Driver,8> a_drivers_{},b_drivers_{};
    DigitalNet& pwm_;Driver pwm_driver_;BytePeripheral& external_;
    std::array<Timer,2> timers_{};
    Adc adc_;
    Dmac dma_;
    DeviceProfile device_;
    std::array<std::uint8_t,4> routes_{};
    std::uint8_t ansel_=0x30,pin_status_=0,pin_key_=0;
    bool pin_locked_=true,last_dreq_=false,pwm_high_=false;
    Tick now_us_=0;
    ClockTree clock_;Watchdog watchdog_;DeadmanTimer deadman_;
    bool revision03_=false,external_hold_=false,core_halted_=false;
    unsigned reset_hold_=0;
    double lf_phase_=0,pb_phase_=0,lfrc_ppm_=0;
    std::uint8_t reset_causes_=0,reset_details_=0;
    std::array<ResetRecord,16> reset_history_{};
    unsigned reset_history_size_=0;
    std::uint64_t reset_serial_=0,adc_fresh_reads_=0,last_adc_read_us_=0;
    unsigned last_adc_read_code_=0,last_adc_read_channel_=0;
    std::optional<unsigned> temperature_adc_code_;
    Tick temperature_adc_us_=0;
    std::array<std::uint64_t,6> irq_deliveries_{};

    std::uint8_t dir_a_=0,out_a_=0,dir_b_=0,out_b_=0;
    std::uint8_t flags_=0,enables_=0,shadow_duty_=0,active_duty_=0,xreg_=0,tach_high_=0;
    std::uint16_t tach_count_=0;
    bool global_=false,pwm_enabled_=false,last_tach_=false,last_vblank_=false;
    Tick ticks_=0;
};
}
