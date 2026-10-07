#include "blender8/sim/board.hpp"
#include <limits>
namespace b8::sim {
namespace {
std::array<DigitalNet,8> nets(Logic bias) {
    return {DigitalNet(bias),DigitalNet(bias),DigitalNet(bias),DigitalNet(bias),
            DigitalNet(bias),DigitalNet(bias),DigitalNet(bias),DigitalNet(bias)};
}
std::array<DigitalNet*,8> pointers(std::array<DigitalNet,8>& n) {
    return {&n[0],&n[1],&n[2],&n[3],&n[4],&n[5],&n[6],&n[7]};
}
template<class T> std::unique_ptr<T> checked(std::unique_ptr<T> p){
    if(!p)throw std::invalid_argument("component factory returned null");
    return p;
}
}
Board::Board(BoardProfile profile,ComponentFactories f,DeviceProfile device,bool watchdog_fused_on)
 :a_(nets(Logic::low)),b_(nets(Logic::low)),contacts_(nets(Logic::high)),
  gate_driver_(gated_enable_.attach(Logic::low)),
  vblank_driver_(b_[6].attach()),
  buttons_(checked<ButtonDevice>(f.buttons?f.buttons({pointers(contacts_),run_permit_,b_[2]}):
        std::make_unique<ButtonAssembly>(pointers(contacts_),run_permit_,b_[2]))),
  mux_(checked<MuxDevice>(f.mux?f.mux({pointers(contacts_),{&a_[0],&a_[1],&a_[2]},a_[3]}):
        std::make_unique<Mux8>(pointers(contacts_),std::array<DigitalNet*,3>{&a_[0],&a_[1],&a_[2]},a_[3]))),
  lcd_(checked<DisplayDevice>(f.display?f.display():std::make_unique<Lcd32>())),
  oscillator_(checked<OscillatorDevice>(f.oscillator?f.oscillator(external_clock_):std::make_unique<CrystalOscillator>(external_clock_))),
  power_domain_(checked<PowerDevice>(f.power?f.power():std::make_unique<PowerDomain>())),
  mcu_(pointers(a_),pointers(b_),pwm_,*lcd_,{&temperature_voltage_,&spare_analog_},
        profile!=BoardProfile::legacy02?&external_clock_:nullptr,device,watchdog_fused_on),
  motor_(checked<MotorDevice>(f.motor?f.motor({pwm_,gated_enable_,b_[1],case_temperature_}):
        std::make_unique<Motor>(pwm_,gated_enable_,b_[1],case_temperature_))),
  temperature_sensor_(checked<TemperatureDevice>(f.sensor?f.sensor({case_temperature_,temperature_voltage_}):
        std::make_unique<TemperatureSensor>(case_temperature_,temperature_voltage_))),
  jar_(b_[3],b_[4],b_[2]),profile_(profile) {
    powered_=profile==BoardProfile::legacy02;ever_powered_=powered_;mcu_.hold_in_reset(!powered_);
    lcd_->reset(0);mcu_.reset();settle();
}
std::uint8_t Board::contact_mask() const{
    std::uint8_t v=0;for(unsigned i=0;i<8;++i)if(!contacts_[i].sample())v|=static_cast<std::uint8_t>(1u<<i);return v;
}
void Board::emergency_inhibit() noexcept {
    failed_=true;gated_enable_.drive(gate_driver_,Logic::low);
    if(motor_)motor_->inhibit_host_failure();
}
void Board::settle() {
    try {
        buttons_->advance(now_);mux_->advance(now_);
        if(profile_==BoardProfile::chassis04)jar_.advance(now_,powered_&&mcu_.reset_released());
        const bool jar_ok=profile_!=BoardProfile::chassis04||(jar_.raw_closed()&&jar_.permitted());
        const bool enable=!failed_&&powered_&&mcu_.reset_released()&&jar_ok&&
          (profile_==BoardProfile::legacy02||power_domain_->motor_supply())&&run_permit_.sample()&&b_[0].sample();
        gated_enable_.drive(gate_driver_,enable?Logic::high:Logic::low);
        if(observer_)observer_(*this);
    }catch(...){emergency_inhibit();throw;}
}
void Board::schedule(Tick at,std::function<void(Board&)> action){
    if(!action||at<now_||applying_)throw std::invalid_argument("invalid event timestamp or recursive event");
    if(events_.size()>=100000)throw std::length_error("event queue limit");
    events_.emplace(at,std::move(action));
    if(at==now_)apply_events();
}
void Board::apply_events(){
    applying_=true;
    try {
        while(!events_.empty()&&events_.begin()->first<=now_){
            auto node=events_.extract(events_.begin());node.mapped()(*this);settle();
        }
        applying_=false;
    }catch(...){applying_=false;emergency_inhibit();throw;}
}
void Board::advance(Tick delta) {
    if(advancing_||applying_)throw std::logic_error("recursive board advance");
    if(delta>std::numeric_limits<Tick>::max()-now_)throw std::overflow_error("simulation time");
    advancing_=true;
    try {
        for(Tick i=0;i<delta;++i){
            ++now_;apply_events();
            if(profile_!=BoardProfile::legacy02){
                power_domain_->advance_one_us(now_);
                const bool good=power_domain_->good();
                if(good!=powered_){
                    powered_=good;
                    if(good){mcu_.reset(ever_powered_?ResetCause::brownout:ResetCause::por,0,now_);ever_powered_=true;}
                    else{mcu_.reset(ResetCause::brownout,0,now_);lcd_->reset(now_);}
                    mcu_.hold_in_reset(!good);
                }
                oscillator_->advance_one_us(good);
            }
            if(powered_)mcu_.advance_one_us(now_);
            settle();motor_->advance_one_us();temperature_sensor_->advance_one_us();
            if(powered_){lcd_->advance(now_);if(mcu_.device()==DeviceProfile::b16)b_[6].drive(vblank_driver_,lcd_->vblank()?Logic::high:Logic::low);if(mcu_.reset_released())mcu_.latch_inputs(b_[1].sample(),lcd_->vblank());}
        }
        advancing_=false;
    }catch(...){advancing_=false;emergency_inhibit();throw;}
}
void Board::power(bool on) {
    if(profile_!=BoardProfile::legacy02){
        auto& supply=power_domain();
        if(on==supply.requested())return;
        supply.request(on,now_);
        if(!on){powered_=false;mcu_.reset(ResetCause::external,0,now_);mcu_.hold_in_reset(true);lcd_->reset(now_);}
        settle();return;
    }
    if(on==powered_)return;
    powered_=on;mcu_.reset();lcd_->reset(now_);settle();
}
}
