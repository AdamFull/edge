#pragma once

#include <mimalloc.h>
#include <spdlog/spdlog.h>

#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <source_location>

#if EDGE_PLATFORM_ANDROID
#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <memory>
#define EDGE_HAVE_STACKTRACE 1
#elif EDGE_PLATFORM_WINDOWS
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#define EDGE_HAVE_STACKTRACE 1
#endif

#undef min
#undef max

#if defined(_MSC_VER)
#define EDGE_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__i386__) || defined(__x86_64__)
#define EDGE_DEBUG_BREAK() __asm__ volatile("int $0x03")
#elif defined(__aarch64__)
#define EDGE_DEBUG_BREAK() __asm__ volatile(".inst 0xd4200000")
#else
#include <csignal>
#define EDGE_DEBUG_BREAK() std::raise(SIGTRAP)
#endif
#else
#include <csignal>
#define EDGE_DEBUG_BREAK() std::raise(SIGTRAP)
#endif

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

namespace edge::mi {
	template<typename T>
	using BasicString = std::basic_string<T, std::char_traits<T>, mi_stl_allocator<T>>;

	using String = BasicString<char>;
	using WString = BasicString<wchar_t>;
	using U8String = BasicString<char8_t>;
	using U16String = BasicString<char16_t>;
	using U32String = BasicString<char32_t>;

	template<typename T>
	using BasicOStringStream = std::basic_ostringstream<T, std::char_traits<T>, mi_stl_allocator<T>>;

	using OStringStream = BasicOStringStream<char>;
	using OWStringStream = BasicOStringStream<wchar_t>;
	using OU8StringStream = BasicOStringStream<char8_t>;
	using OU16StringStream = BasicOStringStream<char16_t>;
	using OU32StringStream = BasicOStringStream<char32_t>;

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

namespace edge::foundation {
    namespace detail {
#if EDGE_PLATFORM_ANDROID
        struct BacktraceState {
            void** current;
            void** end;
        };

        inline auto unwind_callback(struct _Unwind_Context* context, void* arg) -> _Unwind_Reason_Code {
            BacktraceState* state = static_cast<BacktraceState*>(arg);
            uintptr_t pc = _Unwind_GetIP(context);
            if (pc) {
                if (state->current == state->end) {
                    return _URC_END_OF_STACK;
                }
                else {
                    *state->current++ = reinterpret_cast<void*>(pc);
                }
            }
            return _URC_NO_REASON;
        }
#endif
    }

    inline auto stacktrace(int skip_frames = 1) -> mi::String {
#if defined(EDGE_HAVE_STACKTRACE) && EDGE_PLATFORM_ANDROID
        constexpr int max_frames = 64;

        void* buffer[max_frames];

        detail::BacktraceState state = {buffer, buffer + max_frames};
        _Unwind_Backtrace(detail::unwind_callback, &state);
        int frame_count = state.current - buffer;

        mi::OStringStream trace;

        for (int i = skip_frames; i < frame_count; ++i) {
            int frame_num = i - skip_frames;
            void* addr = buffer[i];

            Dl_info info;
            mi::String function_name = "??";
            mi::String module_name = "";

            if (dladdr(addr, &info)) {
                if (info.dli_sname) {
                    int status;
                    char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
                    if (status == 0 && demangled) {
                        function_name = demangled;
                        std::free(demangled);
                    } else {
                        function_name = info.dli_sname;
                    }
                }

                if (info.dli_fname) {
                    module_name = info.dli_fname;
                }
            }

            trace << "\n#" << frame_num << "  ";

            if (!module_name.empty()) {
                const char* filename = strrchr(module_name.c_str(), '/');
                if (filename) {
                    trace << "Object \"" << (filename + 1) << "\"";
                } else {
                    trace << "Object \"" << module_name << "\"";
                }
                trace << ", at " << std::hex << "0x" << reinterpret_cast<uintptr_t>(addr) << std::dec;
            }

            trace << ", in " << function_name;

            if (!module_name.empty()) {
                trace << " [" << module_name << "]";
            }
        }

        return trace.str();
#elif defined(EDGE_HAVE_STACKTRACE) && EDGE_PLATFORM_WINDOWS
        constexpr int max_frames = 64;
        void* stack[max_frames];
        HANDLE process = GetCurrentProcess();

        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        SymInitialize(process, nullptr, TRUE);

        WORD frame_count = CaptureStackBackTrace(skip_frames, max_frames - skip_frames, stack, nullptr);

        mi::OStringStream trace;

        const size_t symbol_size = sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR);
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)mi_malloc(symbol_size);
        symbol->MaxNameLen = MAX_SYM_NAME;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        for (WORD i = 0; i < frame_count; ++i) {
            DWORD64 address = (DWORD64)(stack[i]);

            mi::String function_name = "??";
            mi::String file_location = "";
            mi::String module_name = "";

            DWORD64 displacement = 0;
            if (SymFromAddr(process, address, &displacement, symbol)) {
                function_name = symbol->Name;
            }

            DWORD line_displacement = 0;
            if (SymGetLineFromAddr64(process, address, &line_displacement, &line)) {
                mi::OStringStream loc;
                loc << line.FileName << ", line " << line.LineNumber;
                file_location = loc.str();
            }

            IMAGEHLP_MODULE64 module;
            module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
            if (SymGetModuleInfo64(process, address, &module)) {
                module_name = module.ImageName;
            }

            trace << "\n#" << i << "  ";

            if (!module_name.empty()) {
                const char* filename = strrchr(module_name.c_str(), '\\');
                if (!filename) {
                    filename = strrchr(module_name.c_str(), '/');
                }
                if (filename) {
                    trace << "Object \"" << (filename + 1) << "\"";
                }
                else {
                    trace << "Object \"" << module_name << "\"";
                }
                trace << ", at " << std::hex << "0x" << address << std::dec;
            }

            trace << ", in " << function_name;

            if (!file_location.empty()) {
                trace << "\n    Source \"" << file_location << "\", in " << function_name << " [" << module_name << "]";
            }
            else if (!module_name.empty()) {
                trace << " [" << module_name << "]";
            }
        }

        mi_free(symbol);
        SymCleanup(process);

        return trace.str();
#else
        return "Stack trace not available on this platform";
#endif
    }

    template<typename... Args>
    inline void fatal_error(bool condition, const char* condition_str, const std::source_location& location,
        std::format_string<Args...> fmt, Args&&... args) {
        if (condition) {
            return;
        }

        auto stack_trace = stacktrace();
        auto formatted_message = std::format(fmt, std::forward<Args>(args)...);

        spdlog::critical(
            "Fatal error: {}\n  Message: {}\n  File: {}:{}\n  Function: {}\n  Stack trace:\n{}",
            condition_str, formatted_message, location.file_name(), location.line(), location.function_name(), stack_trace
        );

        spdlog::shutdown();

#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
        EDGE_DEBUG_BREAK();
#endif

        std::abort();
    }
}

#define EDGE_FATAL_ERROR(condition, ...) ::edge::foundation::fatal_error(static_cast<bool>(condition), #condition, std::source_location::current(), __VA_ARGS__)