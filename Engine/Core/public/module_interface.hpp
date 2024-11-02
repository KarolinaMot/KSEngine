#pragma once
#include "common.hpp"
#include <cstdint>


class Engine;
class MainEngine;

enum class ModuleDelegateOrder : uint32_t
{
    eFirst = 0,
    ePreTick = 5,
    eTick = 10,
    ePostTick = 15,
    ePreRender = 20,
    eRender = 25,
    ePostRender = 30,
    eLast = 35
};

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

    // Return the desired tick order for this module
    virtual void Init(Engine& engine) = 0;

    // Modules are shutdown in the order they are initialized
    virtual void Shutdown(Engine& engine) = 0;
};