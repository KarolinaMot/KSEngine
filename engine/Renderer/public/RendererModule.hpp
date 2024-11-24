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

    void SetCamera(const Camera& camera) { set_camera = camera; };
    ForwardRenderer& GetRenderer() { return *forward_renderer; }

private:
    void Initialize(Engine& e) override;
    void Shutdown(MAYBE_UNUSED Engine& e) override;
    void RenderFrame(Engine& e);

    std::unique_ptr<DXSwapchain> main_swapchain {};
    std::unique_ptr<ForwardRenderer> forward_renderer {};

    Camera set_camera = Camera::Perspective(
        {}, -World::UP, 16.0f / 9.0f, glm::radians(90.0f), 0.01f, 1000.0f);
};