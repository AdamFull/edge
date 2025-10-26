#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <format>
#include <span>
#include <concepts>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>

#include <ranges>

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

#undef min
#undef max

namespace edge {
    inline constexpr uint64_t aligned_size(uint64_t size, uint64_t alignment) {
        return (size + alignment - 1ull) & ~(alignment - 1ull);
    }

    namespace mi {
        template<typename T>
        using BasicString = std::basic_string<T, std::char_traits<T>, mi_stl_allocator<T>>;

        using String = BasicString<char>;
        using WString = BasicString<wchar_t>;
        using U8String = BasicString<char8_t>;
        using U16String = BasicString<char16_t>;
        using U32String = BasicString<char32_t>;

        template<typename T>
        using Vector = std::vector<T, mi_stl_allocator<T>>;

        template<typename K, typename T, typename Hasher = std::hash<K>, typename KeyEq = std::equal_to<K>, typename Alloc = mi_stl_allocator<std::pair<const K, T>>>
        using HashMap = std::unordered_map<K, T, Hasher, KeyEq, Alloc>;

        template<typename K, typename Hasher = std::hash<K>, typename KeyEq = std::equal_to<K>, typename Alloc = mi_stl_allocator<K>>
        using HashSet = std::unordered_set<K, Hasher, KeyEq, Alloc>;

        template<std::integral T = uint32_t>
        class FreeList {
        public:
            using value_type = T;

            explicit FreeList(T max_id = std::numeric_limits<T>::max())
                : max_id_(max_id) {
            }

            [[nodiscard]] auto allocate() -> T {
                if (!free_ids_.empty()) {
                    T id = free_ids_.back();
                    free_ids_.pop_back();
                    return id;
                }

                if (next_id_ >= max_id_) {
                    throw std::overflow_error("FreeList exhausted: no more IDs available");
                }

                return next_id_++;
            }

            auto deallocate(T id) -> void {
                if (id >= next_id_) {
                    throw std::invalid_argument(std::format("Cannot deallocate ID {}: never allocated", id));
                }
                free_ids_.push_back(id);
            }

            [[nodiscard]] auto allocated_count() const noexcept -> size_t { return static_cast<std::size_t>(next_id_) - free_ids_.size(); }
            [[nodiscard]] auto free_count() const noexcept -> size_t { return free_ids_.size(); }
            [[nodiscard]] auto total_issued() const noexcept -> T { return next_id_; }
            [[nodiscard]] auto empty() const noexcept -> bool { return allocated_count() == 0; }

            auto clear() noexcept -> void {
                free_ids_.clear();
                next_id_ = 0;
            }

            auto reserve(std::size_t capacity) -> void { free_ids_.reserve(capacity); }

        private:
            T next_id_{ 0 };
            T max_id_{ std::numeric_limits<T>::max() };
            Vector<T> free_ids_{};
        };
    }

    namespace unicode {
        static inline constexpr auto is_continuation_byte(char8_t c) noexcept -> bool {
            return (static_cast<char8_t>(c) & 0xC0) == 0x80;
        }

        static inline constexpr auto char_byte_count(char8_t fb) noexcept -> size_t {
            auto uc = static_cast<unsigned char>(fb);
            if ((uc & 0x80) == 0) return 1;
            if ((uc & 0xE0) == 0xC0) return 2;
            if ((uc & 0xF0) == 0xE0) return 3;
            if ((uc & 0xF8) == 0xF0) return 4;
            return 0;
        }

        template<typename T>
        static inline auto validate_utf8(std::basic_string_view<T, std::char_traits<T>> sv) noexcept -> bool {
            size_t i = 0;
            while (i < sv.size()) {
                auto len = char_byte_count(sv[i]);
                if (len == 0 || i + len > sv.size()) return false;

                for (size_t j = 1; j < len; ++j) {
                    if (!is_continuation_byte(sv[i + j])) return false;
                }
                i += len;
            }
            return true;
        }

        inline constexpr auto is_surrogate(char32_t cp) -> bool {
            return cp >= 0xD800u && cp <= 0xDFFFu;
        }

        inline constexpr auto is_high_surrogate(char16_t cp) -> bool {
            return cp >= 0xD800u && cp <= 0xDBFFu;
        }

        inline constexpr auto is_high_surrogate_invalid(char16_t cp) -> bool {
            return cp < 0xD800u || cp > 0xDBFFu;
        }

