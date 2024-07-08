#pragma once
#include <chrono>

namespace KS
{

class Timer
{
public:
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using FloatMilliseconds = std::chrono::duration<float, std::milli>;

    Timer() { Reset(); }

    FloatMilliseconds TimePassed() const
    {
        return std::chrono::duration_cast<FloatMilliseconds>(last_tick - std::chrono::high_resolution_clock::now());
    }

    FloatMilliseconds Tick()
    {
        auto t = TimePassed();
        Reset();
        return t;
    }

    void Reset()
    {
        last_tick = std::chrono::high_resolution_clock::now();
    }

private:
    TimePoint last_tick;
};

}