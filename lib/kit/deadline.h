#pragma once
// Wall-clock budget for a turn. Useless where turns are metered in fuel, not time.
#include <chrono>

namespace kit {

struct Deadline {
    using Clock = std::chrono::steady_clock;
    Clock::time_point end;

    explicit Deadline(double ms)
        : end(Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::milli>(ms))) {}

    bool passed() const { return Clock::now() >= end; }
    double left_ms() const { return std::chrono::duration<double, std::milli>(end - Clock::now()).count(); }
};

}
