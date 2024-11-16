#include <ApplicationModule.hpp>
#include <DXBackendModule.hpp>
#include <Log.hpp>
#include <MainEngine.hpp>
#include <RendererModule.hpp>
#include <TimeModule.hpp>
#include <Timers.hpp>
#include <glm/glm.hpp>

void print_frame_time(Engine& e)
{
    static int fps = 0;
    static DeltaMS accum { 0 };
    constexpr DeltaMS MAX { 1000 };

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
    Log("Starting up KSEngine");

    for (int i = 0; i < argc; i++)
    {
        Log("Argument {}: {}", i, argv[i]);
    }

    auto engine = MainEngine();

    engine
        .AddModule<TimeModule>()
        .AddModule<DXBackendModule>()
        .AddModule<ApplicationModule>()
        .AddModule<RendererModule>();

    return engine
        .AddExecutionDelegate(print_frame_time, ExecutionOrder::LAST)
        .Run();
}
