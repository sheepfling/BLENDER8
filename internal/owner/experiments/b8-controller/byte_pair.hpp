#pragma once
#include "blender8/numeric.hpp"
#include <cstdint>

namespace firmware::bytes {
using Byte = std::uint8_t;
namespace n = b8::numeric;

// A two-byte representation, with only byte ALU operations in firmware.
// low is first to match the peripheral capture/staging order.
struct Pair { Byte low = 0; Byte high = 0; };

constexpr bool equal(Pair a, Pair b) {
    return n::equal(a.low, b.low) && n::equal(a.high, b.high);
}
constexpr bool less(Pair a, Pair b) {
    return n::less(a.high, b.high)
        || (n::equal(a.high, b.high) && n::less(a.low, b.low));
}
constexpr Pair add(Pair a, Pair b) {
    const Byte low = n::add(a.low, b.low);
    const Byte carry = n::less(low, a.low) ? Byte{1} : Byte{0};
    return {low, n::add(n::add(a.high, b.high), carry)};
}
constexpr Pair subtract(Pair a, Pair b) {
    const Byte borrow = n::less(a.low, b.low) ? Byte{1} : Byte{0};
    return {n::subtract(a.low, b.low), n::subtract(n::subtract(a.high, b.high), borrow)};
}
constexpr Pair elapsed(Pair value, Pair delta) {
    // Saturate at 30,000 ms, including a wrapped addition.
    constexpr Pair cap{0x30, 0x75};
    const Pair result = add(value, delta);
    return less(result, value) || !less(result, cap) ? cap : result;
}
} // namespace firmware::bytes
