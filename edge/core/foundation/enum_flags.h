#pragma once

#include "foundation.h"

#include <type_traits>
#include <algorithm>

namespace edge::foundation {
	template<typename T>
    concept EnumFlag = std::is_enum_v<T> && requires {
        // Ensure the underlying type can be used for bitwise operations
        requires std::is_unsigned_v<std::underlying_type_t<T>> || std::is_signed_v<std::underlying_type_t<T>>;
    };

    // Flag name entry for string conversion
    template<typename E>
    struct flag_name_entry {
        E value;
        std::string_view name;

        constexpr flag_name_entry(E v, std::string_view n) : value(v), name(n) {}
    };

    // Traits for flag names - specialize this for your enum types
    template<EnumFlag E>
    struct flag_traits {
        // Default implementation returns empty span
        static constexpr std::span<const flag_name_entry<E>> names() {
            return {};
        }
    };

    // Helper to check if flag_traits is specialized for a type
    template<typename E>
    concept HasFlagTraits = requires {
        { flag_traits<E>::names() } -> std::convertible_to<std::span<const flag_name_entry<E>>>;
        requires !flag_traits<E>::names().empty();
    };

    template<EnumFlag E>
    class Flags {
    public:
        using enum_type = E;
        using underlying_type = std::underlying_type_t<E>;

        constexpr Flags() noexcept : value_(0) {}
        constexpr Flags(E flag) noexcept : value_(static_cast<underlying_type>(flag)) {}
        constexpr Flags(underlying_type value) noexcept : value_(value) {}

        constexpr Flags(const Flags&) = default;
        constexpr Flags(Flags&&) = default;
        constexpr Flags& operator=(const Flags&) = default;
        constexpr Flags& operator=(Flags&&) = default;

        constexpr operator underlying_type() const noexcept {
            return value_;
        }

        // Assignment from enum
        constexpr auto operator=(E flag) noexcept -> Flags& {
            value_ = static_cast<underlying_type>(flag);
            return *this;
        }

        // Bitwise operations
        constexpr auto operator|(const Flags& other) const noexcept -> Flags {
            return Flags(value_ | other.value_);
        }

        constexpr auto operator|(E flag) const noexcept -> Flags {
            return Flags(value_ | static_cast<underlying_type>(flag));
        }

        constexpr auto operator&(const Flags& other) const noexcept -> Flags {
            return Flags(value_ & other.value_);
        }

        constexpr auto operator&(E flag) const noexcept -> Flags {
            return Flags(value_ & static_cast<underlying_type>(flag));
        }

        constexpr auto operator^(const Flags& other) const noexcept -> Flags {
            return Flags(value_ ^ other.value_);
        }

        constexpr auto operator^(E flag) const noexcept -> Flags {
            return Flags(value_ ^ static_cast<underlying_type>(flag));
        }

        constexpr auto operator~() const noexcept -> Flags {
            return Flags(~value_);
        }

        // Compound assignment operators
        constexpr auto operator|=(const Flags& other) noexcept -> Flags& {
            value_ |= other.value_;
            return *this;
        }

