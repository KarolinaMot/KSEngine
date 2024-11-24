#include <ApplicationModule.hpp>
#include <DXBackendModule.hpp>
#include <LevelModule.hpp>
#include <Log.hpp>
#include <MainEngine.hpp>
#include <RendererModule.hpp>
#include <TimeModule.hpp>
#include <Timers.hpp>
#include <glm/glm.hpp>

#include <FileIO.hpp>
#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>
#include <resources/Model.hpp>

struct ComponentStaticMesh
{
    GPUMesh mesh {};
    GPUMaterial material {};
};

void print_frame_time(Engine& e)
{
    static int fps = 0;
    static DeltaMS accum { 0 };
    constexpr DeltaMS MAX { 1000 };

    fps++;

    const auto& time = e.GetModule<TimeModule>();
    accum += time.GetDeltaTime();

    if (accum > MAX)
    {
        accum -= MAX;
        Log("Updates per second: {}", fps);
        fps = 0;
    }
}

void init_scene(Engine& e)
{
    auto& ecs = e.GetModule<LevelModule>().GetRegistry();

    auto camera_entity = ecs.create();

    ecs.emplace<ComponentFirstPersonCamera>(camera_entity);
    ecs.emplace<ComponentTransform3D>(camera_entity);

    auto mesh_entity = ecs.create();

    auto& t = ecs.emplace<ComponentTransform3D>(mesh_entity);
    t.rotation = glm::quat(glm::vec3(glm::radians(90.0f), glm::radians(180.0f), 0.0f));
    t.translation = { 0.0f, 0.0f, 3.0f };

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
        auto& dx_device = e.GetModule<DXBackendModule>().GetDevice();
        auto upload_commands = dx_device.GetCommandQueue().MakeCommandList(dx_device.Get());

        auto mesh_result = GPUMeshUtility::CreateGPUMesh(upload_commands, dx_device.Get(), mesh_data);
        auto material_result = GPUMaterialUtility::CreateGPUMaterial(upload_commands, dx_device.Get(), material_data);

        dx_device.GetCommandQueue()
            .SubmitCommandList(std::move(upload_commands))
            .Wait();

        auto& mesh = ecs.emplace<ComponentStaticMesh>(mesh_entity);
        mesh.mesh = std::move(mesh_result.mesh);
        mesh.material = std::move(material_result.material);
    }
}

void camera_system(Engine& e)
{
    auto dt = e.GetModule<TimeModule>().GetDeltaTime().count();
    auto& renderer_module = e.GetModule<RendererModule>();
    auto& input = e.GetModule<ApplicationModule>().GetMainWindow().GetInputHandler();
    auto& ecs = e.GetModule<LevelModule>().GetRegistry();

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

    auto view = ecs.view<ComponentFirstPersonCamera, ComponentTransform3D>();
    for (auto&& [entity, camera, transform] : view.each())
    {
        camera.euler_rotation += eulerDelta;

        camera.euler_rotation.x = glm::clamp(
            camera.euler_rotation.x,
            -glm::radians(89.9f),
            glm::radians(89.9f));

        glm::quat camera_rot = glm::quat(camera.euler_rotation);
        transform.translation += camera_rot * (movement_dir * dt * CAM_SPEED);

        Camera frame_camera = Camera::Perspective(
            transform.translation,
            transform.translation + camera_rot * World::FORWARD,
            16.0f / 9.0f,
            camera.field_of_view,
            camera.near_clip,
            camera.far_clip);

        renderer_module.SetCamera(frame_camera);
    }
}

void render_meshes(Engine& e)
{
    auto& renderer = e.GetModule<RendererModule>().GetRenderer();
    auto& ecs = e.GetModule<LevelModule>().GetRegistry();

    auto view = ecs.view<ComponentStaticMesh, ComponentTransform3D>();
    for (auto&& [e, mesh, transform] : view.each())
    {
        renderer.QueueModel({ transform.ToMatrix(), &mesh.mesh, &mesh.material });
    }
}

int main(int argc, const char* argv[])
{
    Log("Starting up KSEngine");

    for (int i = 0; i < argc; i++)
    {
        Log("Argument {}: {}", i, argv[i]);
    }

    auto engine = MainEngine();

    engine
        .AddModule<TimeModule>()
        .AddModule<DXBackendModule>()
        .AddModule<ApplicationModule>()
        .AddModule<RendererModule>()
        .AddModule<LevelModule>();

    init_scene(engine);

    return engine
        .AddExecutionDelegate(print_frame_time, ExecutionOrder::LAST)
        .AddExecutionDelegate(render_meshes, ExecutionOrder::RENDER)
        .AddExecutionDelegate(camera_system, ExecutionOrder::UPDATE)
        .Run();
}
