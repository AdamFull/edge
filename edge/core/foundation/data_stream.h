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

    template<typename ByteType = uint8_t, typename Allocator = std::allocator<ByteType>>
    class MemoryStreamBuf : public std::streambuf {
    public:
        explicit MemoryStreamBuf(std::vector<ByteType, Allocator>& buffer)
            : buffer_(buffer), read_pos_(0) {
            update_put_area();
        }

        auto tell_read() const -> std::streampos { return read_pos_; }
        auto tell_write() const -> std::streampos { return pptr() - pbase(); }
        auto data() const -> std::span<const uint8_t> { return std::span<const uint8_t>(buffer_.data(), buffer_.size()); }
        auto get_buffer() -> std::vector<ByteType, Allocator>& { return buffer_; }
    protected:
        int_type underflow() override {
            if (read_pos_ >= buffer_.size()) {
                return traits_type::eof();
            }

            return traits_type::to_int_type(buffer_[read_pos_]);
        }

        int_type uflow() override {
            if (read_pos_ >= buffer_.size()) {
                return traits_type::eof();
            }

            return traits_type::to_int_type(buffer_[read_pos_++]);
        }

        std::streamsize xsgetn(char* s, std::streamsize count) override {
            std::streamsize available = buffer_.size() - read_pos_;
            std::streamsize to_read = std::min(count, available);
            if (to_read > 0) {
                std::memcpy(s, buffer_.data() + read_pos_, to_read);
                read_pos_ += to_read;
            }
            return to_read;
        }

        int_type overflow(int_type ch) override {
            if (ch != traits_type::eof()) {
                size_t write_pos = pptr() - pbase();
                if (write_pos >= buffer_.size()) {
                    buffer_.resize(write_pos + 1);
                    update_put_area();
                }
                buffer_[write_pos] = static_cast<uint8_t>(ch);
                pbump(1);
                return ch;
            }
            return traits_type::eof();
        }

        std::streamsize xsputn(const char* s, std::streamsize count) override {
            if (count <= 0) {
                return 0;
            }

            size_t write_pos = pptr() - pbase();
            size_t new_size = write_pos + count;
            if (new_size > buffer_.size()) {
                buffer_.resize(new_size);
                update_put_area();
            }
            std::memcpy(buffer_.data() + write_pos, s, count);
            pbump(static_cast<int>(count));
            return count;
        }

        std::streampos seekoff(std::streamoff off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
            std::streampos new_pos;

            if (which & std::ios_base::in) {
                switch (dir) {
                case std::ios_base::beg: new_pos = off; break;
                case std::ios_base::cur: new_pos = read_pos_ + off; break;
                case std::ios_base::end: new_pos = buffer_.size() + off; break;
                default: return std::streampos(std::streamoff(-1));
                }
                if (new_pos < 0 || new_pos > static_cast<std::streampos>(buffer_.size())) {
                    return std::streampos(std::streamoff(-1));
                }
                read_pos_ = new_pos;
            }

            if (which & std::ios_base::out) {
                switch (dir) {
                case std::ios_base::beg: new_pos = off; break;
                case std::ios_base::cur: new_pos = (pptr() - pbase()) + off; break;
                case std::ios_base::end: new_pos = buffer_.size() + off; break;
                default: return std::streampos(std::streamoff(-1));
                }
                if (new_pos < 0) {
                    return std::streampos(std::streamoff(-1));
                }

                size_t pos = static_cast<size_t>(new_pos);
                if (pos > buffer_.size()) {
                    buffer_.resize(pos);
                }
                update_put_area();
                pbump(static_cast<int>(pos));
            }
            return new_pos;
        }

        std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override {
            return seekoff(std::streamoff(pos), std::ios_base::beg, which);
        }
    private:
        auto update_put_area() -> void {
            if (buffer_.empty()) {
                buffer_.resize(256);
            }

            char* base = reinterpret_cast<char*>(buffer_.data());
            setp(base, base + buffer_.size());
        }

        std::vector<ByteType, Allocator>& buffer_;
        size_t read_pos_;
    };

    template<typename ByteType = uint8_t, typename Allocator = std::allocator<ByteType>>
    class MemoryStream : public std::iostream {
    public:
        MemoryStream() 
            : std::iostream(nullptr)
            , buffer_()
            , streambuf_(buffer_) { 
            rdbuf(&streambuf_); 
        }

        explicit MemoryStream(size_t capacity) 
            : std::iostream(nullptr)
            , buffer_()
            , streambuf_(buffer_) {
            buffer_.reserve(capacity);
            rdbuf(&streambuf_);
        }

        explicit MemoryStream(std::vector<ByteType, Allocator>&& buffer)
            : std::iostream(nullptr)
            , buffer_(std::move(buffer))
            , streambuf_(buffer_) { 
            rdbuf(&streambuf_); 
        }

        explicit MemoryStream(std::span<const ByteType> data)
            : std::iostream(nullptr)
            , buffer_(data.begin(), data.end())
            , streambuf_(buffer_) { 
            rdbuf(&streambuf_); 
        }

        auto get_buffer() -> std::vector<ByteType, Allocator>& { return buffer_; }
        auto get_buffer() const -> std::vector<ByteType, Allocator> const& { return buffer_; }
        auto data() const -> std::span<const ByteType> { return streambuf_.data(); }
        auto length() const -> size_t { return buffer_.size(); }
        auto position() const -> std::streampos { return const_cast<MemoryStream*>(this)->tellg(); }
        auto position(std::streampos pos) -> void { seekg(pos); seekp(pos); }
        auto capacity() const -> size_t { return buffer_.capacity(); }
        auto set_capacity(size_t capacity) -> void { buffer_.reserve(capacity); }

        void write_bytes(std::span<const ByteType> bytes) {
            write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        auto read_bytes(size_t count) -> std::vector<ByteType, Allocator> {
            std::vector<ByteType, Allocator> result(count);
            auto actual = read(reinterpret_cast<char*>(result.data()), count).gcount();
            result.resize(actual);
            return result;
        }

    private:
        std::vector<ByteType, Allocator> buffer_;
        MemoryStreamBuf<ByteType, Allocator> streambuf_;
    };
}