#include "blender8/firmware.hpp"
#include "blender8/access.hpp"
#include "blender8/b16.hpp"
#include "blender8/numeric.hpp"
#include "font.hpp"
#include <cstdint>

// Owner experiment. This file is deliberately outside the employee starter.
namespace firmware {
namespace {
using b8::Reg;
using Byte = std::uint8_t;
using Word = std::uint16_t;
namespace n = b8::numeric;
constexpr Byte ck=128, wd=64, dm=32, ts=16, th=8, inf=4, il=2, st=1;
volatile Word ticks=0;
Word last_tick=0, cool=0, sample_age=0, run_age=0, tach_age=0, last_tach=0;
Word stable_age=0, invalid_age=0, jar_age=0, release_age=0;
Byte faults=0, reset_details=0, previous_raw=0, stable=0, selected=0;
Byte hot=0, shown=255;
bool guard=true, armed=false, previous_stop=false, running=false;
bool fresh_since_service=false, clock_ok=false;

Word elapsed(Word value, Word delta) {
    // Saturation avoids timer wrap turning a qualified condition unqualified.
    if(n::less<Word>(value,30000)) return n::add(value,delta);
    return 30000;
}
bool has(Byte value, Byte mask) { return !n::equal<Byte>(n::bit_and(value,mask),0); }
void persist() {
    b8::write8(b16::sram(0x1000),0xC7);
    b8::write8(b16::sram(0x1001),faults);
    b8::write8(b16::sram(0x1002),reset_details);
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
    running=false;run_age=0;tach_age=0;
}
void timer() { b8::acknowledge(b8::Irq::timer0);ticks=n::add<Word>(ticks,1); }
void unused1(){b8::acknowledge(b8::Irq::timer1);}
void unused2(){b8::acknowledge(b8::Irq::tach);}
void unused3(){b8::acknowledge(b8::Irq::vblank);}
void unused4(){b8::acknowledge(b8::Irq::adc);}
void unused5(){b8::write8(Reg::IRQ_FLAGS,0x20);}

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
        b8::write8(Reg::XBUS_DATA,glyphs[next][address]);
    }
    b8::write8(Reg::XBUS_REG,3);b8::write8(Reg::XBUS_DATA,3);
    shown=next;
}
bool clock_start() {
    // Bound the wait by service intervals, before any peripheral timer is running.
    // Clock startup delay is an implementation choice, not a CPU cycle model.
    for(Word wait=0;n::less<Word>(wait,20000);wait=n::add<Word>(wait,1)){
        if(has(b8::read8(Reg::CLK_STATUS),1)){
            // 8 / 2 * 4 / 2 = 8 MHz SYS; / 8 = 1 MHz PB.
            b8::write8(Reg::PLL_PREDIV,2);b8::write8(Reg::PLL_MULT,4);
            b8::write8(Reg::PLL_POSTDIV,2);b8::write8(Reg::PB_DIV,8);
            b8::write8(Reg::CLK_SOURCE,2);
            b8::write8(Reg::CLK_KEY,0xC3);b8::write8(Reg::CLK_KEY,0x3C);
            b8::write8(Reg::CLK_COMMIT,0xA5);
            for(Word settle=0;n::less<Word>(settle,500);settle=n::add<Word>(settle,1))
                b8::idle();
            return n::equal<Byte>(b8::read8(Reg::CLK_ACTIVE),2)
                && n::equal<Byte>(b8::read8(Reg::PB_ACTIVE),8)
                && n::equal<Byte>(n::bit_and<Byte>(b8::read8(Reg::CLK_STATUS),14),2);
        }
    }
    return false;
}
}
const b8::VectorTable vectors={timer,unused1,unused2,unused3,unused4,unused5};

