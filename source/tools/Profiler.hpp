#pragma once
#include "Log.hpp"
#include "Timer.hpp"

namespace KS
{

// Creating this object will log the time it takes until it falls out of scope
class ScopedProfiler
{
public:
    ScopedProfiler(const std::string& section_name)
        : section_name(section_name)
    {
    }

    ~ScopedProfiler()
    {
        LOG(Log::Severity::INFO, "{}: {} ms", timer)
    }

private:
    std::string section_name = "Unnamed Section";
    Timer timer;
};
}
