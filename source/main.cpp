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
#include <scene/Scene.hpp>
#include <editor/Editor.hpp>
#include <tools/Log.hpp>
// #include <vector>
#include <resources/Model.hpp>
#include <tools/Timer.hpp>

using namespace KS;

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

    KS::DeviceInitParams params {};
    params.window_width = 1280;
    params.window_height = 720;

    // Initialize device
    auto device = std::make_shared<KS::Device>(params);
    device->InitializeSwapchain();
    device->FinishInitialization();

    auto input = std::make_shared<KS::RawInput>(device);
    auto editor = std::make_shared<KS::Editor>(*device);
    auto ecs = std::make_shared<KS::EntityComponentSystem>();

    device->NewFrame();

    KS::Renderer renderer = KS::Renderer(*device);
    KS::Scene scene = KS::Scene(*device);

    // Scene Setup
    {
        auto& registry = ecs->GetWorld();
        auto e = registry.create();

        registry.emplace<KS::ComponentTransform>(e, glm::vec3(0.f, 0.f, 0.f));
        registry.emplace<KS::ComponentFirstPersonCamera>(e);
    }

    KS::Timer frametimer{};
    bool raytraced = false;

    scene.SetAmbientLight(glm::vec3(1.f, 1.f, 1.f), .8f);
    scene.QueuePointLight(glm::vec3(0.5, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), 5.f, 5.f);
    scene.QueuePointLight(glm::vec3(-0.5, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), 5.f, 5.f);

    auto model = KS::ModelImporter::ImportFromFile("assets/models/DamagedHelmet.glb").value();

    glm::mat4x4 transform = glm::mat4x4(1.f);
    scene.QueueModel(*device, model, transform, "Damaged helmet");

    device->EndFrame();
    //std::shared_ptr<KS::ShaderInputCollection> mainInputs = KS::ShaderInputsBuilder()

    //                                                   .AddUniform(KS::ShaderInputVisibility::COMPUTE, "camera_matrix")
    //                                                   .AddUniform(KS::ShaderInputVisibility::COMPUTE, "model_index")
    //                                                   .AddTexture(KS::ShaderInputVisibility::PIXEL, "base_tex")
    //                                                   .AddTexture(KS::ShaderInputVisibility::PIXEL, "normal_tex")
    //                                                   .AddTexture(KS::ShaderInputVisibility::PIXEL, "emissive_tex")
    //                                                   .AddTexture(KS::ShaderInputVisibility::PIXEL, "roughmet_tex")
    //                                                   .AddTexture(KS::ShaderInputVisibility::PIXEL, "occlusion_tex")
    //                                                   .AddTexture(KS::ShaderInputVisibility::COMPUTE, "PBRRes", KS::ShaderInputMod::READ_WRITE)
    //                                                   .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer1", KS::ShaderInputMod::READ_WRITE)
    //                                                   .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer2", KS::ShaderInputMod::READ_WRITE)
    //                                                   .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer3", KS::ShaderInputMod::READ_WRITE)
    //                                                   .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer4", KS::ShaderInputMod::READ_WRITE)
    //                                                   .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 100, "dir_lights")
    //                                                   .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 100, "point_lights")
    //                                                   .AddStorageBuffer(KS::ShaderInputVisibility::VERTEX, 200, "model_matrix")
    //                                                   .AddStorageBuffer(KS::ShaderInputVisibility::PIXEL, 200, "material_info")
    //                                                   .AddUniform(KS::ShaderInputVisibility::COMPUTE, "light_info")
    //                                                   .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc {})
    //                                                   .Build(*device, "MAIN SIGNATURE");

    //std::shared_ptr<KS::ShaderInputCollection> raytraceInputs = KS::ShaderInputsBuilder().AddUniform(KS::ShaderInputVisibility::COMPUTE, "camera_matrix").Build(*device, "RAYTRACE SIGNATURE");

    //int positionsInputFlags = KS::Shader::HAS_POSITIONS;
    //int fullInputFlags = KS::Shader::HAS_POSITIONS | KS::Shader::HAS_NORMALS | KS::Shader::HAS_UVS | KS::Shader::HAS_TANGENTS;

    //std::shared_ptr<Shader> mainShader = std::make_shared<Shader>(
    //*device, ShaderType::ST_MESH_RENDER,
    //mainInputs,
    //std::initializer_list<std::string>{"assets/shaders/Deferred.hlsl"},
    //std::initializer_list<Formats>{Formats::R32G32B32A32_FLOAT, Formats::R8G8B8A8_UNORM, Formats::R8G8B8A8_UNORM,
    //                                Formats::R8G8B8A8_UNORM},
    //fullInputFlags);

    //std::shared_ptr<Shader> computePBRShader = std::make_shared<Shader>(
    //    *device, ShaderType::ST_COMPUTE, mainInputs, std::initializer_list<std::string>{"assets/shaders/Main.hlsl"},
    //std::initializer_list<Formats>{});


    //KS::RendererInitParams initParams {};
    //initParams.shaders.push_back(mainShader);
    //initParams.shaders.push_back(computePBRShader);
    //KS::Renderer renderer = KS::Renderer(*device, initParams);





    while (device->IsWindowOpen())
    {
        //auto dt = frametimer.Tick();

        //input->ProcessInput();
        //device->NewFrame();

        //auto camera = FreeCamSystem(input, ecs->GetWorld(), dt.count());

        //if (input->GetKeyboard(KS::KeyboardKey::Space) == KS::InputState::Down)
        //    raytraced = !raytraced;

        //auto renderParams = KS::RendererRenderParams();

        //renderParams.cpuFrame = device->GetFrameIndex();
        //renderParams.projectionMatrix = camera.GetProjection();
        //renderParams.viewMatrix = camera.GetView();
        //renderParams.cameraPos = camera.GetPosition();

        //auto* model_renderer = dynamic_cast<KS::ModelRenderer*>(renderer.m_subrenderers.front().get());
        ////glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
        //glm::mat4x4 transform = glm::mat4x4(1.f);
        ////glm::mat4x4 transform2 = glm::translate(glm::mat4x4(1.f), glm::vec3(2.f, -0.5f, 3.f));
        ////glm::mat4x4 transform3 = glm::translate(glm::mat4x4(1.f), glm::vec3(-2.f, -0.5f, 3.f));
        ////transform = glm::rotate(transform, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        ////transform2 = glm::rotate(transform2, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        ////transform3 = glm::rotate(transform3, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        //renderer.SetAmbientLight(glm::vec3(1.f, 1.f, 1.f), .8f);
        //renderer.QueuePointLight(glm::vec3(0.5, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), 5.f, 5.f);
        //renderer.QueuePointLight(glm::vec3(-0.5, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), 5.f, 5.f);
        //model_renderer->QueueModel(*device, model, transform);
        ////model_renderer->QueueModel(*device, model, transform2);
        ////model_renderer->QueueModel(*device, model, transform3);
        //model_renderer->SetRaytraced(raytraced);
        //renderer.Render(*device, renderParams, raytraced);
        //device->EndFrame();

        auto dt = frametimer.Tick();

        input->ProcessInput();
        device->NewFrame();

        auto camera = FreeCamSystem(input, ecs->GetWorld(), dt.count());

        if (input->GetKeyboard(KS::KeyboardKey::Space) == KS::InputState::Down) raytraced = !raytraced;

        auto renderParams = KS::RenderTickParams();
        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();
        renderParams.cameraPos = camera.GetPosition();
        renderParams.cameraRight = camera.GetRight();

        scene.Tick(*device);
        renderer.Render(*device, scene, renderParams, raytraced);
        editor->RenderWindows(*device, scene, dt.count());
        device->EndFrame();

    }

    device->Flush();

    return 0;
}