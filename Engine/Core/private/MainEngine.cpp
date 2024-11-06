#include <MainEngine.hpp>
#include <cassert>

int MainEngine::Run()
{

    if (m_execution.empty())
    {
        assert(false && "No execution delegates bound for engine run!");
        return 0;
    }

    while (!m_exitRequested)
    {
        MainLoopOnce();
    }

    return m_exitCode;
}

MainEngine& MainEngine::AddExecutionDelegate(Delegate<void(Engine&)>&& delegate, ExecutionOrder order)
{
    Engine::AddExecutionDelegate(std::move(delegate), order);
    return *this;
}

void MainEngine::MainLoopOnce()
{
    for (auto& [delegate, priority] : m_execution)
    {
        delegate(*this);

        if (m_exitRequested)
        {
            return;
        }
    }
}

int MainEngine::GetExitCode() const
{
    return m_exitCode;
}