        inline constexpr auto is_low_surrogate(char16_t cp) -> bool {
            return cp >= 0xDC00u && cp <= 0xDFFFu;
        }

        inline constexpr auto is_low_surrogate_invalid(char16_t cp) -> bool {
            return cp < 0xDC00u || cp > 0xDFFF;
        }

        // UTF-32 codepoint encoding to UTF-8
        inline constexpr auto encode_utf8(char32_t cp, mi::U8String& out) -> bool {
            if (is_surrogate(cp)) {
                return false;
            }

            if (cp <= 0x7F) {
                // 1-byte sequence: 0xxxxxxx
                out += static_cast<char8_t>(cp);
            }
            else if (cp <= 0x7FF) {
                // 2-byte sequence: 110xxxxx 10xxxxxx
                out += static_cast<char8_t>(0xC0 | (cp >> 6));
                out += static_cast<char8_t>(0x80 | (cp & 0x3F));
            }
            else if (cp <= 0xFFFF) {
                // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
                out += static_cast<char8_t>(0xE0 | (cp >> 12));
                out += static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char8_t>(0x80 | (cp & 0x3F));
            }
            else if (cp <= 0x10FFFF) {
                // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                out += static_cast<char8_t>(0xF0 | (cp >> 18));
                out += static_cast<char8_t>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char8_t>(0x80 | (cp & 0x3F));
            }
            else {
                return false;
            }

            return true;
        }

        // UTF-16 codepoint encoding to UTF-8
        inline constexpr auto encode_utf8(char16_t cp, mi::U8String& out) -> bool {
            if (is_high_surrogate(cp) || is_low_surrogate(cp)) {
                return false;
            }

            return encode_utf8(static_cast<char32_t>(cp), out);
        }

        // UTF-16 surrogate encoding to UTF-8
        inline constexpr auto encode_utf8(char16_t cp_high, char16_t cp_low, mi::U8String& out) -> bool {
            if (is_high_surrogate_invalid(cp_high) || is_low_surrogate_invalid(cp_low)) {
                return false;
            }

            auto codepoint = static_cast<char32_t>(0x10000u + ((cp_high - 0xD800u) << 10) + (cp_low - 0xDC00u));
            return encode_utf8(codepoint, out);
        }

        inline constexpr auto encode_utf8(std::u16string_view str, mi::U8String& out) -> bool {
            for (int32_t i = 0; i < static_cast<int32_t>(str.size()); ++i) {
                auto const& c = str[i];
                if (is_high_surrogate(c)) {
                    // Check for incomplete surrogate pair
                    if (i + 1 >= static_cast<int32_t>(str.size())) {
                        return false;
                    }

                    auto const& low = str[i + 1];
                    if (!encode_utf8(c, low, out)) {
                        return false;
                    }
                    // Skip the low surrogate
                    ++i;
                }
                else {
                    if (!encode_utf8(c, out)) {
                        return false;
                    }
                }
            }

            return true;
        }

        inline constexpr auto encode_utf8(std::u32string_view str, mi::U8String& out) -> bool {
            for (int32_t i = 0; i < static_cast<int32_t>(str.size()); ++i) {
                if (!encode_utf8(str[i], out)) {
                    return false;
                }
            }

            return true;
        }

