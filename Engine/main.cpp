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

void check_k(Engine& e)
{
    auto& input = e.GetModule<ApplicationModule>().GetMainWindow().GetInputHandler();
    if (input.GetKeyboard(KeyboardKey::K) == InputState::Down)
    {
        Log("Down K");
    }

    if (input.GetKeyboard(KeyboardKey::K) == InputState::Release)
    {
        Log("Up K");
    }

    auto& window = e.GetModule<ApplicationModule>().GetMainWindow();
    if (window.IsVisible())
    {
        Log("Window is minimized");
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
        .AddModule<DXBackendModule>()
        .AddModule<ApplicationModule>()
        .AddModule<RendererModule>()
        .AddExecutionDelegate(print_frame_time, ExecutionOrder::LAST)
        .AddExecutionDelegate(check_k, ExecutionOrder::UPDATE)
        .Run();
}
