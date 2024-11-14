#include <Engine.hpp>
#include <Renderer.hpp>
#include <display/DXSwapchain.hpp>
#include <memory>
#include <sync/DXFuture.hpp>

class RendererModule : public ModuleInterface
{
public:
    virtual ~RendererModule() = default;

private:
    void Initialize(Engine& e) override;
    void Shutdown(MAYBE_UNUSED Engine& e) override;

    void RenderFrame(Engine& e);

    std::unique_ptr<DXSwapchain> main_swapchain {};
    std::unique_ptr<Renderer> renderer {};

    Mesh test_mesh {};
    // std::shared_ptr<Device> device {};
    //  std::shared_ptr<ShaderInputs> mainInputs {};
    //  std::shared_ptr<Renderer> renderer {};
    // ResourceHandle<Model> model {};

    // TODO: move Swapchain sync stuff somewhere else
};