        inline constexpr auto decode_utf8(const char8_t* utf8, size_t& bytes_readed, char32_t& codepoint) -> bool {
            auto uc0 = utf8[0];

            if ((uc0 & 0x80) == 0) {
                bytes_readed = 1;
                codepoint = static_cast<char32_t>(uc0);
                return true;
            }
            else if ((uc0 & 0xE0) == 0xC0) {
                if (!is_continuation_byte(utf8[1])) {
                    return false;
                }
                bytes_readed = 2;
                codepoint = static_cast<char32_t>(((uc0 & 0x1F) << 6) | (utf8[1] & 0x3F));
                return true;
            }
            else if ((uc0 & 0xF0) == 0xE0) {
                if (!is_continuation_byte(utf8[1]) || !is_continuation_byte(utf8[2])) {
                    return false;
                }
                bytes_readed = 3;
                codepoint = static_cast<char32_t>(((uc0 & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F));
                return true;
            }
            else if ((uc0 & 0xF8) == 0xF0) {
                if (!is_continuation_byte(utf8[1]) || !is_continuation_byte(utf8[2]) || !is_continuation_byte(utf8[3])) {
                    return false;
                }
                bytes_readed = 4;
                codepoint = static_cast<char32_t>(((uc0 & 0x07) << 18) | ((utf8[1] & 0x3F) << 12) | ((utf8[2] & 0x3F) << 6) | (utf8[3] & 0x3F));
                return true;
            }

            bytes_readed = 1;
            codepoint = 0xFFFD;
            return false;
        }

        inline auto decode_utf8(std::u8string_view utf8, mi::U16String& out) -> bool {
            int32_t i = 0;
            while (i < static_cast<int32_t>(utf8.size())) {
                size_t bytes_readed;
                char32_t cp;
                decode_utf8(utf8.data() + i, bytes_readed, cp);

                if (cp <= 0xFFFF) {
                    out += static_cast<char16_t>(cp);
                }
                else {
                    // Encode as surrogate pair
                    cp -= 0x10000;
                    out += static_cast<char16_t>(0xD800 + (cp >> 10));
                    out += static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
                }

                i += bytes_readed;
            }

            return true;
        }

        inline auto decode_utf8(std::u8string_view utf8, mi::U32String& out) -> bool {
            int32_t i = 0;
            while (i < static_cast<int32_t>(utf8.size())) {
                size_t bytes_readed;
                char32_t cp;
                decode_utf8(utf8.data() + i, bytes_readed, cp);
                out += cp;
                i += bytes_readed;
            }
            return true;
        }

        inline auto make_utf8_string(std::string_view sv) -> mi::U8String {
            // TODO: handle validation
            if (!validate_utf8(sv)) {
                return {};
            }
            return mi::U8String{ reinterpret_cast<const char8_t*>(sv.data()), sv.size() };
        }

        inline auto make_utf8_string(char16_t cp) -> mi::U8String {
            mi::U8String result{};
            if (!encode_utf8(cp, result)) {
                return {};
            }
            return result;
        }

        inline auto make_utf8_string(char16_t cp_high, char16_t cp_low) -> mi::U8String {
            mi::U8String result{};
            if (!encode_utf8(cp_high, cp_low, result)) {
                return {};
            }
            return result;
        }

        inline auto make_utf8_string(std::u16string_view sv) -> mi::U8String {
            mi::U8String result{};
            if (!encode_utf8(sv, result)) {
                return {};
            }
            return result;
        }

        inline auto make_utf8_string(char32_t cp) -> mi::U8String {
            mi::U8String result{};
            if (!encode_utf8(cp, result)) {
                return {};
            }
            return result;
        }

        inline auto make_utf8_string(int32_t c) -> mi::U8String {
            // TODO: this is not working correct now
            const auto* p = reinterpret_cast<const char8_t*>(&c);
            auto byte_count = char_byte_count(p[0]);
            if (byte_count == 0) {
                return {};
            }
            return { p, byte_count };
        }

        inline auto make_utf8_string(std::u32string_view sv) -> mi::U8String {
            mi::U8String result{};
            if (!encode_utf8(sv, result)) {
                return {};
            }
            return result;
        }

        inline auto make_utf8_string(std::wstring_view sv) -> mi::U8String {
            if constexpr (sizeof(wchar_t) == 2) {
                // Windows (UTF-16)
                std::u16string_view u16sv(reinterpret_cast<const char16_t*>(sv.data()), sv.size());
                return make_utf8_string(u16sv);
            }
            else if constexpr (sizeof(wchar_t) == 4) {
                // Unix/Linux (UTF-32)
                std::u32string_view u32sv(reinterpret_cast<const char32_t*>(sv.data()), sv.size());
                return make_utf8_string(u32sv);
            }

            return {};
        }

        inline auto make_utf16_string(std::u8string_view sv) -> mi::U16String {
            mi::U16String result;
            if (!decode_utf8(sv, result)) {
                return {};
            }
            return result;
        }

        inline auto make_utf32_string(std::u8string_view sv) -> mi::U32String {
            mi::U32String result;
            if (!decode_utf8(sv, result)) {
                return {};
            }
            return result;
        }

        inline auto make_wide_string(std::u8string_view sv) -> mi::WString {
            if constexpr (sizeof(wchar_t) == 2) {
                // Windows (UTF-16)
                auto u16 = make_utf16_string(sv);
                return mi::WString(reinterpret_cast<const wchar_t*>(u16.data()), u16.size());
            }
            else if constexpr (sizeof(wchar_t) == 4) {
                // Unix/Linux (UTF-32)
                auto u32 = make_utf32_string(sv);
                return mi::WString(reinterpret_cast<const wchar_t*>(u32.data()), u32.size());
            }
            else {
                throw std::runtime_error("Unsupported wchar_t size");
            }
        }
    }

