#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>
#include <device/Device.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/FileIO.hpp>
#include <input/RawInput.hpp>
#include <math/Geometry.hpp>
#include <memory>
#include <renderer/Renderer.hpp>
#include <scene/Scene.hpp>
#include <tools/Log.hpp>
#include <resources/Model.hpp>
#include <tools/Timer.hpp>
#include <editor/Editor.hpp>

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
    auto model = KS::ModelImporter::ImportFromFile("assets/models/Gears.glb").value();

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

        registry.emplace<KS::ComponentTransform>(e, glm::vec3(0.f, 0.f, -1.f));
        registry.emplace<KS::ComponentFirstPersonCamera>(e);
    }

    KS::Timer frametimer {};
    bool raytraced = false;

    glm::vec3 lightPosition1 = glm::vec3(0.f, 0.f, 7.f);
    glm::vec3 lightPosition2 = glm::vec3(-2.f, 0.f, 0.f);

    glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
    glm::mat4x4 transform2 = glm::translate(glm::mat4x4(1.f), glm::vec3(1.f,1.f, 4.f));
    glm::mat4x4 transform3 = glm::translate(glm::mat4x4(1.f), glm::vec3(-1.f, 1.f, 4.f));
    glm::mat4x4 transform4 = glm::translate(glm::mat4x4(1.f), lightPosition1);
    glm::mat4x4 transform5 = glm::translate(glm::mat4x4(1.f), lightPosition2);
    transform = glm::rotate(transform, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
    transform = glm::rotate(transform, glm::radians(40.f), glm::vec3(0.f, 0.f, 1.f));

    transform2 = glm::rotate(transform2, glm::radians(-50.f), glm::vec3(0.f, 1.f, 0.f));
    transform2 = glm::rotate(transform2, glm::radians(120.f), glm::vec3(0.f, 0.f, 1.f));

    transform3 = glm::rotate(transform3, glm::radians(50.f), glm::vec3(0.f, 1.f, 0.f));
    transform3 = glm::rotate(transform3, glm::radians(120.f), glm::vec3(0.f, 0.f, 1.f));

    transform4 = glm::scale(transform4, glm::vec3(0.1f));
    transform5 = glm::scale(transform5, glm::vec3(0.1f));

    float rotationSpeed = 0.1f;

    scene.QueuePointLight(lightPosition1, glm::vec3(0.597202f, 0.450786f, 1.f), 5.f, 10.f);
    scene.QueueModel(*device, model, transform, "Gear1");
    scene.QueueModel(*device, model, transform2, "Gear2");
    scene.QueueModel(*device, model, transform3, "Gear3");

    device->EndFrame();

    while (device->IsWindowOpen())
    {
        auto dt = frametimer.Tick();

        input->ProcessInput();
        device->NewFrame();

        auto camera = FreeCamSystem(input, ecs->GetWorld(), dt.count());

        if (input->GetKeyboard(KS::KeyboardKey::Space) == KS::InputState::Down)
            raytraced = !raytraced;

        auto newTransform = glm::rotate(glm::mat4(1.f), glm::radians(rotationSpeed), glm::vec3(0.f, 1.f, 0.f));
        scene.ApplyModelTransform(*device, "Gear1", newTransform);
        scene.ApplyModelTransform(*device, "Gear2", newTransform);
        scene.ApplyModelTransform(*device, "Gear3", newTransform);

        auto renderParams = KS::RenderTickParams();
        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();
        renderParams.cameraPos = camera.GetPosition();
        renderParams.cameraRight = camera.GetRight();

        scene.Tick(*device);
        renderer.Render(*device, scene, renderParams, raytraced);
        editor->RenderWindows(*device, scene);
        device->EndFrame();
    }

    device->Flush();

    return 0;
}