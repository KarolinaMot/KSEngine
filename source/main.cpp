// #include <cereal/archives/binary.hpp>
// #include <cereal/archives/json.hpp>
// #include <code_utility.hpp>
// #include <compare>
#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>
// #include <containers/SlotMap.hpp>
#include <device/Device.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/FileIO.hpp>
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
#include <input/RawInput.hpp>
#include <math/Geometry.hpp>
// #include <iostream>
#include <memory>
#include <renderer/ModelRenderer.hpp>
#include <renderer/Renderer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputsBuilder.hpp>
#include <renderer/DX12/Helpers/DXSignature.hpp>
#include <tools/Log.hpp>
// #include <vector>
#include <resources/Model.hpp>
#include <tools/Timer.hpp>

KS::Camera FreeCamSystem(std::shared_ptr<KS::RawInput> input, entt::registry& registry, float dt)
{
    constexpr float MOUSE_SENSITIVITY = 0.003f;
    constexpr float CAM_SPEED = 0.003f;

    auto [x, y] = input->GetMouseDelta();
    glm::vec3 eulerDelta {};

    if (input->GetMouseButton(KS::MouseButton::Right) == KS::InputState::Pressed)
    {
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

    if (glm::length(movement_dir) != 0.0f)
    {
        movement_dir = glm::normalize(movement_dir);
    }

    auto view = registry.view<KS::ComponentFirstPersonCamera, KS::ComponentTransform>();
    for (auto&& [e, camera, transform] : view.each())
    {

        camera.eulerAngles += eulerDelta;
        camera.eulerAngles.x = glm::clamp(camera.eulerAngles.x, -glm::radians(89.9f), glm::radians(89.9f));

        // LOG(Log::Severity::INFO, "{} {}", camera.eulerAngles.x, camera.eulerAngles.y);

        auto rotation = glm::quat(camera.eulerAngles);
        auto translation = transform.GetLocalTranslation();

        transform.SetLocalTranslation(translation + rotation * (movement_dir * dt * CAM_SPEED));
        transform.SetLocalRotation(rotation);

        return camera.GenerateCamera(transform.GetWorldMatrix());
    }

    return KS::Camera {};
}

int main()
{
    auto model = KS::ModelImporter::ImportFromFile("assets/models/DamagedHelmet.glb").value();

    KS::DeviceInitParams params {};
    params.window_width = 1280;
    params.window_height = 720;

    // Initialize device
    auto device = std::make_shared<KS::Device>(params);
    device->InitializeSwapchain();
    device->FinishInitialization();

    auto ecs = std::make_shared<KS::EntityComponentSystem>();
    auto input = std::make_shared<KS::RawInput>(device);

    device->NewFrame();

    std::shared_ptr<KS::ShaderInputs> mainInputs = KS::ShaderInputsBuilder()
                                                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, "camera_matrix")
                                                       .AddUniform(KS::ShaderInputVisibility::VERTEX, "model_matrix")
                                                       .AddUniform(KS::ShaderInputVisibility::PIXEL, "material_info")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "base_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "normal_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "emissive_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "roughmet_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "occlusion_tex")
                                                       .AddStorageBuffer(KS::ShaderInputVisibility::PIXEL, 100, "dir_lights")
                                                       .AddStorageBuffer(KS::ShaderInputVisibility::PIXEL, 100, "point_lights")
                                                       .AddUniform(KS::ShaderInputVisibility::PIXEL, "light_info")
                                                       .AddStaticSampler(KS::ShaderInputVisibility::PIXEL, KS::SamplerDesc {})
                                                       .Build(*device, "MAIN SIGNATURE");

    std::string shaderPath
        = "assets/shaders/Main.hlsl";
    std::shared_ptr<KS::Shader> mainShader = std::make_shared<KS::Shader>(*device,
        KS::ShaderType::ST_MESH_RENDER,
        mainInputs,
        shaderPath);

        KS::RendererInitParams initParams {};
    initParams.shaders.push_back(mainShader);
    KS::Renderer renderer = KS::Renderer(*device, initParams);

    device->EndFrame();

    // Scene Setup
    {
        auto& registry = ecs->GetWorld();
        auto e = registry.create();

        registry.emplace<KS::ComponentTransform>(e, glm::vec3(0.f, 0.f, -1.f));
        registry.emplace<KS::ComponentFirstPersonCamera>(e);
    }

    KS::Timer frametimer {};

    while (device->IsWindowOpen())
    {
        auto dt = frametimer.Tick();

        input->ProcessInput();
        device->NewFrame();

        auto camera = FreeCamSystem(input, ecs->GetWorld(), dt.count());

        auto renderParams = KS::RendererRenderParams();

        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();
        renderParams.cameraPos = camera.GetPosition();

        auto* model_renderer = dynamic_cast<KS::ModelRenderer*>(renderer.m_subrenderers.front().get());
        glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
        transform = glm::rotate(transform, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        model_renderer->QueueModel(model, transform);
        model_renderer->SetAmbientLight(glm::vec3(1.f, 1.f, 1.f), .25f);
        model_renderer->QueuePointLight(glm::vec3(0.5, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), 5.f, 5.f);
        model_renderer->QueuePointLight(glm::vec3(-0.5, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), 5.f, 5.f);
        renderer.Render(*device, renderParams);
        device->EndFrame();
    }

    device->Flush();

    return 0;
}