void reset() {
    b8::write8(Reg::IRQ_GLOBAL,0);
    const Byte causes=b8::read8(Reg::RST_CAUSE);
    const Byte details=b8::read8(Reg::RST_DETAIL);
    faults=0;reset_details=details;
    if(!has(causes,3) && n::equal<Byte>(b8::read8(b16::sram(0x1000)),0xC7)){
        faults=b8::read8(b16::sram(0x1001));
        reset_details=n::bit_or(details,b8::read8(b16::sram(0x1002)));
    }
    if(has(causes,16))faults=n::bit_or(faults,ck);
    if(has(causes,4))faults=n::bit_or(faults,wd);
    if(has(causes,8))faults=n::bit_or(faults,dm);
    persist();
    b8::write8(Reg::RST_CAUSE,causes);b8::write8(Reg::RST_DETAIL,details);
    ticks=0;last_tick=0;cool=0;sample_age=0;run_age=0;tach_age=0;last_tach=0;
    stable_age=0;invalid_age=0;jar_age=0;release_age=0;
    previous_raw=0;stable=0;selected=0;hot=0;shown=255;
    guard=true;armed=false;previous_stop=false;running=false;fresh_since_service=false;
    off();
    b8::write8(Reg::GPIOB_DIR,0x21);b8::write8(Reg::GPIOA_DIR,7);
    b8::write8(b16::ANSELA,0x30);
    b8::write8(b16::PIN_KEY,0x5A);b8::write8(b16::PIN_KEY,0xA5);
    b8::write8(b16::PWM_ROUTE,2);b8::write8(b16::TACH_ROUTE,1);
    b8::write8(b16::VBLANK_ROUTE,1);b8::write8(b16::PIN_LOCK,1);
    clock_ok=clock_start();
    if(!clock_ok)fault(ck);
    b8::write8(Reg::WDT_SCALE,3);b8::write8(Reg::WDT_CTRL,0x81);
    // 65.54 ms expiry at 8 MHz; 16.38 ms lower service window.
    b8::write_staged(b8::deadman_limit,512);
    b8::write_staged(b8::deadman_window,128);
    b8::write8(Reg::DMT_CTRL,0x81);
    b8::write8(Reg::ADC_CHANNEL,0);b8::write8(Reg::ADC_PRESCALE,2);
    b8::write8(Reg::ADC_CTRL,3);
    b8::write8(Reg::T0_PRESCALE,0);b8::write_staged(b8::timer0_compare,999);
    b8::write8(Reg::IRQ_FLAGS,0x3F);b8::write8(Reg::IRQ_ENABLE,1);
    b8::write8(Reg::T0_CTRL,3);b8::write8(Reg::IRQ_GLOBAL,1);
    display(label(false));
}

void step() {
    const Word now=ticks;
    if(n::equal(now,last_tick)){b8::idle();return;}
    const Word dt=n::subtract(now,last_tick);last_tick=now;
    const Byte raw=contacts();
    const Byte pins=b8::read8(Reg::GPIOB_IN);
    const bool stop=!has(pins,4), jar=has(pins,8), permit=has(pins,16);
    const bool stop_edge=stop && !previous_stop;previous_stop=stop;
    jar_age=jar ? elapsed(jar_age,dt) : Word{0};
    stable_age=n::equal(raw,previous_raw) ? elapsed(stable_age,dt) : Word{0};
    previous_raw=raw;
    if(!n::less<Word>(stable_age,10))stable=raw;
    invalid_age=n::equal<Byte>(speed(raw),255) ? elapsed(invalid_age,dt) : Word{0};
    if(!n::less<Word>(invalid_age,40))fault(inf);
    if(!jar || (!permit && !guard))fault(il);
    if(clock_ok && (!has(b8::read8(Reg::CLK_STATUS),2)
                    || !n::equal<Byte>(b8::read8(Reg::CLK_ACTIVE),2))){
        clock_ok=false;fault(ck);
    }

    sample_age=elapsed(sample_age,dt);
    if(has(b8::read8(Reg::ADC_STATUS),2)){
        const Word code=b8::read_latched(b8::adc_result);
        const float temperature=static_cast<float>(code)*(330.0F/1023.0F)-50.0F;
        // Nominal AVT10 output is 0.30..1.75 V (-20..125 C).
        // 0.25..1.80 V allows quantization/noise; rails are signal faults only.
        const bool plausible=!n::less<Word>(code,78) && !n::less<Word>(558,code);
        if(!plausible){fault(ts);hot=0;cool=0;}
        else {
            hot=temperature>=85.0F ? n::add<Byte>(hot,n::less<Byte>(hot,2) ? Byte{1} : Byte{0}) : Byte{0};
            if(!n::less<Byte>(hot,2))fault(th);
            cool=temperature<=65.0F ? elapsed(cool,sample_age) : Word{0};
        }
        sample_age=0;fresh_since_service=true;
        b8::write8(Reg::ADC_CTRL,3);
    }
    if(n::less<Word>(100,sample_age)){fault(ts);cool=0;}

    const bool recoverable=clock_ok && jar && !n::less<Word>(jar_age,20)
        && !n::less<Word>(cool,2000) && !n::less<Word>(100,sample_age);
    if(guard && stop_edge && recoverable){
        faults=0;reset_details=0;persist();guard=false;
    }
    if(stop || guard){armed=false;selected=0;release_age=0;}
    else if(!armed){
        release_age=n::equal<Byte>(raw,0) ? elapsed(release_age,dt) : Word{0};
        if(!n::less<Word>(release_age,10)){armed=true;stable=0;}
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
        const Word tach=b8::read_latched(b8::tach_count);
        if(!running){run_age=0;tach_age=0;last_tach=tach;}
        run_age=elapsed(run_age,dt);
        tach_age=n::equal(tach,last_tach) ? elapsed(tach_age,dt) : Word{0};
        last_tach=tach;
        if(!n::less<Word>(run_age,500) && !n::less<Word>(tach_age,100)){
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
    if(fresh_since_service && !n::less<Word>(b8::read_latched(b8::deadman_count),160)){
        b8::write8(Reg::DMT_PRECLR,0x69);b8::write8(Reg::DMT_CLR,0x96);
        b8::write8(Reg::WDT_SERVICE,0xA5);b8::write8(Reg::WDT_SERVICE,0x5A);
        fresh_since_service=false;
    }
}
}
