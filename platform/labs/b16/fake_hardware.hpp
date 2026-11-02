#pragma once

#include "blender8/host/backend.hpp"

#include <cstdint>
#include <deque>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace astin::lab {

// A strict, scripted register endpoint for host-only driver tests. It records byte
// transfers and idle service boundaries; it does not model a peripheral or board.
class FakeHardware final : public b8::host::RegisterBackend {
public:
    enum class Operation { read, write, idle };

    struct Event {
        std::uint64_t index{};
        std::uint64_t service_us{};
        Operation operation{};
        std::optional<b8::Reg> reg;
        std::uint8_t value{};
    };

    void set_read_value(b8::Reg reg, std::uint8_t value);
    void script_reads(b8::Reg reg, std::initializer_list<std::uint8_t> values);

    [[nodiscard]] std::uint8_t read8(b8::Reg reg) override;
    void write8(b8::Reg reg, std::uint8_t value) override;
    void idle() override;

    [[nodiscard]] const std::vector<Event>& events() const noexcept { return events_; }
    [[nodiscard]] std::string trace() const;
    void require_scripts_consumed() const;

private:
    void record(Operation operation, std::optional<b8::Reg> reg, std::uint8_t value);

    std::map<b8::Reg, std::uint8_t> constant_reads_;
    std::map<b8::Reg, std::deque<std::uint8_t>> scripted_reads_;
    std::vector<Event> events_;
    std::uint64_t next_index_{};
    std::uint64_t service_us_{};
};

}  // namespace astin::lab
