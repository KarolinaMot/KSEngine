#include "Common.hpp"
#include <ApplicationModule.hpp>
#include <Log.hpp>
#include <MainEngine.hpp>
#include <TimeModule.hpp>
#include <Timers.hpp>
#include <glm/glm.hpp>

void print_frame_time(Engine& e)
{
    static int fps = 0;
    static DeltaMS accum = {};
    constexpr DeltaMS MAX = DeltaMS { 1000 };

    fps++;

    const auto& time = e.GetModule<TimeModule>();
    accum += time.GetDeltaTime();

    if (accum > MAX)
    {
        accum -= MAX;
        Log("Updates per second: {}", fps);
        fps = 0;
    }
}

int main(int argc, const char* argv[])
{
    Log("Starting up Engine");
    for (int i = 0; i < argc; i++)
    {
        Log("Argument {}: {}", i, argv[i]);
    }

    return MainEngine()
        .AddModule<TimeModule>()
        .AddModule<OldApplicationModule>()
        .AddExecutionDelegate(print_frame_time, ExecutionOrder::LAST)
        .Run();
}
