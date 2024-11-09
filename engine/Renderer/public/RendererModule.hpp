#include <Engine.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/ResourceHandle.hpp>
#include <memory>
#include <rendering/Swapchain.hpp>

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

    void RenderFrame(Engine& e);

    std::unique_ptr<Swapchain> main_swapchain {};

    std::shared_ptr<Device> device {};
    std::shared_ptr<EntityComponentSystem> ecs {};
    // std::shared_ptr<ShaderInputs> mainInputs {};
    // std::shared_ptr<Renderer> renderer {};

    ResourceHandle<Model> model {};
};