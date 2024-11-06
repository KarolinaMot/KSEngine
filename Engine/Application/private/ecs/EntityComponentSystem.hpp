#pragma once
#include <entt/entity/registry.hpp>

class EntityComponentSystem
{
public:
    EntityComponentSystem() = default;
    ~EntityComponentSystem() = default;

    entt::registry& GetWorld() { return m_world; }

private:
    entt::registry m_world;
};