    namespace fs::path {
        inline constexpr auto is_alpha(char8_t c) noexcept -> bool {
            return (c >= u8'A' && c <= u8'Z') || (c >= u8'a' && c <= u8'z');
        }

        inline constexpr auto is_separator(char8_t c) noexcept -> bool {
            return c == u8'/' || c == u8'\\';
        }

        inline constexpr auto find_last_separator(std::u8string_view path) noexcept -> size_t {
            for (int32_t i = static_cast<int32_t>(path.size()); i > 0; --i) {
                if (is_separator(path[i - 1])) {
                    return static_cast<size_t>(i - 1);
                }
            }

            return std::u8string_view::npos;
        }

        inline constexpr auto find_first_separator(std::u8string_view path) noexcept -> size_t {
            for (int32_t i = 0; i < static_cast<int32_t>(path.size()); ++i) {
                if (is_separator(path[i])) {
                    return static_cast<size_t>(i);
                }
            }

            return std::u8string_view::npos;
        }

        inline constexpr auto is_absolute(std::u8string_view path) noexcept -> bool {
            if (path.empty()) {
                return false;
            }

            if (is_separator(path[0])) {
                return true;
            }

            if (path.size() >= 3 && is_alpha(path[0]) && path[1] == u8':' && is_separator(path[2])) {
                return true;
            }

            return false;
        }

        inline constexpr auto filename(std::u8string_view path) noexcept -> std::u8string_view {
            if (path.empty()) {
                return path;
            }

            while (!path.empty() && is_separator(path.back())) {
                path.remove_suffix(1);
            }

            if (path.empty()) {
                return u8"/";
            }

            auto pos = find_last_separator(path);
            if (pos == std::u8string_view::npos) {
                return path;
            }

            return path.substr(pos + 1);
        }

        inline constexpr auto extension(std::u8string_view path) noexcept -> std::u8string_view {
            auto fname = filename(path);
            if (fname.empty() || fname == u8"." || fname == u8"..") {
                return std::u8string_view{};
            }

            auto pos = fname.rfind(u8'.');
            if (pos == std::u8string_view::npos || pos == 0) {
                return std::u8string_view{};
            }

            return fname.substr(pos);
        }

        inline constexpr auto stem(std::u8string_view path) noexcept -> std::u8string_view {
            auto fname = filename(path);
            if (fname.empty() || fname == u8"." || fname == u8"..") {
                return fname;
            }

            auto pos = fname.rfind(u8'.');
            if (pos == std::u8string_view::npos || pos == 0) {
                return fname;
            }

            return fname.substr(0, pos);
        }

        inline constexpr auto parent_path(std::u8string_view path) noexcept -> std::u8string_view {
            if (path.empty()) {
                return path;
            }

            while (!path.empty() && is_separator(path.back())) {
                path.remove_suffix(1);
            }

            if (path.empty()) {
                return std::u8string_view{};
            }

            auto pos = find_last_separator(path);
            if (pos == std::u8string_view::npos) {
                return std::u8string_view{};
            }

            if (pos == 0) {
                return path.substr(0, 1);
            }

            if (pos == 2 && path.size() >= 3 && path[1] == u8':') {
                return path.substr(0, 3);
            }

            return path.substr(0, pos);
        }

        inline auto to_posix(std::u8string_view path) -> mi::U8String {
            mi::U8String result(path);
            std::ranges::replace(result, u8'\\', u8'/');
            return result;
        }

        inline auto to_windows(std::u8string_view path) -> mi::U8String {
            mi::U8String result(path);
            std::ranges::replace(result, u8'/', u8'\\');
            return result;
        }

