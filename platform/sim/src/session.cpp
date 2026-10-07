#include "blender8/sim/session.hpp"
#include "blender8/sim/fixture.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <optional>
#include <sstream>
#include <tuple>
#include <vector>

namespace b8::sim {
namespace {
Tick number(const std::string& text, Tick maximum) {
    Tick n = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), n);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || n > maximum)
        throw std::invalid_argument("invalid time/count");
    return n;
}
void no_extra(std::istringstream& input) {
    std::string extra;
    if (input >> extra) throw std::invalid_argument("unexpected command argument");
}
}
Session::Session(FirmwareImage image, SessionOptions options)
    : image_(std::move(image)), options_(options), board_(options.profile, {}, image_.device),
      runtime_(board_, image_.vectors) {
    if (image_.device == DeviceProfile::b16 && options.profile == BoardProfile::legacy02)
        throw std::invalid_argument("B16 requires the clocked chassis");
    if (!options.bench && options.profile == BoardProfile::legacy02)
        throw std::invalid_argument("legacy02 is a bench-only regression profile");
    if (!options.bench && (!image_.reset || !image_.step))
        throw std::invalid_argument("firmware callbacks required");
}
Session::~Session() { close(); }
void Session::close() noexcept {
    board_.set_observer({});
    board_.emergency_inhibit();
    closed_ = true;
}
void Session::set_bus_trace(std::function<void(const BusTrace&)> sink) {
    runtime_.set_trace(std::move(sink));
}
void Session::advance(Tick duration) {
    if (options_.bench) board_.advance(duration);
    else runtime_.run_for(duration, image_.reset, image_.step);
}
void Session::advance_view(Tick duration) {
    if (closed_ || duration > 20'000 || duration > std::numeric_limits<Tick>::max()-board_.now())
        throw std::invalid_argument("invalid graphical run chunk");
    try { advance(duration); }
    catch (...) { board_.emergency_inhibit(); throw; }
}
std::string Session::hello() {
    return "{\"ok\":true,\"protocol\":1,\"firmware\":" +
        json_string(options_.bench ? "BENCH_NO_FIRMWARE" : image_.name) +
        ",\"device\":" + json_string(image_.device == DeviceProfile::b16 ? "B16" : "B8") +
        ",\"interface\":" + (image_.device == DeviceProfile::b16 ? "4" : "3") +
        ",\"chassis\":" + (options_.profile == BoardProfile::legacy02 ? "2" : "4") +
        ",\"state\":" + snapshot_json(board_) + "}";
}
std::string Session::execute(std::string_view command) {
    try {
        if (closed_) throw std::logic_error("emulator session is closed");
        if (command.empty() || command.size() > 4096 ||
            command.find_first_of("\r\n") != std::string_view::npos ||
            command.find('\0') != std::string_view::npos)
            throw std::invalid_argument("one nonempty command line, at most 4096 bytes");
        std::istringstream input{std::string(command)};
        std::string op, output;
        input >> op;
        if (op == "quit") {
            no_extra(input); close();
            return "{\"ok\":true,\"closed\":true}";
        }
        if (op == "snapshot") {
            no_extra(input);
        } else if (op == "run" || op == "trace") {
            std::string duration_text;
            input >> duration_text;
            const Tick duration = number(duration_text, 10'000'000);
            if (duration > std::numeric_limits<Tick>::max() - board_.now())
                throw std::overflow_error("run duration");
            if (op == "run") {
                no_extra(input); advance(duration);
            } else {
                std::string step_text; input >> step_text;
                const Tick step = number(step_text, 10'000'000);
                if (!step || (duration / step + (duration % step != 0)) > 1000)
                    throw std::invalid_argument("trace sample limit (1000)");
                no_extra(input);
                std::vector<std::string> events;
                using Key = std::tuple<bool,bool,unsigned,unsigned,unsigned,unsigned,
                                       std::uint64_t,std::uint64_t,unsigned,bool,bool>;
                std::optional<Key> previous;
                board_.set_observer([&](Board& board) {
                    const auto m = board.mcu().observe();
                    const Key key{board.ready(), board.drive_enabled(), m.gpiob_out,
                        m.pwm_enabled, m.pwm_shadow, m.clock_source, m.reset_serial,
                        m.adc_fresh_reads, m.wdt_control, board.jar().raw_closed(),
                        board.jar().permitted()};
                    if (!previous || key != *previous) {
                        if (events.size() >= 10000) throw std::length_error("trace event limit");
                        previous = key; events.push_back(snapshot_json(board));
                    }
                });
                output = ",\"samples\":[";
                bool first = true;
                const Tick end = board_.now() + duration;
                while (board_.now() < end) {
                    advance(std::min(step, end - board_.now()));
                    if (!first) output += ',';
                    first = false; output += snapshot_json(board_);
                }
                board_.set_observer({});
                output += "],\"events\":[";
                for (std::size_t i = 0; i < events.size(); ++i) {
                    if (i) output += ',';
                    output += events[i];
                }
                output += ']';
            }
        } else if (op == "schedule") {
            std::string at, fixture; input >> at; std::getline(input, fixture);
            const auto when = number(at, std::numeric_limits<Tick>::max());
            board_.schedule(when, parse_fixture_command(fixture, options_.bench));
        } else {
            auto action = parse_fixture_command(command, options_.bench);
            action(board_); board_.settle();
        }
        return "{\"ok\":true,\"state\":" + snapshot_json(board_) + output + "}";
    } catch (const std::exception& error) {
        board_.set_observer({});
        return "{\"ok\":false,\"error\":" + json_string(error.what()) +
            ",\"host_failed\":" + (board_.failed() ? "true" : "false") + "}";
    } catch (...) {
        board_.set_observer({}); board_.emergency_inhibit();
        return "{\"ok\":false,\"error\":\"unknown execution exception\",\"host_failed\":true}";
    }
}
} // namespace b8::sim
