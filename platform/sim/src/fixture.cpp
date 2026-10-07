#include "blender8/sim/fixture.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <vector>
namespace b8::sim {
namespace {
std::vector<std::string> words(std::string_view s){
    if(s.size()>4096)throw std::length_error("command too long");
    std::istringstream in{std::string(s)};std::vector<std::string> v;
    for(std::string x;in>>x;)v.push_back(x);
    if(v.empty())throw std::invalid_argument("empty command");
    return v;
}
void size(const std::vector<std::string>& v,std::size_t n){if(v.size()!=n)throw std::invalid_argument("wrong command argument count");}
unsigned integer(const std::string& s,unsigned max){
    unsigned v=0;const char* begin=s.data();int base=10;
    if(s.starts_with("0x")){begin+=2;base=16;}
    const auto r=std::from_chars(begin,s.data()+s.size(),v,base);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||v>max)throw std::invalid_argument("invalid unsigned integer/range");
    return v;
}
bool boolean(const std::string& s){return integer(s,1)!=0;}
double real(const std::string& s,double lo,double hi){
    std::istringstream in{s};in.imbue(std::locale::classic());double v=0;in>>v;
    if(!in||!in.eof()||!std::isfinite(v)||v<lo||v>hi)throw std::invalid_argument("invalid finite number/range");
    return v;
}
}
std::function<void(Board&)> parse_fixture_command(std::string_view line,bool bench){
    const auto a=words(line);const auto& cmd=a[0];
    if(cmd=="speed"){size(a,2);auto n=integer(a[1],7);if(!n)throw std::invalid_argument("speed 1..7");return [n](Board& b){b.buttons().press_speed(n,b.now());};}
    if(cmd=="pulse"||cmd=="stop"||cmd=="jar"||cmd=="power"||cmd=="jam"||cmd=="halt"||cmd=="clock_failed"||cmd=="jar_bounce"||cmd=="foreground"||cmd=="adc_stalled"){
        size(a,2);bool v=boolean(a[1]);return [cmd,v](Board& b){
            if(cmd=="foreground")b.set_foreground_enabled(v);else if(cmd=="adc_stalled")b.mcu().adc().set_stalled(v);else if(cmd=="pulse")b.buttons().pulse(v,b.now());else if(cmd=="stop")b.buttons().stop(v,b.now());
            else if(cmd=="jar")b.jar().set_seated(v,b.now());else if(cmd=="power")b.power(v);
            else if(cmd=="jam")b.motor().set_jammed(v);else if(cmd=="halt")b.mcu().halt_core(v);
            else if(cmd=="jar_bounce")b.jar().set_bounce(v);else b.oscillator().set_failed(v);
        };
    }
    if(cmd=="load"){size(a,2);auto n=real(a[1],0,1);return [n](Board& b){b.motor().set_load(n);};}
    if(cmd=="temperature"){size(a,2);auto n=real(a[1],-40,150);return [n](Board& b){b.motor().thermal().set_initial_temperature(n);};}
    if(cmd=="environment"){size(a,4);auto amb=real(a[1],-40,150),food=real(a[2],-40,150),g=real(a[3],0,.2);return [=](Board& b){b.motor().thermal().set_environment(amb,food,g);};}
    if(cmd=="food"){size(a,3);auto food=real(a[1],-40,150),g=real(a[2],0,.2);return [=](Board& b){b.motor().thermal().set_food(food,g);};}
    if(cmd=="noise"){size(a,3);auto amp=real(a[1],0,.02);auto seed=integer(a[2],std::numeric_limits<unsigned>::max());return [=](Board& b){b.temperature_sensor().set_noise(amp,seed);};}
    if(cmd=="calibration"){size(a,3);auto off=real(a[1],-.05,.05),gain=real(a[2],-.1,.1);return [=](Board& b){b.temperature_sensor().set_calibration(off,gain);};}
    if(cmd=="sensor"){
        size(a,2);TemperatureFault f;
        if(a[1]=="healthy")f=TemperatureFault::healthy;else if(a[1]=="open")f=TemperatureFault::open;
        else if(a[1]=="ground")f=TemperatureFault::short_ground;else if(a[1]=="supply")f=TemperatureFault::short_supply;
        else if(a[1]=="frozen")f=TemperatureFault::frozen_output;else throw std::invalid_argument("unknown sensor fault");
        return [f](Board& b){b.temperature_sensor().set_fault(f);};
    }
    if(cmd=="tach"){
        size(a,2);TachFault f;
        if(a[1]=="healthy")f=TachFault::healthy;else if(a[1]=="low")f=TachFault::stuck_low;
        else if(a[1]=="high")f=TachFault::stuck_high;else throw std::invalid_argument("unknown tach fault");
        return [f](Board& b){b.motor().set_tach_fault(f);};
    }
    if(cmd=="jar_fault"){
        size(a,2);JarFault f;
        if(a[1]=="healthy")f=JarFault::healthy;else if(a[1]=="open")f=JarFault::broken_wire;
        else if(a[1]=="bypassed")f=JarFault::bypassed_contact;else throw std::invalid_argument("unknown jar fault");
        return [f](Board& b){b.jar().set_fault(f);};
    }
    if(cmd=="contact"){
        size(a,3);auto n=integer(a[1],7);int f;
        if(a[2]=="normal")f=-1;else if(a[2]=="closed")f=0;else if(a[2]=="open")f=1;else throw std::invalid_argument("unknown contact fault");
        return [=](Board& b){b.buttons().contact_fault(n,f);};
    }
    if(cmd=="bounce"){
        if(a.size()==3&&a[1]=="random"){auto seed=integer(a[2],std::numeric_limits<unsigned>::max());return [seed](Board& b){b.buttons().set_random_bounce(seed);};}
        size(a,2);if(a[1]!="off"&&a[1]!="nominal"&&a[1]!="slow")throw std::invalid_argument("unknown bounce mode");
        auto mode=a[1];return [mode](Board& b){b.buttons().set_bounce(mode!="off");b.buttons().set_slow_bounce(mode=="slow");};
    }
    if(cmd=="xo_ppm"||cmd=="frc_ppm"||cmd=="lfrc_ppm"){
        size(a,2);double limit=cmd=="xo_ppm"?50:(cmd=="frc_ppm"?50000:100000);auto n=real(a[1],-limit,limit);
        return [=](Board& b){if(cmd=="xo_ppm")b.oscillator().set_ppm(n);else if(cmd=="frc_ppm")b.mcu().clocks().set_frc_ppm(n);else b.mcu().set_lfrc_ppm(n);};
    }
    if(cmd=="voltage"){
        size(a,2);std::optional<double> v;if(a[1]!="auto")v=real(a[1],0,3.6);
        return [v](Board& b){b.power_domain().set_voltage_override(v);};
    }
    if(cmd=="fuse"){
        size(a,3);Fuse f;if(a[1]=="input")f=Fuse::input;else if(a[1]=="logic")f=Fuse::logic;
        else if(a[1]=="motor")f=Fuse::motor;else throw std::invalid_argument("unknown fuse branch");
        bool v=boolean(a[2]);return [=](Board& b){b.power_domain().set_fuse(f,v);};
    }
    if(cmd=="reset"){size(a,1);return [](Board& b){b.mcu().reset(ResetCause::external);};}
    if(cmd=="write"){
        if(!bench)throw std::invalid_argument("register writes are available only in --bench mode; firmware owns outputs");
        size(a,3);auto r=static_cast<Reg>(integer(a[1],65535));auto v=static_cast<std::uint8_t>(integer(a[2],255));
        return [=](Board& b){if(!b.ready())throw std::logic_error("bench bus access while controller unavailable");b.mcu().write8(r,v,b.now());};
    }
    throw std::invalid_argument("unknown fixture command");
}
std::string json_string(std::string_view text){
    std::ostringstream s;s<<'"';
    for(unsigned char c:text){
        switch(c){case '"':s<<"\\\"";break;case '\\':s<<"\\\\";break;case '\n':s<<"\\n";break;case '\r':s<<"\\r";break;case '\t':s<<"\\t";break;
        default:if(c<32)s<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;else s<<c;}
    }
    s<<'"';return s.str();
}
std::string snapshot_json(Board& b){
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<std::boolalpha;
    const auto m=b.mcu().observe();
    s<<"{\"time_us\":"<<b.now()<<",\"powered\":"<<b.powered()<<",\"ready\":"<<b.ready()
     <<",\"host_failed\":"<<b.failed()<<",\"drive_enabled\":"<<b.drive_enabled()<<",\"pwm_level\":"<<b.pwm_level()
     <<",\"rear_power_requested\":";
    if(auto* power=dynamic_cast<PowerDomain*>(&b.power_device()))s<<power->requested();else s<<"null";
    s<<",\"motor_supply\":"<<b.power_device().motor_supply()<<",\"foreground_enabled\":"<<b.foreground_enabled()
     <<",\"run_permit\":"<<b.run_permit()<<",\"stop\":"<<b.stop_asserted()<<",\"contacts\":"<<static_cast<unsigned>(b.contact_mask())
     <<",\"jar_present\":"<<b.jar().seated()<<",\"jar_ok\":"<<b.jar().raw_closed()<<",\"jar_permit\":"<<b.jar().permitted()
     <<",\"rail_v\":"<<b.power_device().volts()<<",\"analog_v\":"<<b.analog_voltage()<<",\"motor\":";
    if(auto* motor=dynamic_cast<MotorProbe*>(&b.motor_device())){
        const auto v=motor->observe_motor();s<<"{\"rpm\":"<<v.rpm<<",\"duty\":"<<v.effective_duty<<",\"case_c\":"<<v.case_c<<",\"loss_w\":"<<v.loss_w<<'}';
    }else s<<"null";
    s<<",\"thermal\":";
    if(auto* motor=dynamic_cast<Motor*>(&b.motor_device())){
        const auto& t=motor->thermal();
        s<<"{\"nearby_air_c\":"<<t.air_temperature_c()<<",\"food_c\":";
        if(t.food_present())s<<t.food_temperature_c();else s<<"null";
        s<<",\"food_present\":"<<t.food_present()<<",\"load_current_equivalent_a\":"<<t.last_load_current_a()<<'}';
    }else s<<"null";
    s<<",\"sensor_c\":";
    if(auto* sensor=dynamic_cast<SensorProbe*>(&b.sensor_device());sensor && sensor->debug_sensor_available())s<<sensor->debug_sensor_c();else s<<"null";
    s<<",\"mcu\":{";
#define B8_FIELD(field) s<<"\"" #field "\":"<<m.field<<','
    B8_FIELD(reset_serial);B8_FIELD(reset_causes);B8_FIELD(reset_details);B8_FIELD(gpioa_dir);B8_FIELD(gpioa_out);
    B8_FIELD(gpiob_dir);B8_FIELD(gpiob_out);B8_FIELD(irq_enable);B8_FIELD(irq_flags);B8_FIELD(pwm_enabled);
    B8_FIELD(pwm_shadow);B8_FIELD(pwm_active);B8_FIELD(tach_count);B8_FIELD(clock_source);B8_FIELD(clock_status);
    B8_FIELD(pb_div);B8_FIELD(wdt_control);B8_FIELD(wdt_scale);B8_FIELD(dmt_enabled);B8_FIELD(dmt_locked);
    B8_FIELD(dmt_limit);B8_FIELD(dmt_window);B8_FIELD(adc_status);B8_FIELD(adc_result);B8_FIELD(adc_enabled);B8_FIELD(adc_fresh_reads);B8_FIELD(last_adc_read_us);B8_FIELD(last_adc_read_code);B8_FIELD(last_adc_read_channel);B8_FIELD(system_hz);
#undef B8_FIELD
    s<<"\"irq_deliveries\":[";for(unsigned i=0;i<(b.mcu().device()==DeviceProfile::b16?6u:5u);++i){if(i)s<<',';s<<m.irq_deliveries[i];}s<<"],";
    s<<"\"peripheral_hz\":"<<m.peripheral_hz<<"},\"lcd_vblank\":"<<b.display_device().vblank()<<",\"lcd_pixels\":";
    if(auto* display=dynamic_cast<DisplayProbe*>(&b.display_device())){
        s<<'"';for(unsigned y=0;y<16;++y)for(unsigned x=0;x<32;++x)s<<(display->pixel(x,y)?'1':'0');s<<'"';
    }else s<<"null";
    if(b.mcu().device()==DeviceProfile::b16){
        const auto dma=b.mcu().observe_dma();
        s<<",\"dma\":{\"status\":"<<dma.status<<",\"error\":"<<dma.error<<",\"done\":"<<dma.done
         <<",\"left\":"<<dma.left<<",\"sram_hash\":"<<dma.sram_hash<<'}';
    }
    s<<'}';return s.str();
}
}
