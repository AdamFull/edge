#pragma once

#include <chrono>

namespace edge::foundation {
    class Stopwatch {
        using timepoint_t = std::chrono::steady_clock::time_point;
    public:
        Stopwatch() noexcept {
            sp = std::chrono::high_resolution_clock::now();
        }

        template<class T>
        T stop() {
            auto now = std::chrono::high_resolution_clock::now();
            auto secs = std::chrono::duration<T>(now - sp).count();
            sp = now;
            return secs;
        }
    private:
        timepoint_t sp;
    };
}