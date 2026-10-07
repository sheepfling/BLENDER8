#include "api.h"
#include "view_api.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::string call(const std::string& text) {
    return b8_wasm_command(text.data(), static_cast<uint32_t>(text.size()));
}
void contains(const std::string& text, const char* needle) {
    require(text.find(needle) != std::string::npos, needle);
}
void lifecycle() {
    require(b8_wasm_abi() == 1, "ABI version");
    contains(b8_wasm_hello(), "unavailable");
    require(b8_wasm_init(9) == 1, "invalid mode");
    require(b8_wasm_init(0) == 0, "initialize");
    contains(b8_wasm_hello(), "\"time_us\":0");
    contains(call("run 50000"), "\"ok\":true");
    require(b8_wasm_init(0) == 1, "no implicit session reset");
    contains(b8_wasm_hello(), "\"time_us\":50000");
    b8_wasm_dispose();
    contains(call("snapshot"), "unavailable");
    require(b8_wasm_init(0) == 1, "new module required after dispose");
    b8_wasm_dispose();
}
void commands() {
    require(b8_wasm_init(0) == 0, "initialize");
    contains(call("run 60000"), "\"ready\":true");
    const auto snapshot = call("snapshot");
    contains(call("write 0x29 1"), "only in --bench");
    require(call("snapshot") == snapshot, "rejected poke is nonmutating");
    contains(call("schedule 70000 speed 3"), "\"ok\":true");
    contains(call("trace 20000 1000"), "\"contacts\":4");
    contains(call("quit"), "\"closed\":true");
    contains(call("snapshot"), "closed");
}
void bench() {
    require(b8_wasm_init(1) == 0, "bench initialize");
    contains(b8_wasm_hello(), "BENCH_NO_FIRMWARE");
    contains(call("run 60000"), "\"ready\":true");
    contains(call("write 0x20 7"), "\"gpioa_dir\":7");
    // Observation is not an access; it cannot break a protected clock key sequence.
    contains(call("write 0xA7 195"), "\"ok\":true");
    call("snapshot");
    contains(call("write 0xA7 60"), "\"ok\":true");
    contains(call("write 0xA8 165"), "\"ok\":true");
    contains(call("write 0xB0 1"), "\"ok\":true"); // Enable on the D1 option too.
    contains(call("run 300000"), "\"reset_causes\":5"); // POR|WDT; bench never feeds it.
}
void validation() {
    require(b8_wasm_init(0) == 0, "initialize");
    const auto initial = call("snapshot");
    for (const auto& bad : {std::string{}, std::string(4097, 'x'), std::string("run 1\nrun 1"),
                           std::string("snapshot\0hidden", 15), std::string("trace 1001 1"),
                           std::string("load nan"), std::string("run -1"), std::string("quit extra")})
        contains(call(bad), "\"ok\":false");
    contains(b8_wasm_command(nullptr, 1), "\"ok\":false");
    require(call("snapshot") == initial, "validation must not advance time");
}
void exceptions() {
    require(b8_wasm_init(1) == 0, "initialize");
    contains(call("run 60000"), "\"ok\":true");
    contains(call("write 0xFFFF 1"), "\"ok\":false");
    // A recoverable exception does not abort Wasm or poison subsequent replies.
    contains(call("snapshot"), "\"ok\":true");
    contains(call("write 0xB0 1"), "\"ok\":true");
    contains(call("write 0xB2 0"), "\"ok\":true"); // Bad service key requests hardware reset.
    contains(call("run 2000"), "\"reset_causes\":5");
}
void scene() {
    require(b8_view_abi()==1,"scene ABI");require(b8_view_init()!=0,"scene without machine");
    require(b8_wasm_init(0)==0,"machine init");require(b8_view_init()==0,"scene init");
    require(b8_view_pixels()!=nullptr&&b8_view_width()==1280&&b8_view_height()==900,"frame storage");
    const auto before=call("snapshot");require(b8_view_frame(0)==0,"paint");require(before==call("snapshot"),"paint advances machine");
    require(b8_view_event(4,51,0,0)==0,"key down");require(b8_view_event(5,51,0,0)==0,"key up");
    contains(call("run 10000"),"\"contacts\":4");
    require(b8_view_resize(640,450)==0&&b8_view_width()==640,"resize");
    require(b8_view_resize(0,0)!=0,"invalid resize");contains(b8_view_status(),"\"width\":640");
    require(b8_view_init()!=0,"duplicate scene init");b8_wasm_dispose();
    require(b8_view_pixels()==nullptr&&b8_view_frame(0)!=0,"dispose drops scene");
}

}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("one test name required");
        const std::string test = argv[1];
        if (test == "lifecycle") lifecycle(); else if (test == "commands") commands();
        else if (test == "bench") bench(); else if (test == "validation") validation();
        else if (test == "exceptions") exceptions(); else if(test=="scene") scene(); else throw std::invalid_argument("unknown test");
        b8_wasm_dispose(); std::cout << test << " PASS\n"; return 0;
    } catch (const std::exception& error) { b8_wasm_dispose(); std::cerr << error.what() << '\n'; return 1; }
}
