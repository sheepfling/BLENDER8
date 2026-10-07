#include "test_support.hpp"
#include "blender8/access.hpp"
#include "blender8/host/backend.hpp"
#include "blender8/sim/hwil.hpp"
#include "blender8/sim/fixture.hpp"
#include "blender8/sim/runtime.hpp"
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace b8;
using namespace b8::sim;
using namespace b8::test;
namespace {
void jar_boot_absent(){Board b;b.jar().set_seated(false,0);boot(b);drive(b);b.advance(1000);require(!b.drive_enabled()&&!b.jar().permitted(),"absent jar inhibits despite drive request");}
void jar_open_cutoff(){Board b;boot(b);arm(b);drive(b);b.advance(10000);require(b.drive_enabled(),"drive before opening");b.jar().set_seated(false,b.now());b.settle();require(!b.drive_enabled()&&!b.jar().permitted(),"independent immediate drop");}
void jar_reseat_latched(){Board b;boot(b);arm(b);drive(b);b.jar().set_seated(false,b.now());b.settle();b.jar().set_seated(true,b.now());b.settle();b.advance(25000);require(!b.jar().permitted()&&!b.drive_enabled(),"no rearm from closure/time");}
void jar_held_pulse(){Board b;boot(b);arm(b);b.buttons().pulse(true,b.now());drive(b);b.jar().set_seated(false,b.now());b.settle();b.jar().set_seated(true,b.now());b.advance(25000);arm(b);require(!b.run_permit()&&!b.drive_enabled(),"STOP while rearming blocks held PULSE");b.buttons().pulse(false,b.now());b.buttons().pulse(true,b.now());b.settle();require(b.run_permit(),"fresh pulse only");}
void jar_short_dropout(){Board b;boot(b);arm(b);drive(b);const auto t=b.now();b.schedule(t+5,[](Board& x){x.jar().set_seated(false,x.now());});b.schedule(t+6,[](Board& x){x.jar().set_seated(true,x.now());});b.advance(20);require(b.jar().raw_closed()&&!b.jar().permitted(),"1us opening retained past poll");}
void jar_bounce(){Board b;boot(b);arm(b);drive(b);b.jar().set_bounce(true);b.jar().set_seated(false,b.now());b.settle();b.advance(6000);b.jar().set_seated(true,b.now());b.settle();b.advance(24000);b.buttons().stop(true,b.now());b.settle();require(!b.jar().permitted(),"not yet 20ms after final opening");b.buttons().stop(false,b.now());b.settle();b.advance(2000);arm(b);}
void jar_wire(){Board b;boot(b);arm(b);drive(b);b.jar().set_fault(JarFault::broken_wire);b.settle();require(!b.jar().raw_closed()&&!b.drive_enabled(),"broken wire nonpermissive");}
void jar_stop_already_held(){Board b;b.jar().set_seated(false,0);boot(b);b.buttons().stop(true,b.now());b.settle();b.jar().set_seated(true,b.now());b.settle();b.advance(30000);require(!b.jar().permitted(),"holding STOP does not queue rearm");b.buttons().stop(false,b.now());b.settle();arm(b);}
void jar_reset(){Board b;boot(b);arm(b);drive(b);b.mcu().reset(ResetCause::software);b.settle();b.advance(30000);require(b.jar().raw_closed()&&!b.jar().permitted(),"warm reset disarms jar");}
void jar_core_halted(){Board b;boot(b);arm(b);drive(b);b.mcu().halt_core(true);b.jar().set_seated(false,b.now());b.advance(1);require(!b.drive_enabled(),"no firmware required");}
void jar_masked(){Board b;boot(b);arm(b);drive(b);write(b,Reg::IRQ_GLOBAL,0);b.jar().set_seated(false,b.now());b.settle();require(!b.drive_enabled(),"interrupt masking cannot delay hardware");}
void jar_bypass_limit(){Board b;b.jar().set_seated(false,0);b.jar().set_fault(JarFault::bypassed_contact);boot(b);arm(b);drive(b);require(b.drive_enabled(),"single loop cannot prove unwelded/unbypassed contact; no false safety claim");}
void jar_stop_releases(){Board b;boot(b);drive(b);require(!b.drive_enabled(),"unarmed");arm(b);require(!b.run_permit()&&!b.drive_enabled(),"arming also invokes physical STOP release");}
void jar_clear_dominant(){Board b;boot(b);arm(b);drive(b);b.buttons().stop(true,b.now());b.jar().set_seated(false,b.now());b.settle();require(!b.jar().permitted()&&!b.drive_enabled(),"clear dominates simultaneous set");}
void jar_gpio(){Board b;boot(b);auto x=b.mcu().read8(Reg::GPIOB_IN,b.now());require((x&24)==8,"raw high, latch low");arm(b);x=b.mcu().read8(Reg::GPIOB_IN,b.now());require((x&24)==24,"both high");}
void jar_exact_qualifier(){Board b;boot(b);b.jar().set_seated(false,b.now());b.settle();b.jar().set_seated(true,b.now());b.settle();b.advance(19999);b.buttons().stop(true,b.now());b.settle();require(!b.jar().permitted(),"19999us cannot arm");b.buttons().stop(false,b.now());b.settle();b.advance(1);arm(b);}
unsigned resets=0,steps=0;
void count_reset(){++resets;}
void count_step(){++steps;idle();}
void runtime_chunks(){Board b;VectorTable v{};Runtime r(b,v);resets=steps=0;for(int i=0;i<10;++i)r.run_for(10000,count_reset,count_step);require(resets==1&&steps>0,"run chunks preserve boot epoch");}
void runtime_hardware_reentry(){Board b;VectorTable v{};Runtime r(b,v);resets=steps=0;r.run_for(80000,count_reset,count_step);b.mcu().reset(ResetCause::external);r.run_for(3000,count_reset,count_step);require(resets==2,"actual reset enters once");}
void short_clock_pause(){
    Board b;boot(b);write(b,Reg::CLK_SOURCE,1);write(b,Reg::PB_DIV,8);
    write(b,Reg::CLK_KEY,0xC3);write(b,Reg::CLK_KEY,0x3C);write(b,Reg::CLK_COMMIT,0xA5);
    const auto serial=b.mcu().reset_serial();VectorTable v{};Runtime r(b,v);resets=steps=0;
    b.schedule(b.now()+10,[](Board& x){x.oscillator().set_failed(true);});
    b.schedule(b.now()+20,[](Board& x){x.oscillator().set_failed(false);});
    r.run_for(1000,count_reset,count_step);
    require(resets==1&&b.mcu().reset_serial()==serial,"short source pause resumes, never invents reset entry");
}
void short_core_pause(){
    Board b;boot(b);const auto serial=b.mcu().reset_serial();VectorTable v{};Runtime r(b,v);resets=steps=0;
    b.schedule(b.now()+10,[](Board& x){x.mcu().halt_core(true);});
    b.schedule(b.now()+20,[](Board& x){x.mcu().halt_core(false);});
    r.run_for(1000,count_reset,count_step);
    require(resets==1&&b.mcu().reset_serial()==serial,"short core halt resumes without a firmware reset");
}
void runtime_empty(){Board b;VectorTable v{};Runtime r(b,v);resets=0;r.run_for(0,count_reset,count_step);require(b.now()==0&&resets==0,"zero duration does not boot");}
void spin_sdk(){while(true)idle();}
void runtime_budget(){Board b;boot(b);arm(b);drive(b);VectorTable v{};Runtime r(b,v);r.set_service_budget(50);bool hit=false;try{r.run_for(100,spin_sdk,count_step);}catch(const FirmwareBudgetExceeded&){hit=true;}require(hit&&b.failed()&&!b.drive_enabled(),"budget fail inhibits, not emulated watchdog");require((b.mcu().observe().reset_causes&4)==0,"host failure is not WDT");}
void event_inside_callback(){Board b;boot(b);arm(b);drive(b);VectorTable v{};Runtime r(b,v);auto t=b.now();b.schedule(t+2,[](Board& x){x.jar().set_seated(false,x.now());});for(int i=0;i<4;++i)idle();require(!b.jar().permitted(),"SDK tick executes timed stimulus");}
void event_ties(){Board b;std::vector<int> v;b.schedule(10,[&](Board&){v.push_back(1);});b.schedule(10,[&](Board&){v.push_back(2);});b.advance(10);require(v==std::vector<int>{1,2},"stable insertion order");}
void event_past(){Board b;b.advance(1);rejects([&]{b.schedule(0,[](Board&){});},"reject past schedule");}
void event_recursive(){Board b;b.schedule(2,[](Board& x){x.advance(1);});rejects([&]{b.advance(3);},"recursive advance rejected");require(b.failed(),"event failure is latched");}
void snapshot_keys(){Board b;boot(b);write(b,Reg::CLK_KEY,0xC3);auto a=snapshot_json(b);write(b,Reg::CLK_KEY,0x3C);auto z=snapshot_json(b);write(b,Reg::CLK_COMMIT,0xA5);require((b.mcu().clocks().debug_status()&8)==0,"snapshot may not cancel protected sequence");require(!a.empty()&&!z.empty(),"snapshot present");}
void snapshot_adc(){Board b;boot(b);write(b,Reg::ADC_CTRL,3);b.advance(60);const auto before=b.mcu().observe();for(int i=0;i<3;++i)(void)snapshot_json(b);const auto after=b.mcu().observe();require(before.adc_status==after.adc_status&&(after.adc_status&2),"observing does not acknowledge ADC");}
void snapshot_time(){Board b;boot(b);auto t=b.now();(void)snapshot_json(b);require(b.now()==t,"snapshot never advances time");}
struct MockBackend : b8::host::RegisterBackend {
    std::vector<unsigned> ops;
    std::uint8_t read8(Reg r) override{ops.push_back(static_cast<unsigned>(r));return r==Reg::TACH_COUNT_LO?0x34:0x12;}
    void write8(Reg r,std::uint8_t v) override{ops.push_back((static_cast<unsigned>(r)<<8)|v);}
    void idle() override{ops.push_back(0xffffffff);}
};
void backend_sdk(){MockBackend x;host::ScopedBinding binding(x);require(read_latched(tach_count)==0x1234,"latched low-high helper");require(x.ops==std::vector<unsigned>{0x70,0x71},"exact sequence");acknowledge(Irq::adc);require(x.ops.back()==0x1210,"W1C writes only its mask");}
void backend_unbound(){rejects([]{(void)read8(Reg::SYS_ID);},"unbound access rejected");MockBackend x;{host::ScopedBinding b(x);rejects([&]{host::ScopedBinding c(x);},"no ambiguous nested binding");}rejects([]{idle();},"unbind on destruction");}
struct OtherDisplay final:DisplayDevice,DisplayProbe {
    Lcd32 value;unsigned writes=0;
    void reset(Tick t)override{value.reset(t);}
    void advance(Tick t)override{value.advance(t);}
    bool vblank()const noexcept override{return value.vblank();}
    bool pixel(unsigned x,unsigned y)const override{return value.pixel(x,y);}
    std::uint8_t bus_read(Tick t,std::uint8_t r)override{return value.bus_read(t,r);}
    void bus_write(Tick t,std::uint8_t r,std::uint8_t d)override{++writes;value.bus_write(t,r,d);}
};
void swap_display(){ComponentFactories f;OtherDisplay* installed=nullptr;f.display=[&]{auto d=std::make_unique<OtherDisplay>();installed=d.get();return d;};Board b(BoardProfile::chassis04,f);boot(b);VectorTable v{};Runtime r(b,v);write8(Reg::XBUS_REG,0);write8(Reg::XBUS_DATA,0);write8(Reg::XBUS_REG,1);write8(Reg::XBUS_DATA,0x81);require(installed->writes==2&&installed->value.debug_vram()[0]==0x81,"firmware unchanged through foreign display");rejects([&]{(void)b.lcd();},"reference fixture not available");}
void swap_null(){ComponentFactories f;f.motor=[](MotorPorts){return std::unique_ptr<MotorDevice>{};};rejects([&]{Board b(BoardProfile::chassis04,f);},"null factory rejected");}
struct LoopTransport:MotorPinTransport {
    DigitalNet pwm{Logic::low},en{Logic::low},tach{Logic::low};ThermalNode node;
    Driver pd=pwm.attach(Logic::low),ed=en.attach(Logic::low);Motor model{pwm,en,tach,node};
    unsigned exchanges=0,inhibits=0;int fail=0;MotorLinkCapabilities caps{};
    MotorLinkCapabilities capabilities()const noexcept override{return caps;}
    TachPinReply exchange(const MotorPinRequest& r)override{
        ++exchanges;if(fail==3)throw HardwareLinkError("injected transport loss");
        pwm.drive(pd,r.pwm?Logic::high:Logic::low);en.drive(ed,r.enable?Logic::high:Logic::low);model.advance_one_us();
        return {r.time_us-(fail==1?1:0),r.sequence+(fail==2?1:0),tach.sample(),fail!=4,fail==5?std::optional<double>{}:std::optional<double>{node.celsius}};
    }
    void inhibit()noexcept override{++inhibits;en.drive(ed,Logic::low);pwm.drive(pd,Logic::low);}
};
void swap_motor_transport(){auto link=std::make_shared<LoopTransport>();ComponentFactories f;f.motor=[link](MotorPorts p){return std::make_unique<LockstepMotorAdapter>(p,link);};{
    Board b(BoardProfile::chassis04,f);boot(b);arm(b);drive(b);b.advance(50000);require(link->model.debug_rpm()>1000&&b.mcu().observe().tach_count>0,"tach enters MCU from transport");require(snapshot_json(b).find("\"motor\":null")!=std::string::npos,"no fabricated hardware plant truth");rejects([&]{(void)b.motor();},"hardware transport has no jam fixture");}require(link->inhibits>=2&&!link->en.sample(),"destructor inhibits outputs");}
void hwil_fault(int fcode){auto link=std::make_shared<LoopTransport>();ComponentFactories f;f.motor=[link](MotorPorts p){return std::make_unique<LockstepMotorAdapter>(p,link);};Board b(BoardProfile::chassis04,f);boot(b);arm(b);drive(b);link->fail=fcode;rejects([&]{b.advance(1);},"bad reply must fail");require(b.failed()&&!b.drive_enabled()&&!link->en.sample(),"link error inhibits local and remote");}
void hwil_stale(){hwil_fault(1);}void hwil_order(){hwil_fault(2);}void hwil_loss(){hwil_fault(3);}void hwil_invalid(){hwil_fault(4);}
void hwil_missing_thermal_rejected(){
    auto link=std::make_shared<LoopTransport>();link->fail=5;ComponentFactories f;
    f.motor=[link](MotorPorts n){return std::make_unique<LockstepMotorAdapter>(n,link);};
    Board b(BoardProfile::chassis04,f);
    require(snapshot_json(b).find("\"sensor_c\":null")!=std::string::npos,"no invented ambient sensor reading before link data");
    rejects([&]{b.advance(1);},"unsupported thermal composition fails, not silent ambient");
    require(b.failed()&&!link->en.sample()&&link->inhibits>=2,"host failure inhibits transport immediately");
}
void hwil_sensor_replacement(){
    struct VoltageInput:TemperatureDevice {AnalogNet& n;explicit VoltageInput(AnalogNet& pin):n(pin){}void advance_one_us()override{n.drive(.85);}};
    auto link=std::make_shared<LoopTransport>();link->fail=5;ComponentFactories f;
    f.motor=[link](MotorPorts n){return std::make_unique<LockstepMotorAdapter>(n,link);};
    f.sensor=[](TemperaturePorts n){return std::make_unique<VoltageInput>(n.voltage);};
    Board b(BoardProfile::chassis04,f);boot(b);near(b.analog_voltage(),.85,0,"independent voltage source does not require motor thermal model");
    require(snapshot_json(b).find("\"sensor_c\":null")!=std::string::npos,"voltage-only hardware has no fake package-temperature probe");
}
void hwil_realtime_rejected(){auto link=std::make_shared<LoopTransport>();link->caps.clock=LinkClock::physical_realtime;ComponentFactories f;f.motor=[link](MotorPorts p){return std::make_unique<LockstepMotorAdapter>(p,link);};rejects([&]{Board b(BoardProfile::chassis04,f);},"unpaced real hardware rejected");}
void hwil_slow_rejected(){auto link=std::make_shared<LoopTransport>();link->caps.sample_period_us=1000;ComponentFactories f;f.motor=[link](MotorPorts p){return std::make_unique<LockstepMotorAdapter>(p,link);};rejects([&]{Board b(BoardProfile::chassis04,f);},"cannot hide insufficient transport rate");}
void adc_probe_channel(){
    Board b;boot(b);write(b,Reg::ADC_CHANNEL,1);write(b,Reg::ADC_CTRL,3);b.advance(52);
    write(b,Reg::ADC_CHANNEL,0);(void)b.mcu().read8(Reg::ADC_DATA_LO,b.now());
    require(b.mcu().observe().last_adc_read_channel==1,"observer tags completed source, not later selected channel");
}
void fixture_validation(){for(const auto* cmd:{"speed 0","load nan","load 2","jar 2","noise .04 1","contact 8 open","write 0x21 1","sensor bogus","pulse 1 extra"})rejects([&]{(void)parse_fixture_command(cmd);},"invalid fixture command");}
std::vector<unsigned> bounce_history(unsigned seed){Board b(BoardProfile::legacy02);b.buttons().set_random_bounce(seed);b.buttons().press_speed(1,0);std::vector<unsigned> result;for(unsigned i=0;i<51;++i){result.push_back(b.contact_mask());b.advance(100);}require(b.contact_mask()==1,"bounce settles within5ms");return result;}
void random_bounce_replay(){require(bounce_history(4815)==bounce_history(4815),"same seed exact bounce");require(bounce_history(4815)!=bounce_history(42),"different histories");}
void physics_curve_sweep(){double last=0;for(unsigned i=0;i<=256;++i){const double d=i/256.0;const auto n=Motor::steady_rpm(d,0);require(n>=last&&n>=0&&n<=20000,"monotonic bounded curve");near(Motor::steady_rpm(d,1),n/2,1e-9,"load endpoints");last=n;}near(Motor::steady_rpm(.12,0),0,0,"dead zone");}
void physics_step_response(){Board b(BoardProfile::legacy02);b.buttons().set_bounce(false);drive(b);b.advance(120256);near(b.motor().debug_rpm(),20000*(1-std::exp(-1.0)),.3,"120ms run-up analytic");}
void physics_coast_response(){Board b(BoardProfile::legacy02);b.buttons().set_bounce(false);drive(b);b.advance(500000);double start=b.motor().debug_rpm();write(b,Reg::GPIOB_OUT,0);b.advance(350000);near(b.motor().debug_rpm(),start/std::exp(1.0),1e-6,"350ms coast analytic");}
void physics_load_step(){Board b(BoardProfile::legacy02);b.buttons().set_bounce(false);drive(b);b.advance(1000000);b.motor().set_load(1);double start=b.motor().debug_rpm();b.advance(120000);near(b.motor().debug_rpm(),10000+(start-10000)/std::exp(1.0),1e-5,"load change first-order response");}
void thermal_semigroup(){ThermalNode x,y;LumpedThermal a(x),b(y);a.set_environment(25,0,.15);b.set_environment(25,0,.15);a.advance(5,1,0,20000,true);for(int i=0;i<5000;++i)b.advance(.001,1,0,20000,true);near(x.celsius,y.celsius,1e-8,"thermal step subdivision");near(a.air_temperature_c(),b.air_temperature_c(),1e-8,"air step subdivision");near(a.food_temperature_c(),b.food_temperature_c(),1e-8,"food step subdivision");}
void thermal_long_run(){ThermalNode n;LumpedThermal a(n);a.advance(10000,1,0,20000,true);near(a.air_temperature_c(),25+130./20,1e-8,"ventilated air equilibrium");near(n.celsius,25+130./20+130./.6,1e-8,"locked case equilibrium");a.advance(10000,0,0,0,false);near(n.celsius,25,1e-8,"cooling equilibrium");near(a.air_temperature_c(),25,1e-8,"air returns to room");}
void thermal_food(){ThermalNode x,y;LumpedThermal a(x),b(y);b.set_environment(25,0,.2);a.advance(100,1,0,20000,true);b.advance(100,1,0,20000,true);require(y.celsius<x.celsius&&y.celsius>25,"weak food cooling still powered heat");require(b.food_temperature_c()>0&&b.air_temperature_c()>25,"food warms and nearby air heats");require(!a.food_present()&&b.food_present(),"zero coupling removes food");near(b.last_load_current_a(),10,1e-12,"locked load-current equivalent");ThermalNode hot{70};LumpedThermal exchange(hot);exchange.set_environment(25,0,.2);exchange.advance(.001,0,0,0,false);const double before=60*70+200*25;const double after=60*hot.celsius+200*exchange.air_temperature_c()+1200*exchange.food_temperature_c();near(after,before,1e-4,"internal heat paths conserve energy at first step");require(hot.celsius<70&&exchange.air_temperature_c()>25&&exchange.food_temperature_c()>0,"heat exchanges among three bodies");}
void random_buttons(){Board b(BoardProfile::legacy02);b.buttons().set_random_bounce(19);unsigned rng=19;for(int i=0;i<10000;++i){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;switch(rng%4){case 0:b.buttons().press_speed(1+(rng%7),b.now());break;case 1:b.buttons().pulse((rng&8)!=0,b.now());break;case 2:b.buttons().stop(true,b.now());break;default:b.buttons().stop(false,b.now());}b.settle();const auto mask=b.buttons().ideal_mask();require(std::popcount(static_cast<unsigned>(mask&127))<=1,"mechanical speed one-hot");if(b.stop_asserted())require(!b.run_permit()&&mask==0,"STOP dominates all physical commands");b.advance(10);}}
void live_tick_without_foreground(){
    Board b;VectorTable v{};v[0]=[]{acknowledge(Irq::timer0);};Runtime r(b,v);
    auto boot_fn=[](){write8(Reg::T0_COMPARE_LO,0xE7);write8(Reg::T0_COMPARE_HI,3);write8(Reg::T0_CTRL,3);write8(Reg::IRQ_ENABLE,1);write8(Reg::IRQ_GLOBAL,1);};
    r.run_for(60000,boot_fn,count_step);const auto before=b.mcu().observe().irq_deliveries[0];
    const auto foreground_steps=steps;b.set_foreground_enabled(false);r.run_for(10000,boot_fn,count_step);
    require(steps==foreground_steps&&b.mcu().observe().irq_deliveries[0]>before,"IRQs genuinely run with foreground frozen");
}
void all_factory_slots(){
    unsigned selected=0;ComponentFactories f;
    f.buttons=[&](ButtonPorts n){selected|=1;return std::make_unique<ButtonAssembly>(n.contacts,n.run_permit,n.stop_n);};
    f.mux=[&](MuxPorts n){selected|=2;return std::make_unique<Mux8>(n.inputs,n.select,n.output);};
    f.display=[&](){selected|=4;return std::make_unique<Lcd32>();};
    f.motor=[&](MotorPorts n){selected|=8;return std::make_unique<Motor>(n.pwm,n.enable,n.tach,n.case_temperature);};
    f.sensor=[&](TemperaturePorts n){selected|=16;return std::make_unique<TemperatureSensor>(n.attachment,n.voltage);};
    f.oscillator=[&](ClockNet& n){selected|=32;return std::make_unique<CrystalOscillator>(n);};
    f.power=[&](){selected|=64;return std::make_unique<PowerDomain>();};
    Board b(BoardProfile::chassis04,std::move(f));boot(b);arm(b);drive(b);b.advance(10000);
    require(selected==127&&b.drive_enabled()&&b.motor().debug_rpm()>0,"all seven factory slots connected");
}
void firmware_fixture_block(){Board b;boot(b);auto before=b.mcu().observe();rejects([&]{auto f=parse_fixture_command("write 0x29 1",false);f(b);},"no register poke in firmware mode");require(b.mcu().observe().gpiob_out==before.gpiob_out,"rejected parse cannot mutate");}
}
int main(int argc,char** argv){
    const std::map<std::string,std::function<void()>> tests={
#define T(name) {#name,name}
        T(jar_boot_absent),T(jar_open_cutoff),T(jar_reseat_latched),T(jar_held_pulse),T(jar_short_dropout),T(jar_bounce),
        T(jar_wire),T(jar_stop_already_held),T(jar_reset),T(jar_core_halted),T(jar_masked),T(jar_bypass_limit),
        T(jar_stop_releases),T(jar_clear_dominant),T(jar_gpio),T(jar_exact_qualifier),
        T(live_tick_without_foreground),T(all_factory_slots),T(runtime_chunks),T(runtime_hardware_reentry),T(runtime_empty),T(short_clock_pause),T(short_core_pause),T(runtime_budget),T(event_inside_callback),
        T(event_ties),T(event_past),T(event_recursive),T(snapshot_keys),T(snapshot_adc),T(snapshot_time),
        T(backend_sdk),T(backend_unbound),T(swap_display),T(swap_null),T(swap_motor_transport),
        T(hwil_missing_thermal_rejected),T(hwil_sensor_replacement),T(hwil_stale),T(hwil_order),T(hwil_loss),T(hwil_invalid),T(hwil_realtime_rejected),T(hwil_slow_rejected),
        T(adc_probe_channel),T(fixture_validation),T(random_bounce_replay),T(physics_curve_sweep),T(physics_step_response),T(physics_coast_response),
        T(physics_load_step),T(thermal_semigroup),T(thermal_long_run),T(thermal_food),T(random_buttons),T(firmware_fixture_block)
#undef T
    };
    try{if(argc!=2)throw std::invalid_argument("test name required");tests.at(argv[1])();std::cout<<argv[1]<<" PASS\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
