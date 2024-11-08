#include <Device.hpp>
#include <Engine.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/ResourceHandle.hpp>
#include <input/RawInput.hpp>
#include <memory>
#include <renderer/Renderer.hpp>
#include <renderer/ShaderInputsBuilder.hpp>

class Model;

class RendererModule : public ModuleInterface
{
public:
    virtual ~RendererModule() = default;

private:
    void Initialize(Engine& e) override;

    void Shutdown(MAYBE_UNUSED Engine& e) override
    {
    }

    void UpdateWindow(Engine& e)
    {
        if (device->IsWindowOpen() == false)
        {
            e.SetExit(0);
            return;
        }

        raw_input->ProcessInput();
    }

    void RenderFrame(Engine& e);

    std::shared_ptr<Device> device {};
    std::shared_ptr<EntityComponentSystem> ecs {};
    std::shared_ptr<RawInput> raw_input {};
    std::shared_ptr<ShaderInputs> mainInputs {};
    std::shared_ptr<Renderer> renderer {};

    ResourceHandle<Model> model {};
};