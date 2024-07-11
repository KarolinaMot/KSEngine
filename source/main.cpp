#include <components/ComponentCamera.hpp>
#include <components/ComponentTransform.hpp>
#include <device/Device.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/FileIO.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <input/Input.hpp>
#include <input/mapping/InputContext.hpp>
#include <input/raw_input/RawInputCollector.hpp>
#include <iostream>
#include <math/Geometry.hpp>
#include <memory>
#include <renderer/ModelRenderer.hpp>
#include <renderer/Renderer.hpp>
#include <renderer/Shader.hpp>
#include <renderer/ShaderInputsBuilder.hpp>
#include <resources/Model.hpp>
#include <tools/Log.hpp>
#include <tools/Timer.hpp>

namespace InputActionNames
{

const std::string HORIZONTAL_MOVEMENT = "HORIZONTAL_MOVEMENT";
const std::string VERTICAL_MOVEMENT = "VERTICAL_MOVEMENT";
const std::string UPWARDS_MOVEMENT = "UPWARDS_MOVEMENT";

const std::string MOUSE_POS_VERTICAL = "MOUSE_POS_VERTICAL";
const std::string MOUSE_POS_HORIZONTAL = "MOUSE_POS_HORIZONTAL";

const std::string HOLD_RIGHT_CLICK = "HOLD_RIGHT_CLICK";
}

KS::Camera FreeCamSystem(entt::registry& registry, const KS::FrameInputResult& input, float dt)
{
    constexpr float MOUSE_SENSITIVITY = 0.003f;
    constexpr float CAM_SPEED = 0.003f;

    float dx = input.GetAxisDelta(InputActionNames::MOUSE_POS_HORIZONTAL);
    float dy = input.GetAxisDelta(InputActionNames::MOUSE_POS_VERTICAL);

    // float dx = input.GetAxisDelta(InputActionNames::HORIZONTAL_MOVEMENT);
    // float dy = input.GetAxisDelta(InputActionNames::VERTICAL_MOVEMENT);

    if (dx != 0.0f || dy != 0.0f)
    {
        LOG(Log::Severity::INFO, "{} {}", dx, dy);
    }

    glm::vec3 euler_delta {};

    if (input.GetState(InputActionNames::HOLD_RIGHT_CLICK))
    {
        euler_delta.y = dx * MOUSE_SENSITIVITY;
        euler_delta.x = dy * MOUSE_SENSITIVITY;
    }

    glm::vec3 right = input.GetAxis(InputActionNames::HORIZONTAL_MOVEMENT) * KS::World::RIGHT;
    glm::vec3 forward = input.GetAxis(InputActionNames::VERTICAL_MOVEMENT) * KS::World::FORWARD;
    glm::vec3 upwards = input.GetAxis(InputActionNames::UPWARDS_MOVEMENT) * KS::World::UP;

    glm::vec3 movement_dir = right + forward + upwards;

    if (glm::length(movement_dir) != 0.0f)
    {
        movement_dir = glm::normalize(movement_dir);
    }

    auto view = registry.view<KS::ComponentFirstPersonCamera, KS::ComponentTransform>();
    for (auto&& [e, camera, transform] : view.each())
    {

        camera.eulerAngles += euler_delta;
        camera.eulerAngles.x = glm::clamp(camera.eulerAngles.x, -glm::radians(89.9f), glm::radians(89.9f));

        auto rotation = glm::quat(camera.eulerAngles);
        auto translation = transform.GetLocalTranslation();

        transform.SetLocalTranslation(translation + rotation * (movement_dir * dt * CAM_SPEED));
        transform.SetLocalRotation(rotation);

        return camera.GenerateCamera(transform.GetWorldMatrix());
    }

    return KS::Camera {};
}

KS::InputContext MakeDefaultInputContext()
{
    using namespace KS::RawInput;
    using namespace KS::InputMapping;

    return KS::InputContext()
        .AddBinding(InputActionNames::MOUSE_POS_HORIZONTAL, { Source::KEYBOARD, KeyboardKey::D }, ToAxis { 1.0f })
        .AddBinding(InputActionNames::MOUSE_POS_HORIZONTAL, { Source::KEYBOARD, KeyboardKey::A }, ToAxis { -1.0f })

        .AddBinding(InputActionNames::VERTICAL_MOVEMENT, { Source::KEYBOARD, KeyboardKey::W }, ToAxis { 1.0f })
        .AddBinding(InputActionNames::VERTICAL_MOVEMENT, { Source::KEYBOARD, KeyboardKey::S }, ToAxis { -1.0f })

        .AddBinding(InputActionNames::UPWARDS_MOVEMENT, { Source::KEYBOARD, KeyboardKey::E }, ToAxis { 1.0f })
        .AddBinding(InputActionNames::UPWARDS_MOVEMENT, { Source::KEYBOARD, KeyboardKey::Q }, ToAxis { -1.0f })

        .AddBinding(InputActionNames::MOUSE_POS_HORIZONTAL, { Source::MOUSE_POSITION, Direction::HORIZONTAL }, ToAxis { 1.0f })
        .AddBinding(InputActionNames::MOUSE_POS_VERTICAL, { Source::MOUSE_POSITION, Direction::VERTICAL }, ToAxis { 1.0f })

        .AddBinding(InputActionNames::HOLD_RIGHT_CLICK, { Source::MOUSE_BUTTONS, MouseButton::Right }, ToState());
}

int main()
{
    auto model = KS::ModelImporter::ImportFromFile("assets/models/ElGato.glb").value();

    KS::DeviceInitParams params {};
    params.window_width = 1280;
    params.window_height = 720;

    auto device = std::make_shared<KS::Device>(params);
    auto ecs = std::make_shared<KS::EntityComponentSystem>();

    auto raw_input = KS::RawInputCollector(*device);
    auto input_mapper = MakeDefaultInputContext();
    auto input_result = KS::FrameInputResult();

    device->NewFrame();

    std::shared_ptr<KS::ShaderInputs> mainInputs = KS::ShaderInputsBuilder()
                                                       .AddStruct(KS::ShaderInputVisibility::VERTEX, "camera_matrix")
                                                       .AddStruct(KS::ShaderInputVisibility::VERTEX, "model_matrix")
                                                       .AddTexture(KS::ShaderInputVisibility::PIXEL, "base_tex")
                                                       .AddStaticSampler(KS::ShaderInputVisibility::PIXEL, KS::SamplerDesc {})
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
        auto all_events = raw_input.ProcessInput();
        auto input = input_mapper.ConvertInput(std::move(all_events));

        input_result.StartFrame();
        while (input.size())
        {
            auto& v = input.front();
            input_result.Process(v);
            input.pop();
        }

        auto dt = frametimer.Tick();
        auto camera = FreeCamSystem(ecs->GetWorld(), input_result, dt.count());

        device->NewFrame();

        auto renderParams = KS::RendererRenderParams();

        renderParams.cpuFrame = device->GetFrameIndex();
        renderParams.projectionMatrix = camera.GetProjection();
        renderParams.viewMatrix = camera.GetView();

        auto* model_renderer = dynamic_cast<KS::ModelRenderer*>(renderer.m_subrenderers.front().get());
        glm::mat4x4 transform = glm::translate(glm::mat4x4(1.f), glm::vec3(0.f, -0.5f, 3.f));
        transform = glm::rotate(transform, glm::radians(-180.f), glm::vec3(0.f, 0.f, 1.f));
        model_renderer->QueueModel(model, transform);

        renderer.Render(*device, renderParams);
        device->EndFrame();
    }

    device->Flush();

    return 0;
}