        constexpr auto operator|=(E flag) noexcept -> Flags& {
            value_ |= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr auto operator&=(const Flags& other) noexcept -> Flags& {
            value_ &= other.value_;
            return *this;
        }

        constexpr auto operator&=(E flag) noexcept -> Flags& {
            value_ &= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr auto operator^=(const Flags& other) noexcept -> Flags& {
            value_ ^= other.value_;
            return *this;
        }

        constexpr auto operator^=(E flag) noexcept -> Flags& {
            value_ ^= static_cast<underlying_type>(flag);
            return *this;
        }

        // Comparison operators
        constexpr auto operator==(const Flags& other) const noexcept -> bool = default;
        constexpr auto operator!=(const Flags& other) const noexcept -> bool = default;

        constexpr auto operator==(E flag) const noexcept -> bool {
            return value_ == static_cast<underlying_type>(flag);
        }

        constexpr auto operator!=(E flag) const noexcept -> bool {
            return value_ != static_cast<underlying_type>(flag);
        }

        // Boolean conversion (true if any flags are set)
        constexpr explicit operator bool() const noexcept {
            return value_ != 0;
        }

        // Test if specific flag(s) are set
        constexpr auto test(E flag) const noexcept -> bool {
            const auto flag_value = static_cast<underlying_type>(flag);
            return (value_ & flag_value) == flag_value;
        }

        constexpr auto test(const Flags& flags) const noexcept -> bool {
            return (value_ & flags.value_) == flags.value_;
        }

        // Test if any of the specified flags are set
        constexpr auto test_any(const Flags& flags) const noexcept -> bool {
            return (value_ & flags.value_) != 0;
        }

        constexpr auto test_any(E flag) const noexcept -> bool {
            return (value_ & static_cast<underlying_type>(flag)) != 0;
        }

        // Set flag(s)
        constexpr auto set(E flag) noexcept -> Flags& {
            value_ |= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr auto set(const Flags& flags) noexcept -> Flags& {
            value_ |= flags.value_;
            return *this;
        }

        // Clear flag(s)
        constexpr auto clear(E flag) noexcept -> Flags& {
            value_ &= ~static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr auto clear(const Flags& flags) noexcept -> Flags& {
            value_ &= ~flags.value_;
            return *this;
        }

        // Toggle flag(s)
        constexpr auto toggle(E flag) noexcept -> Flags& {
            value_ ^= static_cast<underlying_type>(flag);
            return *this;
        }

        constexpr auto toggle(const Flags& flags) noexcept -> Flags& {
            value_ ^= flags.value_;
            return *this;
        }

        // Reset all flags
        constexpr auto reset() noexcept -> Flags& {
            value_ = 0;
            return *this;
        }

        // Get underlying value
        constexpr auto value() const noexcept -> underlying_type {
            return value_;
        }

        // Check if no flags are set
        constexpr auto empty() const noexcept -> bool {
            return value_ == 0;
        }

        // Convert flags to string representation
        auto to_string() const -> mi::String {
            if constexpr (HasFlagTraits<E>) {
                if (value_ == 0) {
                    return "None";
                }

                mi::String result;
                auto names = flag_traits<E>::names();
                bool first = true;

                // Check each defined flag
                for (const auto& entry : names) {
                    const auto flag_value = static_cast<underlying_type>(entry.value);
                    if (flag_value != 0 && (value_ & flag_value) == flag_value) {
                        if (!first) {
                            result += " | ";
                        }
                        result += entry.name;
                        first = false;
                    }
                }

                // If we couldn't represent all bits with named flags, show the numeric value too
                underlying_type represented_bits = 0;
                for (const auto& entry : names) {
                    const auto flag_value = static_cast<underlying_type>(entry.value);
                    if (flag_value != 0 && (value_ & flag_value) == flag_value) {
                        represented_bits |= flag_value;
                    }
                }

                if (represented_bits != value_) {
                    if (!first) {
                        result += " | ";
                    }
                    result += std::format("0x{:x}", value_ & ~represented_bits);
                }

                return result.empty() ? mi::String{ std::format("0x{:x}", value_) } : result;
            }
            else {
                // No traits defined, just show numeric value
                return std::format("0x{:x}", value_);
            }
        }
    private:
        underlying_type value_;
    };

    template<EnumFlag E>
    constexpr inline auto to_string(E value) -> std::string_view {
        for (const auto& [v, n] : flag_traits<E>::names()) {
            if (v == value) {
                return n;
            }
        }

        return "";
    }

    template<EnumFlag E>
    constexpr inline auto from_string(std::string_view name) -> E {
        for (const auto& [v, n] : flag_traits<E>::names()) {
            if (name.compare(n) == 0) {
                return v;
            }
        }

        return static_cast<E>(0);
    }
}

#define EDGE_MAKE_ENUM_FLAGS(FlagsType, EnumType) \
    static_assert(edge::foundation::EnumFlag<EnumType>, "EnumType must satisfy EnumFlag concept"); \
    \
    /* Free function operators in the same namespace as the enum for ADL */ \
    constexpr edge::foundation::Flags<EnumType> operator|(EnumType lhs, EnumType rhs) noexcept { \
        return edge::foundation::Flags<EnumType>(lhs) | rhs; \
    } \
    \
    constexpr edge::foundation::Flags<EnumType> operator|(EnumType lhs, edge::foundation::Flags<EnumType> rhs) noexcept { \
        return edge::foundation::Flags<EnumType>(lhs) | rhs; \
    } \
    \
    constexpr edge::foundation::Flags<EnumType> operator&(EnumType lhs, EnumType rhs) noexcept { \
        return edge::foundation::Flags<EnumType>(lhs) & rhs; \
    } \
    \
    constexpr edge::foundation::Flags<EnumType> operator&(EnumType lhs, edge::foundation::Flags<EnumType> rhs) noexcept { \
        return edge::foundation::Flags<EnumType>(lhs) & rhs; \
    } \
    \
    constexpr edge::foundation::Flags<EnumType> operator^(EnumType lhs, EnumType rhs) noexcept { \
        return edge::foundation::Flags<EnumType>(lhs) ^ rhs; \
    } \
    \
    constexpr edge::foundation::Flags<EnumType> operator^(EnumType lhs, edge::foundation::Flags<EnumType> rhs) noexcept { \
        return edge::foundation::Flags<EnumType>(lhs) ^ rhs; \
    } \
    \
    constexpr edge::foundation::Flags<EnumType> operator~(EnumType flag) noexcept { \
        return ~edge::foundation::Flags<EnumType>(flag); \
    }


// Macro to define flag names for string conversion
#define EDGE_DEFINE_FLAG_NAMES(EnumType, ...) \
namespace edge::foundation { \
    template<> \
    struct flag_traits<EnumType> { \
        static constexpr std::array flag_entries = { __VA_ARGS__ }; \
        static constexpr std::span<const flag_name_entry<EnumType>> names() { \
            return flag_entries; \
        } \
    }; \
}

#define EDGE_FLAG_ENTRY(Type, Name) flag_name_entry{Type, Name}

// std::format support
/*template<edge::foundation::EnumFlag E>
struct std::formatter<edge::foundation::Flags<E>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const edge::foundation::Flags<E>& flags, format_context& ctx) const {
        return format_to(ctx.out(), "{}", flags.to_string());
    }
};*/