#include <Log.hpp>
#include <MainEngine.hpp>
#include <Timers.hpp>

class TimeModule : public ModuleInterface
{
public:
    DeltaMS GetDeltaTime() const { return m_frame_timer.GetElapsed(); }
    DeltaMS GetTotalTime() const { return m_total_timer.GetElapsed(); }

private:
    void Initialize(Engine& e) override
    {
        e.AddExecutionDelegate(this, &TimeModule::ResetFrameTimer, ExecutionOrder::FIRST);
    }

    void Shutdown(Engine& e) override { }

    void ResetFrameTimer(Engine& e)
    {
        m_frame_timer.Reset();
    }

    Stopwatch m_frame_timer {};
    Stopwatch m_total_timer {};
};

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
        .AddExecutionDelegate(print_frame_time, ExecutionOrder::LAST)
        .Run();
}
