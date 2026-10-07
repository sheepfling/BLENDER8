#pragma once
#include "blender8/sim/components.hpp"
#include "blender8/sim/mcu.hpp"
#include <optional>
namespace b8::sim {
class Board;
// Host-only, read-only, non-invasive scene observation. Unknown probes stay optional.
struct SceneObservation {
    Tick time_us = 0;
    bool powered = false, ready = false, failed = false, motor_supply = false;
    bool drive = false, pwm = false, run_permit = false, stop = false;
    bool jar_seated = false, jar_ok = false, jar_permit = false, foreground = false;
    bool lcd_vblank = false, clock_valid = false, core_halted = false;
    std::optional<bool> clock_failed, sensor_open, brownout_forced;
    std::array<Logic,8> gpioa{},gpiob{};
    std::uint8_t contacts = 0;
    double rail_v = 0, analog_v = 0;
    McuObservation mcu{};
    std::optional<bool> rear_power;
    std::optional<ButtonObservation> buttons;
    std::optional<MotorObservation> motor;
    std::optional<double> shaft_turns, sensor_c, load, nearby_air_c, food_c, load_current_a;
    std::optional<double> room_c,case_air_w,case_food_w,food_air_w,ventilation_w;
    std::optional<DmaObservation> dma;
    std::optional<bool> food_present;
    std::optional<bool> jammed;
    std::optional<std::array<bool,512>> pixels;
};
[[nodiscard]] SceneObservation observe_scene(Board& board);
}
