#include "firmware.hpp"
#include "board_config.hpp"
#if defined(B8_TARGET_B16)
#include "blender8/b16.hpp"
#endif
namespace firmware {
namespace {
void unimplemented_timer0() { b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::timer0)); }
void unimplemented_timer1() { b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::timer1)); }
void unimplemented_tach() { b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::tach)); }
void unimplemented_vblank() { b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::vblank)); }
#if defined(B8_TARGET_B16)
void unimplemented_dmac() {
    b8::write8(b16::DMA_STATUS, 0x1C);
    b8::write8(b8::Reg::IRQ_FLAGS, b16::dmac_irq_mask);
}
#endif
void unimplemented_adc() { b8::write8(b8::Reg::IRQ_FLAGS, b8::mask(b8::Irq::adc)); }
}
const b8::VectorTable vectors{unimplemented_timer0, unimplemented_timer1,
                            unimplemented_tach, unimplemented_vblank, unimplemented_adc
#if defined(B8_TARGET_B16)
                            , unimplemented_dmac
#endif
};
void reset() {
    // Intentionally safe starter, not a completed assignment.
    b8::write8(b8::Reg::IRQ_GLOBAL, 0);
    b8::write8(b8::Reg::PWM_CTRL, 0);
    b8::write8(b8::Reg::GPIOB_OUT, 0);
    b8::write8(b8::Reg::GPIOB_DIR, board_config::motor_enable_mask);
    // TODO: startup lock, GPIO mux, timers, interrupt enable, LCD initialization.
}
void step() {
    // TODO: scan/debounce, inverse motor mapping, pixel renderer, ADC, fault supervision.
    // Use the current customer correspondence for product behavior.
    // Historical REQ-FW IDs and retired algorithm choices are not the current rubric.
    // Chassis-04 jar status is observed through GPIOB bits 3 and 4; see correspondence.
    b8::idle();
}
}
