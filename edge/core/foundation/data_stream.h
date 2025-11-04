#pragma once

#include <ostream>
#include <istream>
#include <concepts>
#include <type_traits>
#include <vector>
#include <span>
#include <string_view>

#include <streambuf>

namespace edge {
	class BinaryWriter;
	class BinaryReader;

    template<typename T>
    concept Serializable = requires(const T & obj, BinaryWriter& writer) {
        { obj.serialize(writer) } -> std::same_as<void>;
    };

    template<typename T>
    concept Deserializable = requires(T & obj, BinaryReader & reader) {
        { obj.deserialize(reader) } -> std::same_as<void>;
    };

    class BinaryWriter {
    public:
        explicit BinaryWriter(std::ostream& stream) : stream_(stream) {}

        BinaryWriter(const BinaryWriter&) = delete;
        BinaryWriter& operator=(const BinaryWriter&) = delete;
        BinaryWriter(BinaryWriter&&) noexcept = delete;
        BinaryWriter& operator=(BinaryWriter&&) noexcept = delete;

        template<typename T> requires std::is_trivially_copyable_v<T> && (!Serializable<T>)
        auto write(const T& value) -> void {
            stream_.write(reinterpret_cast<const char*>(&value), sizeof(T));
        }

        template<Serializable T>
        auto write(const T& value) -> void {
            value.serialize(*this);
        }

        template<typename T> requires std::is_trivially_copyable_v<T>
        auto write(const T* data, size_t count) -> void {
            stream_.write(reinterpret_cast<const char*>(data), count * sizeof(T));
        }

        // Span
        template<typename T> requires std::is_trivially_copyable_v<T>
        auto write(std::span<const T> data) -> void {
            write(static_cast<uint32_t>(data.size()));
            write(data.data(), data.size());
        }

        template<Serializable T>
        auto write(std::span<const T> data) -> void {
            write(static_cast<uint32_t>(data.size()));
            for (const auto& item : data) {
                write(item);
            }
        }

        // Vector
        template<typename T, typename Allocator = std::allocator<T>> requires std::is_trivially_copyable_v<T>
        auto write(const std::vector<T, Allocator>& data) -> void {
            write(static_cast<uint32_t>(data.size()));
            write(data.data(), data.size());
        }

        template<Serializable T, typename Allocator = std::allocator<T>>
        auto write(const std::vector<T, Allocator>& data) -> void {
            write(static_cast<uint32_t>(data.size()));
            for (const auto& item : data) {
                write(item);
            }
        }

        // String view
        template<typename T, typename CharTraits = std::char_traits<T>>
        auto write(std::basic_string_view<T, CharTraits> str) -> void {
            write(static_cast<uint32_t>(str.size()));
            stream_.write(str.data(), str.size());
        }

        // String
        template<typename T, typename CharTraits = std::char_traits<T>, typename Allocator = std::allocator<T>>
        auto write(const std::basic_string<T, CharTraits, Allocator>& str) -> void {
            write(static_cast<uint32_t>(str.size()));
            stream_.write(str.data(), str.size());
        }

        auto write(const char* str) -> void {
            stream_.write(str, std::strlen(str) + 1);
        }

        auto write(const void* data, size_t size) -> void {
            stream_.write(static_cast<const char*>(data), size);
        }

        auto tell() const -> std::streampos {
            return stream_.tellp();
        }

        auto seek(std::streampos pos) -> void {
            stream_.seekp(pos);
        }

        auto seek_relative(std::streamoff offset) -> void {
            stream_.seekp(offset, std::ios::cur);
        }

        auto good() const -> bool { return stream_.good(); }
        auto fail() const -> bool { return stream_.fail(); }
        explicit operator bool() const { return good(); }

        auto flush() -> void { stream_.flush(); }
    private:
        std::ostream& stream_;
    };

    class BinaryReader {
    public:
        explicit BinaryReader(std::istream& stream) : stream_(stream) {}

        BinaryReader(const BinaryReader&) = delete;
        BinaryReader& operator=(const BinaryReader&) = delete;
        BinaryReader(BinaryReader&&) noexcept = delete;
        BinaryReader& operator=(BinaryReader&&) noexcept = delete;

        template<typename T> requires std::is_trivially_copyable_v<T> && (!Deserializable<T>)
        auto read() -> T {
            T value;
            stream_.read(reinterpret_cast<char*>(&value), sizeof(T));
            return value;
        }

        template<typename T> requires std::is_trivially_copyable_v<T> && (!Deserializable<T>)
        auto read(T& value) -> void {
            stream_.read(reinterpret_cast<char*>(&value), sizeof(T));
        }

        template<Deserializable T>
        auto read() -> T {
            T value;
            value.deserialize(*this);
            return value;
        }

        template<Deserializable T>
        auto read(T& value) -> void {
            value.deserialize(*this);
        }

        template<typename T> requires std::is_trivially_copyable_v<T>
        auto read(T* data, size_t count) -> void {
            stream_.read(reinterpret_cast<char*>(data), count * sizeof(T));
        }

        // Span
        template<typename T> requires std::is_trivially_copyable_v<T>
        auto read(std::span<T> data) -> void {
            read_array(data.data(), data.size());
        }

        // Vector
        template<typename T, typename Allocator = std::allocator<T>> requires std::is_trivially_copyable_v<T>
        auto read(std::vector<T, Allocator>& vec) -> void {
            vec.resize(read<uint32_t>());
            read(vec.data(), vec.size());
        }

        template<Deserializable T, typename Allocator = std::allocator<T>>
        auto read(std::vector<T, Allocator>& vec) -> void {
            auto len = read<uint32_t>();
            vec.reserve(len);
            for (uint32_t i = 0; i < len; ++i) {
                vec.push_back(read<T>());
            }
        }

        // String
        template<typename T, typename CharTraits = std::char_traits<T>, typename Allocator = std::allocator<T>>
        auto read(std::basic_string<T, CharTraits, Allocator>& str) -> void {
            str.resize(read<uint32_t>(), '\0');
            stream_.read(str.data(), str.size());
        }

        auto tell() const -> std::streampos {
            return stream_.tellg();
        }

        auto seek(std::streampos pos) -> void {
            stream_.seekg(pos);
        }

        auto seek_relative(std::streamoff offset) -> void {
            stream_.seekg(offset, std::ios::cur);
        }

        auto good() const -> bool { return stream_.good(); }
        auto fail() const -> bool { return stream_.fail(); }
        auto eof() const -> bool { return stream_.eof(); }
        explicit operator bool() const { return good(); }

        auto can_read(size_t bytes) -> bool {
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