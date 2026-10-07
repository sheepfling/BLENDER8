#include "blender8/sim/mcu.hpp"
#include <stdexcept>
#include <cmath>
namespace b8::sim {
Mcu::Mcu(std::array<DigitalNet*,8> a,std::array<DigitalNet*,8> b,DigitalNet& pwm,BytePeripheral& ext,std::array<const AnalogNet*,2> analog,const ClockNet* clock,DeviceProfile device)
    :a_(a),b_(b),pwm_(pwm),pwm_driver_(pwm.attach(Logic::low)),external_(ext),adc_(analog),device_(device),clock_(clock),revision03_(clock!=nullptr) {
    if(device==DeviceProfile::b16&&!clock)throw std::invalid_argument("B16 requires a clocked board");
    for (unsigned i=0;i<8;++i) { a_drivers_[i]=a_[i]->attach();b_drivers_[i]=b_[i]->attach(); }
}
void Mcu::reset(ResetCause cause,std::uint8_t detail) {
    if(cause==ResetCause::por){reset_causes_=0;reset_details_=0;}
    reset_causes_|=static_cast<std::uint8_t>(cause);reset_details_|=detail;++reset_serial_;
    dma_.reset((static_cast<unsigned>(cause)&3)!=0);routes_={};ansel_=0x30;pin_status_=pin_key_=0;pin_locked_=true;last_dreq_=pwm_high_=false;
    clock_.reset();watchdog_.reset();deadman_.reset();reset_hold_=revision03_?1000:0;
    lf_phase_=pb_phase_=0;core_halted_=false;

    temperature_adc_code_.reset();temperature_adc_us_=0;
    adc_.reset();adc_fresh_reads_=0;last_adc_read_us_=0;last_adc_read_code_=0;last_adc_read_channel_=0;timers_={};dir_a_=out_a_=dir_b_=out_b_=0;flags_=enables_=0;
    shadow_duty_=active_duty_=xreg_=tach_high_=0;tach_count_=0;
    global_=pwm_enabled_=last_tach_=last_vblank_=false;ticks_=0;
    update_gpio();pwm_.drive(pwm_driver_,Logic::low);
}
void Mcu::update_gpio() {
    const bool b16=device_==DeviceProfile::b16;
    for(unsigned i=0;i<8;++i) {
        a_[i]->drive(a_drivers_[i],(dir_a_&(1u<<i))==0?Logic::high_z:((b16&&routes_[0]==1&&i==4&&!(ansel_&0x10)?pwm_high_:(out_a_&(1u<<i))!=0)?Logic::high:Logic::low));
        b_[i]->drive(b_drivers_[i],(dir_b_&(1u<<i))==0?Logic::high_z:((b16&&routes_[0]==2&&i==5?pwm_high_:(out_b_&(1u<<i))!=0)?Logic::high:Logic::low));
    }
}
void Mcu::update_pwm() {
    if(device_==DeviceProfile::b8){pwm_.drive(pwm_driver_,pwm_high_?Logic::high:Logic::low);return;}
    update_gpio();
    // The B16 chassis connects the motor's PWM wire to PB5. Selecting PA4 does not move it.
    pwm_.drive(pwm_driver_,b_[5]->sample()?Logic::high:Logic::low);
}
bool Mcu::routed_input(unsigned role) const {
    const unsigned route=routes_[role];if(!route)return false;
    constexpr unsigned pads[4][2]={{4,5},{1,5},{6,7},{6,7}};
    constexpr unsigned banks[4][2]={{0,1},{1,0},{1,0},{0,1}};
    const auto bit=pads[role][route-1],bank=banks[role][route-1];
    if((bank?dir_b_:dir_a_)&(1u<<bit))return false;
    if(!bank&&(ansel_&(1u<<bit)))return false;
    return (bank?b_:a_)[bit]->sample();
}
bool Mcu::pin_change_allowed() {
    if(pin_locked_){pin_status_|=4;return false;}
    if(pwm_enabled_||adc_.debug_enabled()||dma_.busy()){pin_status_|=2;return false;}
    return true;
}
void Mcu::pin_transaction(unsigned address,bool write,std::uint8_t value) {
    if(write&&address==0xE8){
        if(pin_key_==1&&value==0xA5){
            if(pwm_enabled_||adc_.debug_enabled()||dma_.busy())pin_status_|=2;
            else pin_locked_=false;
            pin_key_=0;
        }else pin_key_=value==0x5A?1:0;
        return;
    }
    if(write&&address==0xE9){
        if(value&1)pin_locked_=true;
    }
    pin_key_=0;
}
void Mcu::set_lfrc_ppm(double v){if(!(v>=-100000 && v<=100000))throw std::invalid_argument("LFRC tolerance");lfrc_ppm_=v;}
bool Mcu::quiescent() const noexcept{return !(timers_[0].ctrl&1) && !(timers_[1].ctrl&1) && !pwm_enabled_ && !adc_.busy() && !deadman_.enabled() && !dma_.busy();}
void Mcu::advance_one_us(Tick now) {
    now_us_=now?now:now_us_+1;
    if(!revision03_){peripheral_tick();return;}
    if(external_hold_)return;
    if(reset_hold_){--reset_hold_;return;}
    lf_phase_+=10000*(1+lfrc_ppm_/1e6)/1e6;
    unsigned lf=0;while(lf_phase_>=1){lf_phase_-=1;++lf;}
    // Collect same-quantum hardware causes before resetting once.
    unsigned causes=0,details=0;
    for(unsigned i=0;i<lf;++i)if(watchdog_.tick()){causes|=4;details|=32;}
    if(clock_.advance_one_us(lf))causes|=16;
    if(deadman_.advance(clock_.system_hz(),!core_halted_&&!clock_.stopped())){causes|=8;details|=16;}
    if(causes){reset(static_cast<ResetCause>(causes),static_cast<std::uint8_t>(details));return;}
    if(clock_.stopped())return;
    // Peripheral clocks continue during a core halt; external components keep their own time.
    pb_phase_+=clock_.peripheral_hz()/1e6;
    while(pb_phase_>=1){pb_phase_-=1;peripheral_tick();}
}
void Mcu::peripheral_tick() {
    if(adc_.advance_clock_tick()) flags_|=mask(Irq::adc);
    for(unsigned i=0;i<2;++i) {
        auto& t=timers_[i];
        if((t.ctrl&1u)==0) continue;
        constexpr std::array<std::uint16_t,4> div{1,8,64,256};
        if(++t.divider<div[t.scale]) continue;
        t.divider=0;
        if(t.count==t.compare) {
            flags_|=static_cast<std::uint8_t>(1u<<i);if(i==0&&device_==DeviceProfile::b16)dma_.request(1);t.count=0;
            if((t.ctrl&2u)==0) t.ctrl&=static_cast<std::uint8_t>(~1u);
        } else ++t.count;
    }
    const auto phase=ticks_%256;
    if(phase==0) active_duty_=shadow_duty_;
    pwm_high_=pwm_enabled_ && (active_duty_==255 || phase<active_duty_);update_pwm();
    if(device_==DeviceProfile::b16){dma_.tick(external_,now_us_);if(dma_.irq())flags_|=0x20;}
    ++ticks_;
}
void Mcu::latch_inputs(bool tach,bool vblank) {
    if(device_==DeviceProfile::b16){
        tach=routed_input(1);vblank=routed_input(2);const bool dreq=routed_input(3);
        if(dreq&&!last_dreq_)dma_.request(2);
        last_dreq_=dreq;
        if(vblank&&!last_vblank_)dma_.request(3);
    }
    if(tach && !last_tach_) { ++tach_count_;flags_|=mask(Irq::tach); }
    if(vblank && !last_vblank_) flags_|=mask(Irq::vblank);
    last_tach_=tach;last_vblank_=vblank;
}
std::optional<Irq> Mcu::pending_irq() const noexcept {
    if(!global_ || !ready()) return std::nullopt;
    const auto pending=static_cast<std::uint8_t>(flags_&enables_);
    for(unsigned i=0;i<(device_==DeviceProfile::b16?6u:5u);++i) if((pending&(1u<<i))!=0) return static_cast<Irq>(i);
    return std::nullopt;
}
std::uint8_t Mcu::read8(Reg reg,Tick now) {
    const auto address=static_cast<unsigned>(reg);
    if(device_==DeviceProfile::b16){
        pin_transaction(address,false);
        if((address>=0xD0&&address<=0xDE)||(address>=0xE0&&address<=0xE1)||(address>=0x1000&&address<=0x13FF)){clock_.other_access();return dma_.read(address);}
        if(address==5){clock_.other_access();return 0x0F;}
        if(address==0x23){clock_.other_access();return ansel_;}
        if(address>=0xE8&&address<=0xEE){clock_.other_access();if(address==0xE8)return 0;if(address==0xE9)return pin_locked_;if(address==0xEA)return pin_status_;return routes_[address-0xEB];}
        if((address==0x80||address==0x81)&&dma_.external_busy()){dma_.ownership_conflict();clock_.other_access();return 0;}
    }
    if(revision03_ && address>=0xA0 && address<=0xA9)return clock_.read(address-0xA0);
    clock_.other_access();
    if(address==2)return reset_causes_;
    if(address==3)return reset_details_;
    if(address==4)return 0;
    if(revision03_ && address>=0xB0 && address<=0xB3){switch(address-0xB0){case 0:return watchdog_.control();case 1:return watchdog_.scale();case 2:return 0;default:return watchdog_.status();}}
    if(revision03_ && address>=0xC0 && address<=0xC9)return deadman_.read(address-0xC0);
    if(address>=0x90 && address<=0x95){
        if(address==0x94 && (adc_.debug_status()&2)){
            ++adc_fresh_reads_;last_adc_read_us_=now;last_adc_read_code_=adc_.debug_result();last_adc_read_channel_=adc_.debug_result_channel();
            if(last_adc_read_channel_==0){temperature_adc_code_=last_adc_read_code_;temperature_adc_us_=now;}
        }
        return adc_.read(address-0x90);
    }
    if((address>=0x40 && address<=0x45)||(address>=0x50 && address<=0x55)) {
        auto& t=timers_[(address-0x40)/16];
        switch(address&15u) {
        case 0:return t.ctrl;case 1:return t.scale;
        case 2:t.latched_high=static_cast<std::uint8_t>(t.count>>8);return static_cast<std::uint8_t>(t.count);
        case 3:return t.latched_high;
        case 4:return static_cast<std::uint8_t>(t.compare);
        default:return static_cast<std::uint8_t>(t.compare>>8);
        }
    }
    const auto sample=[](const auto& pins,unsigned suppressed=0) {
        std::uint8_t value=0;
        for(unsigned i=0;i<8;++i) if(!(suppressed&(1u<<i))&&pins[i]->sample()) value|=static_cast<std::uint8_t>(1u<<i);
        return value;
    };
    switch(reg) {
    case Reg::SYS_ID:return device_==DeviceProfile::b16?0x16:0xB8;case Reg::SYS_REV:return device_==DeviceProfile::b16?4:(revision03_?3:2);
    case Reg::IRQ_GLOBAL:return global_?1:0;case Reg::IRQ_ENABLE:return enables_;case Reg::IRQ_FLAGS:return flags_;
    case Reg::GPIOA_DIR:return dir_a_;case Reg::GPIOA_OUT:return out_a_;case Reg::GPIOA_IN:return sample(a_,device_==DeviceProfile::b16?ansel_:0);
    case Reg::GPIOB_DIR:return dir_b_;case Reg::GPIOB_OUT:return out_b_;case Reg::GPIOB_IN:return sample(b_);
    case Reg::PWM_CTRL:return pwm_enabled_?1:0;case Reg::PWM_DUTY:return shadow_duty_;
    case Reg::TACH_COUNT_LO:tach_high_=static_cast<std::uint8_t>(tach_count_>>8);return static_cast<std::uint8_t>(tach_count_);
    case Reg::TACH_COUNT_HI:return tach_high_;
    case Reg::XBUS_REG:return xreg_;case Reg::XBUS_DATA:return external_.bus_read(now,xreg_);
    default:throw std::out_of_range("unmapped B8 read");
    }
}
void Mcu::write8(Reg reg,std::uint8_t data,Tick now) {
    const auto address=static_cast<unsigned>(reg);
    if(device_==DeviceProfile::b16){
        pin_transaction(address,true,data);
        if((address>=0xD0&&address<=0xDE)||(address>=0xE0&&address<=0xE1)||(address>=0x1000&&address<=0x13FF)){
            clock_.other_access();if(address==0xD0&&(data&1)&&!(data&2)&&!pin_locked_)pin_status_|=2;
            dma_.write(address,data,xreg_,pin_locked_);return;
        }
        if(address==5)throw std::logic_error("read-only SYS_CAPS");
        if(address>=0xE8&&address<=0xEA){clock_.other_access();if(address==0xEA)pin_status_&=static_cast<std::uint8_t>(~(data&7));return;}
        if(address==0x23||(address>=0xEB&&address<=0xEE)){
            clock_.other_access();if(!pin_change_allowed())return;
            if(address==0x23){
                const auto next=data&0x30;
                if((routes_[0]==1&&(next&0x10))||(routes_[1]==2&&(next&0x20))){pin_status_|=1;return;}
                ansel_=next;
            }else {
                if(data>2){pin_status_|=1;return;}
                if(data){
                    constexpr unsigned pads[4][2]={{4,5},{1,5},{6,7},{6,7}};
                    constexpr unsigned banks[4][2]={{0,1},{1,0},{1,0},{0,1}};
                    const auto role=address-0xEB,bit=pads[role][data-1],bank=banks[role][data-1];
                    const bool output=((bank?dir_b_:dir_a_)&(1u<<bit))!=0;
                    if(output!=(role==0)||(!bank&&(ansel_&(1u<<bit)))){pin_status_|=1;return;}
                }
                routes_[address-0xEB]=data;
            }
            if(address==0xEC)last_tach_=routed_input(1);
            if(address==0xED)last_vblank_=routed_input(2);
            if(address==0xEE)last_dreq_=routed_input(3);
            update_gpio();return;
        }
        if((address==0x80||address==0x81)&&dma_.external_busy()){dma_.ownership_conflict();clock_.other_access();return;}
        if(address==0x60&&(data&1)&&!pin_locked_){pin_status_|=2;clock_.other_access();return;}
        if(address==0x90){
            const unsigned bit=4+adc_.read(1);
            const bool bad_pad=(dir_a_&(1u<<bit))||!(ansel_&(1u<<bit));
            if(((data&1)&&!pin_locked_)||((data&2)&&bad_pad)){
                if(!pin_locked_)pin_status_|=2;
                else adc_.write(0,data&1);
                adc_.reject();clock_.other_access();return;
            }
        }
        if(address==0x20||address==0x28){
            unsigned protected_bits=address==0x20?ansel_:0;
            constexpr unsigned pads[4][2]={{4,5},{1,5},{6,7},{6,7}};
            constexpr unsigned banks[4][2]={{0,1},{1,0},{1,0},{0,1}};
            for(unsigned i=0;i<4;++i)if(routes_[i]&&banks[i][routes_[i]-1]==(address==0x28?1u:0u))protected_bits|=1u<<pads[i][routes_[i]-1];
            if(((data^(address==0x20?dir_a_:dir_b_))&protected_bits)&&!pin_change_allowed()){clock_.other_access();return;}
            for(unsigned i=0;i<4;++i)if(routes_[i]&&banks[i][routes_[i]-1]==(address==0x28?1u:0u)){
                if(((data&(1u<<pads[i][routes_[i]-1]))!=0)!=(i==0)){pin_status_|=1;clock_.other_access();return;}
            }
        }
    }
    if(revision03_ && address>=0xA0 && address<=0xA9){clock_.write(address-0xA0,data,quiescent());return;}
    clock_.other_access();
    if(address==2){reset_causes_&=static_cast<std::uint8_t>(~data);return;}
    if(address==3){reset_details_&=static_cast<std::uint8_t>(~data);return;}
    if(address==4){if(data==0xB6)reset(ResetCause::software);return;}
    if(revision03_ && address>=0xB0 && address<=0xB3){switch(address-0xB0){
      case 0:watchdog_.control(data);return;case 1:watchdog_.configure(data);return;
      case 2:if(!watchdog_.service(data))reset(ResetCause::watchdog,1);return;
      default:throw std::logic_error("read-only watchdog status");}}
    if(revision03_ && address>=0xC0 && address<=0xC9){const auto d=deadman_.write(address-0xC0,data);if(d)reset(ResetCause::deadman,d);return;}
    if(address>=0x90 && address<=0x95) {adc_.write(address-0x90,data);return;}
    if((address>=0x40 && address<=0x45)||(address>=0x50 && address<=0x55)) {
        auto& t=timers_[(address-0x40)/16];
        switch(address&15u) {
        case 0:t.ctrl=data&3u;t.count=0;t.divider=0;return;
        case 1:if((t.ctrl&1u)!=0) throw std::logic_error("disable timer before prescale write");t.scale=data&3u;return;
        case 4:if((t.ctrl&1u)!=0) throw std::logic_error("disable timer before compare write");t.staged_low=data;return;
        case 5:if((t.ctrl&1u)!=0) throw std::logic_error("disable timer before compare write");
            t.compare=static_cast<std::uint16_t>((static_cast<unsigned>(data)<<8)|t.staged_low);return;
        default:throw std::logic_error("write to read-only timer register");
        }
    }
    switch(reg) {
    case Reg::IRQ_GLOBAL:global_=(data&1u)!=0;break;
    case Reg::IRQ_ENABLE:enables_=data&(device_==DeviceProfile::b16?63u:31u);break;
    case Reg::IRQ_FLAGS:flags_&=static_cast<std::uint8_t>(~data);break;
    case Reg::GPIOA_DIR:dir_a_=data;update_gpio();break;
    case Reg::GPIOA_OUT:out_a_=data;update_gpio();break;
    case Reg::GPIOB_DIR:dir_b_=data;update_gpio();break;
    case Reg::GPIOB_OUT:out_b_=data;update_gpio();break;
    case Reg::PWM_CTRL:pwm_enabled_=(data&1u)!=0;if(!pwm_enabled_)pwm_high_=false;update_pwm();break;
    case Reg::PWM_DUTY:shadow_duty_=data;break;
    case Reg::XBUS_REG:xreg_=data&3u;break;
    case Reg::XBUS_DATA:external_.bus_write(now,xreg_,data);break;
    case Reg::SYS_ID:case Reg::SYS_REV:case Reg::GPIOA_IN:case Reg::GPIOB_IN:
    case Reg::TACH_COUNT_LO:case Reg::TACH_COUNT_HI:throw std::logic_error("write to read-only B8 register");
    default:throw std::out_of_range("unmapped B8 write");
    }
}
McuObservation Mcu::observe() const noexcept {
    return {reset_serial_,reset_causes_,reset_details_,dir_a_,out_a_,dir_b_,out_b_,
        enables_,flags_,static_cast<unsigned>(pwm_enabled_),shadow_duty_,active_duty_,tach_count_,
        clock_.active_plan().source,clock_.debug_status(),clock_.active_plan().pb,
        watchdog_.control(),watchdog_.scale(),static_cast<unsigned>(deadman_.enabled()),
        static_cast<unsigned>(deadman_.debug_locked()),deadman_.debug_limit(),deadman_.debug_window(),
        adc_.debug_status(),adc_.debug_result(),static_cast<unsigned>(adc_.debug_enabled()),
        clock_.system_hz(),clock_.peripheral_hz(),adc_fresh_reads_,last_adc_read_us_,last_adc_read_code_,last_adc_read_channel_,irq_deliveries_,temperature_adc_code_,temperature_adc_us_,
        {timers_[0].count,timers_[1].count},{timers_[0].compare,timers_[1].compare},
        {routes_[0],routes_[1],routes_[2],routes_[3]},pin_locked_};
}

}
