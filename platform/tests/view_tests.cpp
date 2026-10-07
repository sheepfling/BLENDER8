#include "blender8/view/workbench.hpp"
#include "blender8/sim/fixture.hpp"
#include "blender8/sim/board.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace b8::sim;
using namespace b8::view;
namespace {
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void ok(Workbench& w,std::string_view c){require(w.command(c).starts_with("{\"ok\":true"),"command failed");}
bool consume_adc=false;
void adc_reset(){b8::write8(b8::Reg::ADC_CTRL,3);}
void adc_step(){
 b8::write8(b8::Reg::WDT_SERVICE,0xA5);b8::write8(b8::Reg::WDT_SERVICE,0x5A);
 if(consume_adc&&(b8::read8(b8::Reg::ADC_STATUS)&2)){
  (void)b8::read8(b8::Reg::ADC_DATA_LO);(void)b8::read8(b8::Reg::ADC_DATA_HI);consume_adc=false;
 }else b8::idle();
}
FirmwareImage image(){return {{},[](){},[](){b8::idle();},"scene-test"};}
void click(Workbench& w,Control id,int token=10){if(id==Control::truth)id=Control::thermal;for(auto c:w.controls())if(c.id==id){Point p{c.rect.x+c.rect.w/2,c.rect.y+c.rect.h/2};w.pointer(0,token,p);w.pointer(1,token,p);return;}throw std::runtime_error("missing control");}
void run_case(const std::string& name){
 auto owner=std::make_unique<Session>(image(),SessionOptions{true});auto app=std::make_unique<Workbench>(*owner);auto& s=*owner;auto& w=*app;
 if(name=="initial"){require(s.scene().time_us==0,"constructor stepped time");require(!w.running(),"auto-run");require(w.canvas().pixels().size()==1280*900*4,"framebuffer size");}
 else if(name=="render_purity"){ok(w,"power 1");ok(w,"run 80000");auto before=s.hello();auto hash=w.canvas().hash();for(int i=0;i<20;++i)w.render(16);require(before==s.hello(),"renderer mutated machine");(void)hash;w.render(0);auto stable=w.canvas().hash();w.render(0);require(w.canvas().hash()==stable,"static render nondeterminism");}
 else if(name=="key_latch"){w.key('3',true);w.key('3',false);require(s.scene().buttons->latched_mask==4,"key did not latch");w.key('6',true);w.key('6',false);require(s.scene().buttons->latched_mask==32,"mechanical interlock");}
 else if(name=="release_outside"){w.pointer(0,7,{530,660});require(s.scene().buttons->pulse_held,"pulse not down");w.pointer(1,7,{-100,-100});require(!s.scene().buttons->pulse_held,"release outside stranded PULSE");}
 else if(name=="cancel"){w.pointer(0,7,{590,660});require(s.scene().stop,"STOP not asserted");w.pointer(3,7,{-100,-100});require(!s.scene().stop,"cancel stranded STOP");}
 else if(name=="multisource"){w.pointer(0,7,{530,660});w.key('P',true);w.pointer(1,7,{0,0});require(s.scene().buttons->pulse_held,"released second holder");w.key('P',false);require(!s.scene().buttons->pulse_held,"last holder did not release");}
 else if(name=="held_through_stop"){w.key('P',true);w.key(' ',true);w.key(' ',false);auto st=s.scene();require(st.buttons->pulse_held&&st.buttons->pulse_blocked,"physical and contact state conflated");require(!st.run_permit,"STOP pulse block");for(int i=0;i<3;++i)w.step(20000);w.key('P',false);w.key('P',true);require(s.scene().run_permit,"fresh PULSE");}
 else if(name=="focus_release"){w.key('P',true);w.key(' ',true);click(w,Control::run);w.release_inputs(false);require(w.running()&&!s.scene().stop&&!s.scene().buttons->pulse_held,"focus release paused time or stranded input");}
 else if(name=="fixture_ownership"){ok(w,"stop 1");w.release_inputs();require(s.scene().stop,"blur released a fixture-owned STOP");ok(w,"stop 0");w.key(' ',true);w.release_inputs();require(!s.scene().stop,"blur failed to release GUI-owned STOP");}
 else if(name=="repeat_key"){w.key('R',true);w.key('R',true);require(w.running(),"repeat toggled run");w.key('R',false);w.key('R',true);require(!w.running(),"new press missing");}
 else if(name=="register_purity"){ok(w,"power 1");ok(w,"run 80000");ok(w,"write 0xA7 195");w.render(16);(void)w.status_json();ok(w,"write 0xA7 60");w.render(16);ok(w,"write 0xA8 165");require((s.scene().mcu.clock_status&8)==0,"observation broke clock key");}
 else if(name=="resized_hit"){w.resize(1920,1350);w.pointer(0,5,{(146+23)*1.5,(642+20)*1.5});w.pointer(1,5,{0,0});require(s.scene().buttons->latched_mask==1,"resized hit wrong");}
 else if(name=="letterbox_hit"){w.resize(1920,900);w.pointer(0,1,{5,650});w.pointer(1,1,{5,650});require(s.scene().buttons->latched_mask==0,"letterbox acts as control");}
 else if(name=="cadence"){ok(w,"power 1");click(w,Control::run);w.frame(10);w.frame(10);auto before=s.hello();auto samples=w.signals().size();app.reset();owner.reset();Session other(image(),{true});Workbench v(other);ok(v,"power 1");click(v,Control::run);v.frame(5);v.frame(5);v.frame(5);v.frame(5);require(before==other.hello(),"frame cadence changed physics");require(samples==v.signals().size(),"history depends on frame cadence");}
 else if(name=="pacing_bound"){click(w,Control::run);w.frame(1000);require(s.scene().time_us==20000,"unbounded catchup or skipped time");}
 else if(name=="history"){ok(w,"power 1");for(int i=0;i<14;++i)w.step(10000);require(w.signals().size()==1024,"history bound");for(std::size_t i=1;i<w.signals().size();++i)require(w.signals()[i].time_us-w.signals()[i-1].time_us==100,"wrong sample cadence");}
 else if(name=="sense_vs_mechanical"){ok(w,"contact 2 closed");w.step(10000);require(s.scene().contacts==4&&s.scene().buttons->latched_mask==0,"fault contact invented latch");}
 else if(name=="journal"){ok(w,"power 1");w.step(1000);w.step(1000);ok(w,"speed 4");w.step(1000);const auto j=w.journal_json();require(j.find("run 2000")!=std::string::npos,"run coalesce");require(j.find("\"complete\":true")!=std::string::npos,"journal completeness");auto before=s.hello();app.reset();owner.reset();Session other(image(),{true});for(auto c:{"power 1","run 2000","speed 4","run 1000"})require(other.execute(c).starts_with("{\"ok\":true"),"replay command");require(before==other.hello(),"journal fails replay");}
 else if(name=="invalid"){auto st=s.hello();for(auto value:{-1.,std::numeric_limits<double>::quiet_NaN(),1001.}){bool threw=false;try{w.frame(value);}catch(const std::invalid_argument&){threw=true;}require(threw,"bad interval accepted");}bool threw=false;try{w.resize(100000,900);}catch(const std::out_of_range&){threw=true;}require(threw&&st==s.hello(),"invalid size mutated machine");}
 else if(name=="pause_is_not_stop"){ok(w,"speed 5");w.release_inputs();require(s.scene().buttons->latched_mask==16,"pause released speed latch");}
 else if(name=="exhibit"){click(w,Control::exhibit);for(int i=0;i<80;++i)w.frame(10);require(s.scene().drive&&s.scene().motor->rpm>5000,"exhibit does not drive actual plant");require(s.scene().shaft_turns.has_value(),"missing phase");const auto rpm=s.scene().motor->rpm;click(w,Control::jar);require(!s.scene().drive,"jar animation delayed physical trip");require(s.scene().motor->rpm==rpm,"opening jar erased rotor momentum");w.frame(10);require(s.scene().motor->rpm<rpm&&s.scene().motor->rpm>0,"not coasting");}
 else if(name=="exhibit_finishes"){click(w,Control::exhibit);for(int i=0;i<210;++i)w.frame(10);require(!s.scene().drive&&s.scene().motor->rpm>0,"scheduled shutdown/coast");}
 else if(name=="view_no_time"){auto t=s.scene().time_us;click(w,Control::chassis);w.render(16);require(w.selected_view()==1&&s.scene().time_us==t,"view change steps time");}
 else if(name=="lcd_bus"){
  ok(w,"run 80000");ok(w,"write 0x80 1");ok(w,"write 0x81 129");
  const auto before=s.hello();click(w,Control::lcd_bus);for(unsigned i=0;i<10;++i)w.render(0);
  require(w.selected_view()==7&&s.hello()==before,"LCD page mutated machine or bus/address");
  const auto b=*s.scene().lcd_bus;require(b.data_size==1&&b.data[0].data==129,"page missing actual transfer");
  w.key('L',true);w.key('L',false);require(w.selected_view()==7,"LCD bus keyboard route");
  const auto trace=w.command("trace 20000 100");
  require(trace.find("\"captures_included\":false")!=std::string::npos,"dense trace repeated full captures");
  const auto full=trace.find("\"captures_included\":true");
  require(full!=std::string::npos&&trace.find("\"captures_included\":true",full+1)==std::string::npos,"trace final state missing or repeated full captures");
 }
 else if(name=="supervision"){
  ok(w,"run 80000");ok(w,"write 0xB0 1");ok(w,"write 0xB2 165");
  const auto before=s.hello();click(w,Control::supervision);w.render(0);(void)w.status_json();
  require(w.selected_view()==6&&s.hello()==before,"supervision page mutated machine or feed key");
  ok(w,"write 0xB2 90");require(s.scene().mcu.watchdog.services==1,"observer invalidated watchdog key");
  ok(w,"write 0xC0 1");ok(w,"run 70000");ok(w,"write 0xC8 105");
  const auto armed=s.hello();w.render(0);require(s.hello()==armed,"renderer consumed deadman key");
  ok(w,"write 0xC9 150");require(s.scene().mcu.deadman.services==1,"observer invalidated deadman key");
  ok(w,"write 0xB2 0");ok(w,"run 1000");ok(w,"write 0x02 255");ok(w,"write 0x03 255");w.render(0);
  const auto m=s.scene().mcu;require(m.reset_causes==0&&m.reset_history[m.reset_history_size-1].causes==4,"page lost acknowledged reset");
  w.key('U',true);w.key('U',false);require(w.selected_view()==6,"supervision keyboard route");
 }
 else if(name=="bounded_input"){for(int i=0;i<100;++i)w.pointer(0,i,{530,660});require(w.status_json().find("\"held_inputs\":32")!=std::string::npos,"unbounded input");w.release_inputs();require(!s.scene().buttons->pulse_held,"release all");}
 else if(name=="firmware_separation"){app.reset();owner.reset();Session candidate(image(),{});Workbench v(candidate);require(v.command("write 0x29 1").starts_with("{\"ok\":false"),"GUI granted register writes to firmware mode");click(v,Control::exhibit);require(candidate.scene().time_us==0&&!v.running(),"bench exhibit available in firmware mode");}
 else if(name=="canvas_bounds"){Canvas c(320,240);c.clear({0,0,0});c.rect({-500,-500,2500,2500},{7,8,9},20);c.ellipse({0,0},20,50,{80,90,100});c.text("ABC 123",{20,20},2,{255,255,255});require(c.pixels().size()==320*240*4,"pixel size changed");for(std::size_t i=3;i<c.pixels().size();i+=4)require(c.pixels()[i]==255,"alpha byte");}
 else if(name=="gestures_stress"){std::uint32_t seed=4815;ok(w,"power 1");w.resize(640,450);for(int i=0;i<250;++i){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;const int c=seed%10;w.key(c<7?'1'+c:c==7?'P':c==8?' ':'J',true);w.frame(0);w.key(c<7?'1'+c:c==7?'P':c==8?' ':'J',false);w.step(1000);require(!s.scene().drive,"starterless gestures invented drive");}w.release_inputs();}

 else if(name=="thermal_no_invented_adc"){
  require(w.plant_history().size()==1&&!w.plant_history().back().adc_c,"invented initial ADC reading");
  click(w,Control::thermal);w.render(0);require(!s.scene().mcu.temperature_adc_code,"renderer initiated ADC");
  app.reset();owner.reset();consume_adc=false;
  Session device({{},adc_reset,adc_step,"adc-reader"});Workbench gui(device);
  ok(gui,"power 1");ok(gui,"run 80000");gui.step(1000);
  require(!device.scene().mcu.temperature_adc_code,"conversion alone was called a firmware read");
  consume_adc=true;gui.step(1000);
  require(device.scene().mcu.temperature_adc_code.has_value(),"actual fresh read missing");
  // The next regular sample observes the read without consuming ADC registers.
  for(int i=0;i<5;++i)gui.step(20000);
  require(gui.plant_history().back().adc_c.has_value(),"measured trace missing real ADC read");
  require(gui.plant_history().back().time_us>=100000&&gui.plant_history().back().time_us<120000,"service-boundary deadline skipped plant sample");
  const auto held=*gui.plant_history().back().adc_c;ok(gui,"temperature 100");
  for(int i=0;i<15;++i)gui.step(20000);
  require(gui.plant_history().back().adc_c&&*gui.plant_history().back().adc_c==held,"held ADC changed without a read");
  require(*gui.plant_history().back().wire_c>held+10,"wire did not reflect lagged sensor response");
  auto before=device.hello();click(gui,Control::truth);gui.render(16);require(device.hello()==before,"truth observer mutated machine");
  ok(gui,"reset");require(!device.scene().mcu.temperature_adc_code&&!gui.plant_history().back().adc_c,"reset retained stale ADC measurement");
 }
 else if(name=="thermal_truth"){
  ok(w,"environment 22 5 0.15");ok(w,"temperature 85");
  for(int i=0;i<20;++i)w.step(20000);
  const auto& p=w.plant_history().back();
  require(p.case_c&&p.sensor_c&&p.air_c&&p.food_c&&p.room_c,"truth nodes missing");
  require(*p.case_c>*p.sensor_c&&*p.sensor_c>*p.air_c&&*p.air_c>*p.food_c,"lag or thermal topology wrong");
  require(*p.room_c==22,"room boundary wrong");
  ok(w,"environment 25 5 0");require(!w.plant_history().back().food_c,"absent food invented truth");
  const auto case_before=s.scene().motor->case_c,air_before=*s.scene().nearby_air_c;
  click(w,Control::thermal);click(w,Control::water);
  require(s.scene().food_c&&*s.scene().food_c==25,"GUI food addition missing");
  require(s.scene().motor->case_c==case_before&&*s.scene().nearby_air_c==air_before,"adding food reset other thermal nodes");
  click(w,Control::empty);require(!s.scene().food_c,"GUI food removal missing");
  const auto count=w.plant_history().size();const auto measured=w.canvas().hash();
  click(w,Control::truth);require(w.canvas().hash()==measured&&w.selected_view()==2,"thermal and truth should share one view");
  for(int i=0;i<10;++i)w.render(16);require(w.plant_history().size()==count,"render cadence creates samples");
 }
 else if(name=="plant_cadence"){
  click(w,Control::run);for(int i=0;i<12;++i)w.frame(20);
  const auto count=w.plant_history().size();require(count==3,"wrong 100ms history cadence");
  require(w.plant_history()[1].time_us==100000&&w.plant_history()[2].time_us==200000,"off-grid plant samples");
  click(w,Control::run);w.release_inputs();w.frame(100);require(w.plant_history().size()==count,"pause samples wall time");
 }
 else if(name=="all_subsystems"){
  auto before=s.hello();for(auto c:{Control::thermal,Control::truth,Control::motor,Control::systems,Control::supervision,Control::lcd_bus,Control::chassis,Control::appliance}){
   click(w,c);w.render(16);require(s.hello()==before,"subsystem page changes time or registers");
  }
  for(const auto& a:w.controls())for(const auto& b:w.controls())if(a.id!=b.id)
   require(!(a.rect.x<b.rect.x+b.rect.w&&a.rect.x+a.rect.w>b.rect.x&&a.rect.y<b.rect.y+b.rect.h&&a.rect.y+a.rect.h>b.rect.y),"overlapping controls");
 }
 else if(name=="stop_click"){
  ok(w,"run 80000");w.key(' ',true);w.key(' ',false);
  require(s.scene().stop,"zero-wall-time click lost STOP");
  w.step(20000);require(s.scene().stop,"STOP stroke too short");
  w.step(20000);w.step(20000);require(!s.scene().stop,"STOP stroke never released");
  const auto j=w.journal_json();require(j.find("stop 1")<j.find("run 50000")&&j.find("run 50000")<j.find("stop 0"),"STOP stroke journal order");
 }
 else if(name=="food_presets"){
  ok(w,"run 80000");ok(w,"temperature 50");
  const auto before=s.scene();
  for(auto c:{Control::water,Control::frozen,Control::vegetables}){
   click(w,c);const auto now=s.scene();
   require(now.time_us==before.time_us&&now.motor->case_c==before.motor->case_c&&now.nearby_air_c==before.nearby_air_c,"food reset other plant state");
   require(now.food_c&&now.food_capacity&&now.food_conductance,"food thermal properties absent");
   require(now.food_kind== (c==Control::water?"water":c==Control::frozen?"frozen_fruit":"hot_vegetables"),"wrong preset");
   require(*now.load==(c==Control::water?.15:c==Control::frozen?.8:.45),"missing mechanical load");
   require(*now.food_c==(c==Control::water?25:c==Control::frozen?-10:80),"missing food temperature");
  }
  const auto unchanged=s.hello();require(w.command("food_preset invalid").starts_with("{\"ok\":false"),"bad preset accepted");require(s.hello()==unchanged,"bad food mutated plant");
  ok(w,"load 0.6");require(s.scene().food_kind=="custom","manual load kept stale preset label");
  click(w,Control::empty);require(!s.scene().food_c&&s.scene().load==0,"empty must clear contents and load");
  require(w.journal_json().find("food_preset frozen_fruit")!=std::string::npos,"food missing from replay");
 }
 else if(name=="food_physics"){
  // Actual MD20 model: equal electrical input, different foods, no firmware policy.
  DigitalNet pwm,enable,tach1,tach2;
  const auto pd=pwm.attach(Logic::high),ed=enable.attach(Logic::high);(void)pd;(void)ed;
  ThermalNode a,b;Motor water(pwm,enable,tach1,a),fruit(pwm,enable,tach2,b);
  water.set_food_preset(food_presets[1]);fruit.set_food_preset(food_presets[2]);
  for(unsigned i=0;i<1000000;++i){water.advance_one_us();fruit.advance_one_us();}
  require(fruit.debug_rpm()<water.debug_rpm()*.75,"food load did not cause sag");
  require(fruit.thermal().last_load_current_a()>water.thermal().last_load_current_a(),"food load did not increase current");
  require(fruit.thermal().last_heat_w()>water.thermal().last_heat_w(),"food load did not increase heat");
  require(water.thermal().food_temperature_c()>25&&fruit.thermal().food_temperature_c()>-10,"food conduction did not integrate");
  ThermalNode small_node,large_node;LumpedThermal small(small_node),large(large_node);
  small.set_food(0,.15,600);large.set_food(0,.15,1200);small.advance(10,0,0,0,false);large.advance(10,0,0,0,false);
  require(small.food_temperature_c()>large.food_temperature_c()*1.9,"capacity not in thermal integration");
 }
 else if(name=="cold_injection"){
  click(w,Control::cool);require(s.scene().motor->case_c==5,"minus temperature was an absolute 25C reset");
  click(w,Control::cool);require(s.scene().motor->case_c==-15,"cannot inject cold case");
  for(int i=0;i<4;++i)click(w,Control::cool);require(s.scene().motor->case_c==-40,"cold fixture exceeded range");
 }
 else if(name=="bounce_capture"){
  ok(w,"run 80000");click(w,Control::speed2);w.step(20000);
  const auto captured=w.contact_capture();require(captured.size()>20,"bounce capture missing");
  unsigned edges=0;for(std::size_t i=1;i<captured.size();++i)edges+=captured[i].contacts!=captured[i-1].contacts;
  require(edges>=3,"bounce edges not captured");
  ok(w,"run 500000");w.step(20000);require(w.contact_capture().back().time_us==captured.back().time_us,"settled contact trace erased bounce");
  click(w,Control::bounce);require(s.scene().bounce_mode==2,"slow bounce control");
  click(w,Control::bounce);require(s.scene().bounce_mode==0,"bounce off control");
 }
 else throw std::runtime_error("unknown test");
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("case required");run_case(argv[1]);std::cout<<"PASS "<<argv[1]<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
