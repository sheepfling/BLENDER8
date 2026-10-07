#include "blender8/host/backend.hpp"
#include <stdexcept>
namespace b8::host {
thread_local RegisterBackend* ScopedBinding::active_=nullptr;
ScopedBinding::ScopedBinding(RegisterBackend& backend){
    if(active_)throw std::logic_error("B8 backend already bound on this thread");
    active_=&backend;
}
ScopedBinding::~ScopedBinding(){active_=nullptr;}
RegisterBackend& ScopedBinding::current(){
    if(!active_)throw std::logic_error("B8 register access without an active backend");
    return *active_;
}
}
namespace b8 {
std::uint8_t read8(Reg reg){return host::ScopedBinding::current().read8(reg);}
void write8(Reg reg,std::uint8_t value){host::ScopedBinding::current().write8(reg,value);}
void idle(){host::ScopedBinding::current().idle();}
}
