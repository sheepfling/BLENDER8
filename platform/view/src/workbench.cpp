#include "blender8/view/workbench.hpp"
#include "blender8/sim/fixture.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace b8::view {
namespace {
constexpr Color bg{15,24,32},panel{25,38,47},edge{45,62,71},text{223,233,230},muted{137,158,164};
constexpr Color blue{112,180,245},pink{232,136,189},violet{174,150,245};
constexpr Color mint{104,222,187},amber{242,182,96},dark{8,17,23},cream{210,207,193};
constexpr std::array<double,4> rates{.05,.25,1,4};
std::string num(double n,int places=0){std::ostringstream s;s.imbue(std::locale::classic());s<<std::fixed<<std::setprecision(places)<<n;return s.str();}
std::string hex(unsigned n){std::ostringstream s;s<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<n;return s.str();}
unsigned value(Control c){return static_cast<unsigned>(c);}
}
Workbench::Workbench(b8::sim::Session& s):session_(s),state_(s.scene()) {sample_plant();render();}
void Workbench::resize(unsigned w,unsigned h){canvas_.resize(w,h);render();}
std::vector<ControlBox> Workbench::controls()const{
 std::vector<ControlBox> c;
 for(unsigned i=0;i<7;++i)c.push_back({static_cast<Control>(i+1),{146.0+i*52,642,46,45},std::to_string(i+1),state_.buttons&&((state_.buttons->latched_mask&(1u<<i))!=0),true});
 c.push_back({Control::pulse,{510,642,46,45},"P",state_.buttons&&state_.buttons->pulse_held});
 c.push_back({Control::stop,{566,642,96,45},"STOP",state_.buttons&&state_.buttons->stop_held});
 c.insert(c.end(),{
 {Control::appliance,{24,100,120,36},"APPLIANCE",view_==0}, {Control::chassis,{152,100,128,36},"CHASSIS",view_==1},
 {Control::thermal,{288,100,114,36},"THERMAL",view_==2}, {Control::truth,{410,100,114,36},"TRUTH",view_==3},
 {Control::motor,{532,100,114,36},"MOTOR",view_==5}, {Control::systems,{654,100,124,36},"SYSTEMS",view_==4},
 {Control::run,{804,100,82,36},running_?"PAUSE":"RUN",running_}, {Control::step,{894,100,106,36},"STEP 1 MS"},
 {Control::rate,{1008,100,106,36},num(rates[rate_index_],rate_index_==0?2:rate_index_==1?2:0)+"X RATE"},
 {Control::reset,{1122,100,132,36},"MCU RESET"},
 {Control::power,{804,477,130,36},state_.rear_power.value_or(false)?"POWER ON":"POWER OFF",state_.rear_power.value_or(false),state_.rear_power.has_value()},
 {Control::jar,{946,477,130,36},state_.jar_seated?"JAR SEATED":"JAR LIFTED",state_.jar_seated},
 {Control::jam,{1088,477,142,36},state_.jammed.value_or(false)?"JAMMED":"JAM SHAFT",state_.jammed.value_or(false),state_.jammed.has_value()},
 {Control::load_down,{804,550,46,32},"-",false,state_.load.has_value()}, {Control::load_up,{1184,550,46,32},"+",false,state_.load.has_value()},
 {Control::hot,{804,632,130,34},"INJECT +20C",false,state_.motor.has_value()}, {Control::cool,{946,632,130,34},"INJECT 25C",false,state_.motor.has_value()},
 {Control::brownout,{1088,632,142,34},"BROWNOUT",state_.brownout_forced.value_or(false),state_.brownout_forced.has_value()},
 {Control::clock_loss,{804,676,130,34},"CLOCK LOSS",state_.clock_failed.value_or(false),state_.clock_failed.has_value()}, {Control::core_halt,{946,676,130,34},"CORE HALT",state_.core_halted},
 {Control::foreground,{1088,676,142,34},"NO FOREGROUND",!state_.foreground},
 {Control::sensor_open,{804,720,130,34},"SENSOR OPEN",state_.sensor_open.value_or(false),state_.sensor_open.has_value()},
 {Control::exhibit,{946,720,284,34},"2S MOTOR EXHIBIT",exhibit_until_>state_.time_us,session_.bench_mode()}
 });
 if(view_==2||view_==3)c.push_back({Control::food,{584,226,142,30},state_.food_present.value_or(false)?"REMOVE FOOD":"ADD 25C FOOD",state_.food_present.value_or(false),state_.food_present.has_value()});
 return c;
}
void Workbench::record(std::string_view c){
 if(!journal_complete_)return;
 if(c.starts_with("run ")&&!journal_.empty()&&journal_.back().starts_with("run ")){
   const auto total=std::stoull(journal_.back().substr(4))+std::stoull(std::string(c.substr(4)));
   if(total<=10'000'000){journal_.back()="run "+std::to_string(total);return;}
 }
 if(journal_.size()>=20000){journal_complete_=false;return;}
 journal_.emplace_back(c);
}
std::string Workbench::command(std::string_view c){
 const auto out=session_.execute(c);
 if(out.starts_with("{\"ok\":true")){record(c);error_.clear();}
 else {error_=out;running_=false;}
 state_=session_.scene();if(out.starts_with("{\"ok\":true"))sample_plant();return out;
}
void Workbench::advance(b8::sim::Tick duration){
 if(duration>20000)throw std::invalid_argument("view step is bounded to 20000 us");
 const auto end=state_.time_us+duration;
 if(end<state_.time_us)throw std::overflow_error("logical time overflow");
 while(state_.time_us<end){
   if(next_sample_<=state_.time_us)next_sample_=state_.time_us+(100-state_.time_us%100);
   const auto amount=std::min(end-state_.time_us,next_sample_-state_.time_us);
   session_.advance_view(amount);state_=session_.scene();
   if(state_.time_us>=next_plant_sample_){sample_plant();next_plant_sample_=state_.time_us+(100000-state_.time_us%100000);}
   if(state_.time_us>=next_sample_){signals_.push_back({state_.time_us,state_.contacts,state_.jar_ok,state_.jar_permit,state_.drive});if(signals_.size()>1024)signals_.pop_front();next_sample_=state_.time_us+(100-state_.time_us%100);}
 }
 if(duration)record("run "+std::to_string(duration));
}
void Workbench::step(b8::sim::Tick duration){running_=false;pacing_us_=0;advance(duration);render();}
void Workbench::frame(double ms){
 if(!std::isfinite(ms)||ms<0||ms>1000)throw std::invalid_argument("invalid frame elapsed interval");
 // Bounded real-time pacing. Excess wall-time never becomes skipped hardware cycles.
 if(running_){pacing_us_=std::min(20000.0,pacing_us_+std::min(ms,50.0)*1000*rates[rate_index_]);
 const auto d=static_cast<b8::sim::Tick>(pacing_us_);pacing_us_-=static_cast<double>(d);advance(d);}
 render(std::min(ms,50.0));
}
void Workbench::press(int token,Control id){
 if(held_.contains(token)||held_.size()>=32)return;
 held_[token]=id;
 if(id==Control::pulse||id==Control::stop)sync_momentary();else activate(id);
}
void Workbench::release(int token){auto it=held_.find(token);if(it==held_.end())return;auto c=it->second;held_.erase(it);if(c==Control::pulse||c==Control::stop)sync_momentary();}
void Workbench::sync_momentary(){
 bool p=false,s=false;for(const auto&[token,c]:held_){(void)token;p|=c==Control::pulse;s|=c==Control::stop;}
 // Assert STOP first, release it last, when both gestures change together.
 if(s&&!sent_stop_)(void)command("stop 1");
 const bool old=sent_pulse_;
 if(p!=old)(void)command(std::string("pulse ")+(p?"1":"0"));
 if(!s&&sent_stop_)(void)command("stop 0");
 sent_pulse_=p;sent_stop_=s;
}
void Workbench::release_inputs(){
 held_.clear();sync_momentary();running_=false;pacing_us_=0;render();
}
void Workbench::pointer(unsigned type,int id,Point p){
 if(type>3||id<0||!std::isfinite(p.x)||!std::isfinite(p.y)||std::abs(p.x)>100000||std::abs(p.y)>100000)throw std::invalid_argument("invalid pointer event");
 if(type==1||type==3){release(id);render();return;}
 if(type==2)return;
 const auto q=canvas_.logical(p);
 for(const auto& c:controls())if(c.enabled&&c.rect.contains(q)){press(id,c.id);render();return;}
 if(view_==0&&Rect{252,190,302,293}.contains(q))activate(Control::jar);
 render();
}
void Workbench::key(int ascii,bool down){
 if(ascii<0||ascii>127)throw std::invalid_argument("ASCII input expected");
 const int token=-ascii-1;if(!down){release(token);render();return;}
 std::optional<Control> c;
 if(ascii>='1'&&ascii<='7')c=static_cast<Control>(ascii-'0');
 else switch(ascii){case 'P':case 'p':c=Control::pulse;break;case ' ':c=Control::stop;break;
 case 'J':case 'j':c=Control::jar;break;case 'O':case 'o':c=Control::power;break;
 case 'R':case 'r':c=Control::run;break;
 case 'T':case 't':c=Control::thermal;break;case 'Y':case 'y':c=Control::truth;break;
 case 'M':case 'm':c=Control::motor;break;case 'S':case 's':c=Control::systems;break;case 'V':case 'v':c=view_?Control::appliance:Control::chassis;break;default:break;}
 if(c)press(token,*c);
 render();
}
void Workbench::activate(Control id){
 const auto n=value(id);if(n>=1&&n<=7){(void)command("speed "+std::to_string(n));return;}
 switch(id){
 case Control::appliance:view_=0;break;case Control::chassis:view_=1;break;
 case Control::thermal:view_=2;break;case Control::truth:view_=3;break;
 case Control::systems:view_=4;break;case Control::motor:view_=5;break;
 case Control::run:running_=!running_;pacing_us_=0;break;
 case Control::step:step(1000);break;case Control::rate:rate_index_=(rate_index_+1)%rates.size();break;
 case Control::reset:(void)command("reset");break;
 case Control::power:(void)command(std::string("power ")+(state_.rear_power.value_or(false)?"0":"1"));break;
 case Control::jar:(void)command(std::string("jar ")+(state_.jar_seated?"0":"1"));break;
 case Control::jam:(void)command(std::string("jam ")+(state_.jammed.value_or(false)?"0":"1"));break;
 case Control::load_down:case Control::load_up:if(state_.load)(void)command("load "+num(std::clamp(*state_.load+(id==Control::load_up?.1:-.1),0.,1.),2));break;
 case Control::hot:if(state_.motor)(void)command("temperature "+num(std::min(150.,state_.motor->case_c+20),2));break;
 case Control::cool:(void)command("temperature 25");break;
 case Control::brownout:(void)command(state_.brownout_forced.value_or(false)?"voltage auto":"voltage 2.5");break;
 case Control::clock_loss:(void)command(state_.clock_failed.value_or(false)?"clock_failed 0":"clock_failed 1");break;
 case Control::core_halt:(void)command(state_.core_halted?"halt 0":"halt 1");break;
 case Control::foreground:(void)command(state_.foreground?"foreground 0":"foreground 1");break;
 case Control::sensor_open:(void)command(state_.sensor_open.value_or(false)?"sensor healthy":"sensor open");break;
 case Control::food:(void)command(state_.food_present.value_or(false)?"food 25 0":"food 25 0.15");break;
 case Control::exhibit:start_exhibit();break;default:break;
 }
}
void Workbench::start_exhibit(){
 if(!session_.bench_mode()||exhibit_until_>state_.time_us)return;
 // Explicit host experiment, not supplied firmware. Every write is journaled and uses the
 // exact bench protocol. Hardware permissions still apply. No hidden feed in normal mode.
 if(state_.time_us!=0){error_="EXHIBIT NEEDS A FRESH BENCH SESSION";return;}
 (void)command("power 1");(void)command("jar 1");
 auto schedule=[&](unsigned t,std::string c){(void)command("schedule "+std::to_string(t)+" "+c);};
 schedule(80000,"stop 1");schedule(90000,"stop 0");schedule(95000,"speed 4");
 if(session_.device_name()=="B16"){
  schedule(99990,"write 0xE8 90");schedule(99991,"write 0xE8 165");
  schedule(99992,"write 0x28 33");schedule(99993,"write 0xEB 2");
  schedule(99994,"write 0xEC 1");schedule(99995,"write 0xED 1");schedule(99996,"write 0xE9 1");
 }
 schedule(100000,session_.device_name()=="B16"?"write 0x28 33":"write 0x28 1");schedule(100001,"write 0x29 1");
 schedule(100002,"write 0x61 175");schedule(100003,"write 0x60 1");
 schedule(800000,"write 0x61 220");schedule(1400000,"write 0x61 140");
 schedule(2000000,"write 0x29 0");schedule(2000001,"write 0x60 0");schedule(2000002,"write 0x61 0");
 for(unsigned t=100000;t<3000000;t+=50000){schedule(t+10,"write 0xB2 165");schedule(t+11,"write 0xB2 90");}
 exhibit_until_=3000000;running_=true;
}
void Workbench::card(Rect r,std::string_view title,std::string_view v,std::string_view suffix,Color c){
 canvas_.rect(r,dark,10);canvas_.rect({r.x,r.y,3,r.h},c,1);canvas_.text(title,{r.x+16,r.y+13},1.4,muted);
 canvas_.text(v,{r.x+16,r.y+36},3.3,c);canvas_.text(suffix,{r.x+16,r.y+r.h-19},1.2,muted);
}
void Workbench::draw_lcd(Rect r){
 canvas_.rect({r.x-8,r.y-8,r.w+16,r.h+16},{48,60,59},9);canvas_.rect(r,{131,157,127},2);
 const double sx=r.w/32,sy=r.h/16;
 if(state_.pixels){for(unsigned y=0;y<16;++y)for(unsigned x=0;x<32;++x){const bool on=(*state_.pixels)[y*32+x];canvas_.rect({r.x+x*sx+.4,r.y+y*sy+.4,sx-.8,sy-.8},on?Color{29,47,35}:Color{126,151,122});}}
 else canvas_.text("NO PIXEL PROBE",{r.x+5,r.y+r.h/2},1, dark);
}
void Workbench::draw_appliance(){
 canvas_.text("BLENDER-8",{48,181},2.8,text);canvas_.text("LIVE CONTROLS",{48,210},1.3,muted);
 canvas_.ellipse({405,714},268,20,{4,10,14,190});
 canvas_.rect({161,683,62,24},{5,12,15},7);canvas_.rect({577,683,62,24},{5,12,15},7);
 canvas_.rect({127,489,558,200},{126,133,129},32);canvas_.rect({120,481,558,200},cream,32);
 canvas_.rect({141,502,516,119},{188,190,177},18);canvas_.line({146,500},{650,500},{234,232,214},2);
 canvas_.rect({288,467,234,30},{94,116,113},8);canvas_.ellipse({405,470},108,19,{128,155,148});
 const double lift=jar_lift_;
 // Handle and jug are deliberately geometric drawings, not independent physics.
 canvas_.rect({510,275-lift,70,145},{91,126,130},23);canvas_.rect({526,292-lift,34,110},panel,12);
 const std::array<Point,6> jar{{{268,235-lift},{533,235-lift},{514,420-lift},{485,470-lift},{318,470-lift},{290,420-lift}}};
 canvas_.polygon(jar,{79,125,132});
 const std::array<Point,6> glass{{{277,244-lift},{524,244-lift},{505,417-lift},{480,460-lift},{323,460-lift},{299,417-lift}}};
 canvas_.polygon(glass,{104,158,159});
 const std::array<Point,4> shine{{{301,255-lift},{321,255-lift},{340,444-lift},{324,433-lift}}};canvas_.polygon(shine,{202,228,210,85});
 canvas_.line({490,260-lift},{474,428-lift},{205,233,215,80},3);
 // Volume marks are decoration; no fluid model or invented swirl-induced loading.
 for(unsigned i=0;i<4;++i){double y=280+i*37-lift;canvas_.line({436,y},{464,y},{199,222,203},2);canvas_.text(std::to_string(4-i),{475,y-4},1,{186,210,197});}
 canvas_.rect({269,229-lift,267,20},{46,65,67},7);canvas_.rect({297,213-lift,206,18},{77,103,105},7);canvas_.rect({373,202-lift,50,15},{92,120,117},5);
 canvas_.ellipse({403,444-lift},66,15,{48,83,85});
 if(state_.shaft_turns){
   const double theta=*state_.shaft_turns*6.283185307179586;
   if(state_.motor&&state_.motor->rpm>1200){canvas_.ellipse({403,444-lift},57,11,{178,211,194,110});}
   else for(unsigned i=0;i<4;++i){double a=theta+i*1.5707963267948966;canvas_.line({403,444-lift},{403+54*std::cos(a),444-lift+11*std::sin(a)},{208,222,210},5);}
   canvas_.ellipse({403,444-lift},9,4,{233,237,220});
 }
 else canvas_.text("PHASE N/A",{370,441-lift},1.1,muted);
 canvas_.text("Half-A/Labs",{163,539},1.8,{58,77,72});canvas_.text("CONTROL CHASSIS 04",{164,562},1,{89,107,98});
 draw_lcd({338,520,160,80});
 canvas_.ellipse({572,540},9,9,state_.drive?mint:Color{98,108,94});canvas_.text("DRIVE",{549,560},1.3,{58,77,72});
 canvas_.ellipse({572,586},7,7,state_.jar_permit?mint:amber);
 canvas_.text(state_.jar_permit?"ARMED":"INTERLOCK",{533,603},1,{58,77,72});
 canvas_.text(state_.jar_seated?"JAR SEATED / CLICK JAR TO LIFT":"JAR LIFTED / CLICK TO RESEAT",{235,725},1.25,muted);
 canvas_.text("SHAFT VIEW: NO SEPARATE BLADE / FLUID MODEL",{210,746},1.0,muted);
}
void Workbench::wire(Point a,Point b,bool high){
 const Color c=high?mint:Color{69,89,97};canvas_.line(a,b,c,2);
 // Traveling highlight is a signal activity indicator, not an electrical propagation model.
 if(high){const double f=static_cast<double>(state_.time_us%800000)/800000.;canvas_.ellipse({a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f},3,3,text);}
}
void Workbench::draw_chassis(){
 canvas_.text("OPEN CHASSIS",{48,181},2.6,text);canvas_.text("ONE MACHINE / NON-INVASIVE OBSERVATIONS",{48,210},1.3,muted);
 auto block=[&](Rect r,std::string_view title,std::string_view sub,Color accent){canvas_.rect(r,dark,9);canvas_.rect({r.x,r.y,4,r.h},accent,1);canvas_.text(title,{r.x+13,r.y+14},1.8,accent);canvas_.text(sub,{r.x+13,r.y+39},1.15,muted);};
 wire({197,288},{294,288},state_.contacts!=0);wire({424,289},{470,359},state_.gpioa[3]==b8::sim::Logic::high);
 wire({364,365},{354,316},(state_.mcu.gpioa_out&7)!=0);wire({556,352},{600,289},state_.ready);
 wire({548,416},{590,482},state_.pwm);wire({637,542},{491,416},state_.gpiob[1]==b8::sim::Logic::high);
 canvas_.line({580,538},{440,569},{108,118,124},2);wire({387,550},{393,416},state_.analog_v>.2);
 wire({208,452},{319,393},state_.clock_valid);
 wire({169,547},{300,468},state_.jar_ok);wire({394,490},{580,504},state_.jar_permit&&state_.run_permit);
 block({61,255,151,75},"BA-8","CONTACTS 0-7",amber);block({287,255,143,75},"MX8-1","SELECT "+std::to_string(state_.mcu.gpioa_out&7),mint);
 block({319,348,229,77},"NORTHSTAR "+std::string(session_.device_name()),"SYS "+num(state_.mcu.system_hz/1e6,2)+" / PB "+num(state_.mcu.peripheral_hz/1e6,2),mint);
 block({571,253,143,76},"PX32-16","50 HZ SCANOUT",text);draw_lcd({596,339,96,48});
 block({577,477,137,68},"MD20",state_.drive?"DRIVE ENABLED":"COAST / OFF",state_.drive?mint:muted);
 block({306,546,165,66},"AVT10",num(state_.analog_v,3)+" V OUT",amber);
 block({61,417,157,68},"XO8-33","EXTERNAL SOURCE",mint);
 block({63,520,157,72},"S2 + U_IL",state_.jar_permit?"PERMIT LATCHED":"DISARMED",state_.jar_permit?mint:amber);
 canvas_.text("ACTUAL DRIVE GATE",{318,448},1.3,muted);canvas_.ellipse({398,482},18,18,state_.drive?mint:edge);canvas_.text("&",{392,476},1.4,dark);
 canvas_.text("POWER  RESET  STOP  JAR  REQUEST",{267,507},1.1,muted);
 canvas_.text("THE BUTTON BANK BELOW IS THE SAME PHYSICAL ASSEMBLY",{80,617},1.2,muted);
 canvas_.text("HIGHLIGHTS SHOW LEVELS; NO PROPAGATION SPEED IS IMPLIED",{65,731},1.15,muted);
}
void Workbench::draw_controls(){
 for(const auto& c:controls()){
   const bool physical=value(c.id)>=1&&value(c.id)<=9;
   double travel=physical?button_travel_[value(c.id)-1]:0;
   if(physical){canvas_.rect(c.rect,{47,60,56},7);const auto y=c.rect.y+travel*7;
     Color base=c.id==Control::stop?Color{167,68,59}:c.id==Control::pulse?Color{188,156,77}:Color{222,221,204};
     if(c.active)base=c.id==Control::stop?Color{225,107,87}:mint;
     canvas_.rect({c.rect.x,y,c.rect.w,c.rect.h-7},base,6);canvas_.line({c.rect.x+6,y+4},{c.rect.x+c.rect.w-6,y+4},{255,255,239,120},1);
     canvas_.text(c.label,{c.rect.x+(c.rect.w-c.label.size()*12)/2,y+13},2,{30,44,43});
   }else{
     canvas_.rect(c.rect,c.active?Color{40,79,76}:edge,6);
     canvas_.text(c.label,{c.rect.x+10,c.rect.y+12},c.rect.w<70?1.8:1.25,c.enabled?(c.active?mint:text):Color{84,101,108});
   }
 }
}
void Workbench::draw_instruments(){
 canvas_.text(view_==2?"MEASURED SIGNALS":"HOST INSTRUMENTS",{804,179},1.8,text);canvas_.text("NOT AVAILABLE TO FIRMWARE",{804,201},1.15,muted);
 if(view_==2){
  card({800,225,208,111},"AN0 WIRE",num(state_.analog_v,3),"VOLTS / HOST VOLTMETER",mint);
  card({1022,225,208,111},"LAST AN0 READ",state_.mcu.temperature_adc_code?std::to_string(*state_.mcu.temperature_adc_code):"N/A",
       state_.mcu.temperature_adc_code?"AGE "+num((state_.time_us-state_.mcu.temperature_adc_us)/1e6,3)+" S":"NO FRESH ADC READ",blue);
 }else{
  card({800,225,208,111},"SHAFT SPEED",state_.motor?num(state_.motor->rpm):"N/A","RPM / PLANT OBSERVATION",mint);
  const std::string thermal_suffix=state_.nearby_air_c?
      "AIR "+num(*state_.nearby_air_c,1)+"  FOOD "+(state_.food_c?num(*state_.food_c,1):"N/A"):
      "NO THERMAL PROBE";
  card({1022,225,208,111},"CASE TEMPERATURE",state_.motor?num(state_.motor->case_c,1):"N/A",thermal_suffix,amber);
 }
 canvas_.text("GPIO A",{805,356},1.25,muted);canvas_.text(hex(state_.mcu.gpioa_out),{870,350},2.6,text);
 canvas_.text("GPIO B",{941,356},1.25,muted);canvas_.text(hex(state_.mcu.gpiob_out),{1006,350},2.6,text);
 canvas_.text("TACH",{1074,356},1.25,muted);canvas_.text(std::to_string(state_.mcu.tach_count),{1120,354},1.8,text);
 canvas_.text("3V3 "+num(state_.rail_v,2)+"V   AN0 "+num(state_.analog_v,3)+"V",{805,387},1.45,text);
 canvas_.text("PWM "+(state_.motor?num(state_.motor->effective_duty*100,1)+"%":"N/A")+"   RESET "+hex(state_.mcu.reset_causes),{805,414},1.45,text);
 canvas_.text("PHYSICAL CONTROLS",{804,449},1.5,muted);
 canvas_.text("MECHANICAL LOAD",{804,529},1.35,muted);canvas_.rect({867,550,300,30},dark,5);
 if(state_.load)canvas_.rect({867,550,300**state_.load,30},{53,111,101},5);
 canvas_.text(state_.load?num(*state_.load*100)+"%":"UNAVAILABLE",{982,560},1.25,text);
 canvas_.text("FAULT INJECTION / BENCH EXHIBIT",{804,607},1.3,muted);
}
void Workbench::draw_signals(){
 canvas_.rect({24,780,1206,97},dark,9);canvas_.text("CONTACT SENSE",{42,795},1.5,text);
 canvas_.text("100 US SAMPLES",{42,817},1.1,muted);canvas_.text("LAST 102.4 MS",{42,833},1.1,muted);
 canvas_.text("ACTIVE LOW",{42,850},1.1,amber);
 for(unsigned ch=0;ch<8;++ch){double y=790+ch*10;canvas_.text(ch==7?"P":"C"+std::to_string(ch),{196,y},1,muted);
 canvas_.line({228,y+6},{1215,y+6},edge,1);if(signals_.empty())continue;
 double prev_y=y+((signals_.front().contacts&(1u<<ch))?6:0),prev_x=228;
 // Decimate only presentation if needed, never the modeled contacts or sample record.
 for(std::size_t i=0;i<signals_.size();++i){double x=228+987*static_cast<double>(i)/1024.;double yy=y+((signals_[i].contacts&(1u<<ch))?6:0);
 canvas_.line({prev_x,prev_y},{x,prev_y},ch==7?amber:mint,1);if(yy!=prev_y)canvas_.line({x,prev_y},{x,yy},ch==7?amber:mint,1);prev_y=yy;prev_x=x;}
 }
}
void Workbench::sample_plant(){
 PlantSample p{};p.time_us=state_.time_us;p.wire_c=(state_.analog_v-.5)*100.;
 if(state_.mcu.temperature_adc_code)p.adc_c=(static_cast<double>(*state_.mcu.temperature_adc_code)*3.3/1023.-.5)*100.;
 if(state_.motor){p.case_c=state_.motor->case_c;p.rpm=state_.motor->rpm;p.loss_w=state_.motor->loss_w;p.duty=state_.motor->effective_duty*100.;}
 p.sensor_c=state_.sensor_c;p.air_c=state_.nearby_air_c;p.food_c=state_.food_c;p.room_c=state_.room_c;p.current_a=state_.load_current_a;
 if(!plant_.empty()&&plant_.back().time_us==p.time_us)plant_.back()=p;
 else plant_.push_back(p);
 while(plant_.size()>6001||(!plant_.empty()&&p.time_us-plant_.front().time_us>600000000))plant_.pop_front();
}
void Workbench::plot(Rect r,std::initializer_list<Trace> traces,std::string_view unit){
 canvas_.rect({r.x-4,r.y-4,r.w+8,r.h+8},dark,5);
 double lo=1e9,hi=-1e9;
 for(const auto& p:plant_)for(const auto& t:traces)if(const auto v=p.*(t.member);v&&std::isfinite(*v)){lo=std::min(lo,*v);hi=std::max(hi,*v);}
 if(hi<lo){lo=0;hi=1;}else{const double pad=std::max(1.,(hi-lo)*.1);lo-=pad;hi+=pad;}
 const auto begin=plant_.empty()?state_.time_us:plant_.front().time_us;
 const double span=std::max(1000000.,static_cast<double>(state_.time_us-begin));
 for(unsigned i=0;i<5;++i){const double y=r.y+r.h*i/4.;canvas_.line({r.x,y},{r.x+r.w,y},edge,1);
  canvas_.text(num(hi-(hi-lo)*i/4.,1),{r.x-49,y-3},1,muted);}
 for(unsigned i=0;i<5;++i){const double x=r.x+r.w*i/4.;canvas_.line({x,r.y},{x,r.y+r.h},edge,1);
  canvas_.text(num(begin/1e6+span/1e6*i/4.,2),{x-12,r.y+r.h+12},1,muted);}
 unsigned legend=0;
 for(const auto& t:traces){const double lx=48+(legend%3)*228,ly=r.y-61+(legend/3)*17;
  canvas_.line({lx,ly+4},{lx+14,ly+4},t.color,3);canvas_.text(t.name,{lx+21,ly},1.1,t.color);++legend;
  std::optional<Point> previous; b8::sim::Tick previous_time=0;
  for(const auto& p:plant_){const auto v=p.*(t.member);if(!v||!std::isfinite(*v)){previous.reset();continue;}
   Point q{r.x+r.w*static_cast<double>(p.time_us-begin)/span,r.y+r.h*(hi-*v)/(hi-lo)};
   if(previous&&p.time_us-previous_time<=120000){
    if(t.hold){canvas_.line(*previous,{q.x,previous->y},t.color,2);canvas_.line({q.x,previous->y},q,t.color,2);}
    else canvas_.line(*previous,q,t.color,2);
   }else canvas_.ellipse(q,2,2,t.color);
   previous=q;previous_time=p.time_us;
  }
 }
 canvas_.text(unit,{r.x,r.y-17},1,muted);canvas_.text("LOGICAL TIME / S",{r.x+r.w-125,r.y+r.h+28},1,muted);
}
void Workbench::draw_thermal(bool truth){
 canvas_.text(truth?"THERMAL / TRUTH OVERLAY":"THERMAL / MEASURED",{48,181},2.2,text);
 canvas_.text(truth?"MODEL STATES PLUS THE SAME MEASURED SIGNALS":"SENSOR WIRE AND ACTUAL FIRMWARE ADC READS",{48,210},1.2,muted);
 const auto adc=state_.mcu.temperature_adc_code;
 canvas_.text("WIRE "+num((state_.analog_v-.5)*100.,2)+" C    ADC "+(adc?num((*adc*3.3/1023.-.5)*100.,2)+" C":"NO READ"),{48,235},1.6,mint);
 if(truth)plot({94,332,628,245},{{"AN0 NOMINAL ESTIMATE",mint,&PlantSample::wire_c},
  {"LAST ADC READ / HELD",blue,&PlantSample::adc_c,true},{"CASE / TRUTH",amber,&PlantSample::case_c},
  {"SENSOR LAG / TRUTH",pink,&PlantSample::sensor_c},{"LOCAL AIR / TRUTH",violet,&PlantSample::air_c},
  {"FOOD / IF PRESENT",cream,&PlantSample::food_c},{"ROOM / BOUNDARY",muted,&PlantSample::room_c}},"TEMPERATURE / C");
 else plot({94,332,628,245},{{"AN0 NOMINAL ESTIMATE",mint,&PlantSample::wire_c},
  {"LAST ADC READ / HELD",blue,&PlantSample::adc_c,true}},"TEMPERATURE / C");
 if(truth){
  auto v=[](std::optional<double> n){return n?num(*n,2):"N/A";};
  canvas_.text("HEAT W: CASE>AIR "+v(state_.case_air_w)+"  CASE>FOOD "+v(state_.case_food_w),{48,619},1.1,amber);
  canvas_.text("FOOD>AIR "+v(state_.food_air_w)+"  AIR>ROOM "+v(state_.ventilation_w),{48,715},1.2,muted);
  canvas_.text("LUMPED CONDUCTION + SPEED-DEPENDENT AIR LOSS; NO FLUID MODEL",{48,739},1.0,muted);
 }else{
  canvas_.text("NOMINAL: 3.3V ADC REFERENCE; AVT10 0.5V + 0.010 V/C",{48,619},1.1,muted);
  canvas_.text("OPEN / SHORT / NOISE CAN MAKE THIS ESTIMATE WRONG",{48,715},1.2,amber);
  canvas_.text("ADC IS N/A UNTIL A FRESH AN0 READ; HELD VALUES MAY BE STALE",{48,739},1.0,muted);
 }
 canvas_.text("100 MS SNAPSHOTS / LAST 10 MIN / GAPS ARE NOT INTERPOLATED",{48,760},1.0,muted);
}
void Workbench::draw_motor(){
 canvas_.text("MOTOR / ELECTROMECHANICAL",{48,181},2.1,text);
 canvas_.text("PWM + PERMISSIONS > ROTATION > CURRENT + HEATING",{48,210},1.2,muted);
 plot({94,290,628,100},{{"SHAFT / PLANT TRUTH",mint,&PlantSample::rpm}},"SPEED / RPM");
 plot({94,485,628,100},{{"GENERATED CASE HEAT",amber,&PlantSample::loss_w}},"THERMAL INPUT / W");
 canvas_.text("CURRENT "+(state_.load_current_a?num(*state_.load_current_a,2)+" A":"N/A")+
  "   DUTY "+(state_.motor?num(state_.motor->effective_duty*100.,1)+"%":"N/A"),{48,623},1.2,text);
 canvas_.text("LOAD + JAM + POWER + STOP + JAR AFFECT THE SAME PLANT",{48,715},1.15,muted);
 canvas_.text("100 MS SNAPSHOTS / LAST 10 MIN / NO SEPARATE BLADE MODEL",{48,739},1.05,muted);
}
void Workbench::draw_systems(){
 canvas_.text("SIMULATED SUBSYSTEMS",{48,181},2.3,text);
 canvas_.text("LIVE OBSERVATIONS / NO REGISTER READ SIDE EFFECTS",{48,210},1.2,muted);
 auto block=[&](unsigned i,std::string_view name,const std::string& a,const std::string& b){
  const double x=48+(i%2)*346,y=233+(i/2)*77;
  canvas_.rect({x,y,332,68},dark,6);canvas_.text(name,{x+12,y+10},1.3,mint);
  canvas_.text(a,{x+12,y+29},1.05,text);canvas_.text(b,{x+12,y+47},1.0,muted);
 };
 block(0,"BUTTONS + MX8", "CONTACTS "+hex(state_.contacts)+" / SELECT "+std::to_string(state_.mcu.gpioa_out&7),state_.buttons?"LATCH + BOUNCE + PULSE BLOCK":"NO MECHANICAL PROBE");
 block(1,"JAR + HARD DRIVE GATE",std::string(state_.jar_seated?"SEATED":"LIFTED")+" / "+(state_.jar_permit?"PERMITTED":"DISARMED"),state_.drive?"DRIVE ENABLED":"DRIVE INHIBITED");
 block(2,"POWER + SUPERVISION",num(state_.rail_v,3)+" V / RESET "+hex(state_.mcu.reset_causes),"WDT "+hex(state_.mcu.wdt_control)+" / DMT "+std::to_string(state_.mcu.dmt_enabled));
 block(3,"CLOCKS + CORE", "SYS "+num(state_.mcu.system_hz/1e6,2)+" / PB "+num(state_.mcu.peripheral_hz/1e6,2)+" MHZ","T0 "+std::to_string(state_.mcu.timer_counts[0])+"/"+std::to_string(state_.mcu.timer_compare[0])+" T1 "+std::to_string(state_.mcu.timer_counts[1])+"/"+std::to_string(state_.mcu.timer_compare[1])+(state_.core_halted?" HALT":" RUN"));
 block(4,"MOTOR + TACH",state_.motor?num(state_.motor->rpm)+" RPM":"NO MOTOR PROBE","TACH "+std::to_string(state_.mcu.tach_count)+" / PWM "+std::to_string(state_.mcu.pwm_active));
 block(5,"THERMAL NETWORK",state_.motor?num(state_.motor->case_c,2)+" C CASE":"NO THERMAL PROBE",state_.food_present.value_or(false)?"CASE + AIR + FOOD + ROOM":"CASE + AIR + ROOM / NO FOOD");
 block(6,"AVT10 + ADC",num(state_.analog_v,3)+" V / ADC STATUS "+hex(state_.mcu.adc_status),"SENSOR LAG + NOISE + FAULTS");
 block(7,"PX32-16 + XBUS",state_.lcd_vblank?"VBLANK HIGH":"ACTIVE SCANOUT",state_.pixels?"512 PIXELS / BUS + BUSY TIMING":"NO PIXEL PROBE");
 block(8,"GPIO + INTERRUPTS","A DIR "+hex(state_.mcu.gpioa_dir)+" / B DIR "+hex(state_.mcu.gpiob_dir),"IRQ FLAGS "+hex(state_.mcu.irq_flags)+" / ENABLE "+hex(state_.mcu.irq_enable));
 block(9,"B16 PIN ROUTES + DMAC",state_.dma?"DMA STATUS "+hex(state_.dma->status)+" / ERROR "+hex(state_.dma->error):"NOT FITTED ON B8",state_.dma?"ROUTES "+std::to_string(state_.mcu.pin_routes[0])+std::to_string(state_.mcu.pin_routes[1])+std::to_string(state_.mcu.pin_routes[2])+std::to_string(state_.mcu.pin_routes[3])+(state_.mcu.pin_locked?" LOCK / ":" OPEN / ")+std::to_string(state_.dma->left)+" LEFT":"B8 FIXED PERIPHERAL ROLES");
 canvas_.text("ALL VIEWS OBSERVE ONE MACHINE; SWITCHING PAGES DOES NOT STEP IT",{48,715},1.05,muted);
 canvas_.text("PLANT TRUTH IS HOST-ONLY; NO FLUID / CONTACT-FORCE / MCU ISA MODEL",{48,739},1.0,muted);
}
void Workbench::render(double ms){
 state_=session_.scene();const double mix=ms<=0?0:1-std::exp(-ms/45.);
 jar_lift_+=((state_.jar_seated?0.0:40.0)-jar_lift_)*mix; // Jar state is authoritative; no delayed interlock actuation.
 for(unsigned i=0;i<9;++i){bool down=false;if(state_.buttons){down=i<7?(state_.buttons->latched_mask&(1u<<i))!=0:i==7?state_.buttons->pulse_held:state_.buttons->stop_held;}
 button_travel_[i]+=(static_cast<double>(down)-button_travel_[i])*mix;}
 canvas_.clear(bg);canvas_.rect({0,0,1280,82},dark);canvas_.rect({24,22,7,38},mint,2);
 canvas_.text("Half-A/Labs",{44,21},3,text);canvas_.text(std::string(session_.device_name())+" / VIRTUAL APPLIANCE WORKBENCH",{44,51},1.3,muted);
 canvas_.text("C++ CORE + C++ PIXELS",{815,23},1.6,mint);
 const std::string mode=session_.bench_mode()?"COMPONENT BENCH / NO FIRMWARE":session_.firmware_name().find("probe")!=std::string::npos?"PIXEL PROBE / MOTOR OFF":"SELECTED FIRMWARE";
 canvas_.text(mode,{815,48},1.2,session_.bench_mode()?amber:muted);
 canvas_.rect({24,155,730,608},panel,13);canvas_.rect({780,155,474,608},panel,13);
 if(view_==0)draw_appliance();else if(view_==1)draw_chassis();
 else if(view_==2||view_==3)draw_thermal(view_==3);else if(view_==4)draw_systems();else draw_motor();
 draw_instruments();draw_controls();draw_signals();
 canvas_.text("T "+num(state_.time_us/1e6,3)+" S  |  "+(running_?"RUNNING":"PAUSED")+"  |  1-7 SPEED   P PULSE   SPACE STOP   J JAR   O POWER   R RUN   V VIEW   T THERMAL   Y TRUTH   M MOTOR   S SYSTEMS",{28,883},1.15,muted);
 if(!error_.empty()){canvas_.rect({26,148,1228,30},{98,41,41});canvas_.text(error_.substr(0,110),{38,159},1.3,text);}
}
std::string Workbench::status_json(){
 std::ostringstream s;s.imbue(std::locale::classic());s<<std::boolalpha<<"{\"ok\":true,\"view\":{";
 s<<"\"running\":"<<running_<<",\"page\":"<<view_<<",\"rate\":"<<rates[rate_index_]<<",\"width\":"<<canvas_.width()<<",\"height\":"<<canvas_.height();
 s<<",\"frame_hash\":"<<b8::sim::json_string(std::to_string(canvas_.hash()))<<",\"held_inputs\":"<<held_.size()<<",\"signal_samples\":"<<signals_.size()<<",\"plant_samples\":"<<plant_.size();
 s<<",\"journal_complete\":"<<journal_complete_<<",\"error\":"<<b8::sim::json_string(error_)<<"},\"machine\":"<<session_.hello()<<'}';return s.str();
}
std::string Workbench::journal_json()const{
 std::string s="{\"schema\":1,\"origin\":\"b8-cpp-view\",\"complete\":";s+=journal_complete_?"true":"false";
 s+=",\"bench\":";s+=session_.bench_mode()?"true":"false";s+=",\"firmware\":"+b8::sim::json_string(session_.firmware_name())+",\"commands\":[";
 bool first=true;for(const auto& c:journal_){if(!first)s+=',';first=false;s+=b8::sim::json_string(c);}return s+"]}";
}
}
