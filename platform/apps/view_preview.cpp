#include "blender8/view/workbench.hpp"
#include "blender8/firmware.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
#if defined(B8_TARGET_B16)
constexpr auto selected_device=b8::sim::DeviceProfile::b16;
#else
constexpr auto selected_device=b8::sim::DeviceProfile::b8;
#endif
void save(b8::view::Workbench&w,const fs::path&base){
 w.render(50);const auto&c=w.canvas();std::ofstream f(base.string()+".ppm",std::ios::binary);f<<"P6\n"<<c.width()<<' '<<c.height()<<"\n255\n";
 for(std::size_t i=0;i<c.pixels().size();i+=4)f.write(reinterpret_cast<const char*>(c.pixels().data()+i),3);
 std::ofstream(base.string()+".json")<<w.status_json()<<'\n';
}
int main(int argc,char**argv){
 if(argc!=2){std::cerr<<"usage: b8_view_preview OUTPUT_DIRECTORY\n";return 2;}
 fs::path out(argv[1]);fs::create_directories(out);
 {
 b8::sim::Session s({firmware::vectors,firmware::reset,firmware::step,"pixel_probe_not_product",selected_device});b8::view::Workbench w(s);
 (void)w.command("power 1");(void)w.command("run 80000");w.key('3',true);w.key('3',false);
 for(int i=0;i<12;++i)w.step(10000);save(w,out/"appliance-probe");w.key('V',true);w.key('V',false);save(w,out/"open-chassis");
 }
 {
 b8::sim::Session s({firmware::vectors,firmware::reset,firmware::step,"component-exhibit",selected_device},{true});b8::view::Workbench w(s);
 for(auto c:w.controls())if(c.id==b8::view::Control::exhibit){w.pointer(0,1,{c.rect.x+20,c.rect.y+16});w.pointer(1,1,{0,0});}
 for(int i=0;i<80;++i)w.frame(10);save(w,out/"motor-running");w.key('J',true);w.key('J',false);
 for(int i=0;i<10;++i)w.frame(10);save(w,out/"jar-lift-coast");
 (void)w.command("environment 22 5 0.15");(void)w.command("temperature 85");
 for(int i=0;i<50;++i)w.step(20000);
 for(const auto& entry:std::array<std::pair<b8::view::Control,const char*>,4>{{
  {b8::view::Control::thermal,"thermal-measured"},{b8::view::Control::truth,"thermal-truth"},
  {b8::view::Control::motor,"motor-history"},{b8::view::Control::systems,"subsystems"}}}){
  for(auto c:w.controls())if(c.id==entry.first){w.pointer(0,1,{c.rect.x+20,c.rect.y+16});w.pointer(1,1,{0,0});}
  save(w,out/entry.second);
 }
 std::ofstream(out/"exhibit-journal.json")<<w.journal_json()<<'\n';
 }
}
