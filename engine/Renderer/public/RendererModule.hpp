#pragma once

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

    // std::unique_ptr<DeferredRenderer> deferred_renderer {};
    std::unique_ptr<ForwardRenderer> forward_renderer {};

    glm::vec3 camera_pos = { 0.0f, 0.0f, -3.0f };
    glm::vec3 camera_rot {};

    GPUMesh helmet_mesh {};
    GPUMaterial helmet_material {};
};