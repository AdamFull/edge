#pragma once

#include <array>
#include <memory>
#include <format>
#include <span>

#include <spdlog/spdlog.h>

#ifndef EDGE_LOGGER_PATTERN
#define EDGE_LOGGER_PATTERN "[%Y-%m-%d %H:%M:%S] [%^%l%$] %v"
#endif

#define EDGE_LOGI(...) spdlog::info(__VA_ARGS__)
#define EDGE_SLOGI(...) spdlog::info("[{}]: {}", EDGE_LOGGER_SCOPE, std::format(__VA_ARGS__))

#define EDGE_LOGW(...) spdlog::warn(__VA_ARGS__)
#define EDGE_SLOGW(...) spdlog::warn("[{}]: {}", EDGE_LOGGER_SCOPE, std::format(__VA_ARGS__))

#define EDGE_LOGE(...) spdlog::error(__VA_ARGS__)
#define EDGE_SLOGE(...) spdlog::error("[{}]: {}", EDGE_LOGGER_SCOPE, std::format(__VA_ARGS__))

#ifdef NDEBUG
#define EDGE_LOGD(...)
#define EDGE_SLOGD(...)
#define EDGE_LOGT(...)
#define EDGE_SLOGT(...)
#else
#define EDGE_LOGD(...) spdlog::debug(__VA_ARGS__)
#define EDGE_SLOGD(...) spdlog::debug("[{}]: {}", EDGE_LOGGER_SCOPE, std::format(__VA_ARGS__))
#define EDGE_LOGT(...) spdlog::trace(__VA_ARGS__)
#define EDGE_SLOGT(...) spdlog::trace("[{}]: {}", EDGE_LOGGER_SCOPE, std::format(__VA_ARGS__))
#endif

namespace edge {
	template<typename T, size_t Size>
	using Array = std::array<T, Size>;

	template<typename T, size_t Size = std::dynamic_extent>
	using Span = std::span<T, Size>;

	template<typename T>
	using Shared = std::shared_ptr<T>;

	template<typename T>
	using Weak = std::weak_ptr<T>;

	template<typename T>
	using Owned = std::unique_ptr<T>;

	template<typename T, size_t Capacity = 16>
	class FixedVector {
	public:
		using value_type = T;
		using size_type = size_t;
		using reference = T&;
		using const_reference = const T&;
		using iterator = T*;
		using const_iterator = const T*;

        FixedVector() noexcept : size_(0) {}

        FixedVector(std::initializer_list<T> init) : size_(0) {
            if (init.size() > Capacity) throw std::length_error("FixedVector: too many initializers");
            for (const auto& v : init) emplace_back(v);
        }

        ~FixedVector() { clear(); }

        FixedVector(const FixedVector& other) : size_(0) {
            for (size_type i = 0; i < other.size_; ++i) {
                emplace_back(other[i]);
            }
        }

        auto operator=(const FixedVector& other) -> FixedVector& {
            if (this != &other) {
                clear();
                for (size_type i = 0; i < other.size_; ++i) {
                    emplace_back(other[i]);
                }
            }
            return *this;
        }

        FixedVector(FixedVector&& other) noexcept : size_(0) {
            for (size_type i = 0; i < other.size_; ++i) {
                emplace_back(std::move(other[i]));
            }
            other.clear();
        }

        auto operator=(FixedVector&& other) noexcept -> FixedVector& {
            if (this != &other) {
                clear();
                for (size_type i = 0; i < other.size_; ++i) {
                    emplace_back(std::move(other[i]));
                }
                other.clear();
            }
            return *this;
        }
	
        // Capacity
        constexpr auto capacity() const noexcept -> size_type { return Capacity; }
        constexpr auto size() const noexcept -> size_type { return size_; }
        constexpr auto empty() const noexcept -> bool { return size_ == 0; }
        constexpr auto full() const noexcept -> bool { return size_ == Capacity; }

        // Access
        auto operator[](size_type i) noexcept -> reference { return *std::launder(reinterpret_cast<T*>(&data_[i])); }
        auto operator[](size_type i) const noexcept -> const_reference { return *std::launder(reinterpret_cast<const T*>(&data_[i])); }

        auto at(size_type i) -> reference {
            if (i >= size_) throw std::out_of_range("FixedVector: out of range");
            return (*this)[i];
        }

        auto at(size_type i) const -> const_reference {
            if (i >= size_) throw std::out_of_range("FixedVector: out of range");
            return (*this)[i];
        }

        auto front() noexcept -> reference { return (*this)[0]; }
        auto front() const noexcept -> const_reference { return (*this)[0]; }
        auto back() noexcept -> reference { return (*this)[size_ - 1]; }
        auto back() const noexcept -> const_reference { return (*this)[size_ - 1]; }

        // Iterators
        auto begin() noexcept -> iterator { return &(*this)[0]; }
        auto begin() const noexcept -> const_iterator { return &(*this)[0]; }
        auto end() noexcept -> iterator { return &(*this)[size_]; }
        auto end() const noexcept -> const_iterator { return &(*this)[size_]; }

        auto data() noexcept -> T* { return reinterpret_cast<T*>(data_.data()); }

        template<typename... _Args>
        auto emplace(const_iterator pos, _Args&&... args) -> iterator {
            size_type index = pos - begin();
            if (size_ >= Capacity)
                throw std::length_error("FixedVector: capacity exceeded");
            if (index > size_)
                throw std::out_of_range("FixedVector: insert position out of range");

            if (size_ > 0) {
                ::new (&data_[size_]) T(std::move((*this)[size_ - 1]));
                for (size_type i = size_ - 1; i > index; --i) {
                    (*this)[i] = std::move((*this)[i - 1]);
                }
                (*this)[index].~T();
            }

            ::new (&data_[index]) T(std::forward<_Args>(args)...);
            ++size_;
            return begin() + index;
        }

        auto insert(const_iterator pos, const T& value) -> iterator {
            return emplace(pos, value);
        }

        auto insert(const_iterator pos, T&& value) -> iterator {
            return emplace(pos, std::move(value));
        }

        // Modifiers
        auto push_back(const T& value) -> void {
            emplace_back(value);
        }

        auto push_back(T&& value) -> void {
            emplace_back(std::move(value));
        }

        template <typename... Args>
        auto emplace_back(Args&&... args) -> reference {
            if (size_ >= Capacity)
                throw std::length_error("FixedVector: capacity exceeded");
            ::new (&data_[size_]) T(std::forward<Args>(args)...);
            ++size_;
            return back();
        }

        auto pop_back() -> void {
            if (size_ == 0)
                throw std::out_of_range("FixedVector: pop_back on empty");
            --size_;
            (*this)[size_].~T();
        }

        auto clear() noexcept -> void {
            while (size_ > 0) {
                pop_back();
            }
        }

        constexpr auto as_span() noexcept -> Span<T> {
            return Span<T>(begin(), size());
        }

        constexpr auto as_span() const noexcept -> Span<const T> {
            return Span<const T>(begin(), size());
        }

        constexpr operator Span<T>() noexcept { return as_span(); }
        constexpr operator Span<const T>() const noexcept { return as_span(); }

    private:
        using storage_type = std::aligned_storage_t<sizeof(T), alignof(T)>;
        std::array<storage_type, Capacity> data_;
        size_type size_;

	};
}