        inline auto normalize(std::u8string_view path, char8_t preferred_separator = u8'/') -> mi::U8String {
            if (path.empty()) {
                return mi::U8String{};
            }

            auto absolute = is_absolute(path);
            mi::U8String prefix;
            std::u8string_view work_path = path;

            // Case for windows prefix
            if (work_path.size() >= 2 && work_path[1] == u8':' && is_alpha(work_path[0])) {
                prefix = std::u8string(work_path.substr(0, 2));
                work_path = work_path.substr(2);
                if (!work_path.empty() && is_separator(work_path[0])) {
                    prefix += preferred_separator;
                    work_path = work_path.substr(1);
                }
            }
            else if (absolute) {
                prefix = preferred_separator;
                work_path = work_path.substr(1);
            }

            mi::Vector<std::u8string_view> components;
            size_t start = 0;

            for (std::size_t i = 0; i <= work_path.size(); ++i) {
                if (i == work_path.size() || is_separator(work_path[i])) {
                    if (i > start) {
                        auto component = work_path.substr(start, i - start);
                        if (component == u8"..") {
                            if (!components.empty() && components.back() != u8"..") {
                                components.pop_back();
                            }
                            else if (!absolute) {
                                components.push_back(component);
                            }
                        }
                        else if (component != u8".") {
                            components.push_back(component);
                        }
                    }
                    start = i + 1;
                }
            }

            mi::U8String result = prefix;
            for (size_t i = 0; i < components.size(); ++i) {
                if (i > 0 || !prefix.empty()) {
                    result += preferred_separator;
                }
                result += components[i];
            }

            if (result.empty() && !absolute) {
                result = u8".";
            }

            return result;
        }

        inline auto append(std::u8string_view base, std::u8string_view component, char8_t separator = u8'/') -> mi::U8String {
            if (base.empty()) {
                return mi::U8String(component);
            }

            if (component.empty()) {
                return mi::U8String(base);
            }

            mi::U8String result(base);
            if (!is_separator(result.back()) && !is_separator(component.front())) {
                result += separator;
            }
            result += component;

            return result;
        }

        inline auto split_components(std::u8string_view path) -> mi::Vector<std::u8string_view> {
            mi::Vector<std::u8string_view> result;
            size_t pos = 0;

            // Skip leading separators
            while (pos < path.size() && is_separator(path[pos])) {
                ++pos;
            }

            auto comp_start = pos;
            for (; pos <= path.size(); ++pos) {
                if (pos == path.size() || is_separator(path[pos])) {
                    if (pos > comp_start) {
                        result.push_back(path.substr(comp_start, pos - comp_start));
                    }
                    comp_start = pos + 1;
                }
            }

            return result;
        }

        template<typename... Args>
        inline auto join(char8_t separator, std::u8string_view first, Args&&... args) -> mi::U8String {
            mi::U8String result(first);

            auto append_one = [&](std::u8string_view component) {
                if (!component.empty()) {
                    if (!result.empty() && !is_separator(result.back()) &&
                        !is_separator(component.front())) {
                        result += separator;
                    }
                    result += component;
                }
                };

            (append_one(std::forward<Args>(args)), ...);
            return result;
        }
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
        void write_vector(const mi::Vector<T>& vec) {
            auto len = static_cast<std::uint32_t>(vec.size());
            write(len);
            write_array(vec.data(), vec.size());
        }

        // Write vector of serializable types
        template<Serializable T>
        void write_vector(const mi::Vector<T>& vec) {
            auto len = static_cast<std::uint32_t>(vec.size());
            write(len);
            for (const auto& item : vec) {
                write(item);
            }
        }

        // Write string (with length prefix)
        void write_string(const mi::String& str) {
            auto len = static_cast<std::uint32_t>(str.size());
            write(len);
            stream_.write(str.data(), str.size());
        }

        // Write string without length prefix
        void write_string_raw(const mi::String& str) {
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
        mi::Vector<T> read_vector() {
            auto len = read<std::uint32_t>();
            mi::Vector<T> vec(len);
            read_array(vec.data(), len);
            return vec;
        }

        // Read vector of deserializable types (with length prefix)
        template<Deserializable T>
        mi::Vector<T> read_vector() {
            auto len = read<std::uint32_t>();
            mi::Vector<T> vec;
            vec.reserve(len);
            for (std::uint32_t i = 0; i < len; ++i) {
                vec.push_back(read<T>());
            }
            return vec;
        }

        // Read string (with length prefix)
        mi::String read_string() {
            auto len = read<std::uint32_t>();
            mi::String str(len, '\0');
            stream_.read(str.data(), len);
            return str;
        }

        // Read fixed-size string without length prefix
        mi::String read_string_raw(std::size_t size) {
            mi::String str(size, '\0');
            stream_.read(str.data(), size);
            return str;
        }

        // Read null-terminated string
        mi::String read_cstring() {
            mi::String str;
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