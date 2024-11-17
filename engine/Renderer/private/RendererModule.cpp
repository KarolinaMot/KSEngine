#include <ApplicationModule.hpp>
#include <DXBackendModule.hpp>
#include <FileIO.hpp>
#include <Log.hpp>
#include <RawInputHandler.hpp>
#include <RendererModule.hpp>
#include <SerializationCommon.hpp>
#include <TimeModule.hpp>
#include <constants/MeshContants.hpp>
#include <gpu_resources/DXResourceBuilder.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>
#include <shader/DXShaderInputs.hpp>


void RendererModule::UpdateCamera(Engine& e)
{
    auto dt = e.GetModule<TimeModule>().GetDeltaTime().count();
    auto& input = e.GetModule<ApplicationModule>().GetMainWindow().GetInputHandler();

    constexpr float MOUSE_SENSITIVITY = 0.005f;
    constexpr float CAM_SPEED = 20.0f;

    auto mouse_delta = input.GetMouseDelta();
    glm::vec3 eulerDelta {};

    if (input.GetMouseButton(MouseButton::Right) == InputState::Pressed)
    {
        eulerDelta.y = mouse_delta.x * MOUSE_SENSITIVITY;
        eulerDelta.x = mouse_delta.y * MOUSE_SENSITIVITY;
        eulerDelta.x = glm::clamp(eulerDelta.x, -glm::radians(89.9f), glm::radians(89.9f));
    }

    glm::vec3 movement_dir {};
    if (input.GetKeyboard(KeyboardKey::W) == InputState::Pressed)
        movement_dir += World::FORWARD;

    if (input.GetKeyboard(KeyboardKey::S) == InputState::Pressed)
        movement_dir -= World::FORWARD;

    if (input.GetKeyboard(KeyboardKey::D) == InputState::Pressed)
        movement_dir += World::RIGHT;

    if (input.GetKeyboard(KeyboardKey::A) == InputState::Pressed)
        movement_dir -= World::RIGHT;

    if (input.GetKeyboard(KeyboardKey::E) == InputState::Pressed)
        movement_dir += World::UP;

    if (input.GetKeyboard(KeyboardKey::Q) == InputState::Pressed)
        movement_dir -= World::UP;

    if (glm::length(movement_dir) != 0.0f)
    {
        movement_dir = glm::normalize(movement_dir);
    }

    camera_rot += eulerDelta;
    camera_rot.x = glm::clamp(camera_rot.x, -glm::radians(89.9f), glm::radians(89.9f));

    glm::quat rotation = glm::quat(camera_rot);
    camera_pos = camera_pos + rotation * (movement_dir * dt * CAM_SPEED);
}

void RendererModule::Initialize(Engine& e)
{
    auto& application = e.GetModule<ApplicationModule>();
    auto& backend = e.GetModule<DXBackendModule>();

    auto& window = application.GetMainWindow();
    auto& dx_factory = backend.GetFactory();
    auto& dx_device = backend.GetDevice();

    HWND win_handle = static_cast<HWND>(window.GetNativeWindowHandle());

    auto swapchain = dx_factory.CreateSwapchainForWindow(dx_device.GetCommandQueue().Get(), win_handle, window.GetSize());
    main_swapchain = std::make_unique<DXSwapchain>(swapchain, dx_device.Get());

    auto& compiler = backend.GetShaderCompiler();
    forward_renderer = std::make_unique<ForwardRenderer>(dx_device, compiler, main_swapchain->GetResolution());

    e.AddExecutionDelegate(this, &RendererModule::RenderFrame, ExecutionOrder::RENDER);
    e.AddExecutionDelegate(this, &RendererModule::UpdateCamera, ExecutionOrder::UPDATE);

    // Load test model

    Mesh mesh_data {};
    Material material_data {};

    {
        auto model_file = FileIO::OpenReadStream("assets/models/DamagedHelmet/DamagedHelmet.json").value();
        JSONLoader model_loader { model_file };

        // Model model = ModelUtility::ImportFromFile("assets/models/DamagedHelmet.glb").value();
        Model model {};
        model_loader(model);

        auto mesh_file = FileIO::OpenReadStream(model.meshes.front()).value();
        BinaryLoader mesh_loader { mesh_file };

        mesh_loader(mesh_data);

        material_data = model.materials.front();

        Log("Successfully loaded model data");
    }

    // Upload GPU resources
    {
        auto upload_commands = dx_device.GetCommandQueue().MakeCommandList(dx_device.Get());

        auto mesh_result = GPUMeshUtility::CreateGPUMesh(upload_commands, dx_device.Get(), mesh_data);
        auto material_result = GPUMaterialUtility::CreateGPUMaterial(upload_commands, dx_device.Get(), material_data);

        dx_device.GetCommandQueue()
            .SubmitCommandList(std::move(upload_commands))
            .Wait();

        helmet_mesh = std::move(mesh_result.mesh);
        helmet_material = std::move(material_result.material);
    }
}

void RendererModule::Shutdown(Engine& e)
{
    e.GetModule<DXBackendModule>().GetDevice().GetCommandQueue().Flush();
}

void RendererModule::RenderFrame(Engine& e)
{
    glm::vec2 screen_size = main_swapchain->GetResolution();

    Camera camera = Camera::Perspective(
        camera_pos,
        camera_pos + glm::quat(camera_rot) * World::FORWARD,
        screen_size.x / screen_size.y,
        glm::radians(90.0f),
        0.1f,
        100.0f);

    auto& backend = e.GetModule<DXBackendModule>();

    glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
    transform = glm::rotate(transform, glm::pi<float>(), World::UP);
    transform = glm::rotate(transform, glm::pi<float>() * 0.5f, World::RIGHT);

    static float rotation_add = 0.0f;
    rotation_add += e.GetModule<TimeModule>().GetDeltaTime().count();

    // transform = glm::rotate(transform, rotation_add, World::UP);

    forward_renderer->QueueModel({ transform, &helmet_mesh, &helmet_material });
    forward_renderer->RenderFrame(camera, backend.GetDevice(), *main_swapchain);
}