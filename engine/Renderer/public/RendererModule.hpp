#include <DeferredRenderer.hpp>
#include <Engine.hpp>
#include <ForwardRenderer.hpp>
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
    void UpdateCamera(Engine& e);

    std::unique_ptr<DXSwapchain> main_swapchain {};

    std::unique_ptr<DeferredRenderer> deferred_renderer {};
    std::unique_ptr<ForwardRenderer> forward_renderer {};

    glm::vec3 camera_pos = { 0.0f, 0.0f, -3.0f };
    glm::vec3 camera_rot {};

    Mesh test_mesh {};
    // std::shared_ptr<Device> device {};
    //  std::shared_ptr<ShaderInputs> mainInputs {};
    //  std::shared_ptr<Renderer> renderer {};
    // ResourceHandle<Model> model {};

    // TODO: move Swapchain sync stuff somewhere else
};