#include <code_utility.hpp>
#include <compare>
#include <device/Device.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <entt/entity/registry.hpp>
#include <fileio/FileIO.hpp>
#include <resources/Manager.hpp>
#include <tools/Log.hpp>
#include <types/Text.hpp>


int main()
{
  auto device = std::make_shared<KS::Device>(KS::DeviceInitParams{});
  auto filesystem = std::make_shared<KS::FileIO>();
  auto ecs = std::make_shared<KS::EntityComponentSystem>();
  auto resources = std::make_shared<KS::ResourceManager>();

  // while (device.IsWindowOpen())
  // {
  //   device.NewFrame();
  //   device.EndFrame();
  // }

  return 0;
}