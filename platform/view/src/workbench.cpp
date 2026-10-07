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
 {Control::appliance,{24,100,100,36},"APPLIANCE",view_==0}, {Control::chassis,{132,100,90,36},"CHASSIS",view_==1},
 {Control::thermal,{230,100,154,36},"THERMAL + TRUTH",view_==2||view_==3},
 {Control::motor,{392,100,70,36},"MOTOR",view_==5}, {Control::systems,{470,100,84,36},"SYSTEMS",view_==4},
 {Control::supervision,{562,100,134,36},"SUPERVISION",view_==6},
 {Control::lcd_bus,{704,100,74,36},"LCD BUS",view_==7},
 {Control::run,{804,100,82,36},running_?"PAUSE":"RUN",running_}, {Control::step,{894,100,106,36},"STEP 1 MS"},
 {Control::rate,{1008,100,106,36},num(rates[rate_index_],rate_index_==0?2:rate_index_==1?2:0)+"X RATE"},
 {Control::reset,{1122,100,132,36},"MCU RESET"},
 {Control::power,{804,477,130,36},state_.rear_power.value_or(false)?"POWER ON":"POWER OFF",state_.rear_power.value_or(false),state_.rear_power.has_value()},
 {Control::jar,{946,477,130,36},state_.jar_seated?"JAR SEATED":"JAR LIFTED",state_.jar_seated},
 {Control::load_down,{804,584,46,30},"-",false,state_.load.has_value()}, {Control::load_up,{1184,584,46,30},"+",false,state_.load.has_value()},
 {Control::hot,{804,632,130,34},"CASE +20C",false,state_.motor.has_value()}, {Control::cool,{946,632,130,34},"CASE -20C",false,state_.motor.has_value()},
 {Control::brownout,{1088,632,142,34},"BROWNOUT",state_.brownout_forced.value_or(false),state_.brownout_forced.has_value()},
 {Control::clock_loss,{804,676,130,34},"CLOCK LOSS",state_.clock_failed.value_or(false),state_.clock_failed.has_value()}, {Control::core_halt,{946,676,130,34},"CORE HALT",state_.core_halted},
 {Control::foreground,{1088,676,142,34},"NO FOREGROUND",!state_.foreground},
 {Control::sensor_open,{804,720,130,34},"SENSOR OPEN",state_.sensor_open.value_or(false),state_.sensor_open.has_value()},
 {Control::jam,{946,720,130,34},state_.jammed.value_or(false)?"CLEAR JAM":"JAM SHAFT",state_.jammed.value_or(false),state_.jammed.has_value()},
 {Control::exhibit,{1088,720,142,34},"MOTOR EXHIBIT",exhibit_until_>state_.time_us,session_.bench_mode()},
 {Control::bounce,{1088,477,142,36},"BOUNCE "+std::string(state_.bounce_mode==0?"OFF":state_.bounce_mode==2?"5MS":state_.bounce_mode==3?"RANDOM":"1.8MS"),false,state_.bounce_mode.has_value()},
 {Control::water,{804,522,100,30},"WATER",state_.food_kind=="water",state_.food_present.has_value()},
 {Control::frozen,{912,522,100,30},"FROZEN FRUIT",state_.food_kind=="frozen_fruit",state_.food_present.has_value()},
 {Control::vegetables,{1020,522,100,30},"HOT VEG",state_.food_kind=="hot_vegetables",state_.food_present.has_value()},
 {Control::empty,{1128,522,102,30},"EMPTY JUG",!state_.food_present.value_or(false),state_.food_present.has_value()}
 });
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
 if(out.starts_with("{\"ok\":true")){record(c);error_.clear();if(c.starts_with("food_preset ")&&c!="food_preset empty")food_drop_ms_=900;}
 else {error_=out;running_=false;}
 state_=session_.scene();if(out.starts_with("{\"ok\":true"))sample_plant();return out;
}
void Workbench::advance(b8::sim::Tick duration){
 if(duration>20000)throw std::invalid_argument("view step is bounded to 20000 us");
 const auto end=state_.time_us+duration;
 auto recorded=state_.time_us;
 if(end<state_.time_us)throw std::overflow_error("logical time overflow");
 while(state_.time_us<end){
   if(next_sample_<=state_.time_us)next_sample_=state_.time_us+(100-state_.time_us%100);
   const auto amount=std::min(end-state_.time_us,next_sample_-state_.time_us);
   session_.advance_view(amount);state_=session_.scene();
   if(stop_release_pending_&&state_.time_us-stop_pressed_at_>=50000){
    record("run "+std::to_string(state_.time_us-recorded));recorded=state_.time_us;
    sync_momentary();
   }
   if(state_.time_us>=next_plant_sample_){sample_plant();next_plant_sample_=state_.time_us+(100000-state_.time_us%100000);}
   if(state_.time_us>=next_sample_){
    if(!signals_.empty()&&signals_.back().contacts!=state_.contacts)capture_until_=state_.time_us+10000;
    signals_.push_back({state_.time_us,state_.contacts,state_.jar_ok,state_.jar_permit,state_.drive});
    if(signals_.size()>1024)signals_.pop_front();
    if(capture_until_&&state_.time_us<=capture_until_)contact_capture_=signals_;
    next_sample_=state_.time_us+(100-state_.time_us%100);
   }
 }
 if(state_.time_us>recorded)record("run "+std::to_string(state_.time_us-recorded));
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
 state_=session_.scene();
 bool p=false,s=false;for(const auto&[token,c]:held_){(void)token;p|=c==Control::pulse;s|=c==Control::stop;}
 // Assert STOP first, release it last, when both gestures change together.
 if(s&&!sent_stop_){(void)command("stop 1");stop_pressed_at_=state_.time_us;sent_stop_=true;}
 if(s)stop_release_pending_=false;
 const bool old=sent_pulse_;
 if(p!=old)(void)command(std::string("pulse ")+(p?"1":"0"));
 if(!s&&sent_stop_){
  // A GUI click completes a 50 ms physical stroke, even between browser frames.
  // Raw fixture stop 0/1 still has unrestricted timing for conformance experiments.
  if(state_.time_us-stop_pressed_at_>=50000){(void)command("stop 0");sent_stop_=false;stop_release_pending_=false;}
  else stop_release_pending_=true;
 }
 sent_pulse_=p;
}
void Workbench::release_inputs(bool pause){
 held_.clear();if(sent_stop_){(void)command("stop 0");sent_stop_=false;}stop_release_pending_=false;sync_momentary();if(pause)running_=false;pacing_us_=0;render();
}
void Workbench::pointer(unsigned type,int id,Point p){
 if(type>3||id<0||!std::isfinite(p.x)||!std::isfinite(p.y)||std::abs(p.x)>100000||std::abs(p.y)>100000)throw std::invalid_argument("invalid pointer event");
 if(type==1||type==3){release(id);if(type==3&&sent_stop_&&stop_release_pending_){(void)command("stop 0");sent_stop_=false;stop_release_pending_=false;}render();return;}
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
 case 'U':case 'u':c=Control::supervision;break;
 case 'L':case 'l':c=Control::lcd_bus;break;
 case 'M':case 'm':c=Control::motor;break;case 'S':case 's':c=Control::systems;break;case 'V':case 'v':c=view_?Control::appliance:Control::chassis;break;default:break;}
 if(c)press(token,*c);
 render();
}
void Workbench::activate(Control id){
 const auto n=value(id);if(n>=1&&n<=7){(void)command("speed "+std::to_string(n));return;}
 switch(id){
 case Control::appliance:view_=0;break;case Control::chassis:view_=1;break;
 case Control::thermal:case Control::truth:view_=2;break;
 case Control::systems:view_=4;break;case Control::motor:view_=5;break;
 case Control::supervision:view_=6;break;
 case Control::lcd_bus:view_=7;break;
 case Control::run:running_=!running_;pacing_us_=0;break;
 case Control::step:step(1000);break;case Control::rate:rate_index_=(rate_index_+1)%rates.size();break;
 case Control::reset:(void)command("reset");break;
 case Control::power:(void)command(std::string("power ")+(state_.rear_power.value_or(false)?"0":"1"));break;
 case Control::jar:(void)command(std::string("jar ")+(state_.jar_seated?"0":"1"));break;
 case Control::jam:(void)command(std::string("jam ")+(state_.jammed.value_or(false)?"0":"1"));break;
 case Control::load_down:case Control::load_up:if(state_.load)(void)command("load "+num(std::clamp(*state_.load+(id==Control::load_up?.1:-.1),0.,1.),2));break;
 case Control::hot:if(state_.motor)(void)command("temperature "+num(std::min(150.,state_.motor->case_c+20),2));break;
 case Control::cool:if(state_.motor)(void)command("temperature "+num(std::max(-40.,state_.motor->case_c-20),2));break;
 case Control::brownout:(void)command(state_.brownout_forced.value_or(false)?"voltage auto":"voltage 2.5");break;
 case Control::clock_loss:(void)command(state_.clock_failed.value_or(false)?"clock_failed 0":"clock_failed 1");break;
 case Control::core_halt:(void)command(state_.core_halted?"halt 0":"halt 1");break;
 case Control::foreground:(void)command(state_.foreground?"foreground 0":"foreground 1");break;
 case Control::sensor_open:(void)command(state_.sensor_open.value_or(false)?"sensor healthy":"sensor open");break;
 case Control::food:case Control::empty:(void)command("food_preset empty");break;
 case Control::water:(void)command("food_preset water");break;
 case Control::frozen:(void)command("food_preset frozen_fruit");break;
 case Control::vegetables:(void)command("food_preset hot_vegetables");break;
 case Control::bounce:(void)command(state_.bounce_mode==1?"bounce slow":state_.bounce_mode==2?"bounce off":"bounce nominal");break;
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
 if(state_.food_present.value_or(false)){
  const auto kind=state_.food_kind.value_or("custom");
  const Color food=kind=="water"?Color{86,182,220}:kind=="frozen_fruit"?Color{184,124,199}:Color{216,151,75};
  const double fill=food_drop_ms_>0?1-food_drop_ms_/900:1;
  const double top=443-100*fill;
  const std::array<Point,4> contents{{{310,top-lift},{496,top-lift},{480,434-lift},{323,434-lift}}};
  canvas_.polygon(contents,food);
  if(kind!="water")for(unsigned i=0;i<9;++i){double x=334+(i%3)*55,y=top+12+(i/3)*22;
    if(y<430)canvas_.ellipse({x,y-lift},9,7,kind=="frozen_fruit"?Color{223,174,221}:Color{108,164,85});}
  if(food_drop_ms_>0)for(unsigned i=0;i<8;++i){const double f=std::fmod((900-food_drop_ms_)/600+i*.13,1.);
    canvas_.ellipse({358.+(i%4)*24,174+f*(top-170)-lift},kind=="water"?3:7,kind=="water"?10:6,food);}
  if(state_.motor&&state_.motor->rpm>1200)canvas_.ellipse({404,top-lift},70,4,{231,244,229,130});
 }
 // Contents and drop animation illustrate the fixture; no fluid/phase-change model.
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
 canvas_.ellipse({572,540},9,9,state_.drive?mint:Color{98,108,94});canvas_.text("MOTOR ON",{537,560},1.1,{58,77,72});
 canvas_.ellipse({572,586},7,7,state_.jar_permit?mint:amber);
 canvas_.text(state_.jar_permit?"JAR READY":"JAR LOCKED",{533,603},1,{58,77,72});
 canvas_.text(state_.jar_seated?"JAR SEATED / CLICK JAR TO LIFT":"JAR LIFTED / CLICK TO RESEAT",{235,725},1.25,muted);
 canvas_.text("LAMPS ABOVE: MOTOR POWER / JAR PERMISSION",{210,746},1.0,muted);
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
 wire({364,365},{354,316},(state_.mcu.gpioa_out&7)!=0);
 const auto* bus=state_.lcd_bus?&*state_.lcd_bus:nullptr;
 const bool recent=bus&&bus->data_size&&state_.time_us-bus->data[bus->data_size-1].time_us<100000;
 wire({548,365},{580,318},recent);wire({570,308},{535,348},state_.lcd_vblank);
 wire({548,416},{590,482},state_.pwm);wire({637,542},{491,416},state_.gpiob[1]==b8::sim::Logic::high);
 canvas_.line({580,538},{440,569},{108,118,124},2);wire({387,550},{393,416},state_.analog_v>.2);
 wire({208,452},{319,393},state_.clock_valid);
 wire({169,547},{300,468},state_.jar_ok);wire({394,490},{580,504},state_.jar_permit&&state_.run_permit);
 block({61,255,151,75},"BA-8","CONTACTS 0-7",amber);block({287,255,143,75},"MX8-1","SELECT "+std::to_string(state_.mcu.gpioa_out&7),mint);
 block({319,348,229,77},"NORTHSTAR "+std::string(session_.device_name()),"SYS "+num(state_.mcu.system_hz/1e6,2)+" / PB "+num(state_.mcu.peripheral_hz/1e6,2),mint);
 block({571,253,143,76},"PX32-16","50 HZ SCANOUT",text);draw_lcd({596,339,96,48});
 canvas_.text("DATA > LCD / 8 BIT",{555,400},1.0,recent?mint:muted);
 canvas_.text("VBLANK > MCU "+std::string(state_.lcd_vblank?"HIGH":"LOW"),{555,419},1.0,state_.lcd_vblank?amber:muted);
 canvas_.text("L: LCD BUS TIMING",{555,438},1.0,muted);
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
     canvas_.text(c.label,{c.rect.x+8,c.rect.y+12},c.rect.w<70?1.8:c.rect.w<110?1.:1.25,c.enabled?(c.active?mint:text):Color{84,101,108});
   }
 }
}
void Workbench::draw_instruments(){
 canvas_.text("LIVE PLANT + FIRMWARE LCD",{804,179},1.8,text);
 const auto& w=state_.mcu.watchdog;const auto& d=state_.mcu.deadman;
 canvas_.text("WDT "+std::string(w.enabled?"ON":"OFF")+(w.locked?" LOCK":"")+" / DMT "+(d.enabled?"ON":"OFF")+(d.locked?" LOCK":"")+" / U STATUS",{804,201},1.15,muted);
 card({800,225,208,111},"SHAFT SPEED",state_.motor?num(state_.motor->rpm):"N/A","RPM / PLANT OBSERVATION",mint);
 const std::string thermal_suffix=state_.nearby_air_c?
     "AIR "+num(*state_.nearby_air_c,1)+"  FOOD "+(state_.food_c?num(*state_.food_c,1):"N/A"):"NO THERMAL PROBE";
 card({1022,225,208,111},"CASE TEMPERATURE",state_.motor?num(state_.motor->case_c,1):"N/A",thermal_suffix,amber);
 draw_lcd({812,356,96,48});
 canvas_.text("ACTUAL LCD",{810,418},1.1,muted);
 std::string reason,help;
 if(!state_.rear_power.value_or(false)){reason="POWER OFF";help="SWITCH POWER ON";}
 else if(!state_.ready){reason="POWER SETTLING / RESET";help="WAIT FOR MCU STARTUP";}
 else if(state_.core_halted){reason="CORE HALTED / LCD STALE";help="CLEAR CORE HALT";}
 else if(!state_.foreground){reason="FOREGROUND OFF";help=(w.enabled||d.enabled)?"ENABLED MONITORS CAN TRIP":"MONITORS OFF / NO RECOVERY";}
 else if(state_.clock_failed.value_or(false)){reason="CLOCK LOSS INJECTED";help="CLEAR FAULT; MCU RESET";}
 else if(state_.brownout_forced.value_or(false)){reason="BROWNOUT INJECTED";help="CLEAR BROWNOUT; REARM";}
 else if(!state_.jar_seated){reason="JAR LIFTED / IL";help="RESEAT JAR; THEN RECOVER";}
 else if(state_.sensor_open.value_or(false)){reason="SENSOR OPEN / TS";help="RESTORE SENSOR; THEN RECOVER";}
 else if(state_.jammed.value_or(false)){reason="SHAFT JAM / ST ON REQUEST";help="CLEAR JAM; THEN RECOVER";}
 else if(state_.motor&&state_.motor->case_c>=85){reason="HOT CASE / SENSOR LAGS";help="TH AT TWO READS >=85C";}
 else if(state_.stop){reason="STOP HELD / DRIVE OFF";help="RELEASE, THEN NEW COMMAND";}
 else if(!state_.jar_permit){reason="JAR GATE DISARMED";help="COOL 2S; TAP STOP TO ARM";}
 else if(state_.drive){reason="MOTOR ENERGIZED";help="STOP REMOVES DRIVE; SHAFT COASTS";}
 else if(state_.run_permit){reason="COMMAND INHIBITED";help="CHECK LCD; RECOVERY BELOW";}
 else {reason="DRIVE OFF / SELECT 1-7 OR P";help="FAULT CODE? RECOVER FIRST";}
 canvas_.text(reason,{929,351},1.15,amber);canvas_.text(help,{929,371},1.05,muted);
 canvas_.text("PWM "+(state_.motor?num(state_.motor->effective_duty*100,1)+"%":"N/A")+
  "  HEAT "+(state_.motor?num(state_.motor->loss_w,1)+"W":"N/A"),{929,394},1.15,text);
 canvas_.text("AN0 "+num(state_.analog_v,3)+"V  RESET "+hex(state_.mcu.reset_causes),{929,414},1.15,muted);
 canvas_.text("POWER / JAR / CONTACT BOUNCE",{804,449},1.4,muted);
 const std::string kind=std::string(state_.food_kind.value_or("unavailable"));
 canvas_.text("FOOD "+kind+" / LOAD "+(state_.load?num(*state_.load*100)+"%":"N/A"),{804,565},1.05,muted);
 canvas_.rect({867,584,300,30},dark,5);
 if(state_.load)canvas_.rect({867,584,300**state_.load,30},{53,111,101},5);
 canvas_.text("LOAD - / +",{978,594},1.2,text);
 canvas_.text("FAULTS / +/- CASE TEMPERATURE",{804,620},1.1,muted);
}
void Workbench::draw_signals(){
 canvas_.rect({24,780,1206,97},dark,9);canvas_.text("CONTACT SENSE",{42,795},1.5,text);
 canvas_.text("100 US SAMPLES",{42,817},1.1,muted);canvas_.text("20 MS CAPTURE",{42,833},1.1,muted);
 canvas_.text("ACTIVE LOW",{42,850},1.1,amber);
 const auto& trace=contact_capture_.empty()?signals_:contact_capture_;
 const auto first=trace.size()>201?trace.size()-201:0;
 for(unsigned ch=0;ch<8;++ch){double y=790+ch*10;canvas_.text(ch==7?"P":"C"+std::to_string(ch),{196,y},1,muted);
 canvas_.line({228,y+6},{1215,y+6},edge,1);if(trace.empty())continue;
 double prev_y=y+((trace[first].contacts&(1u<<ch))?6:0),prev_x=228;
 // Decimate only presentation if needed, never the modeled contacts or sample record.
 for(std::size_t i=first;i<trace.size();++i){double x=228+987*static_cast<double>(i-first)/200.;double yy=y+((trace[i].contacts&(1u<<ch))?6:0);
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
 (void)truth;
 canvas_.text("THERMAL + TRUTH",{48,181},2.2,text);
 canvas_.text("MEASURED SIGNALS AND PHYSICAL STATES / ONE NETWORK",{48,210},1.2,muted);
 const auto adc=state_.mcu.temperature_adc_code;
 canvas_.text("WIRE "+num((state_.analog_v-.5)*100.,2)+" C    ADC "+(adc?num((*adc*3.3/1023.-.5)*100.,2)+" C":"NO READ"),{48,235},1.6,mint);
 plot({94,332,628,245},{{"AN0 NOMINAL ESTIMATE",mint,&PlantSample::wire_c},
  {"LAST ADC READ / HELD",blue,&PlantSample::adc_c,true},{"CASE / TRUTH",amber,&PlantSample::case_c},
  {"SENSOR LAG / TRUTH",pink,&PlantSample::sensor_c},{"LOCAL AIR / TRUTH",violet,&PlantSample::air_c},
  {"FOOD / IF PRESENT",cream,&PlantSample::food_c},{"ROOM / BOUNDARY",muted,&PlantSample::room_c}},"TEMPERATURE / C");
 auto v=[](std::optional<double> n){return n?num(*n,2):"N/A";};
 canvas_.text("HEAT W: CASE>AIR "+v(state_.case_air_w)+"  CASE>FOOD "+v(state_.case_food_w),{48,619},1.1,amber);
 canvas_.text("FOOD>AIR "+v(state_.food_air_w)+"  AIR>ROOM "+v(state_.ventilation_w),{48,715},1.2,muted);
 canvas_.text("FOOD C "+v(state_.food_capacity)+" J/K   G "+v(state_.food_conductance)+" W/K",{48,739},1.1,muted);
 canvas_.text("ADC MAY LAG OR BE STALE / 100MS SNAPSHOTS / LAST 10 MIN",{48,760},1.0,muted);
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
void Workbench::draw_lcd_bus(){
 canvas_.text("LCD / DATA + VBLANK",{48,181},2.2,text);
 canvas_.text("MCU > LCD: 8-BIT BYTES / LCD > MCU: VBLANK",{48,210},1.25,muted);
 if(!state_.lcd_bus){canvas_.text("DISPLAY BUS PROBE UNAVAILABLE",{48,258},1.8,amber);return;}
 const auto& b=*state_.lcd_bus;
 canvas_.text("READS "+std::to_string(b.reads)+" / WRITES "+std::to_string(b.writes)+
  " / DATA "+std::to_string(b.data_writes)+" / DROPPED "+std::to_string(b.rejected),{48,235},1.1,b.rejected?pink:mint);
 canvas_.text("ADDR "+hex(b.address)+" / CTRL "+hex(b.control)+" / "+(b.busy?"BUSY":"READY")+
  (b.error?" / ERROR LATCHED":" / NO ERROR")+(state_.powered?"":" / POWER OFF"),{48,255},1.2,b.error?pink:text);
 canvas_.text("VBLANK / "+std::string(state_.lcd_vblank?"HIGH":"LOW")+" / 16 MS SCAN + 4 MS BLANK",{48,279},1.2,amber);
 const auto begin=state_.time_us>40000?state_.time_us-40000:0;
 const double span=40000.,left=94,right=722,top=306,low=330;
 auto x=[&](b8::sim::Tick t){return left+(right-left)*static_cast<double>(t-begin)/span;};
 canvas_.rect({90,300,636,40},dark,4);
 for(unsigned i=0;i<=4;++i){const double xx=left+(right-left)*i/4.;canvas_.line({xx,300},{xx,340},edge,1);}
 bool known=false,level=false;auto cursor=begin;
 for(unsigned i=0;i<b.blank_size;++i){const auto& e=b.blank_edges[i];
  if(e.time_us<=begin){known=true;level=e.high;continue;}
  if(e.time_us>state_.time_us)break;
  if(known){const double y=level?top:low;canvas_.line({x(cursor),y},{x(e.time_us),y},amber,2);
   canvas_.line({x(e.time_us),y},{x(e.time_us),e.high?top:low},e.reset?pink:amber,2);}
  cursor=e.time_us;level=e.high;known=true;
 }
 if(known)canvas_.line({x(cursor),level?top:low},{x(state_.time_us),level?top:low},amber,2);
 canvas_.text(num(begin/1000.,3)+" MS",{94,348},1.0,muted);
 canvas_.text("LAST 40 MS / LOGICAL TIME",{300,348},1.0,muted);
 canvas_.text(num((begin+40000)/1000.,3)+" MS",{614,348},1.0,muted);
 canvas_.text("DATA[7:0] / MOST RECENT WRITE BURST",{48,375},1.3,mint);
 if(b.data_size){
  unsigned first=0;for(unsigned i=1;i<b.data_size;++i)
   if(b.data[i].time_us-b.data[i-1].time_us>1000)first=i;
  const auto start=b.data[first].time_us,end=b.data[b.data_size-1].time_us+8;
  const double duration=static_cast<double>(end-start);
  canvas_.text("@ "+std::to_string(start)+" US / "+std::to_string(end-start)+" US WINDOW / "+
   std::to_string(b.data_size-first)+" BYTES"+(start<b.epoch_us?" / BEFORE LCD RESET":""),{48,397},1.0,muted);
  canvas_.rect({90,416,636,124},dark,4);
  for(unsigned bit=0;bit<8;++bit){const double y=420+bit*14.;
   canvas_.text("D"+std::to_string(7-bit),{58,y},1.0,muted);
   for(unsigned i=first;i<b.data_size;++i){const auto& e=b.data[i];
    const double xx=94+628*static_cast<double>(e.time_us-start)/duration;
    const double next= i+1<b.data_size?94+628*static_cast<double>(b.data[i+1].time_us-start)/duration:722;
    const double yy=y+((e.data&(1u<<(7-bit)))?0:8);
    const auto color=e.accepted?mint:pink;
    canvas_.line({xx,yy},{next,yy},color,1.5);
    if(i>first){const double previous=y+((b.data[i-1].data&(1u<<(7-bit)))?0:8);canvas_.line({xx,previous},{xx,yy},color,1.5);}
    canvas_.ellipse({xx,yy},1.5,1.5,color);
   }
  }
  unsigned blanks=0;for(unsigned i=first;i<b.data_size;++i)if(b.data[i].vblank)++blanks;
  canvas_.text(std::to_string(blanks)+" IN VBLANK / "+std::to_string(b.data_size-first-blanks)+
   " DURING SCAN / PINK = BUSY WRITE DROPPED",{48,551},1.05,muted);
  const unsigned recent=b.data_size>16?b.data_size-16:0;
  canvas_.text("LAST DATA BYTES / HEX / OLDEST > NEWEST",{48,577},1.0,muted);
  for(unsigned i=recent;i<b.data_size;++i){const auto& e=b.data[i];canvas_.text(hex(e.data),{48+(i-recent)*41.,597},1.7,e.accepted?mint:pink);}
 }else canvas_.text("NO DATA WRITES HAVE REACHED THE LCD",{48,450},1.3,muted);
 if(b.transfer_size){const auto& e=b.transfers[b.transfer_size-1];
  constexpr std::array<const char*,4> names{"ADDRESS","DATA","STATUS","CONTROL"};
  canvas_.text("LAST BUS: "+std::string(e.write?"WRITE ":"READ ")+names[e.reg]+" = "+hex(e.data)+
   " @ "+std::to_string(e.time_us)+" US",{48,715},1.05,text);
 }
 canvas_.text("64 DATA / 32 BUS / 32 BLANK EDGES RETAINED; COUNTERS ARE LIFETIME",{48,735},1.0,muted);
 canvas_.text("BYTE VALUES HELD BETWEEN MARKERS / NO ELECTRICAL EDGE TIMING",{48,755},1.0,muted);
}
void Workbench::draw_supervision(){
 const auto& m=state_.mcu;const auto& w=m.watchdog;const auto& d=m.deadman;
 canvas_.text("SUPERVISION + REBOOT STATUS",{48,181},2.1,text);
 canvas_.text("FIRMWARE CONFIGURATION / READ-ONLY INSTRUMENTS",{48,210},1.2,muted);
 auto box=[&](double x,std::string_view name,bool enabled,bool locked,bool armed,bool error,
             unsigned count,unsigned limit,std::uint64_t services,bool counting,double hz){
  canvas_.rect({x,232,332,165},dark,6);canvas_.text(name,{x+12,244},1.4,mint);
  canvas_.text(!m.supervision_available?"NOT FITTED":std::string(enabled?"ENABLED":"DISABLED")+(locked?" / LOCKED":" / UNLOCKED"),{x+12,267},1.35,enabled?mint:muted);
  canvas_.text(!enabled?"NOT COUNTING":counting?"COUNTING":"PAUSED / RESET OR CORE CLOCK",{x+12,288},1.0,amber);
  canvas_.text(std::to_string(count)+" / "+std::to_string(limit)+" TICKS / "+std::to_string(services)+" SERVICES",{x+12,307},1.0,text);
  canvas_.rect({x+12,325,308,6},edge,2);
  if(enabled&&limit)canvas_.rect({x+12,325,308*std::min(1.,static_cast<double>(count)/limit),6},amber,2);
  canvas_.text(enabled&&hz>0?"TO LIMIT ~"+num((limit-std::min(count,limit))*1000./hz,2)+" MS AT CURRENT CLOCK":"TO LIMIT N/A",{x+12,342},1.0,muted);
  canvas_.text(std::string(armed?"KEY 1 ARMED":"NO KEY PENDING")+(error?" / CONFIG ERROR":""),{x+12,361},1.0,error?amber:muted);
 };
 box(48,"INDEPENDENT WATCHDOG / WDT",w.enabled,w.locked,w.armed,w.error,w.count,w.limit,w.services,m.reset_released,m.lfrc_hz);
 box(394,"WINDOWED DEADMAN / DMT",d.enabled,d.locked,d.armed,d.error,d.count,d.limit,d.services,m.reset_released&&!state_.core_halted&&!m.clock_stopped,m.system_hz/1024.);
 canvas_.text(w.fused_on?"FUSE: FORCED ON / LFRC":"FUSE: FIRMWARE CONTROL / LFRC",{60,380},1.0,muted);
 canvas_.text("WINDOW "+std::to_string(d.window)+".."+std::to_string(d.limit?d.limit-1:0)+" / "+(!d.enabled?"DISABLED":d.window_open?"OPEN":"TOO EARLY"),{406,380},1.0,d.window_open?mint:muted);
 canvas_.text("REBOOT REASON BITS",{48,417},1.5,text);
 canvas_.text("RECENT REBOOTS / NEWEST FIRST",{394,417},1.3,text);
 canvas_.text("LATCH "+hex(m.reset_causes)+" / DETAIL "+hex(m.reset_details),{48,442},1.15,amber);
 canvas_.text("EVENT CAUSES SURVIVE FIRMWARE W1C",{394,442},1.0,muted);
 for(unsigned i=0;i<b8::sim::reset_cause_flags.size();++i){const auto& f=b8::sim::reset_cause_flags[i];
  canvas_.text(hex(f.bit)+" "+std::string(f.name)+" / "+std::string(f.description),{48,465+i*19.},1.05,(m.reset_causes&f.bit)?amber:muted);
 }
 for(unsigned i=0;i<std::min(5u,m.reset_history_size);++i){const auto& e=m.reset_history[m.reset_history_size-1-i];const double y=465+i*28.;
  canvas_.text("#"+std::to_string(e.serial)+" @ "+num(e.time_us/1e6,3)+"S / "+hex(e.causes)+" "+b8::sim::reset_cause_names(e.causes),{394,y},1.05,i?muted:mint);
  canvas_.text("DETAIL "+hex(e.details)+" / "+b8::sim::reset_detail_names(e.details),{394,y+13},.9,muted);
 }
 canvas_.text("LAST 16 EVENTS IN DEBUG SNAPSHOTS / RESTART SESSION CLEARS HISTORY",{48,615},1.0,muted);
 canvas_.text("DETAIL: 01 WDT BAD KEY / 02 DMT BAD1 / 04 DMT BAD2",{48,715},1.1,muted);
 canvas_.text("08 DMT EARLY / 10 DMT LATE OR EXPIRED / 20 WDT TIMEOUT",{48,735},1.1,muted);
 canvas_.text("CORE HALT PAUSES DMT; WDT RUNS. LOCKS CLEAR AT MCU RESET.",{48,755},1.0,muted);
}
void Workbench::render(double ms){
 state_=session_.scene();food_drop_ms_=std::max(0.,food_drop_ms_-ms);const double mix=ms<=0?0:1-std::exp(-ms/45.);
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
 else if(view_==2||view_==3)draw_thermal(true);else if(view_==4)draw_systems();else if(view_==6)draw_supervision();else if(view_==7)draw_lcd_bus();else draw_motor();
 draw_instruments();draw_controls();draw_signals();
 canvas_.text("T "+num(state_.time_us/1e6,3)+" S | "+(running_?"RUNNING":"PAUSED / LCD FROZEN")+" | 1-7 SPEED   P PULSE   SPACE STOP   J JAR   O POWER   R RUN   V VIEW   T THERMAL   M MOTOR   S SYSTEMS   U SUPERVISION   L LCD BUS",{28,883},1.0,muted);
 if(!error_.empty()){canvas_.rect({26,148,1228,30},{98,41,41});canvas_.text(error_.substr(0,110),{38,159},1.3,text);}
}
std::string Workbench::status_json(){
 std::ostringstream s;s.imbue(std::locale::classic());s<<std::boolalpha<<"{\"ok\":true,\"view\":{";
 s<<"\"input_release_kind\":7,\"running\":"<<running_<<",\"page\":"<<view_<<",\"rate\":"<<rates[rate_index_]<<",\"width\":"<<canvas_.width()<<",\"height\":"<<canvas_.height();
 s<<",\"frame_hash\":"<<b8::sim::json_string(std::to_string(canvas_.hash()))<<",\"held_inputs\":"<<held_.size()<<",\"signal_samples\":"<<signals_.size()<<",\"plant_samples\":"<<plant_.size();
 s<<",\"contact_capture_samples\":"<<contact_capture_.size()<<",\"food_animation_ms\":"<<food_drop_ms_
  <<",\"journal_complete\":"<<journal_complete_<<",\"error\":"<<b8::sim::json_string(error_)<<"},\"machine\":"<<session_.hello()<<'}';return s.str();
}
std::string Workbench::journal_json()const{
 std::string s="{\"schema\":1,\"origin\":\"b8-cpp-view\",\"complete\":";s+=journal_complete_?"true":"false";
 s+=",\"bench\":";s+=session_.bench_mode()?"true":"false";s+=",\"firmware\":"+b8::sim::json_string(session_.firmware_name())+",\"commands\":[";
 bool first=true;for(const auto& c:journal_){if(!first)s+=',';first=false;s+=b8::sim::json_string(c);}return s+"]}";
}
}
