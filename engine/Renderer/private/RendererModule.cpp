#include <ApplicationModule.hpp>
#include <DXBackendModule.hpp>
#include <Log.hpp>
#include <RawInputHandler.hpp>
#include <RendererModule.hpp>
#include <TimeModule.hpp>
#include <shader/DXShaderInputs.hpp>

// #include <components/ComponentCamera.hpp>
// #include <components/ComponentTransform.hpp>

// #include <renderer/ModelRenderer.hpp>
// #include <resources/Model.hpp>

// Camera FreeCamSystem(const RawInputHandler& input, entt::registry& registry, float dt)
// {
//     constexpr float MOUSE_SENSITIVITY = 0.003f;
//     constexpr float CAM_SPEED = 0.003f;

//     auto mouse_delta = input.GetMouseDelta();
//     glm::vec3 eulerDelta {};

//     if (input.GetMouseButton(MouseButton::Right) == InputState::Pressed)
//     {
//         eulerDelta.y = mouse_delta.x * MOUSE_SENSITIVITY;
//         eulerDelta.x = mouse_delta.y * MOUSE_SENSITIVITY;
//         eulerDelta.x = glm::clamp(eulerDelta.x, -glm::radians(89.9f), glm::radians(89.9f));
//     }

//     glm::vec3 movement_dir {};
//     if (input.GetKeyboard(KeyboardKey::W) == InputState::Pressed)
//         movement_dir += World::FORWARD;

//     if (input.GetKeyboard(KeyboardKey::S) == InputState::Pressed)
//         movement_dir -= World::FORWARD;

//     if (input.GetKeyboard(KeyboardKey::D) == InputState::Pressed)
//         movement_dir += World::RIGHT;

//     if (input.GetKeyboard(KeyboardKey::A) == InputState::Pressed)
//         movement_dir -= World::RIGHT;

//     if (input.GetKeyboard(KeyboardKey::E) == InputState::Pressed)
//         movement_dir += World::UP;

//     if (input.GetKeyboard(KeyboardKey::Q) == InputState::Pressed)
//         movement_dir -= World::UP;

//     if (glm::length(movement_dir) != 0.0f)
//     {
//         movement_dir = glm::normalize(movement_dir);
//     }

//     auto view = registry.view<ComponentFirstPersonCamera, ComponentTransform>();
//     for (auto&& [e, camera, transform] : view.each())
//     {

//         camera.eulerAngles += eulerDelta;
//         camera.eulerAngles.x = glm::clamp(camera.eulerAngles.x, -glm::radians(89.9f), glm::radians(89.9f));

//         // LOG(Log::Severity::INFO, "{} {}", camera.eulerAngles.x, camera.eulerAngles.y);

//         auto rotation = glm::quat(camera.eulerAngles);
//         auto translation = transform.GetLocalTranslation();

//         transform.SetLocalTranslation(translation + rotation * (movement_dir * dt * CAM_SPEED));
//         transform.SetLocalRotation(rotation);

//         return camera.GenerateCamera(transform.GetWorldMatrix());
//     }

//     return Camera {};
// }

