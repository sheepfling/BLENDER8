#pragma once
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

#ifndef B8_NUMERIC_PROFILE
#define B8_NUMERIC_PROFILE 8
#endif
static_assert(B8_NUMERIC_PROFILE == 8 || B8_NUMERIC_PROFILE == 16);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559
              && std::numeric_limits<float>::digits == 24);

// Width-preserving firmware operations. Wider host intermediates belong to this
// binding implementation, not to the device's native instruction repertoire.
namespace b8::numeric {
template<class T>
concept Integer = (std::same_as<T, std::uint8_t> || std::same_as<T, std::int8_t>
                   || std::same_as<T, std::uint16_t> || std::same_as<T, std::int16_t>)
                  && sizeof(T) * 8 <= B8_NUMERIC_PROFILE;

namespace detail {
template<Integer T> constexpr T bits(std::uint32_t value) {
    return std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(value));
}
template<Integer T> constexpr std::uint32_t unsigned_bits(T value) {
    return static_cast<std::make_unsigned_t<T>>(value);
}
} // namespace detail

template<Integer T> [[nodiscard]] constexpr T add(T a, T b) {
    return detail::bits<T>(detail::unsigned_bits(a) + detail::unsigned_bits(b));
}
template<Integer T> [[nodiscard]] constexpr T subtract(T a, T b) {
    return detail::bits<T>(detail::unsigned_bits(a) - detail::unsigned_bits(b));
}
template<Integer T> [[nodiscard]] constexpr T multiply(T a, T b) {
    return detail::bits<T>(detail::unsigned_bits(a) * detail::unsigned_bits(b));
}
template<Integer T> [[nodiscard]] constexpr T bit_and(T a, T b) {
    return detail::bits<T>(detail::unsigned_bits(a) & detail::unsigned_bits(b));
}
template<Integer T> [[nodiscard]] constexpr T bit_or(T a, T b) {
    return detail::bits<T>(detail::unsigned_bits(a) | detail::unsigned_bits(b));
}
template<Integer T> [[nodiscard]] constexpr T bit_xor(T a, T b) {
    return detail::bits<T>(detail::unsigned_bits(a) ^ detail::unsigned_bits(b));
}
template<Integer T> [[nodiscard]] constexpr T bit_not(T value) {
    return detail::bits<T>(~detail::unsigned_bits(value));
}
template<Integer T> [[nodiscard]] constexpr T shift_left(T value, std::uint8_t count) {
    return count >= sizeof(T) * 8 ? T{} : detail::bits<T>(detail::unsigned_bits(value) << count);
}
// This operation is a logical shift of the stored bit pattern, including signed T.
template<Integer T> [[nodiscard]] constexpr T shift_right(T value, std::uint8_t count) {
    return count >= sizeof(T) * 8 ? T{} : detail::bits<T>(detail::unsigned_bits(value) >> count);
}
template<Integer T> [[nodiscard]] constexpr bool equal(T a, T b) { return a == b; }
template<Integer T> [[nodiscard]] constexpr bool less(T a, T b) { return a < b; }

template<Integer T> struct Division {
    T quotient{};
    T remainder{};
    bool divide_by_zero{};
    bool overflow{};
};
template<Integer T> [[nodiscard]] constexpr Division<T> divide(T a, T b) {
    if (b == 0) return {T{}, T{}, true, false};
    const auto left = static_cast<std::int32_t>(a);
    const auto right = static_cast<std::int32_t>(b);
    const bool overflow = std::is_signed_v<T> && left == std::numeric_limits<T>::min()
                          && right == -1;
    return {detail::bits<T>(static_cast<std::uint32_t>(left / right)),
            detail::bits<T>(static_cast<std::uint32_t>(left % right)), false, overflow};
}
} // namespace b8::numeric
