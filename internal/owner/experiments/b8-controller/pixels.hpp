#pragma once
#include "byte_pair.hpp"
#include "compact_font.hpp"

namespace firmware {
// Stream one PX32-16 byte without a framebuffer or a native word temporary.
constexpr bytes::Byte pixel_byte(bytes::Byte label, bytes::Byte address) {
    namespace n = b8::numeric;
    using Byte = bytes::Byte;
    const auto cell = n::divide<Byte>(address, 32);
    const bool single = n::less<Byte>(label, 9);
    const Byte left = single ? Byte{11} : Byte{5};
    const Byte right = single ? Byte{21} : Byte{27};
    if (n::less(cell.remainder, left) || !n::less(cell.remainder, right)) return 0;
    Byte x = n::subtract(cell.remainder, left);
    Byte letter = 0;
    if (!n::less<Byte>(x, 10)) {
        if (n::less<Byte>(x, 12)) return 0;
        x = n::subtract<Byte>(x, 12);
        letter = 1;
    }
    const Byte column = columns[letters[label][letter]][n::divide<Byte>(x, 2).quotient];
    Byte result = 0;
    for (Byte bit = 0; n::less<Byte>(bit, 8); bit = n::add<Byte>(bit, 1)) {
        const Byte y = n::add(bit, n::multiply<Byte>(cell.quotient, 8));
        if (n::equal<Byte>(y, 0) || !n::less<Byte>(y, 15)) continue;
        const Byte row = n::divide<Byte>(n::subtract<Byte>(y, 1), 2).quotient;
        if (!n::equal<Byte>(n::bit_and(column, n::shift_left<Byte>(1, row)), 0))
            result = n::bit_or(result, n::shift_left<Byte>(1, bit));
    }
    return result;
}
} // namespace firmware
