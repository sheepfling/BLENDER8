#include "blender8/sim/scene_observation.hpp"
#include "blender8/sim/board.hpp"
namespace b8::sim {
SceneObservation observe_scene(Board& b) {
    SceneObservation s;
    s.time_us=b.now(); s.powered=b.powered();s.ready=b.ready();s.failed=b.failed();
    s.motor_supply=b.power_device().motor_supply();s.drive=b.drive_enabled();s.pwm=b.pwm_level();
    s.run_permit=b.run_permit();s.stop=b.stop_asserted();s.jar_seated=b.jar().seated();
    s.jar_ok=b.jar().raw_closed();s.jar_permit=b.jar().permitted();s.contacts=b.contact_mask();
    s.rail_v=b.power_device().volts();s.analog_v=b.analog_voltage();s.mcu=b.mcu().observe();
    if(b.mcu().device()==DeviceProfile::b16)s.dma=b.mcu().observe_dma();
    s.clock_valid=b.external_clock().valid;s.core_halted=b.mcu().core_halted();
    if(auto* p=dynamic_cast<CrystalOscillator*>(&b.clock_device()))s.clock_failed=p->failed();
    if(auto* p=dynamic_cast<TemperatureSensor*>(&b.sensor_device()))s.sensor_open=p->fault()==TemperatureFault::open;
    if(auto* p=dynamic_cast<PowerDomain*>(&b.power_device()))s.brownout_forced=p->override_voltage().has_value() && *p->override_voltage()<2.9;
    for(unsigned i=0;i<8;++i){s.gpioa[i]=b.probe_gpio(0,i);s.gpiob[i]=b.probe_gpio(1,i);}
    s.foreground=b.foreground_enabled();s.lcd_vblank=b.display_device().vblank();
    if(auto* p=dynamic_cast<PowerDomain*>(&b.power_device()))s.rear_power=p->requested();
    if(auto* p=dynamic_cast<ButtonProbe*>(&b.button_device()))s.buttons=p->observe_buttons();
    if(auto* p=dynamic_cast<MotorProbe*>(&b.motor_device()))s.motor=p->observe_motor();
    if(auto* p=dynamic_cast<RotationProbe*>(&b.motor_device()))s.shaft_turns=p->shaft_turns();
    if(auto* p=dynamic_cast<Motor*>(&b.motor_device())){
        s.load=p->load();s.jammed=p->jammed();
        s.nearby_air_c=p->thermal().air_temperature_c();
        s.food_present=p->thermal().food_present();
        if(*s.food_present)s.food_c=p->thermal().food_temperature_c();
        s.load_current_a=p->thermal().last_load_current_a();
        s.room_c=p->thermal().room_temperature_c();s.case_air_w=p->thermal().case_air_w();
        s.case_food_w=p->thermal().case_food_w();s.food_air_w=p->thermal().food_air_w();
        s.ventilation_w=p->thermal().ventilation_w();
    }
    if(auto* p=dynamic_cast<SensorProbe*>(&b.sensor_device());p&&p->debug_sensor_available())s.sensor_c=p->debug_sensor_c();
    if(auto* p=dynamic_cast<DisplayProbe*>(&b.display_device())){
        s.pixels.emplace(); for(unsigned y=0;y<16;++y)for(unsigned x=0;x<32;++x)(*s.pixels)[y*32+x]=p->pixel(x,y);
    }
    return s;
}
}
