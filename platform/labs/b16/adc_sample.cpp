#include "adc_sample.hpp"

#include "blender8/access.hpp"
#include "blender8/device.hpp"
#include "blender8/registers.hpp"

namespace astin {
namespace {

inline constexpr std::uint8_t adc_ready = 0x02;
inline constexpr std::uint16_t adc_code_mask = 0x03ff;

}  // namespace

AdcSample read_adc_if_ready() {
    AdcSample sample{};
    sample.status = b8::read8(b8::Reg::ADC_STATUS);
    if ((sample.status & adc_ready) == 0) {
        return sample;
    }

    sample.code = static_cast<std::uint16_t>(b8::read_latched(b8::adc_result) & adc_code_mask);
    sample.fresh = true;
    return sample;
}

}  // namespace astin
