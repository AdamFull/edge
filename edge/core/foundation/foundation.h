#pragma once

#include "foundation_base.h"

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

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
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

#if defined(_MSC_VER)
            int cpu_info[4];
            __cpuid(cpu_info, 1);
            features.has_sse2 = (cpu_info[3] & (1 << 26)) != 0;
            features.has_avx = (cpu_info[2] & (1 << 28)) != 0;

            __cpuidex(cpu_info, 7, 0);
            features.has_avx2 = (cpu_info[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
            unsigned int eax, ebx, ecx, edx;

            if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
                features.has_sse2 = (edx & bit_SSE2) != 0;
                features.has_avx = (ecx & bit_AVX) != 0;
            }

            if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
                features.has_avx2 = (ebx & bit_AVX2) != 0;
            }
#endif

            return features;
        }
    };

    extern CPUFeatures g_cpu_features;

    class ThreadPool {
    public:
        explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
            : stop(false), active_tasks(0) {

            workers.reserve(num_threads);
            worker_queues.resize(num_threads);

            for (size_t i = 0; i < num_threads; ++i) {
                worker_mutexes.emplace_back(std::make_unique<std::mutex>());
            }

            for (size_t i = 0; i < num_threads; ++i) {
                workers.emplace_back([this, i, num_threads] {
                    while (true) {
                        std::function<void()> task;

                        // Try to get task from own queue first
                        {
                            std::unique_lock<std::mutex> lock(*worker_mutexes[i]);
                            if (!worker_queues[i].empty()) {
                                task = std::move(worker_queues[i].front());
                                worker_queues[i].pop_front();
                            }
                        }

                        // If own queue is empty, try work stealing
                        if (!task) {
                            for (size_t j = 0; j < num_threads; ++j) {
                                if (j == i) continue;

                                std::unique_lock<std::mutex> lock(*worker_mutexes[j], std::try_to_lock);
                                if (lock.owns_lock() && !worker_queues[j].empty()) {
                                    // Steal from back (opposite end) for better cache locality
                                    task = std::move(worker_queues[j].back());
                                    worker_queues[j].pop_back();
                                    break;
                                }
                            }
                        }

                        // If still no task, wait on global queue
                        if (!task) {
                            std::unique_lock<std::mutex> lock(global_mutex);
                            global_cv.wait(lock, [this] {
                                return stop || !global_queue.empty();
                                });

                            if (stop && global_queue.empty()) {
                                return;
                            }

                            if (!global_queue.empty()) {
                                task = std::move(global_queue.front());
                                global_queue.pop_front();
                            }
                        }

                        if (task) {
                            task();
                            --active_tasks;
                            completion_cv.notify_all();
                        }
                    }
                    });
            }
        }

        template<class F>
        void enqueue(F&& f) {
            ++active_tasks;
            {
                std::unique_lock<std::mutex> lock(global_mutex);
                global_queue.emplace_back(std::forward<F>(f));
            }
            global_cv.notify_one();
        }

        template<class F, class... Args>
        auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F(Args...)>::type> {
            using return_type = typename std::invoke_result<F(Args...)>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> result = task->get_future();

            ++active_tasks;
            {
                std::unique_lock<std::mutex> lock(global_mutex);
                global_queue.emplace_back([task]() { (*task)(); });
            }
            global_cv.notify_one();

            return result;
        }

        template<class F>
        void parallel_for(int32_t start, int32_t end, F&& func, int32_t min_per_thread = 1) {
            if (end <= start) return;

            int32_t range = end - start;
            int32_t num_workers = static_cast<int32_t>(workers.size());
            int32_t chunk_size = std::max(min_per_thread, (range + num_workers - 1) / num_workers);

            std::atomic<int32_t> tasks_remaining{ 0 };
            std::mutex completion_mutex;
            std::condition_variable local_cv;

            for (int32_t i = start; i < end; i += chunk_size) {
                int32_t chunk_end = std::min(i + chunk_size, end);
                ++tasks_remaining;

                enqueue([i, chunk_end, &func, &tasks_remaining, &local_cv]() {
                    func(i, chunk_end);

                    if (--tasks_remaining == 0) {
                        local_cv.notify_one();
                    }
                    });
            }

            std::unique_lock<std::mutex> lock(completion_mutex);
            local_cv.wait(lock, [&tasks_remaining] { return tasks_remaining == 0; });
        }

        void wait() {
            std::unique_lock<std::mutex> lock(global_mutex);
            completion_cv.wait(lock, [this] {
                return active_tasks == 0;
                });
        }

        size_t num_threads() const {
            return workers.size();
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(global_mutex);
                stop = true;
            }
            global_cv.notify_all();

            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

    private:
        edge::mi::Vector<std::thread> workers;
        edge::mi::Vector<edge::mi::Deque<std::function<void()>>> worker_queues;
        edge::mi::Vector<std::unique_ptr<std::mutex>> worker_mutexes;
        edge::mi::Deque<std::function<void()>> global_queue;

        std::mutex global_mutex;
        std::condition_variable global_cv;
        std::condition_variable completion_cv;

        std::atomic<bool> stop;
        std::atomic<int32_t> active_tasks;
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