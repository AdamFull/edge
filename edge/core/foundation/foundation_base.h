#pragma once

#include <mimalloc.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include <chrono>
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
        struct ErrorContext {
            mi::String scope{};
            mi::String condition{};
            mi::String message{};
            mi::String file{};
            mi::String function{};
            uint32_t line{ 0u };
            std::chrono::system_clock::time_point timestamp{};
            std::thread::id thread_id{};
            mi::HashMap<mi::String, mi::String> additional_data{};

            auto format() const -> mi::String {
                mi::OStringStream oss;

                auto time_t = std::chrono::system_clock::to_time_t(timestamp);
                auto tm = *std::localtime(&time_t);
                oss << std::endl;
                oss << "===============================================================\n";
                oss << "  FATAL ERROR OCCURRED\n";
                oss << "===============================================================\n";
                oss << std::put_time(&tm, "  Time:       %Y-%m-%d %H:%M:%S\n");

                oss << "  Thread ID:  " << thread_id << "\n";
                
                oss << "---------------------------------------------------------------\n";
                oss << "  Location:\n";
                oss << "    File:     " << file << ":" << line << "\n";
                oss << "    Function: " << function << "\n";
                if (!scope.empty()) {
                    oss << "    Scope:    " << scope << "\n";
                }

                oss << "---------------------------------------------------------------\n";
                oss << "  Condition:  " << condition << "\n";
                oss << "  Message:    " << message << "\n";

                if (!additional_data.empty()) {
                    oss << "---------------------------------------------------------------\n";
                    oss << "  Additional Context:\n";
                    for (const auto& [key, value] : additional_data) {
                        oss << "    " << key << ": " << value << "\n";
                    }
                }

                return oss.str();
            }
        };

        class ErrorContextBuilder {
        public:
            explicit ErrorContextBuilder(const char* scope, const char* condition, const std::source_location& location)
                : context_{} {
                context_.scope = scope;
                context_.condition = condition;
                context_.file = location.file_name();
                context_.function = location.function_name();
                context_.line = location.line();
                context_.timestamp = std::chrono::system_clock::now();
                context_.thread_id = std::this_thread::get_id();
            }

            template<typename... Args>
            auto with_message(std::format_string<Args...> fmt, Args&&... args) -> ErrorContextBuilder& {
                context_.message = std::format(fmt, std::forward<Args>(args)...);
                return *this;
            }

            auto add_context(const mi::String& key, const mi::String& value) -> ErrorContextBuilder& {
                context_.additional_data[key] = value;
                return *this;
            }

            auto add_context(const mi::String& key, vk::Result result) -> ErrorContextBuilder& {
                context_.additional_data[key] = vk::to_string(result);
                return *this;
            }

            template<typename T>
            auto add_context(const mi::String& key, T value) -> ErrorContextBuilder& {
                context_.additional_data[key] = std::format("{}", value);
                return *this;
            }

            auto build() -> ErrorContext {
                return std::move(context_);
            }

        private:
            ErrorContext context_;
        };

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
    inline void report_fatal_error(const detail::ErrorContext& context) {
        auto formatted = context.format();
        auto stack_trace = foundation::stacktrace(2);

        spdlog::critical("{}\n  Stack trace:{}\n===============================================================",
            formatted, stack_trace);

        spdlog::shutdown();

#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
        EDGE_DEBUG_BREAK();
#endif

        // TODO: Make minidump

        std::abort();
    }

    inline auto make_vulkan_error(const char* scope, const char* condition,
        const std::source_location& location, vk::Result result, const mi::String& operation)
        -> detail::ErrorContextBuilder {
        return detail::ErrorContextBuilder(scope, condition, location)
            .with_message("Vulkan operation failed: {}", operation)
            .add_context("Result", result)
            .add_context("Result Code", static_cast<int32_t>(result));
    }

    inline auto make_resource_error(const char* scope, const char* condition,
        const std::source_location& location, const mi::String& resource_type, uint32_t resource_id)
        -> detail::ErrorContextBuilder {
        return detail::ErrorContextBuilder(scope, condition, location)
            .with_message("Invalid resource access")
            .add_context("Resource Type", resource_type)
            .add_context("Resource ID", resource_id);
    }

    inline auto make_memory_error(const char* scope, const char* condition,
        const std::source_location& location, vk::DeviceSize requested, vk::DeviceSize available)
        -> detail::ErrorContextBuilder {
        return detail::ErrorContextBuilder(scope, condition, location)
            .with_message("Memory allocation or access error")
            .add_context("Requested Size", std::format("{} bytes", requested))
            .add_context("Available Size", std::format("{} bytes", available));
    }

    inline auto make_buffer_error(const char* scope, const char* condition,
        const std::source_location& location, const mi::String& operation,
        vk::DeviceSize buffer_size, vk::DeviceSize offset, vk::DeviceSize size)
        -> detail::ErrorContextBuilder {
        return detail::ErrorContextBuilder(scope, condition, location)
            .with_message("Buffer operation error: {}", operation)
            .add_context("Buffer Size", std::format("{} bytes", buffer_size))
            .add_context("Offset", std::format("{} bytes", offset))
            .add_context("Operation Size", std::format("{} bytes", size))
            .add_context("End Position", std::format("{} bytes", offset + size))
            .add_context("Overflow", std::format("{} bytes", (offset + size) - buffer_size));
    }

    inline auto make_image_error(const char* scope, const char* condition,
        const std::source_location& location, const mi::String& operation,
        vk::Extent3D extent, vk::Format format, uint32_t mip_levels, uint32_t array_layers)
        -> detail::ErrorContextBuilder {
        return detail::ErrorContextBuilder(scope, condition, location)
            .with_message("Image operation error: {}", operation)
            .add_context("Extent", std::format("{}x{}x{}", extent.width, extent.height, extent.depth))
            .add_context("Format", vk::to_string(format))
            .add_context("Mip Levels", mip_levels)
            .add_context("Array Layers", array_layers);
    }
}

#define EDGE_FATAL_ERROR_CTX(condition, builder) \
    do { \
        if (!static_cast<bool>(condition)) { \
            auto context = builder.build(); \
            ::edge::foundation::report_fatal_error(context); \
        } \
    } while(0)

#define EDGE_FATAL_ERROR(condition, ...) EDGE_FATAL_ERROR_CTX(condition, ::edge::foundation::detail::ErrorContextBuilder(EDGE_LOGGER_SCOPE, #condition, std::source_location::current()).with_message(__VA_ARGS__))
#define EDGE_FATAL_VK_ERROR(result, operation) EDGE_FATAL_ERROR_CTX(result == vk::Result::eSuccess, ::edge::foundation::make_vulkan_error(EDGE_LOGGER_SCOPE, "result == vk::Result::eSuccess", std::source_location::current(), result, operation))
#define EDGE_FATAL_VK_BUFFER_ERROR(condition, operation, buffer_size, offset, size) EDGE_FATAL_ERROR_CTX(condition, ::edge::foundation::make_buffer_error(EDGE_LOGGER_SCOPE, #condition, std::source_location::current(), operation, buffer_size, offset, size))
#define EDGE_FATAL_VK_IMAGE_ERROR(condition, operation, extent, format, mip_levels, array_layers) EDGE_FATAL_ERROR_CTX(condition, ::edge::foundation::make_image_error(EDGE_LOGGER_SCOPE, #condition, std::source_location::current(), operation, extent, format, mip_levels, array_layers))