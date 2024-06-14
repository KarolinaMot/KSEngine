#include <code_utility.hpp>
#include <compare>
#include <device/device.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <entt/entity/registry.hpp>
#include <fileio/FileIO.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <input/RawInput.hpp>
#include <iostream>
#include <memory>
#include <renderer/Renderer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputsBuilder.hpp>
#include <resources/Manager.hpp>
#include <tools/Log.hpp>
#include <types/Text.hpp>
#include <vector>

#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>

KS::Camera FreeCamSystem(std::shared_ptr<KS::RawInput> input, entt::registry& registry, float dt)
{
    constexpr float MOUSE_SENSITIVITY = 0.003f;
    constexpr float CAM_SPEED = 0.1f;

    auto [x, y] = input->GetMouseDelta();
    glm::vec3 eulerDelta {};

    if (input->GetMouseButton(KS::MouseButton::Right) == KS::InputState::Pressed) {
        eulerDelta.y = x * MOUSE_SENSITIVITY;
        eulerDelta.x = y * MOUSE_SENSITIVITY;
        eulerDelta.x = glm::clamp(eulerDelta.x, -glm::radians(89.9f), glm::radians(89.9f));
    }

    glm::vec3 movement_dir {};
    if (input->GetKeyboard(KS::KeyboardKey::W) == KS::InputState::Pressed)
        movement_dir += KS::World::FORWARD;

    if (input->GetKeyboard(KS::KeyboardKey::S) == KS::InputState::Pressed)
        movement_dir -= KS::World::FORWARD;

    if (input->GetKeyboard(KS::KeyboardKey::D) == KS::InputState::Pressed)
        movement_dir += KS::World::RIGHT;

    if (input->GetKeyboard(KS::KeyboardKey::A) == KS::InputState::Pressed)
        movement_dir -= KS::World::RIGHT;

    if (input->GetKeyboard(KS::KeyboardKey::E) == KS::InputState::Pressed)
        movement_dir += KS::World::UP;

    if (input->GetKeyboard(KS::KeyboardKey::Q) == KS::InputState::Pressed)
        movement_dir -= KS::World::UP;

    if (glm::length(movement_dir) != 0.0f) {
        movement_dir = glm::normalize(movement_dir);
    }

    auto view = registry.view<KS::ComponentFirstPersonCamera, KS::ComponentTransform>();
    for (auto&& [e, camera, transform] : view.each()) {

        camera.eulerAngles += eulerDelta;
        camera.eulerAngles.x = glm::clamp(camera.eulerAngles.x, -glm::radians(89.9f), glm::radians(89.9f));
        LOG(Log::Severity::INFO, "{} {}", camera.eulerAngles.x, camera.eulerAngles.y);

        auto rotation = glm::quat(camera.eulerAngles);
        auto translation = transform.GetLocalTranslation();

        transform.SetLocalTranslation(translation + rotation * (movement_dir * dt * CAM_SPEED));
        transform.SetLocalRotation(rotation);

        return camera.GenerateCamera(transform.GetWorldMatrix());
    }
}

int main()
{
    KS::DeviceInitParams params {};
    params.window_width = 1280;
    params.window_height = 720;

    auto device = std::make_shared<KS::Device>(params);
    auto filesystem = std::make_shared<KS::FileIO>();
    auto ecs = std::make_shared<KS::EntityComponentSystem>();
    auto resources = std::make_shared<KS::ResourceManager>();
    auto input = std::make_shared<KS::RawInput>(device);

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

    KS::RendererInitParams initParams {};
    initParams.shaders.push_back(mainShader);
    KS::Renderer renderer = KS::Renderer(*device, initParams);

    device->EndFrame();

    { // Create camera entity
        auto& registry = ecs->GetWorld();
        auto e = registry.create();

        auto& transform = registry.emplace<KS::ComponentTransform>(e, glm::vec3(0.f, 0.f, -1.f));
        auto& camera = registry.emplace<KS::ComponentFirstPersonCamera>(e);
    }

    while (device->IsWindowOpen()) {
        input->ProcessInput();
        device->NewFrame();

        auto camera = FreeCamSystem(input, ecs->GetWorld(), 0.16f);

        auto renderParams = KS::RendererRenderParams();
        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();
        renderer.Render(*device, renderParams);
        device->EndFrame();
    }

  return 0;
}