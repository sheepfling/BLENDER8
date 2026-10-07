#include "blender8/firmware.hpp"
#include "blender8/numeric.hpp"
#include "byte_pair.hpp"
#include "pixels.hpp"
#include <cstdint>

// Owner experiment. This file is deliberately outside the employee starter.
namespace firmware {
namespace {
using b8::Reg;
using Byte = std::uint8_t;
using Pair = bytes::Pair;
namespace pair = bytes;
namespace n = b8::numeric;
constexpr Byte ck=128, wd=64, dm=32, ts=16, th=8, inf=4, il=2, st=1;
Pair ticks{};
Pair last_tick{}, cool{}, sample_age{}, run_age{}, tach_age{}, last_tach{};
Pair stable_age{}, invalid_age{}, jar_age{}, release_age{};
Byte retention_magic=0, retained_faults=0, retained_details=0;
Byte faults=0, reset_details=0, previous_raw=0, stable=0, selected=0;
Byte hot=0, shown=255;
bool guard=true, armed=false, previous_stop=false, running=false;
bool fresh_since_service=false, clock_ok=false;

Pair read_pair(Reg low, Reg high) {
    const Byte lo=b8::read8(low);
    const Byte hi=b8::read8(high);
    return {lo,hi};
}
void write_pair(Reg low, Reg high, Pair value) {
    b8::write8(low,value.low);b8::write8(high,value.high);
}
Pair tick_snapshot() {
    // Own both bytes while the timer ISR is masked; restore the previous mask.
    const Byte enabled=b8::read8(Reg::IRQ_GLOBAL);
    b8::write8(Reg::IRQ_GLOBAL,0);
    const Pair now=ticks;
    b8::write8(Reg::IRQ_GLOBAL,enabled);
    return now;
}
bool has(Byte value, Byte mask) { return !n::equal<Byte>(n::bit_and(value,mask),0); }
void persist() {
    // The supplied C++ binding does not rerun static constructors on MCU reset.
    // Keep only diagnosis; reset() clears it on POR/BOR and initializes all commands.
    retained_faults=faults;retained_details=reset_details;retention_magic=0xC7;
}
void fault(Byte cause) {
    const Byte next=n::bit_or(faults,cause);
    if(!n::equal(next,faults)){faults=next;persist();}
    guard=true;armed=false;selected=0;
}
void off() {
    b8::write8(Reg::GPIOB_OUT,0);
    b8::write8(Reg::PWM_CTRL,0);
    b8::write8(Reg::PWM_DUTY,0);
    running=false;run_age={};tach_age={};
}
void timer() { b8::write8(Reg::IRQ_FLAGS,1);ticks=pair::add(ticks,{1,0}); }
void unused1(){b8::write8(Reg::IRQ_FLAGS,2);}
void unused2(){b8::write8(Reg::IRQ_FLAGS,4);}
void unused3(){b8::write8(Reg::IRQ_FLAGS,8);}
void unused4(){b8::write8(Reg::IRQ_FLAGS,16);}

Byte contacts() {
    Byte result=0;
    for(Byte channel=0;n::less<Byte>(channel,8);channel=n::add<Byte>(channel,1)){
        b8::write8(Reg::GPIOA_OUT,channel);
        // MX8-1 requires 2 us after the final selection transition.
        b8::idle();b8::idle();
        if(!has(b8::read8(Reg::GPIOA_IN),8))
            result=n::bit_or(result,n::shift_left<Byte>(1,channel));
    }
    return result;
}
Byte speed(Byte value) {
    Byte found=0;
    for(Byte bit=0;n::less<Byte>(bit,7);bit=n::add<Byte>(bit,1)){
        if(has(value,n::shift_left<Byte>(1,bit))){
            if(!n::equal<Byte>(found,0))return 255;
            found=n::add<Byte>(bit,1);
        }
    }
    return found;
}
Byte duty(Byte level, bool pulse) {
    // Rounded inverse of MD20 characteristic; full duty is the special 255 code.
    // Entries: idle, 3000, 5500, 8000, 10500, 13000, 15500, 18000 RPM.
    constexpr Byte normal[8]={0,100,131,158,181,203,223,242};
    constexpr Byte boosted[8]={84,125,153,177,199,219,238,255};
    return pulse ? boosted[level] : normal[level];
}
Byte label(bool pulse) {
    if(has(faults,ck))return 16;
    if(has(faults,wd))return 17;
    if(has(faults,dm))return 18;
    if(has(faults,ts))return 19;
    if(has(faults,th))return 20;
    if(has(faults,inf))return 21;
    if(has(faults,il))return 22;
    if(has(faults,st))return 23;
    return pulse ? n::add<Byte>(selected,8) : selected;
}
void display(Byte next) {
    if(n::equal(next,shown))return;
    // One bounded, busy-aware full update. No ISR owns XBUS or ADC pairs.
    b8::write8(Reg::XBUS_REG,0);b8::write8(Reg::XBUS_DATA,0);
    for(Byte address=0;n::less<Byte>(address,64);address=n::add<Byte>(address,1)){
        b8::write8(Reg::XBUS_REG,2);
        while(has(b8::read8(Reg::XBUS_DATA),1))b8::idle();
        b8::write8(Reg::XBUS_REG,1);
        b8::write8(Reg::XBUS_DATA,pixel_byte(next,address));
    }
    b8::write8(Reg::XBUS_REG,3);b8::write8(Reg::XBUS_DATA,3);
    shown=next;
}
bool clock_start() {
    // Bound the wait by service intervals, before any peripheral timer is running.
    // Clock startup delay is an implementation choice, not a CPU cycle model.
    for(Pair wait{};pair::less(wait,{0x20,0x4E});wait=pair::add(wait,{1,0})){
        if(has(b8::read8(Reg::CLK_STATUS),1)){
            // 8 / 2 * 4 / 2 = 8 MHz SYS; / 8 = 1 MHz PB.
            b8::write8(Reg::PLL_PREDIV,2);b8::write8(Reg::PLL_MULT,4);
            b8::write8(Reg::PLL_POSTDIV,2);b8::write8(Reg::PB_DIV,8);
            b8::write8(Reg::CLK_SOURCE,2);
            b8::write8(Reg::CLK_KEY,0xC3);b8::write8(Reg::CLK_KEY,0x3C);
            b8::write8(Reg::CLK_COMMIT,0xA5);
            for(Pair settle{};pair::less(settle,{0xF4,1});settle=pair::add(settle,{1,0}))
                b8::idle();
            return n::equal<Byte>(b8::read8(Reg::CLK_ACTIVE),2)
                && n::equal<Byte>(b8::read8(Reg::PB_ACTIVE),8)
                && n::equal<Byte>(n::bit_and<Byte>(b8::read8(Reg::CLK_STATUS),14),2);
        }
    }
    return false;
}
}
const b8::VectorTable vectors={timer,unused1,unused2,unused3,unused4};

void reset() {
    b8::write8(Reg::IRQ_GLOBAL,0);
    const Byte causes=b8::read8(Reg::RST_CAUSE);
    const Byte details=b8::read8(Reg::RST_DETAIL);
    faults=0;reset_details=details;
    if(!has(causes,3) && n::equal<Byte>(retention_magic,0xC7)){
        faults=retained_faults;
        reset_details=n::bit_or(details,retained_details);
    }
    if(has(causes,16))faults=n::bit_or(faults,ck);
    if(has(causes,4))faults=n::bit_or(faults,wd);
    if(has(causes,8))faults=n::bit_or(faults,dm);
    persist();
    b8::write8(Reg::RST_CAUSE,causes);b8::write8(Reg::RST_DETAIL,details);
    ticks={};last_tick={};cool={};sample_age={};run_age={};tach_age={};last_tach={};
    stable_age={};invalid_age={};jar_age={};release_age={};
    previous_raw=0;stable=0;selected=0;hot=0;shown=255;
    guard=true;armed=false;previous_stop=false;running=false;fresh_since_service=false;
    off();
    b8::write8(Reg::GPIOB_DIR,1);b8::write8(Reg::GPIOA_DIR,7);
    clock_ok=clock_start();
    if(!clock_ok)fault(ck);
    b8::write8(Reg::WDT_SCALE,3);b8::write8(Reg::WDT_CTRL,0x81);
    // 65.54 ms expiry at 8 MHz; 16.38 ms lower service window.
    write_pair(Reg::DMT_LIMIT_LO,Reg::DMT_LIMIT_HI,{0,2});
    write_pair(Reg::DMT_WINDOW_LO,Reg::DMT_WINDOW_HI,{128,0});
    b8::write8(Reg::DMT_CTRL,0x81);
    b8::write8(Reg::ADC_CHANNEL,0);b8::write8(Reg::ADC_PRESCALE,2);
    b8::write8(Reg::ADC_CTRL,3);
    b8::write8(Reg::T0_PRESCALE,0);write_pair(Reg::T0_COMPARE_LO,Reg::T0_COMPARE_HI,{0xE7,3});
    b8::write8(Reg::IRQ_FLAGS,0x1F);b8::write8(Reg::IRQ_ENABLE,1);
    b8::write8(Reg::T0_CTRL,3);b8::write8(Reg::IRQ_GLOBAL,1);
    display(label(false));
}

void step() {
    const Pair now=tick_snapshot();
    if(pair::equal(now,last_tick)){b8::idle();return;}
    const Pair dt=pair::subtract(now,last_tick);last_tick=now;
    const Byte raw=contacts();
    const Byte pins=b8::read8(Reg::GPIOB_IN);
    const bool stop=!has(pins,4), jar=has(pins,8), permit=has(pins,16);
    const bool stop_edge=stop && !previous_stop;previous_stop=stop;
    jar_age=jar ? pair::elapsed(jar_age,dt) : Pair{};
    stable_age=n::equal(raw,previous_raw) ? pair::elapsed(stable_age,dt) : Pair{};
    previous_raw=raw;
    if(!pair::less(stable_age,{10,0}))stable=raw;
    invalid_age=n::equal<Byte>(speed(raw),255) ? pair::elapsed(invalid_age,dt) : Pair{};
    if(!pair::less(invalid_age,{40,0}))fault(inf);
    if(!jar || (!permit && !guard))fault(il);
    if(clock_ok && (!has(b8::read8(Reg::CLK_STATUS),2)
                    || !n::equal<Byte>(b8::read8(Reg::CLK_ACTIVE),2))){
        clock_ok=false;fault(ck);
    }

    sample_age=pair::elapsed(sample_age,dt);
    if(has(b8::read8(Reg::ADC_STATUS),2)){
        const Pair code=read_pair(Reg::ADC_DATA_LO,Reg::ADC_DATA_HI);
        // Nominal ADC thresholds: ceil(135*1023/330)=419, floor(115*1023/330)=356.
        // Compare captured bytes directly; no floating point or native word arithmetic.
        // Nominal AVT10 output is 0.30..1.75 V (-20..125 C).
        // 0.25..1.80 V allows quantization/noise; rails are signal faults only.
        const bool plausible=!pair::less(code,{78,0}) && !pair::less({0x2E,2},code);
        if(!plausible){fault(ts);hot=0;cool={};}
        else {
            hot=!pair::less(code,{0xA3,1}) ? n::add<Byte>(hot,n::less<Byte>(hot,2) ? Byte{1} : Byte{0}) : Byte{0};
            if(!n::less<Byte>(hot,2))fault(th);
            cool=!pair::less({0x64,1},code) ? pair::elapsed(cool,sample_age) : Pair{};
        }
        sample_age={};fresh_since_service=true;
        b8::write8(Reg::ADC_CTRL,3);
    }
    if(pair::less({100,0},sample_age)){fault(ts);cool={};}

    const bool recoverable=clock_ok && jar && !pair::less(jar_age,{20,0})
        && !pair::less(cool,{0xD0,7}) && !pair::less({100,0},sample_age);
    if(guard && stop_edge && recoverable){
        faults=0;reset_details=0;persist();guard=false;
    }
    if(stop || guard){armed=false;selected=0;release_age={};}
    else if(!armed){
        release_age=n::equal<Byte>(raw,0) ? pair::elapsed(release_age,dt) : Pair{};
        if(!pair::less(release_age,{10,0})){armed=true;stable=0;}
    }
    bool pulse=false;
    if(armed){
        const Byte next=speed(stable);
        if(!n::equal<Byte>(next,255))selected=next;
        pulse=has(stable,128);
    }
    const bool request=armed && !stop && permit && n::equal<Byte>(faults,0)
        && (!n::equal<Byte>(selected,0) || pulse);
    if(request){
        const Pair tach=read_pair(Reg::TACH_COUNT_LO,Reg::TACH_COUNT_HI);
        if(!running){run_age={};tach_age={};last_tach=tach;}
        run_age=pair::elapsed(run_age,dt);
        tach_age=pair::equal(tach,last_tach) ? pair::elapsed(tach_age,dt) : Pair{};
        last_tach=tach;
        if(!pair::less(run_age,{0xF4,1}) && !pair::less(tach_age,{100,0})){
            fault(st);off();pulse=false;
        }else{
            b8::write8(Reg::PWM_DUTY,duty(selected,pulse));
            b8::write8(Reg::PWM_CTRL,1);b8::write8(Reg::GPIOB_OUT,1);running=true;
        }
    }else off();
    display(label(pulse));

    // Only foreground can authorize. This tick scanned inputs, consumed a NEW
    // ADC completion since the previous service, decided protection and applied
    // all outputs. A completion token is consumed once; the ISR never feeds.
    if(fresh_since_service && !pair::less(read_pair(Reg::DMT_COUNT_LO,Reg::DMT_COUNT_HI),{160,0})){
        b8::write8(Reg::DMT_PRECLR,0x69);b8::write8(Reg::DMT_CLR,0x96);
        b8::write8(Reg::WDT_SERVICE,0xA5);b8::write8(Reg::WDT_SERVICE,0x5A);
        fresh_since_service=false;
    }
}
}
