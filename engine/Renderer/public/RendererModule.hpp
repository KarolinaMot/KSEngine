#include <Engine.hpp>
#include <Renderer.hpp>
#include <ecs/EntityComponentSystem.hpp>
#include <fileio/ResourceHandle.hpp>
#include <memory>
#include <rendering/DXSwapchain.hpp>
#include <sync/DXFuture.hpp>

class Model;

class RendererModule : public ModuleInterface
{
public:
    virtual ~RendererModule() = default;

private:
    void Initialize(Engine& e) override;
    void Shutdown(MAYBE_UNUSED Engine& e) override { };

    void RenderFrame(Engine& e);

    std::shared_ptr<EntityComponentSystem> ecs {};
    std::unique_ptr<DXSwapchain> main_swapchain {};
    std::unique_ptr<Renderer> renderer {};

    // std::shared_ptr<Device> device {};
    //  std::shared_ptr<ShaderInputs> mainInputs {};
    //  std::shared_ptr<Renderer> renderer {};
    ResourceHandle<Model> model {};

    // TODO: move Swapchain sync stuff somewhere else
    std::array<DXFuture, FRAME_BUFFER_COUNT> frame_futures {};
    size_t current_cpu_frame {};
};