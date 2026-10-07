#include "blender8/view/workbench.hpp"
#include "blender8/firmware.hpp"
#include "blender8/sim/fixture.hpp"
#include <iostream>
#include <sstream>
#if defined(B8_TARGET_B16)
constexpr auto selected_device=b8::sim::DeviceProfile::b16;
#else
constexpr auto selected_device=b8::sim::DeviceProfile::b8;
#endif
#ifndef B8_FIRMWARE_NAME
#define B8_FIRMWARE_NAME "student"
#endif
int main(int argc,char**argv){
 try{
  const bool bench=argc==2&&std::string_view(argv[1])=="--bench";
  if(argc>2||(argc==2&&!bench))throw std::invalid_argument("only --bench is supported");
  b8::sim::Session session({firmware::vectors,firmware::reset,firmware::step,B8_FIRMWARE_NAME,selected_device},{bench});
  b8::view::Workbench view(session);
  auto reply=[&](bool frame,std::string extra=""){
   const auto& c=view.canvas();const auto size=frame?c.pixels().size():0;
   std::cout<<"{\"ok\":true,\"bytes\":"<<size<<",\"state\":"<<view.status_json()<<extra<<"}\n";
   if(frame)std::cout.write(reinterpret_cast<const char*>(c.pixels().data()),static_cast<std::streamsize>(size));
   std::cout.flush();
  };
  reply(true);
  for(std::string line;std::getline(std::cin,line);){
   try{
    if(line.size()>4096)throw std::invalid_argument("request too large");
    std::istringstream input(line);std::string op,extra;input>>op;
    if(op=="frame"){double ms;if(!(input>>ms)||(input>>extra))throw std::invalid_argument("frame interval required");view.frame(ms);}
    else if(op=="event"){
     unsigned kind;int id;double x,y;if(!(input>>kind>>id>>x>>y)||(input>>extra))throw std::invalid_argument("event fields required");
     if(kind<4)view.pointer(kind,id,{x,y});else if(kind==4||kind==5)view.key(id,kind==4);else if(kind==6||kind==7)view.release_inputs(kind==6);else throw std::invalid_argument("invalid event kind");
    }else if(op=="resize"){
     unsigned w,h;if(!(input>>w>>h)||(input>>extra))throw std::invalid_argument("dimensions required");view.resize(w,h);
    }else if(op=="journal"){if(input>>extra)throw std::invalid_argument("no arguments");reply(false,",\"journal\":"+view.journal_json());continue;}
    else if(op=="status"){if(input>>extra)throw std::invalid_argument("no arguments");reply(false);continue;}
    else if(op=="quit"){session.close();break;}
    else throw std::invalid_argument("unknown scene operation");
    reply(true);
   }catch(const std::exception& e){std::cout<<"{\"ok\":false,\"bytes\":0,\"error\":"<<b8::sim::json_string(e.what())<<"}\n"<<std::flush;}
  }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
