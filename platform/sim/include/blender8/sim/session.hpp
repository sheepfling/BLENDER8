#pragma once
#include "blender8/sim/runtime.hpp"
#include "blender8/sim/scene_observation.hpp"
#include <string>
#include <string_view>

namespace b8::sim {
// HOST ONLY. This facade is shared by the native process and the Wasm bridge.
// Firmware still receives only the B8 SDK and never a Session or Board reference.
struct FirmwareImage {
    std::span<const Isr> vectors{};
    void (*reset)() = nullptr;
    void (*step)() = nullptr;
    std::string name = "student";
    DeviceProfile device = DeviceProfile::b8;
};
struct SessionOptions {
    bool bench = false;
    BoardProfile profile = BoardProfile::chassis04;
};
class Session final {
public:
    Session(FirmwareImage image, SessionOptions options = {});
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    [[nodiscard]] std::string hello();
    // One nonempty command, at most 4096 bytes. Rejected commands return ok:false.
    // Execution failures latch the existing host inhibit, NOT a simulated reset.
    [[nodiscard]] std::string execute(std::string_view command);
    // Host presentation observes without consuming MMIO data or modifying peripheral state.
    [[nodiscard]] SceneObservation scene() { return observe_scene(board_); }
    // Same deterministic advancement as the command path; no rendering/wall clock enters the plant.
    void advance_view(Tick duration);
    [[nodiscard]] std::string_view device_name() const noexcept {return image_.device==DeviceProfile::b16?"B16":"B8";}
    [[nodiscard]] bool bench_mode() const noexcept { return options_.bench; }
    [[nodiscard]] const std::string& firmware_name() const noexcept { return image_.name; }
    void set_bus_trace(std::function<void(const BusTrace&)> sink);
    void close() noexcept;
    [[nodiscard]] bool closed() const noexcept { return closed_; }
private:
    void advance(Tick duration);
    FirmwareImage image_;
    SessionOptions options_;
    Board board_;
    Runtime runtime_;
    bool closed_ = false;
};
} // namespace b8::sim
