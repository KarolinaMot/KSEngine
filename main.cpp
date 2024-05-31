#include <code_utility.hpp>
#include <compare>
#include <device/device.hpp>
#include <renderer/Renderer.hpp>
#include <renderer/ShaderInputsBuilder.hpp>
#include <renderer/Shader.hpp>
#include <iostream>
#include <ecs/EntityComponentSystem.hpp>
#include <entt/entity/registry.hpp>
#include <fileio/FileIO.hpp>
#include <resources/Manager.hpp>
#include <tools/Log.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>
#include <types/Text.hpp>

int main()
{
  auto device = std::make_shared<KS::Device>(KS::DeviceInitParams{});
  auto filesystem = std::make_shared<KS::FileIO>();
  auto ecs = std::make_shared<KS::EntityComponentSystem>();
  auto resources = std::make_shared<KS::ResourceManager>();

  device->NewFrame();

  std::shared_ptr<KS::ShaderInputs>
      mainInputs = KS::ShaderInputsBuilder()
                       .AddStruct(KS::ShaderInputVisibility::VERTEX, "camera_matrix")
                       .AddStruct(KS::ShaderInputVisibility::VERTEX, "model_matrix")
                       .Build(*device, "MAIN SIGNATURE");
  std::string shaderPath = "assets/shaders/Main.hlsl";
  std::shared_ptr<KS::Shader> mainShader = std::make_shared<KS::Shader>(*device,
                                                                        KS::ShaderType::ST_MESH_RENDER,
                                                                        mainInputs,
                                                                        shaderPath);

  KS::RendererInitParams initParams{};
  initParams.shaders.push_back(mainShader);
  KS::Renderer renderer = KS::Renderer(*device, initParams);

  device->EndFrame();

  while (device->IsWindowOpen())
  {
    device->NewFrame();
    auto renderParams = KS::RendererRenderParams();
    renderParams.cpuFrame = device->GetFrameIndex();
    renderParams.projectionMatrix = glm::perspectiveLH_ZO(glm::radians(90.f), static_cast<float>(device->GetWidth()) / static_cast<float>(device->GetHeight()), 0.1f, 500.f);
    renderParams.viewMatrix = glm::lookAtLH(glm::vec3(0.f, 1.f, -1.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    renderer.Render(*device, renderParams);
    device->EndFrame();
  }

  return 0;
}