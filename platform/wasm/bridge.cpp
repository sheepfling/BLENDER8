#include "api.h"
#include "view_api.h"
#include "blender8/view/workbench.hpp"
#include "blender8/firmware.hpp"
#include "blender8/sim/fixture.hpp"
#include "blender8/sim/session.hpp"
#include <memory>
#include <string>
#include <string_view>
#if defined(B8_TARGET_B16)
constexpr auto selected_device=b8::sim::DeviceProfile::b16;
#else
constexpr auto selected_device=b8::sim::DeviceProfile::b8;
#endif
#ifndef B8_FIRMWARE_NAME
#define B8_FIRMWARE_NAME "student"
#endif
namespace {
std::unique_ptr<b8::sim::Session> session;
std::unique_ptr<b8::view::Workbench> view;
std::string reply;
bool initialized = false;
constexpr const char* unavailable = "{\"ok\":false,\"error\":\"session unavailable; instantiate a new module\"}";
constexpr const char* allocation_failure = "{\"ok\":false,\"error\":\"bridge allocation failure\",\"host_failed\":true}";
const char* failure(const char* message) noexcept {
    try { reply = "{\"ok\":false,\"error\":" + b8::sim::json_string(message) + "}"; return reply.c_str(); }
    catch (...) { if (session) session->close(); return allocation_failure; }
}
}
extern "C" uint32_t b8_wasm_abi(void) { return 1; }
extern "C" int b8_wasm_init(uint32_t mode) {
    try {
        if (mode > 2) { failure("invalid session mode"); return 1; }
        if (initialized) { failure("module already initialized; restart with a new module"); return 1; }
        b8::sim::SessionOptions options;
#if defined(B8_FIXED_PRODUCTION_FUSE)
        options.watchdog_fused_on = true;
#endif
        options.bench = mode != 0;
        if (mode == 2) options.profile = b8::sim::BoardProfile::legacy02;
        session = std::make_unique<b8::sim::Session>(b8::sim::FirmwareImage{
            firmware::vectors, firmware::reset, firmware::step, B8_FIRMWARE_NAME, selected_device}, options);
        initialized = true;
        reply = session->hello();
        return 0;
    } catch (const std::exception& e) { failure(e.what()); return 1; }
      catch (...) { failure("unknown initialization failure"); return 1; }
}
extern "C" const char* b8_wasm_hello(void) {
    try { if (!session) return reply.empty() ? unavailable : reply.c_str();
          reply = session->hello(); return reply.c_str(); }
    catch (const std::exception& e) { return failure(e.what()); }
    catch (...) { return failure("unknown handshake failure"); }
}
extern "C" const char* b8_wasm_command(const char* utf8, uint32_t length) {
    try {
        if (!session) return unavailable;
        if (!utf8 || !length || length > 4096) return failure("one nonempty command line, at most 4096 bytes");
        reply = session->execute(std::string_view(utf8, length));
        return reply.c_str();
    } catch (const std::exception& e) { if (session) session->close(); return failure(e.what()); }
      catch (...) { if (session) session->close(); return failure("unknown bridge failure"); }
}
extern "C" void b8_wasm_dispose(void) {
    view.reset();
    session.reset();
    reply.clear();
    // initialized deliberately stays true: C++ firmware globals need a new module.
}

namespace {
template<class F> int view_call(F fn) noexcept {
    try { if(!view)throw std::logic_error("graphical workbench not initialized");fn();return 0; }
    catch(const std::exception& e){failure(e.what());return 1;}
    catch(...){if(session)session->close();failure("unknown view failure");return 1;}
}
}
extern "C" uint32_t b8_view_abi(void){return 1;}
extern "C" int b8_view_init(void){
    try {if(!session||session->closed()||view)throw std::logic_error("initialize machine once before view");
        view=std::make_unique<b8::view::Workbench>(*session);return 0;}
    catch(const std::exception& e){failure(e.what());return 1;}
    catch(...){failure("view allocation failure");return 1;}
}
extern "C" int b8_view_resize(uint32_t w,uint32_t h){return view_call([&]{view->resize(w,h);});}
extern "C" int b8_view_frame(double ms){return view_call([&]{view->frame(ms);});}
extern "C" int b8_view_event(uint32_t kind,int32_t id,double x,double y){
    return view_call([&]{if(kind<4)view->pointer(kind,id,{x,y});else if(kind==4||kind==5)view->key(id,kind==4);
       else if(kind==6||kind==7)view->release_inputs(kind==6);else throw std::invalid_argument("unknown input kind");});
}
extern "C" const uint8_t* b8_view_pixels(void){return view?view->canvas().pixels().data():nullptr;}
extern "C" uint32_t b8_view_width(void){return view?view->canvas().width():0;}
extern "C" uint32_t b8_view_height(void){return view?view->canvas().height():0;}
extern "C" const char* b8_view_status(void){
    try{if(!view)return unavailable;reply=view->status_json();return reply.c_str();}
    catch(const std::exception& e){return failure(e.what());}catch(...){return allocation_failure;}
}
extern "C" const char* b8_view_journal(void){
    try{if(!view)return unavailable;reply=view->journal_json();return reply.c_str();}
    catch(const std::exception& e){return failure(e.what());}catch(...){return allocation_failure;}
}
