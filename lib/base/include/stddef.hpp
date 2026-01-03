#ifndef EDGE_STDDEF_H
#define EDGE_STDDEF_H

#include <cassert>
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

	template<typename E>
		requires std::is_enum_v<E>
	struct Range {
		struct iterator {
			using underlying_t = std::underlying_type_t<E>;
			underlying_t val;

			E operator*() const { return static_cast<E>(val); }
			iterator& operator++() { ++val; return *this; }
			bool operator!=(iterator other) const { return val != other.val; }
		};

		E first, last;

		iterator begin() const noexcept { 
			return { 
				.val = static_cast<iterator::underlying_t>(first)
			}; 
		}

		iterator end() const noexcept { 
			auto end_val = static_cast<iterator::underlying_t>(last);
			++end_val;
			return { 
				.val = end_val 
			};
		}
	};

	template<typename T>
	struct NotNull {
		static_assert(std::is_pointer_v<T>, "NotNull requires a pointer type");

		using pointer = T;
		using element_type = std::remove_pointer_t<T>;

		constexpr NotNull(T ptr) : m_ptr(ptr) {
			assert(ptr != nullptr);
		}

		constexpr NotNull(const NotNull& other) = default;
		constexpr NotNull(NotNull&& other) noexcept = default;

		constexpr NotNull& operator=(const NotNull& other) = default;
		constexpr NotNull& operator=(NotNull&& other) noexcept = default;

		constexpr NotNull& operator=(T ptr) {
			m_ptr = ptr;
			return *this;
		}

		NotNull(std::nullptr_t) = delete;
		NotNull& operator=(std::nullptr_t) = delete;

		constexpr element_type& operator*() const {
			return *m_ptr;
		}

		constexpr T operator->() const noexcept {
			return m_ptr;
		}

		T m_ptr;
	};

	template<typename V, typename E>
	struct Result {
		Result(V value) noexcept 
			: m_has_value(true), m_value(value) {
		}

		Result(E error) noexcept
			: m_has_value(false), m_error(error) {
		}

		explicit operator bool() const noexcept {
			return m_has_value;
		}

		V* operator->() noexcept {
			assert(m_has_value && "operator->() called on error state");
			return &m_value;
		}

		V& operator*() noexcept {
			assert(m_has_value && "operator*() called on error state");
			return m_value;
		}

		V value() const noexcept {
			assert(m_has_value && "Result::value() called on error state");
			return m_value;
		}

		E error() const noexcept {
			assert(!m_has_value && "Result::error() called on value state");
			return m_error;
		}
	private:
		bool m_has_value = false;
		union {
			V m_value;
			E m_error;
		};
	};

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