void RendererModule::Initialize(Engine& e)
{
    // Preamble
    // {
    //     // model = ModelImporter::ImportFromFile("assets/models/DamagedHelmet.glb").value();

    //     ecs = std::make_unique<EntityComponentSystem>();
    //     auto& registry = ecs->GetWorld();
    //     auto e = registry.create();

    //     registry.emplace<ComponentTransform>(e, glm::vec3(0.f, 0.f, -1.f));
    //     registry.emplace<ComponentFirstPersonCamera>(e);
    // }

    auto& application = e.GetModule<ApplicationModule>();
    auto& backend = e.GetModule<DXBackendModule>();

    auto& window = application.GetMainWindow();
    auto& dx_factory = backend.GetFactory();
    auto& dx_device = backend.GetDevice();

    HWND win_handle = static_cast<HWND>(window.GetNativeWindowHandle());
    auto swapchain = dx_factory.CreateSwapchainForWindow(dx_device.GetCommandQueue().Get(), win_handle, window.GetSize());

    auto& dx_render_target_heap = dx_device.GetDescriptorHeap(DXDevice::DescriptorHeap::RT_HEAP);
    main_swapchain = std::make_unique<DXSwapchain>(swapchain, dx_device.Get(), dx_render_target_heap);

    renderer = std::make_unique<Renderer>();

    auto& compiler = backend.GetShaderCompiler();
    compiler.AddIncludeDirectory(L"assets/shaders");

    auto deferred_vs = compiler
                           .CompileFromPath("assets/shaders/Deferred.hlsl", DXShader::Type::VERTEX, L"mainVS")
                           .value();

    auto deferred_ps = compiler
                           .CompileFromPath("assets/shaders/Deferred.hlsl", DXShader::Type::VERTEX, L"mainPS")
                           .value();

    auto pbr_resolve_cs = compiler
                              .CompileFromPath("assets/shaders/Main.hlsl", DXShader::Type::COMPUTE, L"main")
                              .value();

    auto builder = DXShaderInputsBuilder();

    builder
        // Uniforms
        .AddUniformBuffer("camera_matrix", 0, D3D12_SHADER_VISIBILITY_ALL)
        .AddUniformBuffer("model_index", 1, D3D12_SHADER_VISIBILITY_ALL)
        .AddUniformBuffer("light_info", 2, D3D12_SHADER_VISIBILITY_ALL)

        // Textures (RO)
        .AddDescriptorTable("base_tex", 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
        .AddDescriptorTable("normal_tex", 1, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
        .AddDescriptorTable("emissive_tex", 2, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
        .AddDescriptorTable("roughmet_tex", 3, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)
        .AddDescriptorTable("occlusion_tex", 4, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)

        // StorageBuffers (RO)
        .AddDescriptorTable("dir_lights", 5, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL)
        .AddDescriptorTable("point_lights", 6, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_ALL)
        .AddDescriptorTable("model_matrix", 7, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_VERTEX)
        .AddDescriptorTable("material_info", 8, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_SHADER_VISIBILITY_PIXEL)

        // Render Targets (RW)
        .AddDescriptorTable("PBRRes", 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
        .AddDescriptorTable("GBuffer1", 1, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
        .AddDescriptorTable("GBuffer2", 2, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
        .AddDescriptorTable("GBuffer3", 3, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)
        .AddDescriptorTable("GBuffer4", 4, 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_SHADER_VISIBILITY_ALL)

        // Static Sampler
        .AddStaticSampler("main_sampler", 0, {}, D3D12_SHADER_VISIBILITY_ALL);

    auto shader_inputs = builder.Build(dx_device.Get(), L"Deferred Pipeline Inputs").value();

    // std::string shaderPath = "assets/shaders/Deferred.hlsl";
    // Formats formats[4] = { Formats::R32G32B32A32_FLOAT, Formats::R8G8B8A8_UNORM, Formats::R8G8B8A8_UNORM, Formats::R8G8B8A8_UNORM };
    // std::shared_ptr<Shader> mainShader = std::make_shared<Shader>(*device,
    //     ShaderType::ST_MESH_RENDER,
    //     mainInputs,
    //     shaderPath,
    //     formats, 4);

    // std::shared_ptr<Shader> computePBRShader = std::make_shared<Shader>(*device,
    //     ShaderType::ST_COMPUTE,
    //     mainInputs,
    //     "assets/shaders/Main.hlsl");

    // RendererInitParams initParams {};
    // initParams.shaders.push_back(mainShader);
    // initParams.shaders.push_back(computePBRShader);
    // renderer = std::make_shared<Renderer>(*device, initParams);

    // device->EndFrame();

    e.AddExecutionDelegate(this, &RendererModule::RenderFrame, ExecutionOrder::RENDER);
}

void RendererModule::Shutdown(Engine& e)
{
    e.GetModule<DXBackendModule>().GetDevice().GetCommandQueue().Flush();
}

void RendererModule::RenderFrame(Engine& e)
{
    auto& backend = e.GetModule<DXBackendModule>();
    renderer->RenderFrame(backend.GetDevice(), *main_swapchain);

    // auto dt = e.GetModule<TimeModule>().GetDeltaTime();

    // device->NewFrame();

    // auto camera = FreeCamSystem(raw_input, ecs->GetWorld(), dt.count());

    // // if (input->GetKeyboard(KeyboardKey::Space) == InputState::Down)
    // //     raytraced = !raytraced;

    // auto renderParams = RendererRenderParams();

    // renderParams.cpuFrame = device->GetFrameIndex();
    // renderParams.projectionMatrix = camera.GetProjection();
    // renderParams.viewMatrix = camera.GetView();
    // renderParams.cameraPos = camera.GetPosition();

    // auto* model_renderer = dynamic_cast<ModelRenderer*>(renderer->m_subrenderers.front().get());

    // glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
    // glm::mat4x4 transform2 = glm::translate(glm::mat4x4(1.f), glm::vec3(2.f, -0.5f, 3.f));
    // glm::mat4x4 transform3 = glm::translate(glm::mat4x4(1.f), glm::vec3(-2.f, -0.5f, 3.f));

    // transform = glm::rotate(transform, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
    // transform2 = glm::rotate(transform2, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
    // transform3 = glm::rotate(transform3, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));

    // renderer->SetAmbientLight(glm::vec3(1.f, 1.f, 1.f), .8f);
    // renderer->QueuePointLight(glm::vec3(0.5, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), 5.f, 5.f);
    // renderer->QueuePointLight(glm::vec3(-0.5, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), 5.f, 5.f);
    // model_renderer->QueueModel(*device, model, transform);
    // model_renderer->QueueModel(*device, model, transform2);
    // model_renderer->QueueModel(*device, model, transform3);
    // renderer->Render(*device, renderParams, false);
    // device->EndFrame();
}