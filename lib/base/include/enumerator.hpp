#ifndef EDGE_FLAGS_H
#define EDGE_FLAGS_H

#include "stddef.hpp"

// TODO: Move here enum range based iterator

namespace edge {
template <typename E> struct IsEnumFlags : std::false_type {};

template <typename E>
  requires std::is_enum_v<E>
class Flags {
public:
  using underlying_type = std::underlying_type_t<E>;

  constexpr Flags() noexcept : m_value(0) {}
  explicit constexpr Flags(E bit) noexcept
      : m_value(static_cast<underlying_type>(bit)) {}
  explicit constexpr Flags(underlying_type flags) noexcept : m_value(flags) {}

  constexpr underlying_type value() const noexcept { return m_value; }

  constexpr bool has(E flag) const noexcept {
    return (m_value & static_cast<underlying_type>(flag)) != 0;
  }

  constexpr bool has_all(Flags f) const noexcept {
    return (m_value & f.m_value) == f.m_value;
  }

  constexpr Flags &set(E flag) noexcept {
    m_value |= static_cast<underlying_type>(flag);
    return *this;
  }

  constexpr Flags &clear(E flag) noexcept {
    m_value &= ~static_cast<underlying_type>(flag);
    return *this;
  }

  constexpr Flags &toggle(E flag) noexcept {
    m_value ^= static_cast<underlying_type>(flag);
    return *this;
  }

  constexpr Flags &reset() noexcept {
    m_value = 0;
    return *this;
  }

  constexpr bool any() const noexcept { return m_value != 0; }
  constexpr bool none() const noexcept { return m_value == 0; }

  constexpr Flags operator|(E flag) const noexcept {
    return Flags(m_value | static_cast<underlying_type>(flag));
  }

  constexpr Flags operator|(Flags other) const noexcept {
    return Flags(m_value | other.m_value);
  }

  constexpr Flags operator&(E flag) const noexcept {
    return Flags(m_value & static_cast<underlying_type>(flag));
  }

  constexpr Flags operator&(Flags other) const noexcept {
    return Flags(m_value & other.m_value);
  }

  constexpr Flags operator^(E flag) const noexcept {
    return Flags(m_value ^ static_cast<underlying_type>(flag));
  }

  constexpr Flags operator^(Flags other) const noexcept {
    return Flags(m_value ^ other.m_value);
  }

  constexpr Flags operator~() const noexcept { return Flags(~m_value); }

  constexpr Flags &operator|=(E flag) noexcept {
    m_value |= static_cast<underlying_type>(flag);
    return *this;
  }

  constexpr Flags &operator|=(Flags other) noexcept {
    m_value |= other.m_value;
    return *this;
  }

  constexpr Flags &operator&=(E flag) noexcept {
    m_value &= static_cast<underlying_type>(flag);
    return *this;
  }

  constexpr Flags &operator&=(Flags other) noexcept {
    m_value &= other.m_value;
    return *this;
  }

  constexpr Flags &operator^=(E flag) noexcept {
    m_value ^= static_cast<underlying_type>(flag);
    return *this;
  }

  constexpr Flags &operator^=(Flags other) noexcept {
    m_value ^= other.m_value;
    return *this;
  }

private:
  underlying_type m_value;
};
} // namespace edge

#define EDGE_ENUM_FLAGS(EnumType)                                              \
  namespace edge {                                                             \
  template <> struct IsEnumFlags<EnumType> : std::true_type {};                \
  constexpr Flags<EnumType> operator|(EnumType lhs, EnumType rhs) noexcept {   \
    return ::edge::Flags<EnumType>(lhs) | rhs;                                 \
  }                                                                            \
  constexpr Flags<EnumType> operator&(EnumType lhs, EnumType rhs) noexcept {   \
    return ::edge::Flags<EnumType>(lhs) & rhs;                                 \
  }                                                                            \
  constexpr Flags<EnumType> operator^(EnumType lhs, EnumType rhs) noexcept {   \
    return ::edge::Flags<EnumType>(lhs) ^ rhs;                                 \
  }                                                                            \
  constexpr Flags<EnumType> operator~(EnumType flag) noexcept {                \
    return ~::edge::Flags<EnumType>(flag);                                     \
  }                                                                            \
  }

#endif