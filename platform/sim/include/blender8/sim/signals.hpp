#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace b8::sim {
using Tick = std::uint64_t; // absolute microseconds, instructor side only
using Driver = std::size_t;
enum class Logic { low, high, high_z, unknown };
class DigitalNet {
public:
    explicit DigitalNet(Logic bias=Logic::unknown) : bias_(bias) {}
    [[nodiscard]] Driver attach(Logic initial=Logic::high_z) {
        drivers_.push_back(initial); return drivers_.size()-1;
    }
    void drive(Driver driver, Logic value) { drivers_.at(driver)=value; }
    [[nodiscard]] Logic resolve() const {
        bool low=false, high=false;
        for (const auto value:drivers_) {
            if (value==Logic::unknown) return Logic::unknown;
            low |= value==Logic::low; high |= value==Logic::high;
        }
        if (low && high) return Logic::unknown;
        if (low) return Logic::low;
        if (high) return Logic::high;
        return bias_;
    }
    [[nodiscard]] bool sample() const {
        const auto value=resolve();
        if (value!=Logic::high && value!=Logic::low)
            throw std::runtime_error("floating or contended digital net");
        return value==Logic::high;
    }
private:
    Logic bias_;
    std::vector<Logic> drivers_;
};
class BytePeripheral {
public:
    virtual ~BytePeripheral()=default;
    [[nodiscard]] virtual std::uint8_t bus_read(Tick now, std::uint8_t reg)=0;
    virtual void bus_write(Tick now, std::uint8_t reg, std::uint8_t data)=0;
};
} // namespace b8::sim
