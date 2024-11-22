#include <Log.hpp>
#include <Timers.hpp>

Stopwatch::Stopwatch()
{
    Reset();
}

DeltaMS Stopwatch::GetElapsed() const
{
    return std::chrono::duration_cast<DeltaMS>(std::chrono::high_resolution_clock::now() - _start);
}

void Stopwatch::Reset()
{
    _start = std::chrono::high_resolution_clock::now();
}

SectionStopwatch::SectionStopwatch(const char* name)
    : name(name)
{
}

SectionStopwatch::~SectionStopwatch()
{
    if (name)
    {
        Log("{} - {}ms", name, track.GetElapsed().count());
    }
    else
    {
        Log("{} - {}ms", "Section Stopwatch", track.GetElapsed().count());
    }
}