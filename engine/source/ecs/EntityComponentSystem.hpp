#pragma once

#include <code_utility.hpp>
#include <entt/entity/registry.hpp>

namespace KS {

class EntityComponentSystem {
public:
  EntityComponentSystem() = default;
  ~EntityComponentSystem() = default;

  entt::registry &GetWorld() { return m_world; }

private:
  entt::registry m_world;
};

} // namespace KS