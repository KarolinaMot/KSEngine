#pragma once
#include <Common.hpp>
#include <Delegate.hpp>
#include <ModuleInterface.hpp>
#include <typeindex>
#include <unordered_map>
#include <vector>

enum class ExecutionOrder : uint32_t
{
    FIRST = 0,
    PRE_UPDATE = 5,
    UPDATE = 10,
    POST_UPDATE = 15,
    PRE_RENDER = 20,
    RENDER = 25,
    POST_RENDER = 30,
    LAST = 35
};

// Service locator for all modules
// Instantiate a MainEngine to run the engine, which inherits from this class
class Engine
{
public:
    Engine() = default;
    virtual ~Engine() { Reset(); }

    NON_COPYABLE(Engine);
    NON_MOVABLE(Engine);

    // Unsafe to call in Shutdown if requested module is already freed (instead, use GetModuleSafe())
    template <typename Module>
    Module& GetModule();

    // Returns Null if the module is not found
    template <typename Module>
    Module* GetModuleSafe();

    // Adds a delegate (function) to be executed once per frame
    Engine& AddExecutionDelegate(Delegate<void(Engine&)>&& delegate, ExecutionOrder order);

    template <typename Module>
    Engine& AddExecutionDelegate(Module* module, void (Module::*member_func)(Engine&), ExecutionOrder order);

    // Sets the exit code for the program and stops any further execution
    // Should not be called in Shutdown
    void SetExit(int exit_code);

protected:
    int m_exitCode = 0;
    bool m_exitRequested = false;
    std::vector<std::pair<Delegate<void(Engine&)>, ExecutionOrder>> m_execution {};

    // Cleans up all modules
    void Reset();

private:
    ModuleInterface* GetModuleUntyped(std::type_index type) const;
    void RegisterNewModule(std::type_index moduleType, ModuleInterface* module);

    // Raw pointers are used because deallocation order of modules is important
    std::unordered_map<std::type_index, ModuleInterface*> m_modules {};
    std::vector<ModuleInterface*> m_initOrder {};
};

template <typename Module>
inline Module& Engine::GetModule()
{
    if (auto modulePtr = GetModuleSafe<Module>())
    {
        return static_cast<Module&>(*modulePtr);
    }

    auto type = std::type_index(typeid(Module));
    auto* newModule = new Module();

    RegisterNewModule(type, newModule);
    return *newModule;
}

template <typename Module>
Module* Engine::GetModuleSafe()
{
    return static_cast<Module*>(GetModuleUntyped(std::type_index(typeid(Module))));
}

template <typename Module>
inline Engine& Engine::AddExecutionDelegate(Module* module, void (Module::*member_func)(Engine&), ExecutionOrder order)
{
    return AddExecutionDelegate({ module, member_func }, order);
}