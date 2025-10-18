#pragma once

#include <array>
#include <memory>
#include <format>
#include <span>
#include <concepts>
#include <vector>
#include <unordered_map>

#include <string>
#include <fstream>

#include <mimalloc.h>

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
    inline constexpr uint64_t aligned_size(uint64_t size, uint64_t alignment) {
        return (size + alignment - 1ull) & ~(alignment - 1ull);
    }

    namespace mi {
        template<typename T>
        using BasicString = std::basic_string<T, std::char_traits<T>, mi_stl_allocator<T>>;

        using String = BasicString<char>;
        using WString = BasicString<wchar_t>;

        template<typename T>
        using Vector = std::vector<T, mi_stl_allocator<T>>;

        template<typename K, typename T, typename Hasher = std::hash<K>, typename KeyEq = std::equal_to<K>, typename Alloc = mi_stl_allocator<std::pair<const K, T>>>
        using HashMap = std::unordered_map<K, T, Hasher, KeyEq, Alloc>;
    }

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

    template<typename _Ty>
    concept Arithmetic = std::is_arithmetic_v<_Ty>;

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

    class NonCopyable {
    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;

    public:
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };

    class NonMovable {
    protected:
        NonMovable() = default;
        ~NonMovable() = default;

    public:
        NonMovable(NonMovable&&) noexcept = delete;
        NonMovable& operator=(NonMovable&&) noexcept = delete;
    };

    class NonCopyMovable : public NonCopyable, public NonMovable {
    protected:
        NonCopyMovable() = default;
        ~NonCopyMovable() = default;
    };

    template <class _Ty>
    class Singleton : public NonCopyMovable {
    public:
        static inline const std::unique_ptr<_Ty>& get_instance() {
            static std::unique_ptr<_Ty> _instance;
            if (!_instance) {
                _instance = std::make_unique<_Ty>();
            }
            return _instance;
        }
    };

    class BinaryWriter;
    class BinaryReader;

    template<typename T>
    concept Serializable = requires(const T & obj, BinaryWriter & writer) {
        { obj.serialize(writer) } -> std::same_as<void>;
    };

    template<typename T>
    concept Deserializable = requires(T & obj, BinaryReader & reader) {
        { obj.deserialize(reader) } -> std::same_as<void>;
    };

    class BinaryWriter : public NonCopyMovable {
    public:
        explicit BinaryWriter(std::ostream& stream) : stream_(stream) {}

        // Write trivially copyable types
        template<typename T>
            requires std::is_trivially_copyable_v<T> && (!Serializable<T>)
        void write(const T& value) {
            stream_.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }

        // Write custom serializable types
        template<Serializable T>
        void write(const T& value) {
            value.serialize(*this);
        }

        // Write array of trivially copyable types
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void write_array(const T* data, std::size_t count) {
            stream_.write(reinterpret_cast<const char*>(data), count * sizeof(T));
        }

        // Write span
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void write_span(std::span<const T> data) {
            write_array(data.data(), data.size());
        }

        // Write vector of trivially copyable types
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void write_vector(const std::vector<T>& vec) {
            auto len = static_cast<std::uint32_t>(vec.size());
            write(len);
            write_array(vec.data(), vec.size());
        }

        // Write vector of serializable types
        template<Serializable T>
        void write_vector(const std::vector<T>& vec) {
            auto len = static_cast<std::uint32_t>(vec.size());
            write(len);
            for (const auto& item : vec) {
                write(item);
            }
        }

        // Write string (with length prefix)
        void write_string(const std::string& str) {
            auto len = static_cast<std::uint32_t>(str.size());
            write(len);
            stream_.write(str.data(), str.size());
        }

        // Write string without length prefix
        void write_string_raw(const std::string& str) {
            stream_.write(str.data(), str.size());
        }

        // Write null-terminated string
        void write_cstring(const char* str) {
            stream_.write(str, std::strlen(str) + 1);
        }

        // Write bytes
        void write_bytes(const void* data, std::size_t size) {
            stream_.write(static_cast<const char*>(data), size);
        }

        // Endianness conversion helpers (C++23 <bit>)
        template<typename T>
            requires std::integral<T>
        void write_le(T value) {
            if constexpr (std::endian::native == std::endian::little) {
                write(value);
            }
            else {
                T swapped = std::byteswap(value);
                write(swapped);
            }
        }

        template<typename T>
            requires std::integral<T>
        void write_be(T value) {
            if constexpr (std::endian::native == std::endian::big) {
                write(value);
            }
            else {
                T swapped = std::byteswap(value);
                write(swapped);
            }
        }

        // Positioning
        std::streampos tell() const {
            return stream_.tellp();
        }

        void seek(std::streampos pos) {
            stream_.seekp(pos);
        }

        void seek_relative(std::streamoff offset) {
            stream_.seekp(offset, std::ios::cur);
        }

        // Stream state
        bool good() const { return stream_.good(); }
        bool fail() const { return stream_.fail(); }
        explicit operator bool() const { return good(); }

        void flush() {
            stream_.flush();
        }
    private:
        std::ostream& stream_;
    };

    class BinaryReader {
    public:
        explicit BinaryReader(std::istream& stream) : stream_(stream) {}

        // Read trivially copyable types
        template<typename T>
            requires std::is_trivially_copyable_v<T> && (!Deserializable<T>)
        T read() {
            T value;
            stream_.read(reinterpret_cast<char*>(&value), sizeof(T));
            return value;
        }

        // Read into existing trivially copyable variable
        template<typename T>
            requires std::is_trivially_copyable_v<T> && (!Deserializable<T>)
        void read(T& value) {
            stream_.read(reinterpret_cast<char*>(&value), sizeof(T));
        }

        // Read custom deserializable types
        template<Deserializable T>
        T read() {
            T value;
            value.deserialize(*this);
            return value;
        }

        // Read into existing deserializable variable
        template<Deserializable T>
        void read(T& value) {
            value.deserialize(*this);
        }

        // Read array of trivially copyable types
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void read_array(T* data, std::size_t count) {
            stream_.read(reinterpret_cast<char*>(data), count * sizeof(T));
        }

        // Read span
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        void read_span(std::span<T> data) {
            read_array(data.data(), data.size());
        }

        // Read vector of trivially copyable types (with length prefix)
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        std::vector<T> read_vector() {
            auto len = read<std::uint32_t>();
            std::vector<T> vec(len);
            read_array(vec.data(), len);
            return vec;
        }

        // Read vector of deserializable types (with length prefix)
        template<Deserializable T>
        std::vector<T> read_vector() {
            auto len = read<std::uint32_t>();
            std::vector<T> vec;
            vec.reserve(len);
            for (std::uint32_t i = 0; i < len; ++i) {
                vec.push_back(read<T>());
            }
            return vec;
        }

        // Read string (with length prefix)
        std::string read_string() {
            auto len = read<std::uint32_t>();
            std::string str(len, '\0');
            stream_.read(str.data(), len);
            return str;
        }

        // Read fixed-size string without length prefix
        std::string read_string_raw(std::size_t size) {
            std::string str(size, '\0');
            stream_.read(str.data(), size);
            return str;
        }

        // Read null-terminated string
        std::string read_cstring() {
            std::string str;
            char ch;
            while (stream_.read(&ch, 1) && ch != '\0') {
                str += ch;
            }
            return str;
        }

        // Read bytes
        void read_bytes(void* data, std::size_t size) {
            stream_.read(static_cast<char*>(data), size);
        }

        // Endianness conversion helpers
        template<typename T>
            requires std::integral<T>
        T read_le() {
            T value = read<T>();
            if constexpr (std::endian::native != std::endian::little) {
                value = std::byteswap(value);
            }
            return value;
        }

        template<typename T>
            requires std::integral<T>
        T read_be() {
            T value = read<T>();
            if constexpr (std::endian::native != std::endian::big) {
                value = std::byteswap(value);
            }
            return value;
        }

        // Positioning
        std::streampos tell() const {
            return stream_.tellg();
        }

        void seek(std::streampos pos) {
            stream_.seekg(pos);
        }

        void seek_relative(std::streamoff offset) {
            stream_.seekg(offset, std::ios::cur);
        }

        // Stream state
        bool good() const { return stream_.good(); }
        bool fail() const { return stream_.fail(); }
        bool eof() const { return stream_.eof(); }
        explicit operator bool() const { return good(); }

        // Check if we can read N bytes
        bool can_read(std::size_t bytes) {
            auto current = tell();
            stream_.seekg(0, std::ios::end);
            auto end = tell();
            stream_.seekg(current);
            return (end - current) >= static_cast<std::streamoff>(bytes);
        }

    private:
        std::istream& stream_;
    };
}