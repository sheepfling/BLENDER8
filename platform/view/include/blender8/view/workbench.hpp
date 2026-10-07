#pragma once
#include "blender8/view/canvas.hpp"
#include "blender8/sim/session.hpp"
#include <array>
#include <deque>
#include <map>
#include <initializer_list>
#include <optional>
#include <string>
namespace b8::view {
enum class Control : unsigned {
    speed1=1,speed2,speed3,speed4,speed5,speed6,speed7,pulse,stop,
    appliance=20,chassis,run,step,rate,reset,power,jar,jam,load_down,load_up,
    hot,cool,brownout,clock_loss,core_halt,foreground,sensor_open,exhibit,thermal,truth,systems,motor,food,water,frozen,vegetables,empty,bounce,supervision,lcd_bus
};
struct ControlBox {Control id;Rect rect;std::string label;bool active=false,enabled=true;};
struct SignalSample { b8::sim::Tick time_us; unsigned contacts;bool jar,permit,drive; };
struct PlantSample {
    b8::sim::Tick time_us;
    std::optional<double> wire_c,adc_c,case_c,sensor_c,air_c,food_c,room_c,rpm,loss_w,current_a,duty;
};
// Host application, NOT firmware. Scene logic, hit testing and all drawing live in C++.
class Workbench final {
public:
    explicit Workbench(b8::sim::Session& session);
    void resize(unsigned width,unsigned height);
    void frame(double elapsed_ms);   // pacing only; never used as a peripheral clock
    void render(double animation_ms=0); // strictly observational; can run without stepping
    void step(b8::sim::Tick duration);
    // Coordinates are physical canvas pixels; C++ owns layout conversion and hit testing.
    void pointer(unsigned type,int id,Point position); // 0 down, 1 up, 2 move, 3 cancel
    void key(int ascii,bool down);
    void release_inputs(bool pause=true); // legacy pause or explicit release-only input
    [[nodiscard]] std::string command(std::string_view line);
    [[nodiscard]] std::string status_json();
    [[nodiscard]] std::string journal_json()const;
    [[nodiscard]] const Canvas& canvas()const noexcept{return canvas_;}
    [[nodiscard]] bool running()const noexcept{return running_;}
    [[nodiscard]] unsigned selected_view()const noexcept{return view_;}
    [[nodiscard]] std::vector<ControlBox> controls()const;
    [[nodiscard]] const std::deque<PlantSample>& plant_history()const noexcept{return plant_;}
    [[nodiscard]] const std::deque<SignalSample>& signals()const noexcept{return signals_;}
    [[nodiscard]] const std::deque<SignalSample>& contact_capture()const noexcept{return contact_capture_;}
private:
    void activate(Control id);
    void press(int token,Control id);
    void release(int token);
    void sync_momentary();
    void advance(b8::sim::Tick duration);
    void record(std::string_view command);
    void start_exhibit();
    void draw_controls();
    void draw_appliance();
    void draw_chassis();
    void draw_instruments();
    void sample_plant();
    void draw_thermal(bool truth);
    void draw_motor();
    void draw_systems();
    void draw_supervision();
    void draw_lcd_bus();
    struct Trace {std::string_view name;Color color;std::optional<double> PlantSample::*member;bool hold=false;};
    void plot(Rect r,std::initializer_list<Trace> traces,std::string_view unit);
    void draw_signals();
    void draw_lcd(Rect rect);
    void wire(Point a,Point b,bool high);
    void card(Rect r,std::string_view title,std::string_view value,std::string_view suffix,Color color);
    b8::sim::Session& session_;
    b8::sim::SceneObservation state_{};
    Canvas canvas_;
    std::map<int,Control> held_;
    std::deque<SignalSample> signals_,contact_capture_;
    std::deque<PlantSample> plant_;
    std::vector<std::string> journal_;
    bool journal_complete_=true,running_=false,sent_pulse_=false,sent_stop_=false,stop_release_pending_=false;
    unsigned view_=0,rate_index_=2;
    double pacing_us_=0,jar_lift_=0,food_drop_ms_=0;
    std::array<double,9> button_travel_{};
    b8::sim::Tick next_sample_=100,next_plant_sample_=100000,exhibit_until_=0,capture_until_=0,stop_pressed_at_=0;
    std::string error_;
};
}
