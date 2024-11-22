#pragma once
#include <Common.hpp>
#include <chrono>

using DeltaMS = std::chrono::duration<float, std::milli>;

class Stopwatch
{
public:
    Stopwatch();
    DeltaMS GetElapsed() const;
    void Reset();

private:
    std::chrono::high_resolution_clock::time_point _start;
};

// RAII Stopwatch
class SectionStopwatch
{
public:
    SectionStopwatch() = default;
    SectionStopwatch(const char* name);
    ~SectionStopwatch();

    NON_COPYABLE(SectionStopwatch);
    NON_MOVABLE(SectionStopwatch);

private:
    const char* name = nullptr;
    Stopwatch track {};
};