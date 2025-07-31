#pragma once

#include <array>
#include <optional>

namespace edge::foundation {
	template<typename KeyType, typename ValueType, std::size_t N>
	struct LookupTable {
		struct Entry {
			KeyType key;
			ValueType value;
		};

		std::array<Entry, N> entries;

        constexpr LookupTable(std::array<Entry, N> init) : entries(init) {}

        constexpr auto operator[](KeyType key) const -> std::optional<ValueType> {
            for (const auto& entry : entries) {
                if (entry.key == key) {
                    return entry.value;
                }
            }
            return std::nullopt;
        }

        constexpr auto at(KeyType key) const -> ValueType {
            for (const auto& entry : entries) {
                if (entry.key == key) {
                    return entry.value;
                }
            }

            throw std::out_of_range("Key not found");
        }
	};

    template<typename KeyType, typename ValueType, std::size_t N>
    constexpr auto make_lut(std::array<typename LookupTable<KeyType, ValueType, N>::Entry, N> entries) {
        return LookupTable<KeyType, ValueType, N>(entries);
    }
}