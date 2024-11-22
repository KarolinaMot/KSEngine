#pragma once
#include <Common.hpp>

class Engine;
class MainEngine;

// Main interface for defining engine modules
// Requires overriding: Init, Tick, Shutdown
class ModuleInterface
{
public:
    ModuleInterface() = default;
    virtual ~ModuleInterface() = default;

    NON_COPYABLE(ModuleInterface);
    NON_MOVABLE(ModuleInterface);

private:
    friend Engine;
    friend MainEngine;

    virtual void Initialize(Engine& engine) = 0;

    // Modules are shutdown in the order they are initialized
    virtual void Shutdown(Engine& engine) = 0;
};