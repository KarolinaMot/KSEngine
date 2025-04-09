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
#include <renderer/ShaderInputCollection.hpp>
#include <renderer/ShaderInputCollectionBuilder.hpp>
#include <renderer/ShaderInput.hpp>
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

    std::shared_ptr<KS::ShaderInputCollection> mainInputs = KS::ShaderInputCollectionBuilder()
                                                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"camera_matrix"})
                                                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"model_index", "fog_info"})
            .AddTexture(KS::ShaderInputVisibility::COMPUTE, "base_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "normal_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "emissive_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "roughmet_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "occlusion_tex")
                                                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "PBRRes", KS::ShaderInputMod::READ_WRITE)
                                                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer1", KS::ShaderInputMod::READ_WRITE)
                                                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer2", KS::ShaderInputMod::READ_WRITE)
                                                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer3", KS::ShaderInputMod::READ_WRITE)
                                                       .AddTexture(KS::ShaderInputVisibility::COMPUTE, "GBuffer4", KS::ShaderInputMod::READ_WRITE)
                                                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 100, "dir_lights")
                                                       .AddStorageBuffer(KS::ShaderInputVisibility::COMPUTE, 100, "point_lights")
                                                       .AddStorageBuffer(KS::ShaderInputVisibility::VERTEX, 200, "model_matrix")
                                                       .AddStorageBuffer(KS::ShaderInputVisibility::PIXEL, 200, "material_info")
                                                       .AddUniform(KS::ShaderInputVisibility::COMPUTE, {"light_info"})
                                                       .AddStaticSampler(KS::ShaderInputVisibility::COMPUTE, KS::SamplerDesc {})
                                                       .Build(*device, "MAIN SIGNATURE");

    int fullInputFlags = KS::Shader::HAS_POSITIONS | KS::Shader::HAS_NORMALS | KS::Shader::HAS_UVS |  KS::Shader::HAS_TANGENTS;
    int positionsInputFlags = KS::Shader::HAS_POSITIONS;

    std::string shaderPath = "assets/shaders/Deferred.hlsl";
    KS::Formats formats[4] = { KS::Formats::R32G32B32A32_FLOAT, KS::Formats::R8G8B8A8_UNORM, KS::Formats::R8G8B8A8_UNORM, KS::Formats::R8G8B8A8_UNORM };
    KS::Formats format[1] = {KS::Formats::R32G32B32A32_FLOAT};

    std::shared_ptr<KS::Shader> mainShader = std::make_shared<KS::Shader>(*device,
        KS::ShaderType::ST_MESH_RENDER,
        mainInputs,
        shaderPath, fullInputFlags,
        formats, 4);

    std::shared_ptr<KS::Shader> lightOccluderShader = std::make_shared<KS::Shader>(*device, KS::ShaderType::ST_MESH_RENDER, mainInputs, "assets/shaders/OccluderShader.hlsl",
                                     positionsInputFlags, format, 1);


    std::shared_ptr<KS::Shader> computePBRShader = std::make_shared<KS::Shader>(*device,
        KS::ShaderType::ST_COMPUTE,
        mainInputs,
        "assets/shaders/Main.hlsl", 0);

    std::shared_ptr<KS::Shader> lightRendererShader = std::make_shared<KS::Shader>(*device, 
        KS::ShaderType::ST_COMPUTE, 
        mainInputs, 
        "assets/shaders/LightRenderer.hlsl", 0);

    std::shared_ptr<KS::Shader> lightShaftShader = std::make_shared<KS::Shader>(*device,
        KS::ShaderType::ST_COMPUTE,
        mainInputs,
        "assets/shaders/LightShaftShader.hlsl");

    KS::RendererInitParams initParams {};

    KS::SubRendererDesc subRenderer1;
    subRenderer1.shader = mainShader;
    initParams.subRenderers.push_back(subRenderer1);

    KS::SubRendererDesc subRenderer2;
    subRenderer2.shader = computePBRShader;
    initParams.subRenderers.push_back(subRenderer2);

    KS::SubRendererDesc subRenderer3;
    subRenderer3.shader = lightRendererShader;
    initParams.subRenderers.push_back(subRenderer3);

    KS::SubRendererDesc subRenderer4;
    subRenderer4.shader = lightOccluderShader;
    initParams.subRenderers.push_back(subRenderer4);

    KS::SubRendererDesc subRenderer5;
    subRenderer5.shader = lightShaftShader;
    initParams.subRenderers.push_back(subRenderer5);


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
    bool raytraced = false;

    while (device->IsWindowOpen())
    {
        auto dt = frametimer.Tick();

        input->ProcessInput();
        device->NewFrame();

        auto camera = FreeCamSystem(input, ecs->GetWorld(), dt.count());

        if (input->GetKeyboard(KS::KeyboardKey::Space) == KS::InputState::Down)
            raytraced = !raytraced;

        auto renderParams = KS::RendererRenderParams();

        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();
        renderParams.cameraPos = camera.GetPosition();
        renderParams.cameraRight = camera.GetRight();

        glm::vec3 lightPosition1 = glm::vec3(2.f, 0.f, 0.f);
        glm::vec3 lightPosition2 = glm::vec3(-2.f, 0.f, 0.f);

        auto* model_renderer = dynamic_cast<KS::ModelRenderer*>(renderer.m_subrenderers.front().get());
        glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
        glm::mat4x4 transform2 = glm::translate(glm::mat4x4(1.f), glm::vec3(2.f, -0.5f, 3.f));
        glm::mat4x4 transform3 = glm::translate(glm::mat4x4(1.f), glm::vec3(-2.f, -0.5f, 3.f));
        glm::mat4x4 transform4 = glm::translate(glm::mat4x4(1.f), lightPosition1);
        glm::mat4x4 transform5 = glm::translate(glm::mat4x4(1.f), lightPosition2);
        transform = glm::rotate(transform, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        transform2 = glm::rotate(transform2, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        transform3 = glm::rotate(transform3, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        transform4 = glm::rotate(transform4, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        transform5 = glm::rotate(transform5, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        transform4 = glm::scale(transform4, glm::vec3(0.1f));
        transform5 = glm::scale(transform5, glm::vec3(0.1f));
        renderer.SetAmbientLight(glm::vec3(1.f, 1.f, 1.f), .8f);
        renderer.QueuePointLight(lightPosition1, glm::vec3(1.f, 0.f, 0.f), 5.f, 5.f);
        //renderer.QueuePointLight(lightPosition2, glm::vec3(0.f, 0.f, 1.f), 5.f, 2.f);
        renderer.QueueModel(*device, model, transform);
        renderer.QueueModel(*device, model, transform2);
        renderer.QueueModel(*device, model, transform3);
        renderer.QueueModel(*device, model, transform4);
        //renderer.QueueModel(*device, model, transform5);
        model_renderer->SetRaytraced(raytraced);
        renderer.Render(*device, renderParams, raytraced);
        device->EndFrame();
    }

    device->Flush();

    return 0;
}