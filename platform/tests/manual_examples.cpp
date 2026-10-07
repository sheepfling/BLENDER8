// Owner-side execution checks for the exact peripheral examples printed in MCU-001.
#include "blender8/sim/runtime.hpp"
#include "../examples/manual_snippets.inc"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using b8::Reg;
void check(bool okay, const char* message) {
    if (!okay) { throw std::runtime_error(message); }
}
void run(const std::string& name) {
    b8::sim::Board board(b8::sim::BoardProfile::clocked03);
    board.advance(45000);
    check(board.ready(), "board did not release reset");
    const b8::VectorTable vectors{};
    b8::sim::Runtime runtime(board, vectors);
    if (name == "ack") {
        b8::write8(Reg::T0_CTRL, 3);
        b8::write8(Reg::T0_CTRL, 0); // pending event remains
        b8::write8(Reg::ADC_CTRL, 3);
        for (unsigned i=0; i<60; ++i) { b8::idle(); }
        check((b8::read8(Reg::IRQ_FLAGS)&0x11u)==0x11u, "fixtures not pending");
        nmd_examples::acknowledge_timer0();
        check((b8::read8(Reg::IRQ_FLAGS)&0x11u)==0x10u, "ack erased unrelated ADC");
    } else if (name == "gpio") {
        b8::write8(Reg::GPIOA_OUT, 0xFF);
        nmd_examples::configure_pad5();
        check(b8::read8(Reg::GPIOA_DIR)==0x20, "wrong pad direction");
        check(b8::read8(Reg::GPIOA_OUT)==0xDF, "wrong preloaded latch");
    } else if (name == "handler") {
        b8::write8(Reg::T0_CTRL, 3); b8::write8(Reg::T0_CTRL, 0);
        nmd_examples::timer0_handler();
        check(!(b8::read8(Reg::IRQ_FLAGS)&1u), "handler did not acknowledge");
    } else if (name == "timer") {
        nmd_examples::configure_timer1(1, 249);
        check(b8::read8(Reg::T1_COMPARE_LO)==249, "wrong compare low");
        check(b8::read8(Reg::T1_COMPARE_HI)==0, "wrong compare high");
        check(b8::read8(Reg::T1_PRESCALE)==1, "wrong prescaler");
        check(b8::read8(Reg::T1_CTRL)==3, "timer not periodic/enabled");
    } else if (name == "pair") {
        b8::write8(Reg::IRQ_GLOBAL, 1);
        check(nmd_examples::read_pair(Reg::TACH_COUNT_LO, Reg::TACH_COUNT_HI)==0,
              "unexpected stationary count");
        check(b8::read8(Reg::IRQ_GLOBAL)==1, "pair failed to restore global enable");
    } else if (name == "external") {
        check(nmd_examples::read_external(3)==2, "LCD control reset state");
    } else if (name == "adc") {
        check(!nmd_examples::collect_adc().has_value(), "invented completion");
        b8::write8(Reg::ADC_CTRL,3);
        for (unsigned i=0;i<60;++i) { b8::idle(); }
        const auto code=nmd_examples::collect_adc();
        check(code.has_value() && *code<=1023, "lost ADC completion");
        check(!nmd_examples::collect_adc().has_value(), "reused old completion");
        check(b8::read8(Reg::IRQ_FLAGS)&0x10u, "data read erased ADC IRQ");
    } else if (name == "clock") {
        // Commit reset's staged FRC plan: intentionally no application PLL tuple supplied.
        b8::write8(Reg::IRQ_GLOBAL,1);
        nmd_examples::commit_clock();
        check(!(b8::read8(Reg::CLK_STATUS)&8u), "valid protected commit rejected");
        check(b8::read8(Reg::CLK_ACTIVE)==0, "wrong FRC source");
        check(b8::read8(Reg::IRQ_GLOBAL)==1, "clock wrapper lost global state");
    } else { throw std::invalid_argument("unknown example"); }
}
}
int main(int argc, char** argv) {
    try {
        if (argc!=2) { throw std::invalid_argument("one example name required"); }
        run(argv[1]);
        std::cout << "PASS manual." << argv[1] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
