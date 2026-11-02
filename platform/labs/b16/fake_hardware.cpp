#include "fake_hardware.hpp"

#include "blender8/b16.hpp"

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace astin::lab {
namespace {

std::string_view register_name(b8::Reg reg) {
    switch (reg) {
        case b16::SYS_CAPS: return "SYS_CAPS";
        case b8::Reg::IRQ_GLOBAL: return "IRQ_GLOBAL";
        case b8::Reg::IRQ_ENABLE: return "IRQ_ENABLE";
        case b8::Reg::IRQ_FLAGS: return "IRQ_FLAGS";
        case b8::Reg::GPIOA_DIR: return "GPIOA_DIR";
        case b8::Reg::GPIOA_OUT: return "GPIOA_OUT";
        case b8::Reg::GPIOA_IN: return "GPIOA_IN";
        case b16::ANSELA: return "ANSELA";
        case b8::Reg::GPIOB_DIR: return "GPIOB_DIR";
        case b8::Reg::GPIOB_OUT: return "GPIOB_OUT";
        case b8::Reg::GPIOB_IN: return "GPIOB_IN";
        case b8::Reg::T0_CTRL: return "T0_CTRL";
        case b8::Reg::T0_PRESCALE: return "T0_PRESCALE";
        case b8::Reg::T0_COUNT_LO: return "T0_COUNT_LO";
        case b8::Reg::T0_COUNT_HI: return "T0_COUNT_HI";
        case b8::Reg::T0_COMPARE_LO: return "T0_COMPARE_LO";
        case b8::Reg::T0_COMPARE_HI: return "T0_COMPARE_HI";
        case b8::Reg::T1_CTRL: return "T1_CTRL";
        case b8::Reg::T1_PRESCALE: return "T1_PRESCALE";
        case b8::Reg::T1_COUNT_LO: return "T1_COUNT_LO";
        case b8::Reg::T1_COUNT_HI: return "T1_COUNT_HI";
        case b8::Reg::T1_COMPARE_LO: return "T1_COMPARE_LO";
        case b8::Reg::T1_COMPARE_HI: return "T1_COMPARE_HI";
        case b8::Reg::PWM_CTRL: return "PWM_CTRL";
        case b8::Reg::PWM_DUTY: return "PWM_DUTY";
        case b8::Reg::TACH_COUNT_LO: return "TACH_COUNT_LO";
        case b8::Reg::TACH_COUNT_HI: return "TACH_COUNT_HI";
        case b8::Reg::XBUS_REG: return "XBUS_REG";
        case b8::Reg::XBUS_DATA: return "XBUS_DATA";
        case b8::Reg::ADC_CTRL: return "ADC_CTRL";
        case b8::Reg::ADC_CHANNEL: return "ADC_CHANNEL";
        case b8::Reg::ADC_PRESCALE: return "ADC_PRESCALE";
        case b8::Reg::ADC_STATUS: return "ADC_STATUS";
        case b8::Reg::ADC_DATA_LO: return "ADC_DATA_LO";
        case b8::Reg::ADC_DATA_HI: return "ADC_DATA_HI";
        case b16::DMA_CMD: return "DMA_CMD";
        case b16::DMA_CONFIG: return "DMA_CONFIG";
        case b16::DMA_SRC_LO: return "DMA_SRC_LO";
        case b16::DMA_SRC_HI: return "DMA_SRC_HI";
        case b16::DMA_DST_LO: return "DMA_DST_LO";
        case b16::DMA_DST_HI: return "DMA_DST_HI";
        case b16::DMA_LEN_LO: return "DMA_LEN_LO";
        case b16::DMA_LEN_HI: return "DMA_LEN_HI";
        case b16::DMA_TRIGGER: return "DMA_TRIGGER";
        case b16::DMA_PACE: return "DMA_PACE";
        case b16::DMA_STATUS: return "DMA_STATUS";
        case b16::DMA_IEN: return "DMA_IEN";
        case b16::DMA_ERROR: return "DMA_ERROR";
        case b16::DMA_DONE_LO: return "DMA_DONE_LO";
        case b16::DMA_DONE_HI: return "DMA_DONE_HI";
        case b16::DMA_LEFT_LO: return "DMA_LEFT_LO";
        case b16::DMA_LEFT_HI: return "DMA_LEFT_HI";
        case b16::PIN_KEY: return "PIN_KEY";
        case b16::PIN_LOCK: return "PIN_LOCK";
        case b16::PIN_STATUS: return "PIN_STATUS";
        case b16::PWM_ROUTE: return "PWM_ROUTE";
        case b16::TACH_ROUTE: return "TACH_ROUTE";
        case b16::VBLANK_ROUTE: return "VBLANK_ROUTE";
        case b16::DREQ_ROUTE: return "DREQ_ROUTE";
        default: return "REG";
    }
}

std::uint16_t address(b8::Reg reg) {
    return static_cast<std::uint16_t>(reg);
}

std::string register_label(b8::Reg reg) {
    std::ostringstream label;
    label << register_name(reg) << "[0x" << std::hex << std::uppercase << std::setw(4)
          << std::setfill('0') << address(reg) << ']';
    return label.str();
}

}  // namespace

void FakeHardware::set_read_value(b8::Reg reg, std::uint8_t value) {
    if (scripted_reads_.contains(reg)) {
        throw std::logic_error("cannot mix constant and scripted reads for one register");
    }
    constant_reads_[reg] = value;
}

void FakeHardware::script_reads(b8::Reg reg, std::initializer_list<std::uint8_t> values) {
    if (constant_reads_.contains(reg)) {
        throw std::logic_error("cannot mix constant and scripted reads for one register");
    }
    if (values.size() == 0) {
        throw std::invalid_argument("a read script must contain at least one value");
    }
    auto& script = scripted_reads_[reg];
    script.insert(script.end(), values.begin(), values.end());
}

std::uint8_t FakeHardware::read8(b8::Reg reg) {
    std::uint8_t value{};
    if (auto script = scripted_reads_.find(reg); script != scripted_reads_.end()) {
        if (script->second.empty()) {
            throw std::logic_error("read script exhausted for " + register_label(reg));
        }
        value = script->second.front();
        script->second.pop_front();
    } else if (auto constant = constant_reads_.find(reg); constant != constant_reads_.end()) {
        value = constant->second;
    } else {
        throw std::logic_error("unexpected unscripted read of " + register_label(reg));
    }

    record(Operation::read, reg, value);
    return value;
}

void FakeHardware::write8(b8::Reg reg, std::uint8_t value) {
    record(Operation::write, reg, value);
}

void FakeHardware::idle() {
    record(Operation::idle, std::nullopt, 0);
}

void FakeHardware::record(Operation operation, std::optional<b8::Reg> reg, std::uint8_t value) {
    if (next_index_ == std::numeric_limits<std::uint64_t>::max() ||
        service_us_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("fake hardware trace counter overflow");
    }
    events_.push_back(Event{next_index_, service_us_, operation, reg, value});
    ++next_index_;
    ++service_us_;
}

std::string FakeHardware::trace() const {
    std::ostringstream output;
    for (const auto& event : events_) {
        output << '#' << std::setfill('0') << std::setw(2) << event.index << " @" << std::dec
               << event.service_us << "us ";
        switch (event.operation) {
            case Operation::read: output << "READ "; break;
            case Operation::write: output << "WRITE "; break;
            case Operation::idle: output << "IDLE"; break;
        }
        if (event.reg) {
            output << register_name(*event.reg) << "[0x" << std::hex << std::uppercase
                   << std::setw(4) << std::setfill('0') << address(*event.reg) << ']';
            if (event.operation != Operation::idle) {
                output << (event.operation == Operation::read ? " -> " : " <- ") << "0x"
                       << std::setw(2) << static_cast<unsigned>(event.value);
            }
        }
        output << '\n';
    }
    return output.str();
}

void FakeHardware::require_scripts_consumed() const {
    for (const auto& [reg, script] : scripted_reads_) {
        if (!script.empty()) {
            throw std::logic_error("unconsumed read script for " + register_label(reg));
        }
    }
}

}  // namespace astin::lab
