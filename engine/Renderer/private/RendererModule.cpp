#include <ApplicationModule.hpp>
#include <DXBackendModule.hpp>
#include <FileIO.hpp>
#include <Log.hpp>
#include <RawInputHandler.hpp>
#include <RendererModule.hpp>
#include <SerializationCommon.hpp>
#include <TimeModule.hpp>
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

    {

        // auto model_file = FileIO::OpenReadStream("assets/models/DamagedHelmet/DamagedHelmet.json").value();
        // JSONLoader model_loader { model_file };

        Model model = ModelUtility::ImportFromFile("assets/models/DamagedHelmet.glb").value();
        // model_loader(model);

        auto mesh_file = FileIO::OpenReadStream(model.meshes.front()).value();
        BinaryLoader mesh_loader { mesh_file };

        mesh_loader(mesh_data);

        Log("Successfully loaded mesh data");
    }

    {
        auto* positions = mesh_data.GetAttribute(MeshConstants::ATTRIBUTE_POSITIONS_NAME);
        auto* normals = mesh_data.GetAttribute(MeshConstants::ATTRIBUTE_NORMALS_NAME);
        auto* texture_uvs = mesh_data.GetAttribute(MeshConstants::ATTRIBUTE_TEXTURE_UVS_NAME);
        auto* tangents = mesh_data.GetAttribute(MeshConstants::ATTRIBUTE_TANGENTS_NAME);
        auto* indices = mesh_data.GetAttribute(MeshConstants::ATTRIBUTE_INDICES_NAME);

        auto upload_data = [](ID3D12Device* device, const ByteBuffer* data)
        {
            DXResourceBuilder builder {};
            builder
                .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
                .WithInitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);

            auto upload_buffer = builder.MakeBuffer(device, data->GetSize()).value();
            auto mapped_ptr = upload_buffer.Map(0);
            std::memcpy(mapped_ptr.Get(), data->GetData(), data->GetSize());
            upload_buffer.Unmap(std::move(mapped_ptr));

            return upload_buffer;
        };

        auto positions_upload_data = upload_data(dx_device.Get(), positions);
        auto normals_upload_data = upload_data(dx_device.Get(), normals);
        auto texture_uvs_upload_data = upload_data(dx_device.Get(), texture_uvs);
        auto tangents_upload_data = upload_data(dx_device.Get(), tangents);
        auto indices_upload_data = upload_data(dx_device.Get(), indices);

        auto command_list = dx_device.GetCommandQueue().MakeCommandList(dx_device.Get());

        auto stage_data = [](ID3D12Device* device, DXCommandList& command_list, DXResource& upload_resource, size_t size)
        {
            DXResourceBuilder builder {};
            auto resource = builder.MakeBuffer(device, size).value();
            command_list.CopyBuffer(upload_resource, 0, resource, 0, size);

            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                resource.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            command_list.SetResourceBarriers(1, &barrier);

            return resource;
        };

        test_mesh.position = stage_data(dx_device.Get(), command_list, positions_upload_data, positions->GetSize());
        test_mesh.normals = stage_data(dx_device.Get(), command_list, normals_upload_data, normals->GetSize());
        test_mesh.uvs = stage_data(dx_device.Get(), command_list, texture_uvs_upload_data, texture_uvs->GetSize());
        test_mesh.tangents = stage_data(dx_device.Get(), command_list, tangents_upload_data, tangents->GetSize());
        test_mesh.indices = stage_data(dx_device.Get(), command_list, indices_upload_data, indices->GetSize());
        test_mesh.index_count = indices->GetView<uint32_t>().count();

        dx_device.GetCommandQueue().SubmitCommandList(std::move(command_list)).Wait();
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
    forward_renderer->QueueModel(transform, &test_mesh);
    forward_renderer->RenderFrame(camera, backend.GetDevice(), *main_swapchain);
}