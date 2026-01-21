#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>
#include <device/Device.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/FileIO.hpp>
#include <input/RawInput.hpp>
#include <math/Geometry.hpp>
#include <memory>
#include <renderer/ModelRenderer.hpp>
#include <renderer/Renderer.hpp>
#include <renderer/Shader.hpp>
#include <scene/Scene.hpp>
#include <editor/Editor.hpp>
#include <tools/Log.hpp>
#include <resources/Model.hpp>
#include <tools/Timer.hpp>

using namespace KS;

KS::Camera FreeCamSystem(std::shared_ptr<KS::RawInput> input, entt::registry& registry, float dt, float aspectRatio)
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
    auto it = view.each().begin();
    auto [e, camera, transform] = *it;

    camera.aspectRatio = aspectRatio;
    camera.eulerAngles += eulerDelta;
    camera.eulerAngles.x = glm::clamp(camera.eulerAngles.x, -glm::radians(89.9f), glm::radians(89.9f));

    auto rotation = glm::quat(camera.eulerAngles);
    auto translation = transform.GetLocalTranslation();

    transform.SetLocalTranslation(translation + rotation * (movement_dir * dt * CAM_SPEED));
    transform.SetLocalRotation(rotation);

    return camera.GenerateCamera(transform.GetWorldMatrix());
}

int main()
{

    KS::DeviceInitParams params {};
    params.window_width = 1280;
    params.window_height = 720;
#if _DEBUG
    params.debug_context = true;
#else
    params.debug_context = false;
#endif

    // Initialize device
    auto device = std::make_shared<KS::Device>(params);
    device->InitializeSwapchain();
    device->FinishInitialization();

    auto input = std::make_shared<KS::RawInput>(device);
    auto editor = std::make_shared<KS::Editor>(*device);
    auto ecs = std::make_shared<KS::EntityComponentSystem>();

    device->NewFrame();

    std::unique_ptr<KS::Scene> scenes[KS::ScenesToChoose::COUNT];
    scenes[TEST_SCENE] = std::make_unique<KS::Scene>(*device, "Test scene", TEST_SCENE);
    KS::Renderer renderer = KS::Renderer(*device);

    // Scene Setup
    {
        auto& registry = ecs->GetWorld();
        auto e = registry.create();

        registry.emplace<KS::ComponentTransform>(e, glm::vec3(8.874f, 2.660f, 5.367f));
        registry.emplace<KS::ComponentFirstPersonCamera>(e);
    }

    KS::Timer frametimer{};
    bool raytraced = true;

    //scenes[TEST_SCENE]->SetAmbientLight(glm::vec3(1.f, 1.f, 1.f), .8f);

    auto sanMiguelModel =
        KS::ModelImporter::ImportFromFile("assets/models/SanMiguel.glb", aiProcess_FindInstances).value();
    //auto damagedHelmetModel = KS::ModelImporter::ImportFromFile("assets/models/DamagedHelmet.glb").value();
    auto cubeModel = KS::ModelImporter::ImportFromFile("assets/models/Cube.glb").value();
    //auto sphereModel = KS::ModelImporter::ImportFromFile("assets/models/Sphere.glb").value();

    glm::mat4x4 transform = glm::mat4x4(1.f);
    transform = glm::mat4x4(1.f);
    scenes[TEST_SCENE]->QueueModel(*device, cubeModel, transform, "Cube");
    scenes[TEST_SCENE]->QueueModel(*device, sanMiguelModel, transform, "San Miguel");

    device->EndFrame();
    bool recomp = false;
    int chosenScene = 0;
    glm::vec3 lastCameraTranslation = glm::vec3(0.f, 0.f, 0.f);
    glm::vec3 lastCameraRight = glm::vec3(0.f, 0.f, 0.f);
    bool cameraChange = true;

    while (device->IsWindowOpen())
    {
        auto dt = frametimer.Tick();

        input->ProcessInput();
        device->NewFrame();

        glm::vec2 viewportSize = editor->GetViewportSize();
        auto camera = FreeCamSystem(input, ecs->GetWorld(), dt.count(), viewportSize.x / viewportSize.y);
       
        if (lastCameraTranslation != camera.GetPosition() || lastCameraRight != camera.GetRight())
        {
            cameraChange = true;
            lastCameraTranslation = camera.GetPosition();
            lastCameraRight = camera.GetRight();
        }

        auto renderParams = KS::RenderTickParams();
        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();
        renderParams.cameraPos = camera.GetPosition();
        renderParams.cameraRight = camera.GetRight();
        renderParams.frustum = camera.GetFrustum();
        renderParams.cameraUpdated = cameraChange;
        cameraChange = false;

        Scene* activeScene = scenes[TEST_SCENE].get();
        switch(chosenScene){
            case TEST_SCENE:
                activeScene = scenes[TEST_SCENE].get();
                break;
        }

        activeScene->Tick(*device);
        renderer.Render(*device, *activeScene, renderParams, raytraced, recomp);
        recomp = false;

        auto view = ecs->GetWorld().view<KS::ComponentFirstPersonCamera, KS::ComponentTransform>();
        auto it = view.each().begin();
        auto [e, cameraInfo, camTransform] = *it;
        editor->RenderWindows(*device, scenes, KS::ScenesToChoose::COUNT, frametimer.GetFPS(), frametimer.GetMS(), recomp,
                              raytraced,
                              chosenScene,
                              cameraInfo, camTransform);
        device->EndFrame();

    }

    device->Flush();

    return 0;
}