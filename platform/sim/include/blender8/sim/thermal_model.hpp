#pragma once
#include "blender8/sim/analog.hpp"
namespace b8::sim {
// Teaching-scale case, nearby-air, and optional food heat capacities.
class LumpedThermal {
public:
    explicit LumpedThermal(ThermalNode& node) : node_(node) {}
    // Fixture action: replace the local air and food with material at these temperatures.
    void set_environment(double ambient_c, double food_c, double food_w_per_k);
    void set_food(double food_c, double food_w_per_k); // fixture: preserve case and nearby air
    void set_initial_temperature(double celsius); // fixture-only; preserves physical state on MCU reset
    void advance(double seconds, double duty, double rpm, double no_load_rpm, bool enabled);
    [[nodiscard]] double temperature_c() const noexcept { return node_.celsius; }
    [[nodiscard]] double air_temperature_c() const noexcept { return air_c_; }
    [[nodiscard]] double food_temperature_c() const noexcept { return food_c_; }
    [[nodiscard]] bool food_present() const noexcept { return food_g_ > 0; }
    [[nodiscard]] double room_temperature_c() const noexcept { return room_c_; }
    [[nodiscard]] double case_air_w() const noexcept { return air_g_*(node_.celsius-air_c_); }
    [[nodiscard]] double case_food_w() const noexcept { return food_g_*(node_.celsius-food_c_); }
    [[nodiscard]] double food_air_w() const noexcept { return food_present()?0.12*(food_c_-air_c_):0; }
    [[nodiscard]] double ventilation_w() const noexcept { return 20*(air_c_-room_c_); }
    [[nodiscard]] double last_heat_w() const noexcept { return heat_w_; }
    [[nodiscard]] double last_load_current_a() const noexcept { return load_current_a_; }
    [[nodiscard]] static double heat_w(double duty, double rpm, double no_load_rpm, bool enabled);
private:
    ThermalNode& node_;
    double room_c_=25,air_c_=25,food_c_=25,food_g_=0,air_g_=0.6,heat_w_=0,load_current_a_=0;
};
}
