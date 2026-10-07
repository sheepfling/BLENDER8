#include "test_support.hpp"
#include "blender8/sim/fixture.hpp"
#include <bit>
#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace b8;using namespace b8::sim;using namespace b8::test;
namespace {
unsigned next(unsigned& x){x^=x<<13;x^=x>>17;x^=x<<5;return x;}
std::uint64_t digest(std::uint64_t h,std::uint64_t value){for(int i=0;i<8;++i){h^=value&255;h*=1099511628211ULL;value>>=8;}return h;}
std::uint64_t trajectory(unsigned seed,unsigned actions){
    Board b;boot(b);b.buttons().set_random_bounce(seed);b.temperature_sensor().set_noise(.002,seed);
    unsigned rng=seed?seed:1;std::uint64_t hash=1469598103934665603ULL;std::vector<std::string> history;
    try{
        for(unsigned a=0;a<actions;++a){
            const auto v=next(rng);std::string cmd;
            switch(v%15){
            case 0:cmd="speed "+std::to_string(1+next(rng)%7);break;
            case 1:cmd="pulse "+std::to_string(next(rng)%2);break;
            case 2:cmd="stop "+std::to_string(next(rng)%2);break;
            case 3:cmd="jar "+std::to_string(next(rng)%2);break;
            case 4:cmd="jam "+std::to_string(next(rng)%2);break;
            case 5:cmd="load "+std::to_string((next(rng)%101)/100.0);break;
            case 6:cmd="contact "+std::to_string(next(rng)%8)+(next(rng)%2?" normal":" closed");break;
            case 7:cmd="sensor "+std::string(next(rng)%2?"healthy":"ground");break;
            case 8:cmd="tach "+std::string(next(rng)%2?"healthy":"low");break;
            case 9:cmd="voltage "+std::string(next(rng)%2?"auto":"2.7");break;
            case 10:cmd="fuse motor "+std::to_string(next(rng)%2);break;
            case 11:cmd="reset";break;
            case 12:cmd="jar_fault "+std::string(next(rng)%2?"healthy":"open");break;
            case 13:cmd="environment 25 0 0.15";break;
            default:cmd="power "+std::to_string(next(rng)%2);break;
            }
            history.push_back(cmd);parse_fixture_command(cmd)(b);b.settle();
            if(b.ready()){
                service(b);write(b,Reg::GPIOB_DIR,1);write(b,Reg::GPIOB_OUT,1);
                write(b,Reg::PWM_DUTY,static_cast<std::uint8_t>(next(rng)%256));write(b,Reg::PWM_CTRL,1);
            }
            // Assert physical invariants every microsecond, not just at sparse trace points.
            for(unsigned i=0;i<1000;++i){
                b.advance(1);const auto m=b.motor().observe_motor();
                require(std::isfinite(m.rpm)&&m.rpm>=0&&m.rpm<=20000.00001,"bounded RPM");
                require(std::isfinite(m.case_c)&&m.case_c>-50&&m.case_c<300,"finite thermal state");
                require(!b.drive_enabled()||(b.powered()&&b.mcu().reset_released()&&b.power_device().motor_supply()&&b.jar().raw_closed()&&b.jar().permitted()&&b.run_permit()),"drive permission invariant");
                require(std::popcount(static_cast<unsigned>(b.buttons().ideal_mask()&127))<=1,"one-hot physical latch");
            }
            hash=digest(hash,b.now());hash=digest(hash,std::bit_cast<std::uint64_t>(b.motor().debug_rpm()));
            hash=digest(hash,std::bit_cast<std::uint64_t>(b.motor().thermal().temperature_c()));
            hash=digest(hash,b.contact_mask());hash=digest(hash,b.jar().permitted());hash=digest(hash,b.mcu().reset_serial());
        }
    }catch(const std::exception& e){
        std::cerr<<"seed="<<seed<<" actions="<<actions<<" last_time_us="<<b.now()<<" error="<<e.what()<<"\ncommands:\n";
        for(const auto& cmd:history)std::cerr<<cmd<<'\n';
        throw;
    }
    return hash;
}
unsigned parse(const char* s){unsigned n=0;std::string v(s);auto r=std::from_chars(v.data(),v.data()+v.size(),n);if(r.ec!=std::errc{}||r.ptr!=v.data()+v.size()||!n)throw std::invalid_argument("positive integer required");return n;}
}
int main(int argc,char** argv){
    try{
        unsigned seed=4815,episodes=32,actions=256;
        for(int i=1;i<argc;i+=2){if(i+1==argc)throw std::invalid_argument("option value missing");std::string k=argv[i];auto v=parse(argv[i+1]);if(k=="--seed")seed=v;else if(k=="--episodes")episodes=v;else if(k=="--actions")actions=v;else throw std::invalid_argument("unknown stress option");}
        if(episodes>10000||actions>100000)throw std::invalid_argument("stress bounds exceeded");
        std::uint64_t h=0;
        for(unsigned i=0;i<episodes;++i){const unsigned s=seed+i;const auto a=trajectory(s,actions),b=trajectory(s,actions);require(a==b,"same seed replay differs");h=digest(h,a);}
        std::cout<<"{\"passed\":true,\"seed\":"<<seed<<",\"episodes\":"<<episodes<<",\"replays_per_episode\":2,\"actions_per_episode\":"<<actions<<",\"checked_microsteps\":"<<static_cast<std::uint64_t>(episodes)*2*actions*1000<<",\"digest\":"<<h<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
