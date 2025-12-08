#pragma once
#include <chrono>

namespace KS
{

class Timer
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using FloatMilliseconds = std::chrono::duration<float, std::milli>;

    Timer() { Reset(); }

    // Time since last Tick (or Reset)
    FloatMilliseconds Tick()
    {
        auto now = Clock::now();
        FloatMilliseconds dt = std::chrono::duration_cast<FloatMilliseconds>(now - last_tick);
        last_tick = now;  // <-- update every frame

        float ms = dt.count();
        frames++;
        accumTime += ms;  // accumulated ms in current window

        // update FPS / MS about once per second
        if (accumTime >= 1000.0f && frames > 0)
        {
            MS = accumTime / frames;  // avg ms per frame over last ~1s
            FPS = 1000.0f / MS;       // avg fps

            frames = 0;
            accumTime = 0.0f;
        }

        return dt;
    }

    FloatMilliseconds TimePassed() const { return std::chrono::duration_cast<FloatMilliseconds>(Clock::now() - last_tick); }

    float GetFPS() const { return FPS; }
    float GetMS() const { return MS; }

    void Reset()
    {
        last_tick = Clock::now();
        frames = 0;
        accumTime = 0.0f;
        FPS = 0.0f;
        MS = 0.0f;
    }

private:
    TimePoint last_tick;
    uint32_t frames = 0;
    float accumTime = 0.0f;  // ms
    float FPS = 0.0f;
    float MS = 0.0f;
};

}  // namespace KS
