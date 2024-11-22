#include <Engine.hpp>
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

    void Shutdown(MAYBE_UNUSED Engine& e) override { }

    void ResetFrameTimer(MAYBE_UNUSED Engine& e)
    {
        m_frame_timer.Reset();
    }

    Stopwatch m_frame_timer {};
    Stopwatch m_total_timer {};
};