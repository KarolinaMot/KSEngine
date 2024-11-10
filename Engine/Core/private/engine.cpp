#include <Engine.hpp>
#include <algorithm>
#include <utility>

void Engine::SetExit(int code)
{
    m_exitRequested = true;
    m_exitCode = code;
}

void Engine::Reset()
{
    for (auto it = m_initOrder.rbegin(); it != m_initOrder.rend(); ++it)
    {
        auto* module = *it;
        module->Shutdown(*this);
    }

    for (auto it = m_initOrder.rbegin(); it != m_initOrder.rend(); ++it)
    {
        auto* module = *it;
        delete module;
    }

    m_modules.clear();
    m_initOrder.clear();
    m_exitRequested = false;
    m_exitCode = 0;
}
ModuleInterface* Engine::GetModuleUntyped(std::type_index type) const
{
    if (auto it = m_modules.find(type); it != m_modules.end())
    {
        return it->second;
    }
    else
    {
        return nullptr;
    }
}
Engine& Engine::AddExecutionDelegate(Delegate<void(Engine&)>&& delegate, ExecutionOrder order)
{
    // sorted emplace, based on tick priority
    auto compare = [](const auto& new_elem, const auto& old_elem)
    {
        return new_elem.second < old_elem.second;
    };

    auto pair = std::make_pair(std::move(delegate), order);

    auto insert_it = std::upper_bound(
        m_execution.begin(), m_execution.end(), pair, compare);

    m_execution.emplace(insert_it, std::move(pair));
    return *this;
}

void Engine::RegisterNewModule(std::type_index moduleType, ModuleInterface* module)
{
    module->Initialize(*this);
    auto [it, success] = m_modules.emplace(moduleType, module);
    m_initOrder.emplace_back(it->second);
}