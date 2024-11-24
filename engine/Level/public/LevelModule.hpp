#pragma once
#include <Engine.hpp>
#include <entt/entity/registry.hpp>

enum class SystemType : size_t
{
    UPDATE,
    RENDER,
    MAX_TYPES
};

class LevelModule : public ModuleInterface
{
    void Initialize(Engine& e) override;
    void Shutdown(Engine& e) override;

public:
    ~LevelModule() override = default;
    entt::registry& GetRegistry() { return registry; }

private:
    void UpdateSystems();
    void RenderSystems();

    entt::registry registry {};
};