#include "blender8/sim/thermal_model.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace b8::sim {
namespace {
constexpr double case_capacity=60.0,air_capacity=200.0;
constexpr double ventilation_g=20.0,food_air_g=0.12,load_resistance=1.0;
double load_current(double duty,double rpm,double free_rpm,bool enabled) {
    if(!enabled) return 0;
    const double d=std::clamp(duty,0.0,1.0);
    const double motion=free_rpm>1?std::clamp(rpm/free_rpm,0.0,1.0):0;
    return 10*d*std::sqrt(1-motion);
}
}
void LumpedThermal::set_environment(double ambient,double food,double g) {
    if(!std::isfinite(ambient)||!std::isfinite(food)||!std::isfinite(g)||g<0||g>0.2)
        throw std::invalid_argument("thermal environment: finite values; Gfood in [0,0.2]");
    room_c_=ambient;air_c_=ambient;food_c_=food;food_g_=g;food_capacity_=1200;custom_food();
}
void LumpedThermal::set_food(double food,double g,double capacity,std::string_view kind) {
    if(!std::isfinite(food)||food < -40||food>150||!std::isfinite(g)||g<0||g>.2||!std::isfinite(capacity)||capacity<100||capacity>10000)
        throw std::invalid_argument("food temperature [-40,150] C; conductance [0,0.2] W/K");
    food_c_=food;food_g_=g;food_capacity_=capacity;food_kind_=g>0?kind:"empty";
}
void LumpedThermal::set_initial_temperature(double t) {
    if(!std::isfinite(t)||t < -40||t>150) throw std::out_of_range("initial temperature [-40,150] C");
    node_.celsius=t;
}
double LumpedThermal::heat_w(double duty,double rpm,double free_rpm,bool enabled) {
    if(!std::isfinite(duty)||!std::isfinite(rpm)||!std::isfinite(free_rpm))
        throw std::invalid_argument("non-finite thermal input");
    if(!enabled) return 0;
    const double d=std::clamp(duty,0.0,1.0);
    const double current=load_current(d,rpm,free_rpm,enabled);
    return 8*d+22*d*d+load_resistance*current*current;
}
void LumpedThermal::advance(double dt,double duty,double rpm,double free_rpm,bool enabled) {
    if(!std::isfinite(dt)||dt<0) throw std::invalid_argument("thermal dt must be finite and nonnegative");
    if(dt>100000) throw std::invalid_argument("thermal dt exceeds supported fixture interval");
    heat_w_=heat_w(duty,rpm,free_rpm,enabled);
    load_current_a_=load_current(duty,rpm,free_rpm,enabled);
    const double g_air=0.6+0.8*std::clamp(rpm/20000.0,0.0,1.0);
    air_g_=g_air;
    const double g_food_air=food_present()?food_air_g:0.0;
    using State=std::array<double,3>; // case, local air, optional food
    auto rate=[&](const State& t) {
        const double case_air=g_air*(t[0]-t[1]);
        const double case_food=food_g_*(t[0]-t[2]);
        const double food_air=g_food_air*(t[2]-t[1]);
        return State{(heat_w_-case_air-case_food)/case_capacity,
                     (case_air+food_air-ventilation_g*(t[1]-room_c_))/air_capacity,
                     (case_food-food_air)/food_capacity_};
    };
    auto add=[](const State& a,const State& b,double scale) {
        return State{a[0]+scale*b[0],a[1]+scale*b[1],a[2]+scale*b[2]};
    };
    State t{node_.celsius,air_c_,food_c_};
    const auto steps=static_cast<unsigned>(std::ceil(dt/0.1));
    if(steps==0) return;
    const double h=dt/steps;
    for(unsigned i=0;i<steps;++i) {
        const State k1=rate(t),k2=rate(add(t,k1,h/2)),k3=rate(add(t,k2,h/2)),k4=rate(add(t,k3,h));
        for(unsigned j=0;j<3;++j)t[j]+=h*(k1[j]+2*k2[j]+2*k3[j]+k4[j])/6;
    }
    node_.celsius=t[0];air_c_=t[1];food_c_=t[2];
}
}
