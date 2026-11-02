#pragma once

#include <cstdint>

namespace astin {

struct AdcSample {
    std::uint8_t status{};
    std::uint16_t code{};
    bool fresh{};
};

// Read one completed result. The caller must own this sequence across foreground/ISR use.
// `fresh` means READY was observed and the latched result was consumed; the caller still
// interprets ERROR and OVERRUN policy. This helper does not mask interrupts.
[[nodiscard]] AdcSample read_adc_if_ready();

}  // namespace astin
