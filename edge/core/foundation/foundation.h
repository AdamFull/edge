#pragma once

#include "foundation_base.h"
#include "data_stream.h"

#include <algorithm>
#include <array>
#include <memory>
#include <format>
#include <span>
#include <concepts>
#include <limits>

#include <ranges>

#include <fstream>

// For thread pool
#include <thread>
#include <future>
#include <condition_variable>
#include <mutex>
#include <latch>

#ifndef EDGE_PLATFORM_ANDROID
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#if !defined(__aarch64__)
#include <cpuid.h>
#endif // __aarch64__
#endif
#endif

namespace edge {
    inline constexpr uint64_t aligned_size(uint64_t size, uint64_t alignment) {
        return (size + alignment - 1ull) & ~(alignment - 1ull);
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

    struct CPUFeatures {
        bool has_sse2 = false;
        bool has_avx = false;
        bool has_avx2 = false;

        static auto detect() -> CPUFeatures {
            CPUFeatures features;

#ifndef EDGE_PLATFORM_ANDROID
#if defined(_MSC_VER)
            int cpu_info[4];
            __cpuid(cpu_info, 1);
            features.has_sse2 = (cpu_info[3] & (1 << 26)) != 0;
            features.has_avx = (cpu_info[2] & (1 << 28)) != 0;

            __cpuidex(cpu_info, 7, 0);
            features.has_avx2 = (cpu_info[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
            unsigned int eax, ebx, ecx, edx;

#if !defined(__aarch64__)
            if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
                features.has_sse2 = (edx & bit_SSE2) != 0;
                features.has_avx = (ecx & bit_AVX) != 0;
            }

            if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
                features.has_avx2 = (ebx & bit_AVX2) != 0;
            }
#endif // __aarch64__
#endif
#endif

            return features;
        }
    };

    extern CPUFeatures g_cpu_features;

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

    class ThreadPool : public NonCopyMovable {
    public:
        explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
            : stop_(false), active_tasks_(0) {

            if (num_threads == 0) {
                num_threads = 1;
            }

            workers_.reserve(num_threads);
            worker_queues_.resize(num_threads);

            // Create mutexes for each worker queue
            for (size_t i = 0; i < num_threads; ++i) {
                worker_mutexes_.emplace_back(std::make_unique<std::mutex>());
            }

            // Create worker threads
            for (size_t i = 0; i < num_threads; ++i) {
                workers_.emplace_back([this, i, num_threads] {
                    worker_thread(i, num_threads);
                    });
            }
        }

        ~ThreadPool() {
            shutdown();
        }

        template<class F>
        void enqueue(F&& f) {
            ++active_tasks_;
            {
                std::unique_lock<std::mutex> lock(global_mutex_);
                global_queue_.emplace_back(std::forward<F>(f));
            }
            global_cv_.notify_one();
        }

        template<class F, class... Args>
        auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F(Args...)>::type> {
            using return_type = typename std::invoke_result<F(Args...)>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> result = task->get_future();

            ++active_tasks_;
            {
                std::unique_lock<std::mutex> lock(global_mutex_);
                global_queue_.emplace_back([task]() { (*task)(); });
            }
            global_cv_.notify_one();

            return result;
        }

        template<class F>
        void parallel_for(int32_t start, int32_t end, F&& func, int32_t min_per_thread = 1) {
            if (end <= start) return;

            int32_t range = end - start;
            int32_t num_workers = static_cast<int32_t>(workers_.size());
            int32_t chunk_size = std::max(min_per_thread, (range + num_workers - 1) / num_workers);

            int32_t num_tasks = (range + chunk_size - 1) / chunk_size;
            std::latch completion_latch(num_tasks);

            for (int32_t i = start; i < end; i += chunk_size) {
                int32_t chunk_end = std::min(i + chunk_size, end);

                enqueue([i, chunk_end, &func, &completion_latch]() {
                    func(i, chunk_end);
                    completion_latch.count_down();
                    });
            }

            completion_latch.wait();
        }

        void wait() {
            std::unique_lock<std::mutex> lock(global_mutex_);
            completion_cv_.wait(lock, [this] {
                return active_tasks_ == 0;
                });
        }

        size_t num_threads() const {
            return workers_.size();
        }

    private:
        void worker_thread(size_t thread_id, size_t num_threads) {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(*worker_mutexes_[thread_id]);
                    if (!worker_queues_[thread_id].empty()) {
                        task = std::move(worker_queues_[thread_id].front());
                        worker_queues_[thread_id].pop_front();
                    }
                }

                // If own queue is empty, try work stealing
                if (!task) {
                    for (size_t j = 0; j < num_threads; ++j) {
                        if (j == thread_id) continue;

                        std::unique_lock<std::mutex> lock(*worker_mutexes_[j], std::try_to_lock);
                        if (lock.owns_lock() && !worker_queues_[j].empty()) {
                            // Steal from back (opposite end) for better cache locality
                            task = std::move(worker_queues_[j].back());
                            worker_queues_[j].pop_back();
                            break;
                        }
                    }
                }

                // If still no task, wait on global queue
                if (!task) {
                    std::unique_lock<std::mutex> lock(global_mutex_);
                    global_cv_.wait(lock, [this] {
                        return stop_ || !global_queue_.empty();
                        });

                    if (stop_ && global_queue_.empty()) {
                        return;
                    }

                    if (!global_queue_.empty()) {
                        task = std::move(global_queue_.front());
                        global_queue_.pop_front();
                    }
                }

                if (task) {
                    task();
                    --active_tasks_;
                    completion_cv_.notify_all();
                }
            }
        }

        void shutdown() {
            if (workers_.empty()) return;

            {
                std::unique_lock<std::mutex> lock(global_mutex_);
                stop_.store(true, std::memory_order_release);
            }
            global_cv_.notify_all();

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }

            workers_.clear();
        }

        mi::Vector<std::thread> workers_;
        mi::Vector<edge::mi::Deque<std::function<void()>>> worker_queues_;
        mi::Vector<std::unique_ptr<std::mutex>> worker_mutexes_;
        mi::Deque<std::function<void()>> global_queue_;

        std::mutex global_mutex_;
        std::condition_variable global_cv_;
        std::condition_variable completion_cv_;

        std::atomic<bool> stop_;
        std::atomic<int32_t> active_tasks_;
    };
}