#ifndef EDGE_STDDEF_H
#define EDGE_STDDEF_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
using usize = size_t;
using isize = ptrdiff_t;

namespace edge {
	template<typename T>
	concept TrivialType = std::is_trivially_destructible_v<T> || std::is_trivially_copyable_v<T>;

    template<typename T>
    concept Character = 
        std::same_as<std::remove_cv_t<T>, char> ||
        std::same_as<std::remove_cv_t<T>, char8_t> ||
        std::same_as<std::remove_cv_t<T>, wchar_t> ||
        std::same_as<std::remove_cv_t<T>, char16_t> ||
        std::same_as<std::remove_cv_t<T>, char32_t>;

	template<typename T>
	concept Arithmetic = std::is_arithmetic_v<T>;

	template<typename T>
	concept SignedArithmetic = std::is_signed_v<T> && std::is_arithmetic_v<T>;

	template<typename T>
	concept UnsignedArithmetic = std::is_unsigned_v<T> && std::is_arithmetic_v<T>;

	template<typename T>
	concept FloatingPoint = std::is_floating_point_v<T>;

	template<typename Container>
	constexpr auto array_size(const Container& c) -> decltype(c.size()) {
		return c.size();
	}

	template<typename T, usize N>
	constexpr usize array_size(const T(&)[N]) noexcept {
		return N;
	}
}

#endif